#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSettings>
#include "rpc_client.h"

namespace {
constexpr QNetworkRequest::Attribute RpcRequestTypeAttribute =
    QNetworkRequest::User;

constexpr QNetworkRequest::Attribute RpcMethodAttribute =
    static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 1);

constexpr QNetworkRequest::Attribute TorrentFilePathAttribute =
    static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 2);

constexpr QNetworkRequest::Attribute DeleteTorrentFileOnSuccessAttribute =
    static_cast<QNetworkRequest::Attribute>(QNetworkRequest::User + 3);
}

static QJsonArray idsToJsonArray(const QList<int> &ids)
{
    QJsonArray array;

    for (int id : ids)
        array.append(id);

    return array;
}

void rpc_client::postRpc(const QString &method,
                         const QJsonObject &arguments,
                         RpcRequestType type)
{
    RpcRequestContext context;
    context.method = method;
    context.arguments = arguments;
    context.type = type;

    postRpc(context);
}

void rpc_client::postRpc(const RpcRequestContext &context)
{
    QNetworkRequest request = makeRequest();

    request.setAttribute(
        RpcRequestTypeAttribute,
        static_cast<int>(context.type)
        );

    request.setAttribute(RpcMethodAttribute, context.method);

    QNetworkReply *reply = na_manager->post(
        request,
        makeRpcPayload(context.method, context.arguments)
        );

    pendingRequests.insert(reply, context);
}

rpc_client::rpc_client(QObject *parent)
    : QObject(parent)
{
}

void rpc_client::init()
{
    na_manager = new QNetworkAccessManager(this);

    connect(na_manager, &QNetworkAccessManager::finished,
            this, &rpc_client::replyFinished);

    if (!loadCurrentServerFromSettings()) {
        qWarning() << "No valid Transmission server configured.";
        emit updateFailed("No valid Transmission server configured.");
        return;
    }

    getTorrentList();
}

void rpc_client::replyFinished(QNetworkReply *reply)
{
    RpcRequestContext context =
        pendingRequests.take(reply);

    const RpcRequestType requestType = context.type;
    const bool isTorrentGet =
        requestType == RpcRequestType::TorrentGet;

    /*
    const bool isTorrentDetails =
       requestType == RpcRequestType::TorrentDetails;
*/
    const auto finishTorrentGet = [this, isTorrentGet]() {
        if (isTorrentGet) {
            updateInProgress = false;
            emit updateFinished();
        }
    };

    if (reply->error() == QNetworkReply::ContentConflictError) {
        const QByteArray token =
            reply->rawHeader("X-Transmission-Session-Id");

        reply->deleteLater();

        if (token.isEmpty()) {
            if (isTorrentGet) {
                emit updateFailed("Transmission returned 409 without a session token.");
            }

            finishTorrentGet();
            return;
        }

        setSessionToken(token);

        if (context.retriedAfterAuth) {
            if (isTorrentGet) {
                emit updateFailed("Transmission session token retry failed.");
            }

            finishTorrentGet();
            return;
        }

        context.retriedAfterAuth = true;

        if (isTorrentGet)
            updateInProgress = false;

        postRpc(context);

        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString message = reply->errorString();

        qDebug() << "Network reply ERROR:" << message;

        if (isTorrentGet) {
            emit updateFailed(message);
        }

        reply->deleteLater();
        finishTorrentGet();
        return;
    }

    const QByteArray body = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qDebug() << "Invalid JSON response:" << parseError.errorString();

        if (isTorrentGet) {
            emit updateFailed("Invalid JSON response from Transmission.");
        }

        finishTorrentGet();
        return;
    }

    if (requestType == RpcRequestType::SessionGet) {
        const QJsonObject arguments =
            doc.object().value("arguments").toObject();

        emit sessionSettingsReceived(arguments);
        return;
    }

    if (requestType == RpcRequestType::FreeSpace) {
        const QJsonObject arguments =
            doc.object().value(QStringLiteral("arguments")).toObject();

        const QString path =
            arguments.value(QStringLiteral("path")).toString();

        const qint64 sizeBytes =
            static_cast<qint64>(arguments.value(QStringLiteral("size-bytes")).toDouble());

        emit freeSpaceReceived(path, sizeBytes);
        return;
    }

    const QJsonObject root = doc.object();
    const QString result = root.value("result").toString();

    if (result != "success") {
        const QString message =
            result.isEmpty()
                ? QStringLiteral("Transmission RPC call failed.")
                : result;

        qDebug() << "Transmission RPC error:" << message;

        if (isTorrentGet) {
            emit updateFailed(message);
        }

        finishTorrentGet();
        return;
    }

    if (requestType == RpcRequestType::Command) {
        if (context.method == "torrent-add" &&
            context.deleteTorrentFileOnSuccess &&
            !context.torrentFilePath.isEmpty()) {

            if (QFile::remove(context.torrentFilePath)) {
                qDebug() << "Deleted torrent file after successful add:"
                         << context.torrentFilePath;
            } else {
                qWarning() << "Could not delete torrent file after add:"
                           << context.torrentFilePath;
            }
        }

        emit commandSucceeded(context.method);
        return;
    }

    const QJsonObject arguments =
        root.value("arguments").toObject();

    const QJsonValue torrentsValue =
        arguments.value("torrents");

    if (!torrentsValue.isArray()) {
        qDebug() << "torrent-get response did not contain arguments.torrents";

        emit updateFailed("Torrent list response did not contain torrents.");
        finishTorrentGet();
        return;
    }

    const QJsonArray newTorrentList =
        torrentsValue.toArray();

    if (requestType == RpcRequestType::TorrentDetails) {
        if (!newTorrentList.isEmpty()) {
            const QJsonObject detail = newTorrentList.first().toObject();
            const int torrentId = detail.value("id").toInt(-1);

            if (torrentId >= 0) {
                emit torrentDetailsReceived(torrentId, detail);
            }
        }

        return;
    }

    if (requestType == RpcRequestType::TorrentGet) {
        QVector<torrent> incoming;
        incoming.reserve(newTorrentList.size());

        for (const QJsonValue &obj : newTorrentList) {
            incoming.append(torrent(obj));
        }

        emit torrentsReceived(incoming);

        //emit listUpdated();

        finishTorrentGet();
        return;
    }

    if (requestType == RpcRequestType::Command) {
        emit commandSucceeded(context.method);
        return;
    }
}

void rpc_client::setSessionToken(QByteArray token)
{
    _session_token = token;
    _clientReady = true;
}

rpc_client::TransmissionServer rpc_client::readServerFromSettings(int index, bool *ok)
{
    if (ok)
        *ok = false;

    QSettings settings;

    const int count = settings.beginReadArray("servers");

    if (index < 0 || index >= count) {
        settings.endArray();
        return {};
    }

    settings.setArrayIndex(index);

    TransmissionServer server;
    server.name = settings.value("name").toString().trimmed();
    server.rpcUrl = settings.value("rpcUrl").toString().trimmed();
    server.username = settings.value("username").toString();
    server.password = settings.value("password").toString();

    settings.endArray();

    if (ok)
        *ok = server.isValid();

    return server;
}

bool rpc_client::loadCurrentServerFromSettings()
{
    QSettings settings;

    const int defaultIndex =
        settings.value("servers/defaultIndex", -1).toInt();

    if (setServerFromSettingsIndex(defaultIndex))
        return true;

    /*
     * Backward-compatible fallback:
     * If no valid default exists, try the old currentIndex setting.
     * This is only for older preferences, not normal startup behavior.
     */
    const int legacyCurrentIndex =
        settings.value("servers/currentIndex", -1).toInt();

    if (legacyCurrentIndex != defaultIndex &&
        setServerFromSettingsIndex(legacyCurrentIndex)) {
        return true;
    }

    /*
     * Last fallback: first valid configured server.
     */
    return setServerFromSettingsIndex(0);
}

bool rpc_client::setServerFromSettingsIndex(int index)
{
    bool ok = false;
    const TransmissionServer server = readServerFromSettings(index, &ok);

    if (!ok)
        return false;

    setServer(server);
    return true;
}

void rpc_client::setServer(const TransmissionServer &server)
{
    serverName = server.name;
    rpcUrl = server.rpcUrl;
    username = server.username;
    password = server.password;

    _session_token.clear();
    _clientReady = false;
    updateInProgress = false;

    emit serverChanged();
}

QString rpc_client::getServer()
{
    if (!serverName.isEmpty())
        return serverName;

    if (!rpcUrl.isEmpty())
        return rpcUrl;

    return "No server configured";
}

void rpc_client::getTorrentList()
{
    if (rpcUrl.trimmed().isEmpty()) {
        emit updateFailed("No Transmission server configured.");
        return;
    }

    if (updateInProgress)
        return;

    updateInProgress = true;
    emit updateStarted();

    QJsonObject arguments;
    arguments["fields"] = QJsonArray {
        "id",
        "name",
        "percentDone",
        "status",
        "rateDownload",
        "rateUpload",
        "uploadRatio",
        "eta",
        "sizeWhenDone",
        "queuePosition"
    };

    postRpc("torrent-get", arguments, RpcRequestType::TorrentGet);
}

QByteArray rpc_client::makeRpcPayload(const QString &method,
                                      const QJsonObject &arguments) const
{
    QJsonObject root;
    root["method"] = method;

    if (!arguments.isEmpty())
        root["arguments"] = arguments;

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QNetworkRequest rpc_client::makeRequest() const
{
    QNetworkRequest request((QUrl(rpcUrl)));

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(15000);

    if (!_session_token.isEmpty()) {
        request.setRawHeader("X-Transmission-Session-Id", _session_token);
    }

    if (!username.isEmpty() || !password.isEmpty()) {
        const QByteArray auth =
            QString("%1:%2").arg(username, password).toUtf8().toBase64();

        request.setRawHeader("Authorization", "Basic " + auth);
    }

    return request;
}

void rpc_client::addTorrentFromFile(const QString &filePath,
                                    bool deleteFileOnSuccess)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open torrent file:"
                   << filePath
                   << file.errorString();
        return;
    }

    const QByteArray torrentData = file.readAll();

    if (torrentData.isEmpty()) {
        qWarning() << "Torrent file is empty:" << filePath;
        return;
    }

    QJsonObject arguments;
    arguments["metainfo"] = QString::fromLatin1(torrentData.toBase64());

    RpcRequestContext context;
    context.method = "torrent-add";
    context.arguments = arguments;
    context.type = RpcRequestType::Command;
    context.torrentFilePath = filePath;
    context.deleteTorrentFileOnSuccess = deleteFileOnSuccess;

    postRpc(context);
}

void rpc_client::addTorrentFromMagnet(const QString &magnetLink)
{
    const QString trimmedLink = magnetLink.trimmed();

    if (trimmedLink.isEmpty()) {
        qWarning() << "Empty magnet link";
        return;
    }

    QJsonObject arguments;
    arguments["filename"] = trimmedLink;

    postRpc("torrent-add", arguments, RpcRequestType::Command);
}

void rpc_client::startTorrents(const QList<int> &ids)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = idsToJsonArray(ids);

    postRpc("torrent-start", arguments, RpcRequestType::Command);
}

void rpc_client::stopTorrents(const QList<int> &ids)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = idsToJsonArray(ids);

    postRpc("torrent-stop", arguments, RpcRequestType::Command);
}

void rpc_client::removeTorrents(const QList<int> &ids, bool deleteLocalData)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = idsToJsonArray(ids);
    arguments["delete-local-data"] = deleteLocalData;

    postRpc("torrent-remove", arguments, RpcRequestType::Command);
}

void rpc_client::verifyTorrents(const QList<int> &ids)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = idsToJsonArray(ids);

    postRpc("torrent-verify", arguments, RpcRequestType::Command);
}

void rpc_client::reannounceTorrents(const QList<int> &ids)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = idsToJsonArray(ids);

    postRpc("torrent-reannounce", arguments, RpcRequestType::Command);
}

void rpc_client::getTorrentDetails(int id)
{
    if (id < 0)
        return;

    QJsonObject arguments;
    arguments["ids"] = QJsonArray { id };

    arguments["fields"] = QJsonArray {
        "id",
        "name",
        "comment",
        "creator",
        "dateCreated",
        "downloadDir",
        "hashString",
        "magnetLink",
        "totalSize",

        "trackers",
        "trackerStats",

        "files",
        "peers",
        "priorities",
        "wanted"
    };

    postRpc("torrent-get", arguments, RpcRequestType::TorrentDetails);
}

void rpc_client::setTorrentFilesWanted(int torrentId,
                                       const QList<int> &fileIndices,
                                       bool wanted)
{
    if (torrentId < 0 || fileIndices.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = QJsonArray { torrentId };

    if (wanted)
        arguments["files-wanted"] = idsToJsonArray(fileIndices);
    else
        arguments["files-unwanted"] = idsToJsonArray(fileIndices);

    postRpc("torrent-set", arguments, RpcRequestType::Command);
}

void rpc_client::setTorrentFilesPriority(int torrentId,
                                         const QList<int> &fileIndices,
                                         int priority)
{
    if (torrentId < 0 || fileIndices.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = QJsonArray { torrentId };

    switch (priority) {
    case 1:
        arguments["priority-high"] = idsToJsonArray(fileIndices);
        break;
    case -1:
        arguments["priority-low"] = idsToJsonArray(fileIndices);
        break;
    case 0:
    default:
        arguments["priority-normal"] = idsToJsonArray(fileIndices);
        break;
    }

    postRpc("torrent-set", arguments, RpcRequestType::Command);
}

void rpc_client::queueMoveTop(const QList<int> &ids)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = idsToJsonArray(ids);

    postRpc("queue-move-top", arguments, RpcRequestType::Command);
}

void rpc_client::queueMoveUp(const QList<int> &ids)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = idsToJsonArray(ids);

    postRpc("queue-move-up", arguments, RpcRequestType::Command);
}

void rpc_client::queueMoveDown(const QList<int> &ids)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = idsToJsonArray(ids);

    postRpc("queue-move-down", arguments, RpcRequestType::Command);
}

void rpc_client::queueMoveBottom(const QList<int> &ids)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = idsToJsonArray(ids);

    postRpc("queue-move-bottom", arguments, RpcRequestType::Command);
}

void rpc_client::getSessionSettings()
{
    QJsonObject arguments;

    arguments["fields"] = QJsonArray {
        "peer-port",
        "peer-port-random-on-start",
        "port-forwarding-enabled",
        "encryption",
        "dht-enabled",
        "pex-enabled",
        "lpd-enabled",
        "peer-limit-global",
        "peer-limit-per-torrent",

        "speed-limit-down-enabled",
        "speed-limit-down",
        "speed-limit-up-enabled",
        "speed-limit-up",
        "alt-speed-enabled",
        "alt-speed-down",
        "alt-speed-up",

        "download-queue-enabled",
        "download-queue-size",
        "seed-queue-enabled",
        "seed-queue-size",
        "queue-stalled-enabled",
        "queue-stalled-minutes",

        "download-dir",
        "incomplete-dir-enabled",
        "incomplete-dir",
        "rename-partial-files",
        "start-added-torrents",

        "seedRatioLimited",
        "seedRatioLimit",
        "idle-seeding-limit-enabled",
        "idle-seeding-limit"
    };

    postRpc("session-get", arguments, RpcRequestType::SessionGet);
}

void rpc_client::setSessionSettings(const QJsonObject &settings)
{
    if (settings.isEmpty())
        return;

    postRpc("session-set", settings, RpcRequestType::Command);
}

void rpc_client::getFreeSpace(const QString &path)
{
    if (path.trimmed().isEmpty())
        return;

    QJsonObject arguments;
    arguments["path"] = path;

    postRpc(QStringLiteral("free-space"), arguments, RpcRequestType::FreeSpace);
}
