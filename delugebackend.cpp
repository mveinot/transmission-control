#include "delugebackend.h"

#include "settingskeys.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

DelugeBackend::DelugeBackend(QObject *parent)
    : TorrentBackend(parent)
{
    connect(&m_network, &QNetworkAccessManager::finished,
            this, &DelugeBackend::handleReply);
}

QString DelugeBackend::backendName() const
{
    return QStringLiteral("Deluge");
}

QString DelugeBackend::serverDisplayName() const
{
    return !m_serverName.isEmpty() ? m_serverName : m_baseUrl;
}

QString DelugeBackend::endpointUrl() const
{
    return m_baseUrl;
}

TorrentBackendCapabilities DelugeBackend::capabilities() const
{
    // Capabilities are enabled only when their normalized implementation is
    // present; authentication alone must not expose non-functional controls.
    return {};
}

bool DelugeBackend::loadCurrentServerFromSettings()
{
    QSettings settings;
    const int defaultIndex =
        settings.value(SettingsKeys::ServersDefaultIndex, -1).toInt();
    if (setServerFromSettingsIndex(defaultIndex))
        return true;

    const int currentIndex =
        settings.value(SettingsKeys::ServersCurrentIndex, -1).toInt();
    if (currentIndex != defaultIndex && setServerFromSettingsIndex(currentIndex))
        return true;

    return setServerFromSettingsIndex(0);
}

bool DelugeBackend::setServerFromSettingsIndex(int index)
{
    QSettings settings;
    const int count = settings.beginReadArray(SettingsKeys::ServersArray);
    if (index < 0 || index >= count) {
        settings.endArray();
        return false;
    }

    settings.setArrayIndex(index);
    const QString type =
        settings.value(SettingsKeys::ServerBackendType,
                       QStringLiteral("transmission"))
            .toString().trimmed().toLower();
    const QString name =
        settings.value(SettingsKeys::ServerName).toString().trimmed();
    const QString url =
        settings.value(SettingsKeys::ServerRpcUrl).toString().trimmed();
    const QString password =
        settings.value(SettingsKeys::ServerPassword).toString();
    settings.endArray();

    const QUrl parsedUrl(url);
    if (type != QStringLiteral("deluge")
        || !parsedUrl.isValid()
        || parsedUrl.scheme().isEmpty()
        || parsedUrl.host().isEmpty()) {
        return false;
    }

    setServer(name, url, password);
    return true;
}

void DelugeBackend::setServer(const QString &name, const QString &url,
                              const QString &password)
{
    // Incrementing the generation makes a late callback harmless even if a
    // platform network stack completes it after abortRequests().
    ++m_generation;
    abortRequests();
    m_serverName = name;
    m_baseUrl = url.trimmed();
    while (m_baseUrl.endsWith(QLatin1Char('/')))
        m_baseUrl.chop(1);
    m_rpcUrl = m_baseUrl.endsWith(QStringLiteral("/json"))
                   ? m_baseUrl
                   : m_baseUrl + QStringLiteral("/json");
    m_password = password;
    m_authenticated = false;
    m_ready = false;
    m_authenticationPending = false;
    m_connectionCheckPending = false;
    m_connectionCheckRetriedAuthentication = false;
    emit serverChanged();
    emit capabilitiesChanged(capabilities());
}

void DelugeBackend::init()
{
    emit updateStarted();
    authenticate();
}

QNetworkRequest DelugeBackend::makeRequest() const
{
    QNetworkRequest request{QUrl(m_rpcUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Planetary"));
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(30000);
#endif
    return request;
}

void DelugeBackend::postRpc(RequestContext context)
{
    context.id = m_nextRequestId++;
    context.generation = m_generation;

    const QJsonObject payload{
        {QStringLiteral("method"), context.method},
        {QStringLiteral("params"), context.parameters},
        {QStringLiteral("id"), context.id}
    };
    QNetworkReply *reply =
        m_network.post(makeRequest(),
                       QJsonDocument(payload).toJson(QJsonDocument::Compact));
    m_requests.insert(reply, context);
}

void DelugeBackend::authenticate(bool preserveConnectionRetry)
{
    if (m_rpcUrl.isEmpty() || m_authenticationPending)
        return;

    if (!preserveConnectionRetry)
        m_connectionCheckRetriedAuthentication = false;
    m_authenticated = false;
    m_ready = false;
    m_authenticationPending = true;
    RequestContext context;
    context.kind = RequestKind::Login;
    context.method = QStringLiteral("auth.login");
    context.parameters = QJsonArray{m_password};
    postRpc(context);
}

void DelugeBackend::checkDaemonConnection()
{
    if (!m_authenticated || m_connectionCheckPending)
        return;

    m_connectionCheckPending = true;
    RequestContext context;
    context.kind = RequestKind::DaemonConnectionCheck;
    context.method = QStringLiteral("web.connected");
    context.retriedAuthentication =
        m_connectionCheckRetriedAuthentication;
    postRpc(context);
}

QString DelugeBackend::rpcErrorMessage(const QJsonObject &error)
{
    const QString message = error.value(QStringLiteral("message")).toString();
    if (!message.isEmpty())
        return message;

    return tr("Deluge returned JSON-RPC error %1.")
        .arg(error.value(QStringLiteral("code")).toInt());
}

bool DelugeBackend::isAuthenticationError(const QJsonObject &error)
{
    const QString message =
        error.value(QStringLiteral("message")).toString().toLower();
    return message.contains(QStringLiteral("not authenticated"))
           || message.contains(QStringLiteral("authentication required"))
           || message.contains(QStringLiteral("session expired"));
}

void DelugeBackend::handleAuthenticationFailure(const QString &reason)
{
    m_authenticated = false;
    m_ready = false;
    m_authenticationPending = false;
    m_connectionCheckPending = false;
    m_connectionCheckRetriedAuthentication = false;
    emit updateFailed(tr("Deluge authentication failed: %1").arg(reason));
}

void DelugeBackend::retryAfterAuthentication()
{
    // Stage one has only the readiness check, so re-authentication can safely
    // resume it after login without maintaining a general pending RPC queue.
    m_connectionCheckRetriedAuthentication = true;
    m_connectionCheckPending = false;
    authenticate(true);
}

void DelugeBackend::handleReply(QNetworkReply *reply)
{
    if (!m_requests.contains(reply)) {
        reply->deleteLater();
        return;
    }

    const RequestContext context = m_requests.take(reply);
    const QByteArray body = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (context.generation != m_generation)
        return;

    if (context.kind == RequestKind::Login)
        m_authenticationPending = false;
    else if (context.kind == RequestKind::DaemonConnectionCheck)
        m_connectionCheckPending = false;

    if (networkError != QNetworkReply::NoError) {
        const QString reason =
            httpStatus > 0
                ? tr("the server returned HTTP %1").arg(httpStatus)
                : networkErrorText;
        if (context.kind == RequestKind::Login) {
            handleAuthenticationFailure(reason);
        } else if ((httpStatus == 401 || httpStatus == 403)
                   && !context.retriedAuthentication) {
            retryAfterAuthentication();
        } else {
            emit updateFailed(
                tr("Deluge connection check failed: %1").arg(reason));
        }
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        const QString reason =
            parseError.error != QJsonParseError::NoError
                ? parseError.errorString()
                : tr("the response was not a JSON object");
        if (context.kind == RequestKind::Login)
            handleAuthenticationFailure(reason);
        else
            emit updateFailed(tr("Invalid response from Deluge: %1").arg(reason));
        return;
    }

    const QJsonObject response = document.object();
    if (response.value(QStringLiteral("id")).toVariant().toLongLong()
        != context.id) {
        emit updateFailed(tr("Deluge returned a mismatched JSON-RPC response."));
        return;
    }

    const QJsonValue errorValue = response.value(QStringLiteral("error"));
    if (!errorValue.isNull() && !errorValue.isUndefined()) {
        const QJsonObject error = errorValue.toObject();
        const QString reason = rpcErrorMessage(error);
        if (context.kind != RequestKind::Login
            && isAuthenticationError(error)
            && !context.retriedAuthentication) {
            retryAfterAuthentication();
        } else if (context.kind == RequestKind::Login) {
            handleAuthenticationFailure(reason);
        } else {
            emit updateFailed(tr("Deluge request failed: %1").arg(reason));
        }
        return;
    }

    if (context.kind == RequestKind::Login) {
        if (!response.value(QStringLiteral("result")).toBool()) {
            handleAuthenticationFailure(
                tr("the Web UI rejected the password"));
            return;
        }

        m_authenticated = true;
        checkDaemonConnection();
        return;
    }

    if (!response.value(QStringLiteral("result")).toBool()) {
        emit updateFailed(
            tr("Deluge Web is authenticated, but it is not connected to a daemon."));
        return;
    }

    // Authentication and daemon reachability form the stage-one readiness
    // boundary. The torrent-list request is intentionally deferred to stage 2.
    m_ready = true;
    m_connectionCheckRetriedAuthentication = false;
    emit updateFinished();
}

void DelugeBackend::abortRequests()
{
    const QList<QNetworkReply *> replies = m_requests.keys();
    m_requests.clear();
    for (QNetworkReply *reply : replies) {
        reply->abort();
        reply->deleteLater();
    }
}

void DelugeBackend::getTorrentList()
{
    if (m_ready)
        return;
    if (!m_authenticated) {
        authenticate();
        return;
    }
    checkDaemonConnection();
}

void DelugeBackend::emitUnsupported(const QString &operation)
{
    emit commandFailed(
        operation,
        tr("%1 is not implemented for Deluge yet.").arg(operation));
}

void DelugeBackend::getTorrentTrackerMetadata() {}
void DelugeBackend::getTorrentDetails(const TorrentKey &) {}
void DelugeBackend::getTorrentFiles(const TorrentKey &) {}
void DelugeBackend::getTorrentPeers(const TorrentKey &) {}
void DelugeBackend::getTorrentTrackers(const TorrentKey &) {}
void DelugeBackend::getTorrentPieces(const TorrentKey &) {}
void DelugeBackend::getTorrentProperties(const TorrentKey &) {}
void DelugeBackend::cancelTorrentDetailRequests() {}
void DelugeBackend::addTorrentFromFile(const QString &, bool)
{ emitUnsupported(tr("Add torrent")); }
void DelugeBackend::addTorrentFromMagnet(const QString &)
{ emitUnsupported(tr("Add torrent")); }
void DelugeBackend::addTorrentFile(const QString &, const QString &, bool,
                                   const QList<int> &, const QList<int> &,
                                   const QList<int> &, bool)
{ emitUnsupported(tr("Add torrent")); }
void DelugeBackend::addMagnetLink(const QString &, const QString &, bool)
{ emitUnsupported(tr("Add torrent")); }
void DelugeBackend::startTorrents(const QList<TorrentKey> &)
{ emitUnsupported(tr("Start torrents")); }
void DelugeBackend::startAllTorrents()
{ emitUnsupported(tr("Start all torrents")); }
void DelugeBackend::startTorrentsNow(const QList<TorrentKey> &)
{ emitUnsupported(tr("Force start torrents")); }
void DelugeBackend::stopTorrents(const QList<TorrentKey> &)
{ emitUnsupported(tr("Stop torrents")); }
void DelugeBackend::stopAllTorrents()
{ emitUnsupported(tr("Stop all torrents")); }
void DelugeBackend::removeTorrents(const QList<TorrentKey> &, bool)
{ emitUnsupported(tr("Remove torrents")); }
void DelugeBackend::verifyTorrents(const QList<TorrentKey> &)
{ emitUnsupported(tr("Verify torrents")); }
void DelugeBackend::reannounceTorrents(const QList<TorrentKey> &)
{ emitUnsupported(tr("Reannounce torrents")); }
void DelugeBackend::setTorrentLocation(const QList<TorrentKey> &,
                                       const QString &, bool)
{ emitUnsupported(tr("Set location")); }
void DelugeBackend::setTorrentFilesWanted(const TorrentKey &,
                                          const QList<int> &, bool)
{ emitUnsupported(tr("Set file selection")); }
void DelugeBackend::setTorrentFilesPriority(const TorrentKey &,
                                            const QList<int> &, int)
{ emitUnsupported(tr("Set file priority")); }
void DelugeBackend::setTorrentFilesWantedAndPriority(
    const TorrentKey &, const QList<int> &, bool, int)
{ emitUnsupported(tr("Set file selection and priority")); }
void DelugeBackend::addTorrentTracker(const TorrentKey &, const QString &)
{ emitUnsupported(tr("Add tracker")); }
void DelugeBackend::editTorrentTracker(const TorrentKey &, int,
                                       const QString &)
{ emitUnsupported(tr("Edit tracker")); }
void DelugeBackend::removeTorrentTracker(const TorrentKey &, int)
{ emitUnsupported(tr("Remove tracker")); }
void DelugeBackend::renameTorrentPath(const TorrentKey &, const QString &,
                                      const QString &)
{ emitUnsupported(tr("Rename path")); }
void DelugeBackend::setTorrentProperties(const TorrentKey &,
                                         const TorrentPropertyChanges &)
{ emitUnsupported(tr("Set torrent properties")); }
void DelugeBackend::setTorrentsSequentialDownload(
    const QList<TorrentKey> &, bool)
{ emitUnsupported(tr("Set sequential download")); }
void DelugeBackend::setTorrentsBandwidthPriority(
    const QList<TorrentKey> &, int)
{ emitUnsupported(tr("Set priority")); }
void DelugeBackend::queueMoveTop(const QList<TorrentKey> &)
{ emitUnsupported(tr("Move to top of queue")); }
void DelugeBackend::queueMoveUp(const QList<TorrentKey> &)
{ emitUnsupported(tr("Move up in queue")); }
void DelugeBackend::queueMoveDown(const QList<TorrentKey> &)
{ emitUnsupported(tr("Move down in queue")); }
void DelugeBackend::queueMoveBottom(const QList<TorrentKey> &)
{ emitUnsupported(tr("Move to bottom of queue")); }
void DelugeBackend::getSessionSettings() {}
void DelugeBackend::getSessionStatistics() {}
void DelugeBackend::setSessionSettings(const QJsonObject &)
{ emitUnsupported(tr("Set session settings")); }
void DelugeBackend::getFreeSpace(const QString &) {}
void DelugeBackend::testPortForwarding() {}
void DelugeBackend::updateBlocklist(const QJsonObject &)
{ emitUnsupported(tr("Update blocklist")); }
