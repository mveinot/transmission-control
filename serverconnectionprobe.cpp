#include "serverconnectionprobe.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <utility>

namespace {
constexpr int ConnectionTestTimeoutMs = 10000;

int defaultPort(const QString &backendType)
{
    if (backendType == QStringLiteral("qbittorrent"))
        return 8080;
    if (backendType == QStringLiteral("deluge"))
        return 8112;
    return 9091;
}

bool isAuthenticationResponse(int status, const QByteArray &body)
{
    return status == 401 || status == 403
           || body.trimmed() == QByteArrayLiteral("Fails.");
}
}

ServerConnectionProbe::ServerConnectionProbe(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    connect(m_network, &QNetworkAccessManager::finished,
            this, &ServerConnectionProbe::handleReply);
}

void ServerConnectionProbe::start(const QString &backendType,
                                  const QUrl &enteredUrl,
                                  const QString &username,
                                  const QString &password)
{
    cancel();
    m_backendType = backendType.trimmed().toLower();
    m_username = username.trimmed();
    m_password = password;
    m_candidates = buildCandidates(m_backendType, enteredUrl);
    m_candidateIndex = -1;
    m_retryScope = RetryScope::None;

    if (m_candidates.isEmpty()) {
        finishFailure(tr("Enter a valid HTTP or HTTPS server URL."),
                      FailureKind::Connection);
        return;
    }

    beginCandidate(0);
}

void ServerConnectionProbe::cancel()
{
    if (m_reply) {
        // Clear identity before aborting: some network backends emit finished
        // synchronously from abort(), and that reply must remain obsolete.
        QNetworkReply *reply = std::exchange(m_reply, nullptr);
        reply->abort();
        reply->deleteLater();
    }

    m_candidates.clear();
    m_candidateIndex = -1;
    m_stage = Stage::None;
    m_retryScope = RetryScope::None;
    m_transmissionSessionRetried = false;
}

bool ServerConnectionProbe::isRunning() const
{
    return m_reply != nullptr;
}

QList<QUrl> ServerConnectionProbe::candidateUrls(
    const QString &backendType, const QUrl &enteredUrl)
{
    QList<QUrl> urls;
    const QVector<Candidate> candidates =
        buildCandidates(backendType.trimmed().toLower(), enteredUrl);
    urls.reserve(candidates.size());
    for (const Candidate &candidate : candidates)
        urls.append(candidate.url);
    return urls;
}

QVector<ServerConnectionProbe::Candidate>
ServerConnectionProbe::buildCandidates(const QString &backendType,
                                       const QUrl &enteredUrl)
{
    if (!enteredUrl.isValid()
        || (enteredUrl.scheme() != QStringLiteral("http")
            && enteredUrl.scheme() != QStringLiteral("https"))
        || enteredUrl.host().isEmpty()) {
        return {};
    }

    QVector<Candidate> candidates;
    const auto appendUnique = [&candidates](Candidate candidate) {
        const QString encoded = candidate.url.toString(QUrl::FullyEncoded);
        for (const Candidate &existing : std::as_const(candidates)) {
            if (existing.url.toString(QUrl::FullyEncoded) == encoded)
                return;
        }
        candidates.append(candidate);
    };

    Candidate original{enteredUrl, false, false};
    appendUnique(original);

    const bool portMissing = enteredUrl.port() < 0;
    QUrl portAdjusted = enteredUrl;
    if (portMissing)
        portAdjusted.setPort(defaultPort(backendType));

    if (backendType == QStringLiteral("transmission")) {
        const QUrl pathAdjusted =
            appendPath(enteredUrl, QStringLiteral("transmission/rpc"));
        appendUnique({pathAdjusted, false, pathAdjusted != enteredUrl});

        if (portMissing)
            appendUnique({portAdjusted, true, false});

        const QUrl fullyAdjusted =
            appendPath(portAdjusted, QStringLiteral("transmission/rpc"));
        appendUnique({fullyAdjusted, portMissing,
                      fullyAdjusted.path() != enteredUrl.path()});
    } else {
        if (backendType == QStringLiteral("qbittorrent")) {
            const QString path = enteredUrl.path();
            const QString apiSuffix = QStringLiteral("/api/v2/auth/login");
            if (path.endsWith(apiSuffix)) {
                QUrl baseAdjusted = enteredUrl;
                QString basePath = path.left(path.size() - apiSuffix.size());
                if (basePath.endsWith(QLatin1Char('/')))
                    basePath.chop(1);
                baseAdjusted.setPath(basePath);
                appendUnique({baseAdjusted, false, true});

                if (portMissing) {
                    baseAdjusted.setPort(defaultPort(backendType));
                    appendUnique({baseAdjusted, true, true});
                }
            } else if (portMissing) {
                appendUnique({portAdjusted, true, false});
            }
        } else if (portMissing) {
            appendUnique({portAdjusted, true, false});
        }
    }

    return candidates;
}

QUrl ServerConnectionProbe::appendPath(const QUrl &baseUrl,
                                       const QString &suffix)
{
    QUrl adjusted = baseUrl;
    QString path = adjusted.path();
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);

    if (path.endsWith(QStringLiteral("/transmission"),
                      Qt::CaseInsensitive)
        && suffix == QStringLiteral("transmission/rpc")) {
        path += QStringLiteral("/rpc");
    } else if (!path.endsWith(QStringLiteral("/transmission/rpc"),
                              Qt::CaseInsensitive)) {
        path += QLatin1Char('/') + suffix;
    }

    adjusted.setPath(path);
    return adjusted;
}

void ServerConnectionProbe::beginCandidate(int index)
{
    if (index < 0 || index >= m_candidates.size())
        return;

    m_candidateIndex = index;
    m_transmissionSessionRetried = false;

    if (m_backendType == QStringLiteral("qbittorrent"))
        sendQBittorrentLogin();
    else if (m_backendType == QStringLiteral("deluge"))
        sendDelugeRequest(Stage::DelugeLogin);
    else
        sendTransmissionRequest();
}

void ServerConnectionProbe::sendTransmissionRequest(
    const QByteArray &sessionToken)
{
    QNetworkRequest request(m_candidates.at(m_candidateIndex).url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Planetary"));
    request.setTransferTimeout(ConnectionTestTimeoutMs);
    if (!sessionToken.isEmpty())
        request.setRawHeader("X-Transmission-Session-Id", sessionToken);
    if (!m_username.isEmpty() || !m_password.isEmpty()) {
        request.setRawHeader(
            "Authorization",
            QByteArrayLiteral("Basic ")
                + (m_username + QLatin1Char(':') + m_password)
                      .toUtf8().toBase64());
    }

    const QJsonObject payload{
        {QStringLiteral("method"), QStringLiteral("session-get")},
        {QStringLiteral("arguments"),
         QJsonObject{{QStringLiteral("fields"),
                      QJsonArray{QStringLiteral("version")}}}}};
    m_stage = Stage::Transmission;
    m_reply = m_network->post(
        request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void ServerConnectionProbe::sendQBittorrentLogin()
{
    QUrl loginUrl = appendPath(m_candidates.at(m_candidateIndex).url,
                               QStringLiteral("api/v2/auth/login"));
    QNetworkRequest request(loginUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Planetary"));
    request.setTransferTimeout(ConnectionTestTimeoutMs);

    QUrl origin = m_candidates.at(m_candidateIndex).url;
    origin.setPath(QString());
    origin.setQuery(QString());
    origin.setFragment(QString());
    const QByteArray originValue = origin.toString(QUrl::RemovePath).toUtf8();
    request.setRawHeader("Origin", originValue);
    request.setRawHeader("Referer", originValue + '/');

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("username"), m_username);
    form.addQueryItem(QStringLiteral("password"), m_password);
    m_stage = Stage::QBittorrentLogin;
    m_reply = m_network->post(
        request, form.toString(QUrl::FullyEncoded).toUtf8());
}

void ServerConnectionProbe::sendDelugeRequest(Stage stage)
{
    QUrl rpcUrl = m_candidates.at(m_candidateIndex).url;
    QString rpcPath = rpcUrl.path();
    while (rpcPath.endsWith(QLatin1Char('/')))
        rpcPath.chop(1);
    rpcUrl.setPath(rpcPath);
    if (!rpcUrl.path().endsWith(QStringLiteral("/json")))
        rpcUrl = appendPath(rpcUrl, QStringLiteral("json"));

    QNetworkRequest request(rpcUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Planetary"));
    request.setTransferTimeout(ConnectionTestTimeoutMs);

    const QString method = stage == Stage::DelugeLogin
                               ? QStringLiteral("auth.login")
                               : QStringLiteral("web.connected");
    const QJsonArray parameters = stage == Stage::DelugeLogin
                                      ? QJsonArray{m_password}
                                      : QJsonArray{};
    const QJsonObject payload{
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), parameters},
        {QStringLiteral("id"), 1}};
    m_stage = stage;
    m_reply = m_network->post(
        request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

bool ServerConnectionProbe::isTlsError(QNetworkReply *reply)
{
    if (!reply)
        return false;
    return reply->error() == QNetworkReply::SslHandshakeFailedError;
}

void ServerConnectionProbe::handleReply(QNetworkReply *reply)
{
    if (!reply)
        return;
    if (reply != m_reply) {
        reply->deleteLater();
        return;
    }

    m_reply = nullptr;
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll().trimmed();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString errorText = reply->errorString();

    if (m_stage == Stage::Transmission && status == 409
        && !m_transmissionSessionRetried) {
        const QByteArray token = reply->rawHeader("X-Transmission-Session-Id");
        reply->deleteLater();
        if (!token.isEmpty()) {
            m_transmissionSessionRetried = true;
            sendTransmissionRequest(token);
            return;
        }
    }

    if (isAuthenticationResponse(status, body)) {
        reply->deleteLater();
        finishFailure(tr("Authentication failed."),
                      FailureKind::Authentication);
        return;
    }

    if (isTlsError(reply)) {
        reply->deleteLater();
        finishFailure(tr("Secure connection failed: %1").arg(errorText),
                      FailureKind::Tls);
        return;
    }

    if (networkError != QNetworkReply::NoError) {
        RetryScope scope = RetryScope::AllCandidates;
        if (networkError == QNetworkReply::HostNotFoundError)
            scope = RetryScope::None;
        else if (networkError == QNetworkReply::ConnectionRefusedError
                 || networkError == QNetworkReply::TimeoutError)
            scope = RetryScope::PortChanges;
        reply->deleteLater();
        failAttempt(tr("Connection failed: %1").arg(errorText),
                    FailureKind::Connection, scope);
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(body);
    const QJsonObject object = document.object();

    if (m_stage == Stage::Transmission) {
        if (status >= 200 && status < 300 && document.isObject()
            && object.value(QStringLiteral("result")).toString()
                   == QStringLiteral("success")) {
            reply->deleteLater();
            finishSuccess();
            return;
        }
    } else if (m_stage == Stage::QBittorrentLogin) {
        if (status >= 200 && status < 300
            && (body == QByteArrayLiteral("Ok.")
                || (status == 204 && body.isEmpty()))) {
            reply->deleteLater();
            finishSuccess();
            return;
        }
    } else if (m_stage == Stage::DelugeLogin) {
        if (status >= 200 && status < 300 && document.isObject()) {
            const QJsonValue result = object.value(QStringLiteral("result"));
            if (result.isBool() && !result.toBool()) {
                reply->deleteLater();
                finishFailure(tr("Authentication failed."),
                              FailureKind::Authentication);
                return;
            }
            if (result.isBool()) {
                reply->deleteLater();
                sendDelugeRequest(Stage::DelugeConnection);
                return;
            }
        }
    } else if (m_stage == Stage::DelugeConnection) {
        if (status >= 200 && status < 300 && document.isObject()) {
            const QJsonValue result = object.value(QStringLiteral("result"));
            if (result.isBool()) {
                reply->deleteLater();
                if (result.toBool()) {
                    finishSuccess();
                } else {
                    finishFailure(
                        tr("Deluge Web is authenticated, but it is not connected to a daemon."),
                        FailureKind::DaemonUnavailable);
                }
                return;
            }
        }
    }

    reply->deleteLater();
    const QString reason = status > 0
                               ? tr("The server returned HTTP %1 or an unexpected response.")
                                     .arg(status)
                               : tr("The server returned an unexpected response.");
    failAttempt(tr("Connection failed: %1").arg(reason),
                FailureKind::Connection, RetryScope::AllCandidates);
}

void ServerConnectionProbe::failAttempt(const QString &message,
                                        FailureKind kind,
                                        RetryScope retryScope)
{
    if (m_candidateIndex == 0)
        m_retryScope = retryScope;

    const int next = nextCandidateIndex();
    if (next >= 0) {
        beginCandidate(next);
        return;
    }

    finishFailure(message, kind);
}

int ServerConnectionProbe::nextCandidateIndex() const
{
    if (m_retryScope == RetryScope::None)
        return -1;

    for (int index = m_candidateIndex + 1;
         index < m_candidates.size(); ++index) {
        if (m_retryScope == RetryScope::AllCandidates
            || m_candidates.at(index).changesPort) {
            return index;
        }
    }
    return -1;
}

void ServerConnectionProbe::finishSuccess()
{
    const QUrl workingUrl = m_candidates.at(m_candidateIndex).url;
    const bool adjusted = m_candidateIndex > 0;
    m_stage = Stage::None;
    emit connectionSucceeded(workingUrl, adjusted);
}

void ServerConnectionProbe::finishFailure(const QString &message,
                                          FailureKind kind)
{
    m_stage = Stage::None;
    emit connectionFailed(message, kind);
}
