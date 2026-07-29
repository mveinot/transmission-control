#include "delugebackend.h"

#include "settingskeys.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

#include <algorithm>
#include <utility>

namespace {

const QJsonArray TorrentListFields{
    QStringLiteral("name"),
    QStringLiteral("state"),
    QStringLiteral("progress"),
    QStringLiteral("total_wanted"),
    QStringLiteral("total_remaining"),
    QStringLiteral("total_done"),
    QStringLiteral("all_time_download"),
    QStringLiteral("total_uploaded"),
    QStringLiteral("download_payload_rate"),
    QStringLiteral("upload_payload_rate"),
    QStringLiteral("ratio"),
    QStringLiteral("eta"),
    QStringLiteral("num_seeds"),
    QStringLiteral("total_seeds"),
    QStringLiteral("num_peers"),
    QStringLiteral("total_peers"),
    QStringLiteral("time_added"),
    QStringLiteral("completed_time"),
    QStringLiteral("download_location"),
    QStringLiteral("queue"),
    QStringLiteral("tracker"),
    QStringLiteral("tracker_host"),
    QStringLiteral("label"),
    QStringLiteral("message"),
    QStringLiteral("is_finished"),
    QStringLiteral("paused"),
    QStringLiteral("auto_managed")
};

int normalizedStatus(const QString &state, bool finished)
{
    const QString value = state.trimmed().toLower();
    if (value == QStringLiteral("checking"))
        return static_cast<int>(torrent::Status::Verifying);
    if (value == QStringLiteral("queued")) {
        return static_cast<int>(finished
                                    ? torrent::Status::WaitingToSeed
                                    : torrent::Status::Queued);
    }
    if (value == QStringLiteral("downloading")
        || value == QStringLiteral("allocating")
        || value == QStringLiteral("moving")) {
        return static_cast<int>(torrent::Status::Downloading);
    }
    if (value == QStringLiteral("seeding"))
        return static_cast<int>(torrent::Status::Seeding);

    return static_cast<int>(torrent::Status::Paused);
}

QJsonArray keysToJson(const QList<TorrentKey> &keys)
{
    QJsonArray result;
    for (const TorrentKey &key : keys) {
        if (isValidTorrentKey(key))
            result.append(key);
    }
    return result;
}

QJsonArray singleArrayParameter(const QJsonArray &value)
{
    QJsonArray parameters;
    parameters.append(value);
    return parameters;
}

QByteArray completedPieceBitfield(const QJsonArray &nativePieces)
{
    QByteArray result((nativePieces.size() + 7) / 8, '\0');
    for (int index = 0; index < nativePieces.size(); ++index) {
        const QJsonValue value = nativePieces.at(index);
        const bool complete =
            value.isBool() ? value.toBool() : value.toInt() > 0;
        if (!complete)
            continue;
        const int byteIndex = index / 8;
        const int bitIndex = 7 - (index % 8);
        result[byteIndex] =
            static_cast<char>(
                static_cast<uchar>(result.at(byteIndex))
                | static_cast<uchar>(1u << bitIndex));
    }
    return result;
}

} // namespace

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
    // Core pause/resume/remove/recheck/reannounce/add operations are baseline
    // actions and do not require explicit capability flags.
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
    m_listPendingAfterReady = false;
    m_listInProgress = false;
    m_listRequestedWhileInProgress = false;
    m_requestsPendingAfterAuthentication.clear();
    emit serverChanged();
    emit capabilitiesChanged(capabilities());
}

void DelugeBackend::init()
{
    getTorrentList();
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

void DelugeBackend::queueOrPostRpc(RequestContext context)
{
    if (m_ready) {
        postRpc(context);
        return;
    }

    m_requestsPendingAfterAuthentication.append(context);
    if (!m_authenticated)
        authenticate();
    else
        checkDaemonConnection();
}

void DelugeBackend::postCommand(const QString &rpcMethod,
                                const QJsonArray &parameters,
                                const QString &commandMethod)
{
    RequestContext context;
    context.kind = RequestKind::Command;
    context.method = rpcMethod;
    context.commandMethod = commandMethod;
    context.parameters = parameters;
    queueOrPostRpc(context);
}

void DelugeBackend::postTorrentStatus(RequestKind kind,
                                      const TorrentKey &torrentKey,
                                      const QJsonArray &fields)
{
    if (!isValidTorrentKey(torrentKey))
        return;

    RequestContext context;
    context.kind = kind;
    context.method = QStringLiteral("core.get_torrent_status");
    context.torrentKey = torrentKey;
    context.parameters = QJsonArray{torrentKey, fields};
    queueOrPostRpc(context);
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

void DelugeBackend::sendTorrentListRequest()
{
    if (m_listInProgress)
        return;

    m_listPendingAfterReady = false;
    m_listInProgress = true;
    emit updateStarted();

    RequestContext context;
    context.kind = RequestKind::TorrentList;
    context.method = QStringLiteral("core.get_torrents_status");
    context.parameters = QJsonArray{QJsonObject{}, TorrentListFields};
    postRpc(context);
}

void DelugeBackend::finishTorrentListRequest()
{
    m_listInProgress = false;
    emit updateFinished();

    if (m_listRequestedWhileInProgress) {
        m_listRequestedWhileInProgress = false;
        sendTorrentListRequest();
    }
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
    m_listPendingAfterReady = false;
    m_listInProgress = false;
    m_listRequestedWhileInProgress = false;
    const QList<RequestContext> pending =
        std::exchange(m_requestsPendingAfterAuthentication, {});
    for (const RequestContext &request : pending)
        failRequest(request, reason);
    emit updateFailed(tr("Deluge authentication failed: %1").arg(reason));
}

void DelugeBackend::retryAfterAuthentication(RequestContext context)
{
    context.retriedAuthentication = true;
    if (context.kind != RequestKind::DaemonConnectionCheck)
        m_requestsPendingAfterAuthentication.append(context);
    m_connectionCheckRetriedAuthentication = true;
    m_connectionCheckPending = false;
    authenticate(true);
}

void DelugeBackend::failRequest(const RequestContext &context,
                                const QString &reason)
{
    if (context.kind == RequestKind::TorrentList) {
        emit updateFailed(tr("Deluge torrent request failed: %1").arg(reason));
        m_listInProgress = false;
        m_listRequestedWhileInProgress = false;
        return;
    }

    if (context.kind == RequestKind::AddTorrent) {
        failAdd(context, reason);
        return;
    }

    if (context.kind == RequestKind::Command) {
        emit commandFailed(context.commandMethod, reason);
        return;
    }

    if (context.kind >= RequestKind::TorrentDetails
        && context.kind <= RequestKind::TorrentProperties) {
        // Detail failures are deliberately non-fatal to the list poll. The
        // selected pane will retry on its next normal refresh.
        emit commandFailed(QStringLiteral("torrent-get"), reason);
        return;
    }

    emit updateFailed(tr("Deluge connection check failed: %1").arg(reason));
}

void DelugeBackend::failAdd(const RequestContext &context,
                            const QString &reason)
{
    if (!context.torrentFilePath.isEmpty())
        emit torrentFileAddFailed(context.torrentFilePath, reason);
    emit commandFailed(QStringLiteral("torrent-add"), reason);
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
            retryAfterAuthentication(context);
        } else {
            failRequest(context, reason);
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
            failRequest(context,
                        tr("Invalid response from Deluge: %1").arg(reason));
        return;
    }

    const QJsonObject response = document.object();
    if (response.value(QStringLiteral("id")).toVariant().toLongLong()
        != context.id) {
        emit updateFailed(tr("Deluge returned a mismatched JSON-RPC response."));
        if (context.kind == RequestKind::TorrentList) {
            m_listInProgress = false;
            m_listRequestedWhileInProgress = false;
        }
        return;
    }

    const QJsonValue errorValue = response.value(QStringLiteral("error"));
    if (!errorValue.isNull() && !errorValue.isUndefined()) {
        const QJsonObject error = errorValue.toObject();
        const QString reason = rpcErrorMessage(error);
        if (context.kind != RequestKind::Login
            && isAuthenticationError(error)
            && !context.retriedAuthentication) {
            retryAfterAuthentication(context);
        } else if (context.kind == RequestKind::Login) {
            handleAuthenticationFailure(reason);
        } else {
            failRequest(context, reason);
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

    if (context.kind == RequestKind::DaemonConnectionCheck
        && !response.value(QStringLiteral("result")).toBool()) {
        emit updateFailed(
            tr("Deluge Web is authenticated, but it is not connected to a daemon."));
        m_listPendingAfterReady = false;
        m_listInProgress = false;
        m_listRequestedWhileInProgress = false;
        return;
    }

    if (context.kind == RequestKind::DaemonConnectionCheck) {
        m_ready = true;
        m_connectionCheckRetriedAuthentication = false;

        const QList<RequestContext> pending =
            std::exchange(m_requestsPendingAfterAuthentication, {});
        for (RequestContext request : pending)
            postRpc(request);

        if (m_listPendingAfterReady
            && std::none_of(pending.cbegin(), pending.cend(),
                            [](const RequestContext &request) {
                                return request.kind == RequestKind::TorrentList;
                            })) {
            sendTorrentListRequest();
        } else if (pending.isEmpty()) {
            emit updateFinished();
        }
        return;
    }

    if (context.kind == RequestKind::TorrentList) {
        const QJsonValue result = response.value(QStringLiteral("result"));
        if (!result.isObject()) {
            failRequest(context,
                        tr("Deluge returned an invalid torrent list."));
            return;
        }

        const QJsonObject nativeTorrents = result.toObject();
        QVector<torrent> torrents;
        torrents.reserve(nativeTorrents.size());
        for (auto it = nativeTorrents.constBegin();
             it != nativeTorrents.constEnd(); ++it) {
            if (!it.value().isObject())
                continue;
            torrents.append(
                torrent(normalizeTorrent(it.key(), it.value().toObject())));
        }

        emit torrentsReceived(torrents);
        finishTorrentListRequest();
        return;
    }

    if (context.kind >= RequestKind::TorrentDetails
        && context.kind <= RequestKind::TorrentProperties) {
        const QJsonValue result = response.value(QStringLiteral("result"));
        if (!result.isObject()) {
            failRequest(context,
                        tr("Deluge returned invalid torrent details."));
            return;
        }
        handleTorrentStatus(context, result.toObject());
        return;
    }

    if (context.kind == RequestKind::Command) {
        if (context.method == QStringLiteral("core.remove_torrents")) {
            const QJsonArray errors =
                response.value(QStringLiteral("result")).toArray();
            if (!errors.isEmpty()) {
                emit commandFailed(
                    context.commandMethod,
                    tr("Deluge could not remove %1 torrent(s).")
                        .arg(errors.size()));
                return;
            }
        }

        emit commandSucceeded(context.commandMethod);
        return;
    }

    if (context.kind == RequestKind::AddTorrent) {
        const QString torrentKey =
            response.value(QStringLiteral("result")).toString().trimmed();
        if (!isValidTorrentKey(torrentKey)) {
            failAdd(context, tr("Deluge did not return a torrent ID."));
            return;
        }

        if (!context.torrentFilePath.isEmpty()) {
            if (context.deleteTorrentFileOnSuccess)
                QFile::remove(context.torrentFilePath);
            emit torrentFileAddSucceeded(context.torrentFilePath);
        }
        emit torrentAdded(torrentKey, context.torrentName);
        emit commandSucceeded(QStringLiteral("torrent-add"));
    }
}

void DelugeBackend::handleTorrentStatus(const RequestContext &context,
                                        const QJsonObject &status)
{
    if (context.kind == RequestKind::TorrentDetails) {
        TorrentDetails details;
        details.key = context.torrentKey;
        details.name = status.value(QStringLiteral("name")).toString();
        details.comment = status.value(QStringLiteral("comment")).toString();
        details.creator = status.value(QStringLiteral("creator")).toString();
        details.downloadDirectory =
            status.value(QStringLiteral("download_location")).toString();
        details.hashString =
            status.value(QStringLiteral("hash")).toString(context.torrentKey);
        details.magnetLink =
            status.value(QStringLiteral("magnet_uri")).toString();
        details.totalSize =
            status.value(QStringLiteral("total_size")).toVariant().toLongLong();
        details.creationTime =
            status.value(QStringLiteral("time_created"))
                .toVariant().toLongLong();
        details.hasSequentialDownload =
            status.contains(QStringLiteral("sequential_download"));
        details.sequentialDownload =
            status.value(QStringLiteral("sequential_download")).toBool();
        details.fields = status.toVariantMap();
        emit torrentDetailsReceived(details);
        return;
    }

    if (context.kind == RequestKind::TorrentFiles) {
        TorrentFiles files;
        files.key = context.torrentKey;
        files.downloadDirectory =
            status.value(QStringLiteral("download_location")).toString();
        const QJsonArray nativeFiles =
            status.value(QStringLiteral("files")).toArray();
        const QJsonArray progress =
            status.value(QStringLiteral("file_progress")).toArray();
        const QJsonArray priorities =
            status.value(QStringLiteral("file_priorities")).toArray();
        files.files.reserve(nativeFiles.size());
        for (int index = 0; index < nativeFiles.size(); ++index) {
            const QJsonObject source = nativeFiles.at(index).toObject();
            TorrentFile file;
            file.index = source.value(QStringLiteral("index")).toInt(index);
            file.path = source.value(QStringLiteral("path")).toString();
            file.length =
                source.value(QStringLiteral("size")).toVariant().toLongLong();
            const double fraction =
                index < progress.size() ? progress.at(index).toDouble() : 0.0;
            file.bytesCompleted = qRound64(file.length * fraction);
            const int nativePriority =
                index < priorities.size() ? priorities.at(index).toInt() : 1;
            // Deluge/libtorrent uses 0=unwanted, 1=low, 4=normal, 7=high.
            file.wanted = nativePriority > 0;
            file.priority =
                nativePriority >= 7 ? 1 : (nativePriority == 1 ? -1 : 0);
            files.files.append(file);
        }
        emit torrentFilesReceived(files);
        return;
    }

    if (context.kind == RequestKind::TorrentPeers) {
        TorrentPeers peers;
        peers.key = context.torrentKey;
        const QJsonArray nativePeers =
            status.value(QStringLiteral("peers")).toArray();
        peers.peers.reserve(nativePeers.size());
        for (const QJsonValue &value : nativePeers) {
            const QJsonObject source = value.toObject();
            TorrentPeer peer;
            const QString endpoint =
                source.value(QStringLiteral("ip")).toString();
            const QUrl parsed(QStringLiteral("tcp://") + endpoint);
            peer.address = parsed.host();
            peer.port = parsed.port();
            if (peer.address.isEmpty())
                peer.address = endpoint;
            peer.clientName =
                source.value(QStringLiteral("client")).toString();
            peer.progress =
                source.value(QStringLiteral("progress")).toDouble();
            if (peer.progress > 1.0)
                peer.progress /= 100.0;
            peer.downloadRate =
                source.value(QStringLiteral("down_speed"))
                    .toVariant().toLongLong();
            peer.uploadRate =
                source.value(QStringLiteral("up_speed"))
                    .toVariant().toLongLong();
            peers.peers.append(peer);
        }
        emit torrentPeersReceived(peers);
        return;
    }

    if (context.kind == RequestKind::TorrentTrackers) {
        TorrentTrackers trackers;
        trackers.key = context.torrentKey;
        const QJsonArray nativeTrackers =
            status.value(QStringLiteral("trackers")).toArray();
        trackers.trackers.reserve(nativeTrackers.size());
        for (int index = 0; index < nativeTrackers.size(); ++index) {
            const QJsonObject source = nativeTrackers.at(index).toObject();
            TorrentTracker tracker;
            tracker.id = index;
            tracker.tier = source.value(QStringLiteral("tier")).toInt(-1);
            tracker.announceUrl =
                source.value(QStringLiteral("url")).toString();
            tracker.host = QUrl(tracker.announceUrl).host();
            trackers.trackers.append(tracker);
        }
        emit torrentTrackersReceived(trackers);
        return;
    }

    if (context.kind == RequestKind::TorrentPieces) {
        TorrentPieces pieces;
        pieces.key = context.torrentKey;
        const QJsonArray nativePieces =
            status.value(QStringLiteral("pieces")).toArray();
        pieces.pieceCount = nativePieces.size();
        pieces.completedPieces = completedPieceBitfield(nativePieces);
        pieces.percentDone =
            status.value(QStringLiteral("progress")).toDouble() / 100.0;
        emit torrentPiecesReceived(pieces);
        return;
    }

    TorrentProperties properties;
    properties.key = context.torrentKey;
    properties.name = status.value(QStringLiteral("name")).toString();
    properties.hashString =
        status.value(QStringLiteral("hash")).toString(context.torrentKey);
    properties.queuePosition =
        status.value(QStringLiteral("queue")).toInt();
    properties.peerLimit =
        status.value(QStringLiteral("max_connections")).toInt(-1);
    const int downloadLimit =
        status.value(QStringLiteral("max_download_speed")).toInt(-1);
    const int uploadLimit =
        status.value(QStringLiteral("max_upload_speed")).toInt(-1);
    properties.downloadLimited = downloadLimit >= 0;
    properties.downloadLimit = std::max(0, downloadLimit);
    properties.uploadLimited = uploadLimit >= 0;
    properties.uploadLimit = std::max(0, uploadLimit);
    const double ratioLimit =
        status.value(QStringLiteral("stop_ratio")).toDouble(-1.0);
    properties.seedRatioMode = ratioLimit < 0.0 ? 0 : 1;
    properties.seedRatioLimit = std::max(0.0, ratioLimit);
    const QString label =
        status.value(QStringLiteral("label")).toString().trimmed();
    if (!label.isEmpty())
        properties.labels.append(label);
    properties.fields = status.toVariantMap();
    emit torrentPropertiesReceived(properties);
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
    if (m_listInProgress) {
        m_listRequestedWhileInProgress = true;
        return;
    }
    if (m_ready) {
        sendTorrentListRequest();
        return;
    }

    m_listPendingAfterReady = true;
    if (!m_authenticated) {
        authenticate();
        return;
    }
    checkDaemonConnection();
}

QJsonObject DelugeBackend::normalizeTorrent(const QString &key,
                                            const QJsonObject &source)
{
    const QString hash =
        source.value(QStringLiteral("hash")).toString(key).trimmed();
    const QString state = source.value(QStringLiteral("state")).toString();
    const double progress =
        source.value(QStringLiteral("progress")).toDouble();
    const bool finished =
        source.value(QStringLiteral("is_finished")).toBool()
        || progress >= 100.0
        || state.compare(QStringLiteral("Seeding"),
                         Qt::CaseInsensitive) == 0;
    const qint64 totalWanted =
        source.value(QStringLiteral("total_wanted")).toVariant().toLongLong();
    const qint64 remaining =
        source.value(QStringLiteral("total_remaining")).toVariant().toLongLong();
    const int connectedSeeds =
        source.value(QStringLiteral("num_seeds")).toInt();
    const int connectedPeers =
        source.value(QStringLiteral("num_peers")).toInt();
    const int totalSeeds =
        source.value(QStringLiteral("total_seeds")).toInt(-1);
    const int totalPeers =
        source.value(QStringLiteral("total_peers")).toInt(-1);
    const QString tracker =
        source.value(QStringLiteral("tracker")).toString();
    const QString trackerHost =
        source.value(QStringLiteral("tracker_host")).toString();

    QJsonObject normalized{
        {QStringLiteral("hashString"), hash},
        {QStringLiteral("name"), source.value(QStringLiteral("name"))},
        {QStringLiteral("status"), normalizedStatus(state, finished)},
        {QStringLiteral("percentDone"), progress / 100.0},
        {QStringLiteral("eta"), source.value(QStringLiteral("eta"))},
        {QStringLiteral("rateDownload"),
         source.value(QStringLiteral("download_payload_rate"))},
        {QStringLiteral("rateUpload"),
         source.value(QStringLiteral("upload_payload_rate"))},
        {QStringLiteral("uploadRatio"), source.value(QStringLiteral("ratio"))},
        {QStringLiteral("sizeWhenDone"), totalWanted},
        {QStringLiteral("totalSize"), totalWanted},
        {QStringLiteral("addedDate"), source.value(QStringLiteral("time_added"))},
        {QStringLiteral("doneDate"),
         source.value(QStringLiteral("completed_time"))},
        {QStringLiteral("downloadedEver"),
         source.value(QStringLiteral("all_time_download"))},
        {QStringLiteral("uploadedEver"),
         source.value(QStringLiteral("total_uploaded"))},
        {QStringLiteral("downloadDir"),
         source.value(QStringLiteral("download_location"))},
        {QStringLiteral("peersConnected"),
         connectedSeeds + connectedPeers},
        {QStringLiteral("peersSendingToUs"), connectedSeeds},
        {QStringLiteral("peersGettingFromUs"), connectedPeers},
        {QStringLiteral("queuePosition"),
         source.value(QStringLiteral("queue"))},
        {QStringLiteral("leftUntilDone"), remaining},
        {QStringLiteral("desiredAvailable"), remaining}
    };

    if (!tracker.isEmpty()) {
        normalized.insert(
            QStringLiteral("trackers"),
            QJsonArray{QJsonObject{{QStringLiteral("announce"), tracker}}});
    }
    normalized.insert(
        QStringLiteral("trackerStats"),
        QJsonArray{QJsonObject{
            {QStringLiteral("host"), trackerHost},
            {QStringLiteral("announce"), tracker},
            {QStringLiteral("seederCount"), totalSeeds},
            {QStringLiteral("leecherCount"), totalPeers}
        }});

    if (source.contains(QStringLiteral("label"))) {
        QJsonArray labels;
        const QString label =
            source.value(QStringLiteral("label")).toString().trimmed();
        if (!label.isEmpty())
            labels.append(label);
        normalized.insert(QStringLiteral("labels"), labels);
    }

    const QString message =
        source.value(QStringLiteral("message")).toString().trimmed();
    if (state.compare(QStringLiteral("Error"), Qt::CaseInsensitive) == 0
        || (!message.isEmpty()
            && message.compare(QStringLiteral("OK"),
                               Qt::CaseInsensitive) != 0)) {
        normalized.insert(QStringLiteral("error"), 1);
        normalized.insert(QStringLiteral("errorString"),
                          message.isEmpty() ? state : message);
    }

    return normalized;
}

void DelugeBackend::emitUnsupported(const QString &operation)
{
    emit commandFailed(
        operation,
        tr("%1 is not implemented for Deluge yet.").arg(operation));
}

void DelugeBackend::getTorrentTrackerMetadata() {}
void DelugeBackend::getTorrentDetails(const TorrentKey &key)
{
    postTorrentStatus(
        RequestKind::TorrentDetails, key,
        QJsonArray{
            QStringLiteral("name"), QStringLiteral("comment"),
            QStringLiteral("creator"), QStringLiteral("download_location"),
            QStringLiteral("hash"), QStringLiteral("magnet_uri"),
            QStringLiteral("total_size"), QStringLiteral("time_created"),
            QStringLiteral("sequential_download"), QStringLiteral("state"),
            QStringLiteral("progress"), QStringLiteral("ratio"),
            QStringLiteral("eta"), QStringLiteral("tracker_status")
        });
}
void DelugeBackend::getTorrentFiles(const TorrentKey &key)
{
    postTorrentStatus(
        RequestKind::TorrentFiles, key,
        QJsonArray{
            QStringLiteral("download_location"), QStringLiteral("files"),
            QStringLiteral("file_progress"), QStringLiteral("file_priorities")
        });
}
void DelugeBackend::getTorrentPeers(const TorrentKey &key)
{
    postTorrentStatus(RequestKind::TorrentPeers, key,
                      QJsonArray{QStringLiteral("peers")});
}
void DelugeBackend::getTorrentTrackers(const TorrentKey &key)
{
    postTorrentStatus(RequestKind::TorrentTrackers, key,
                      QJsonArray{QStringLiteral("trackers")});
}
void DelugeBackend::getTorrentPieces(const TorrentKey &key)
{
    postTorrentStatus(
        RequestKind::TorrentPieces, key,
        QJsonArray{QStringLiteral("pieces"), QStringLiteral("progress")});
}
void DelugeBackend::getTorrentProperties(const TorrentKey &key)
{
    postTorrentStatus(
        RequestKind::TorrentProperties, key,
        QJsonArray{
            QStringLiteral("name"), QStringLiteral("hash"),
            QStringLiteral("queue"), QStringLiteral("max_connections"),
            QStringLiteral("max_download_speed"),
            QStringLiteral("max_upload_speed"), QStringLiteral("stop_ratio"),
            QStringLiteral("label")
        });
}
void DelugeBackend::cancelTorrentDetailRequests()
{
    const QList<QNetworkReply *> replies = m_requests.keys();
    for (QNetworkReply *reply : replies) {
        const RequestContext context = m_requests.value(reply);
        if (context.kind < RequestKind::TorrentDetails
            || context.kind > RequestKind::TorrentProperties) {
            continue;
        }
        m_requests.remove(reply);
        reply->abort();
        reply->deleteLater();
    }

    m_requestsPendingAfterAuthentication.erase(
        std::remove_if(
            m_requestsPendingAfterAuthentication.begin(),
            m_requestsPendingAfterAuthentication.end(),
            [](const RequestContext &context) {
                return context.kind >= RequestKind::TorrentDetails
                       && context.kind <= RequestKind::TorrentProperties;
            }),
        m_requestsPendingAfterAuthentication.end());
}
void DelugeBackend::addTorrentFromFile(const QString &filePath,
                                       bool deleteFileOnSuccess)
{
    addTorrentFile(filePath, QString(), false, {}, {}, {},
                   deleteFileOnSuccess);
}
void DelugeBackend::addTorrentFromMagnet(const QString &magnetLink)
{
    addMagnetLink(magnetLink, QString(), false);
}
void DelugeBackend::addTorrentFile(const QString &filePath,
                                   const QString &downloadDir,
                                   bool paused,
                                   const QList<int> &,
                                   const QList<int> &,
                                   const QList<int> &,
                                   bool deleteFileOnSuccess)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        RequestContext context;
        context.torrentFilePath = filePath;
        failAdd(context,
                tr("Could not open torrent file: %1")
                    .arg(file.errorString()));
        return;
    }

    QJsonObject options{
        {QStringLiteral("add_paused"), paused}
    };
    if (!downloadDir.trimmed().isEmpty()) {
        options.insert(QStringLiteral("download_location"),
                       downloadDir.trimmed());
    }

    RequestContext context;
    context.kind = RequestKind::AddTorrent;
    context.method = QStringLiteral("core.add_torrent_file_async");
    context.commandMethod = QStringLiteral("torrent-add");
    context.torrentFilePath = filePath;
    context.torrentName = QFileInfo(filePath).completeBaseName();
    context.deleteTorrentFileOnSuccess = deleteFileOnSuccess;
    context.parameters = QJsonArray{
        QFileInfo(filePath).fileName(),
        QString::fromLatin1(file.readAll().toBase64()),
        options
    };
    queueOrPostRpc(context);
}
void DelugeBackend::addMagnetLink(const QString &magnetLink,
                                  const QString &downloadDir,
                                  bool paused)
{
    const QString normalized = magnetLink.trimmed();
    if (normalized.isEmpty()) {
        RequestContext context;
        failAdd(context, tr("No magnet link was specified."));
        return;
    }

    QJsonObject options{
        {QStringLiteral("add_paused"), paused}
    };
    if (!downloadDir.trimmed().isEmpty()) {
        options.insert(QStringLiteral("download_location"),
                       downloadDir.trimmed());
    }

    RequestContext context;
    context.kind = RequestKind::AddTorrent;
    context.method = QStringLiteral("core.add_torrent_magnet");
    context.commandMethod = QStringLiteral("torrent-add");
    context.parameters = QJsonArray{normalized, options};
    queueOrPostRpc(context);
}
void DelugeBackend::startTorrents(const QList<TorrentKey> &keys)
{
    const QJsonArray ids = keysToJson(keys);
    if (!ids.isEmpty()) {
        postCommand(QStringLiteral("core.resume_torrents"),
                    singleArrayParameter(ids),
                    QStringLiteral("torrent-start"));
    }
}
void DelugeBackend::startAllTorrents()
{
    postCommand(QStringLiteral("core.resume_torrents"), {},
                QStringLiteral("torrent-start"));
}
void DelugeBackend::startTorrentsNow(const QList<TorrentKey> &)
{ emitUnsupported(tr("Force start torrents")); }
void DelugeBackend::stopTorrents(const QList<TorrentKey> &keys)
{
    const QJsonArray ids = keysToJson(keys);
    if (!ids.isEmpty()) {
        postCommand(QStringLiteral("core.pause_torrents"),
                    singleArrayParameter(ids),
                    QStringLiteral("torrent-stop"));
    }
}
void DelugeBackend::stopAllTorrents()
{
    postCommand(QStringLiteral("core.pause_torrents"), {},
                QStringLiteral("torrent-stop"));
}
void DelugeBackend::removeTorrents(const QList<TorrentKey> &keys,
                                   bool deleteLocalData)
{
    const QJsonArray ids = keysToJson(keys);
    if (!ids.isEmpty()) {
        postCommand(QStringLiteral("core.remove_torrents"),
                    QJsonArray{ids, deleteLocalData},
                    QStringLiteral("torrent-remove"));
    }
}
void DelugeBackend::verifyTorrents(const QList<TorrentKey> &keys)
{
    const QJsonArray ids = keysToJson(keys);
    if (!ids.isEmpty()) {
        postCommand(QStringLiteral("core.force_recheck"),
                    singleArrayParameter(ids),
                    QStringLiteral("torrent-verify"));
    }
}
void DelugeBackend::reannounceTorrents(const QList<TorrentKey> &keys)
{
    const QJsonArray ids = keysToJson(keys);
    if (!ids.isEmpty()) {
        postCommand(QStringLiteral("core.force_reannounce"),
                    singleArrayParameter(ids),
                    QStringLiteral("torrent-reannounce"));
    }
}
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
