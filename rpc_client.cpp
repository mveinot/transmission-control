#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSettings>
#include <QSet>
#include <QFile>
#include <QFileInfo>
#include "rpc_client.h"
#include "settingskeys.h"

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


static bool semverAtLeast(const QString &version, int requiredMajor, int requiredMinor, int requiredPatch)
{
    const QStringList parts = version.split(QLatin1Char('.'));

    const int major = parts.value(0).toInt();
    const int minor = parts.value(1).toInt();
    const int patch = parts.value(2).toInt();

    if (major != requiredMajor)
        return major > requiredMajor;

    if (minor != requiredMinor)
        return minor > requiredMinor;

    return patch >= requiredPatch;
}

static bool sessionSupportsSequentialDownload(const QJsonObject &settings)
{
    const int legacyRpcVersion = settings.value(QStringLiteral("rpc-version")).toInt(
        settings.value(QStringLiteral("rpc_version")).toInt()
        );

    if (legacyRpcVersion >= 18)
        return true;

    const QString rpcVersionSemver = settings.value(QStringLiteral("rpc-version-semver")).toString(
        settings.value(QStringLiteral("rpc_version_semver")).toString()
        );

    return semverAtLeast(rpcVersionSemver, 6, 0, 0);
}

rpc_client::rpc_client(QObject *parent)
    : QObject(parent)
    , na_manager(new QNetworkAccessManager(this))
{
    connect(na_manager, &QNetworkAccessManager::finished,
            this, &rpc_client::replyFinished);
}

void rpc_client::init()
{
    if (!loadCurrentServerFromSettings()) {
        qWarning() << "No valid Transmission server configured.";
        emit updateFailed(tr("No valid Transmission server configured."));
        return;
    }

    getTorrentList();
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

    // Reply identity is the correlation key; the context also preserves retry
    // metadata that is not encoded in QNetworkRequest attributes.
    pendingRequests.insert(reply, context);
}


void rpc_client::postIdsCommand(const QString &method, const QList<int> &ids)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = idsToJsonArray(ids);

    postRpc(method, arguments, RpcRequestType::Command);
}

void rpc_client::postSingleTorrentSet(int torrentId, const QJsonObject &arguments)
{
    if (torrentId < 0 || arguments.isEmpty())
        return;

    QJsonObject payload = arguments;
    payload[QStringLiteral("ids")] = QJsonArray { torrentId };

    postRpc(QStringLiteral("torrent-set"), payload, RpcRequestType::Command);
}

bool rpc_client::isTorrentDetailRequest(RpcRequestType type)
{
    return type == RpcRequestType::TorrentDetails
        || type == RpcRequestType::TorrentFiles
        || type == RpcRequestType::TorrentPeers
        || type == RpcRequestType::TorrentTrackers
        || type == RpcRequestType::TorrentPieces;
}

bool rpc_client::prepareTorrentDetailRequest(RpcRequestType type, int torrentId)
{
    QList<QNetworkReply *> obsoleteReplies;

    for (auto it = pendingRequests.cbegin(); it != pendingRequests.cend(); ++it) {
        if (it.value().type != type)
            continue;

        const QJsonArray ids = it.value().arguments.value(QStringLiteral("ids")).toArray();
        if (ids.size() == 1 && ids.first().toInt(-1) == torrentId)
            return false;

        obsoleteReplies.append(it.key());
    }

    // Remove correlation state before aborting because abort() may synchronously
    // deliver finished(). replyFinished() treats removed replies as obsolete.
    for (QNetworkReply *reply : std::as_const(obsoleteReplies)) {
        pendingRequests.remove(reply);
        reply->abort();
        reply->deleteLater();
    }

    return true;
}

void rpc_client::cancelTorrentDetailRequests()
{
    QList<QNetworkReply *> obsoleteReplies;

    for (auto it = pendingRequests.cbegin(); it != pendingRequests.cend(); ++it) {
        if (isTorrentDetailRequest(it.value().type))
            obsoleteReplies.append(it.key());
    }

    for (QNetworkReply *reply : std::as_const(obsoleteReplies)) {
        pendingRequests.remove(reply);
        reply->abort();
        reply->deleteLater();
    }
}

void rpc_client::replyFinished(QNetworkReply *reply)
{
    /*
     * A server change removes its outstanding replies from pendingRequests
     * before aborting them.  Their finished signals can still be delivered,
     * but they no longer belong to the active server and must be ignored.
     */
    if (!pendingRequests.contains(reply)) {
        reply->deleteLater();
        return;
    }

    RpcRequestContext context =
        pendingRequests.take(reply);

    const RpcRequestType requestType = context.type;
    const bool isTorrentGet =
        requestType == RpcRequestType::TorrentGet;

    const auto finishTorrentGet = [this, isTorrentGet]() {
        if (isTorrentGet) {
            updateInProgress = false;
            emit updateFinished();

            if (updateRequestedWhileInProgress) {
                updateRequestedWhileInProgress = false;
                getTorrentList();
            }
        }
    };

    const auto emitRequestFailed = [this, isTorrentGet, requestType, &context](const QString &message) {
        if (isTorrentGet) {
            emit updateFailed(message);
            return;
        }

        if (requestType == RpcRequestType::PortTest) {
            emit portTestFailed(message);
            return;
        }

        if (requestType == RpcRequestType::SessionStats) {
            emit sessionStatisticsFailed(message);
            return;
        }

        if (requestType == RpcRequestType::Command
            && context.method == QStringLiteral("torrent-add")
            && !context.torrentFilePath.isEmpty()) {
            emit torrentFileAddFailed(context.torrentFilePath, message);
        }

        emit commandFailed(context.method, message);
    };

    if (reply->error() == QNetworkReply::ContentConflictError) {
        // Transmission uses HTTP 409 as a session-token challenge. Replay the
        // original semantic request once after installing the supplied token.
        const QByteArray token =
            reply->rawHeader("X-Transmission-Session-Id");

        reply->deleteLater();

        if (token.isEmpty()) {
            const QString message =
                tr("Transmission returned 409 without a session token.");

            emitRequestFailed(message);
            finishTorrentGet();
            return;
        }

        setSessionToken(token);

        if (context.retriedAfterAuth) {
            const QString message =
                tr("Transmission session token retry failed.");

            emitRequestFailed(message);
            finishTorrentGet();
            return;
        }

        context.retriedAfterAuth = true;

        postRpc(context);

        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString message = reply->errorString();

        qDebug() << "Network reply ERROR:" << message;

        emitRequestFailed(message);

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

        const QString message =
            tr("Invalid JSON response from Transmission.");

        emitRequestFailed(message);
        finishTorrentGet();
        return;
    }

    const QJsonObject root = doc.object();
    const QString result = root.value("result").toString();

    if (result != "success") {
        const QString message =
            result.isEmpty()
                ? tr("Transmission RPC call failed.")
                : result;

        qDebug() << "Transmission RPC error:" << message;

        emitRequestFailed(message);
        finishTorrentGet();
        return;
    }

    if (requestType == RpcRequestType::SessionGet) {
        const QJsonObject arguments =
            root.value("arguments").toObject();

        m_sequentialDownloadSupported = sessionSupportsSequentialDownload(arguments);

        emit sessionSettingsReceived(arguments);
        return;
    }

    if (requestType == RpcRequestType::SessionStats) {
        emit sessionStatisticsReceived(
            root.value(QStringLiteral("arguments")).toObject());
        return;
    }

    if (requestType == RpcRequestType::FreeSpace) {
        const QJsonObject arguments =
            root.value(QStringLiteral("arguments")).toObject();

        const QString path =
            arguments.value(QStringLiteral("path")).toString();

        const qint64 sizeBytes =
            static_cast<qint64>(arguments.value(QStringLiteral("size-bytes")).toDouble());

        emit freeSpaceReceived(path, sizeBytes);
        return;
    }

    if (requestType == RpcRequestType::PortTest) {
        const QJsonObject arguments =
            root.value(QStringLiteral("arguments")).toObject();

        const bool portIsOpen =
            arguments.value(QStringLiteral("port-is-open")).toBool(
                arguments.value(QStringLiteral("port_is_open")).toBool(false)
                );

        const QString ipProtocol =
            arguments.value(QStringLiteral("ip-protocol")).toString(
                arguments.value(QStringLiteral("ip_protocol")).toString()
                );

        emit portTestFinished(portIsOpen, ipProtocol);
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

        if (context.method == QStringLiteral("torrent-add")) {
            const QJsonObject arguments =
                root.value(QStringLiteral("arguments")).toObject();
            const QJsonObject added =
                arguments.value(QStringLiteral("torrent-added")).toObject();

            if (!added.isEmpty()) {
                emit torrentAdded(
                    added.value(QStringLiteral("id")).toInt(-1),
                    added.value(QStringLiteral("name")).toString()
                    );
            }

            if (!context.torrentFilePath.isEmpty())
                emit torrentFileAddSucceeded(context.torrentFilePath);
        }

        const bool trackerMetadataChanged =
            context.method == QStringLiteral("torrent-add")
            || context.method == QStringLiteral("torrent-remove")
            || context.arguments.contains(QStringLiteral("trackerAdd"))
            || context.arguments.contains(QStringLiteral("trackerReplace"))
            || context.arguments.contains(QStringLiteral("trackerRemove"));

        if (trackerMetadataChanged) {
            // The fast list poll intentionally omits tracker arrays. Refresh the
            // slow cache immediately after operations that can change its keys.
            getTorrentTrackerMetadata();
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

        emitRequestFailed(tr("Torrent response did not contain torrents."));
        finishTorrentGet();
        return;
    }

    const QJsonArray newTorrentList =
        torrentsValue.toArray();

    if (requestType == RpcRequestType::TorrentListTrackers) {
        QSet<int> receivedIds;

        for (const QJsonValue &value : newTorrentList) {
            const QJsonObject object = value.toObject();
            const int torrentId = object.value(QStringLiteral("id")).toInt(-1);
            if (torrentId < 0)
                continue;

            receivedIds.insert(torrentId);
            torrentTrackerMetadata.insert(torrentId, object);
        }

        // Drop metadata for torrents removed since the previous slow poll.
        const QList<int> cachedIds = torrentTrackerMetadata.keys();
        for (int torrentId : cachedIds) {
            if (!receivedIds.contains(torrentId))
                torrentTrackerMetadata.remove(torrentId);
        }

        emit torrentTrackerMetadataUpdated();
        return;
    }

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

    if (requestType == RpcRequestType::TorrentFiles
        || requestType == RpcRequestType::TorrentPeers
        || requestType == RpcRequestType::TorrentTrackers) {
        if (!newTorrentList.isEmpty()) {
            const QJsonObject detail = newTorrentList.first().toObject();
            const int torrentId = detail.value(QStringLiteral("id")).toInt(-1);

            if (torrentId >= 0) {
                if (requestType == RpcRequestType::TorrentFiles)
                    emit torrentFilesReceived(torrentId, detail);
                else if (requestType == RpcRequestType::TorrentPeers)
                    emit torrentPeersReceived(torrentId, detail);
                else
                    emit torrentTrackersReceived(torrentId, detail);
            }
        }

        return;
    }

    if (requestType == RpcRequestType::TorrentPieces) {
        if (!newTorrentList.isEmpty()) {
            const QJsonObject detail = newTorrentList.first().toObject();
            const int torrentId = detail.value(QStringLiteral("id")).toInt(-1);

            if (torrentId >= 0)
                emit torrentPiecesReceived(torrentId, detail);
        }

        return;
    }

    if (requestType == RpcRequestType::TorrentProperties) {
        if (!newTorrentList.isEmpty()) {
            const QJsonObject detail = newTorrentList.first().toObject();
            const int torrentId = detail.value(QStringLiteral("id")).toInt(-1);

            if (torrentId >= 0)
                emit torrentPropertiesReceived(torrentId, detail);
        }

        return;
    }

    if (requestType == RpcRequestType::TorrentGet) {
        QVector<torrent> incoming;
        incoming.reserve(newTorrentList.size());

        for (const QJsonValue &value : newTorrentList) {
            QJsonObject object = value.toObject();
            const int torrentId = object.value(QStringLiteral("id")).toInt(-1);
            const QJsonObject metadata = torrentTrackerMetadata.value(torrentId);

            if (!metadata.isEmpty()) {
                object.insert(QStringLiteral("trackers"),
                              metadata.value(QStringLiteral("trackers")));
                object.insert(QStringLiteral("trackerStats"),
                              metadata.value(QStringLiteral("trackerStats")));
            }

            incoming.append(torrent(object));
        }

        emit torrentsReceived(incoming);

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

    const int count = settings.beginReadArray(SettingsKeys::ServersArray);

    if (index < 0 || index >= count) {
        settings.endArray();
        return {};
    }

    settings.setArrayIndex(index);

    TransmissionServer server;
    server.name = settings.value(SettingsKeys::ServerName).toString().trimmed();
    server.rpcUrl = settings.value(SettingsKeys::ServerRpcUrl).toString().trimmed();
    server.username = settings.value(SettingsKeys::ServerUsername).toString();
    server.password = settings.value(SettingsKeys::ServerPassword).toString();

    settings.endArray();

    if (ok)
        *ok = server.isValid();

    return server;
}

bool rpc_client::loadCurrentServerFromSettings()
{
    QSettings settings;

    const int defaultIndex =
        settings.value(SettingsKeys::ServersDefaultIndex, -1).toInt();

    if (setServerFromSettingsIndex(defaultIndex))
        return true;

    /*
     * Backward-compatible fallback:
     * If no valid default exists, try the old currentIndex setting.
     * This is only for older preferences, not normal startup behavior.
     */
    const int legacyCurrentIndex =
        settings.value(SettingsKeys::ServersCurrentIndex, -1).toInt();

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
    /*
     * Do not allow responses from the previous server to update the new
     * server's session token, torrent data, or command state.  Clear the
     * bookkeeping first because abort() may synchronously emit finished().
     */
    const QList<QNetworkReply *> staleReplies = pendingRequests.keys();
    pendingRequests.clear();
    torrentTrackerMetadata.clear();

    for (QNetworkReply *reply : staleReplies) {
        if (!reply)
            continue;

        reply->abort();
        reply->deleteLater();
    }

    serverName = server.name;
    rpcUrl = server.rpcUrl;
    username = server.username;
    password = server.password;

    _session_token.clear();
    _clientReady = false;
    updateInProgress = false;
    updateRequestedWhileInProgress = false;

    emit serverChanged();
}

QString rpc_client::getServer()
{
    if (!serverName.isEmpty())
        return serverName;

    if (!rpcUrl.isEmpty())
        return rpcUrl;

    return tr("No server configured");
}

QString rpc_client::getRpcUrl() const
{
    return rpcUrl;
}


void rpc_client::getTorrentList()
{
    if (rpcUrl.trimmed().isEmpty()) {
        emit updateFailed(tr("No Transmission server configured."));
        return;
    }

    // Timer ticks and command-triggered refreshes may overlap. A single list
    // request is sufficient because its response is a complete snapshot.
    if (updateInProgress) {
        updateRequestedWhileInProgress = true;
        return;
    }

    updateInProgress = true;
    emit updateStarted();

    QJsonObject arguments;
    arguments["fields"] = QJsonArray {
        "id",
        "name",
        "hashString",
        "percentDone",
        "status",
        "rateDownload",
        "rateUpload",
        "uploadRatio",
        "eta",
        "sizeWhenDone",
        "queuePosition",
        "addedDate",
        "downloadedEver",
        "uploadedEver",
        "downloadDir",
        "error",
        "errorString",
        "isStalled",
        "desiredAvailable",
        "leftUntilDone",
        "peersConnected",
        "peersSendingToUs",
        "peersGettingFromUs"
    };

    postRpc("torrent-get", arguments, RpcRequestType::TorrentGet);
}

void rpc_client::getTorrentTrackerMetadata()
{
    for (auto it = pendingRequests.cbegin(); it != pendingRequests.cend(); ++it) {
        if (it.value().type == RpcRequestType::TorrentListTrackers)
            return;
    }

    QJsonObject arguments;
    arguments[QStringLiteral("fields")] = QJsonArray {
        QStringLiteral("id"), QStringLiteral("trackers"),
        QStringLiteral("trackerStats")
    };
    postRpc(QStringLiteral("torrent-get"), arguments,
            RpcRequestType::TorrentListTrackers);
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
        const QString message =
            tr("Could not open torrent file: %1").arg(filePath);

        qWarning() << "Could not open torrent file:"
                   << filePath
                   << file.errorString();

        emit torrentFileAddFailed(filePath, message);
        emit commandFailed(QStringLiteral("torrent-add"), message);
        return;
    }

    const QByteArray torrentData = file.readAll();

    if (torrentData.isEmpty()) {
        const QString message =
            tr("Torrent file is empty: %1").arg(filePath);

        qWarning() << "Torrent file is empty:" << filePath;

        emit torrentFileAddFailed(filePath, message);
        emit commandFailed(QStringLiteral("torrent-add"), message);
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
    postIdsCommand(QStringLiteral("torrent-start"), ids);
}

void rpc_client::startAllTorrents()
{
    postRpc(QStringLiteral("torrent-start"), QJsonObject(), RpcRequestType::Command);
}

void rpc_client::startTorrentsNow(const QList<int> &ids)
{
    postIdsCommand(QStringLiteral("torrent-start-now"), ids);
}

void rpc_client::stopTorrents(const QList<int> &ids)
{
    postIdsCommand(QStringLiteral("torrent-stop"), ids);
}

void rpc_client::stopAllTorrents()
{
    postRpc(QStringLiteral("torrent-stop"), QJsonObject(), RpcRequestType::Command);
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
    postIdsCommand(QStringLiteral("torrent-verify"), ids);
}

void rpc_client::reannounceTorrents(const QList<int> &ids)
{
    postIdsCommand(QStringLiteral("torrent-reannounce"), ids);
}

void rpc_client::setTorrentLocation(const QList<int> &ids,
                                    const QString &location,
                                    bool moveData)
{
    const QString trimmedLocation = location.trimmed();

    if (ids.isEmpty() || trimmedLocation.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = idsToJsonArray(ids);
    arguments[QStringLiteral("location")] = trimmedLocation;
    arguments[QStringLiteral("move")] = moveData;

    postRpc(QStringLiteral("torrent-set-location"),
            arguments,
            RpcRequestType::Command);
}

void rpc_client::getTorrentDetails(int id)
{
    if (id < 0 || !prepareTorrentDetailRequest(RpcRequestType::TorrentDetails, id))
        return;

    QJsonObject arguments;
    arguments["ids"] = QJsonArray { id };

    arguments["fields"] = QJsonArray {
        QStringLiteral("id"),
        QStringLiteral("name"),
        QStringLiteral("comment"),
        QStringLiteral("creator"),
        QStringLiteral("dateCreated"),
        QStringLiteral("downloadDir"),
        QStringLiteral("hashString"),
        QStringLiteral("magnetLink"),
        QStringLiteral("totalSize"),
        QStringLiteral("activityDate"),
        QStringLiteral("addedDate"),
        QStringLiteral("bandwidthPriority"),
        QStringLiteral("corruptEver"),
        QStringLiteral("desiredAvailable"),
        QStringLiteral("doneDate"),
        QStringLiteral("downloadedEver"),
        QStringLiteral("downloadLimit"),
        QStringLiteral("downloadLimited"),
        QStringLiteral("editDate"),
        QStringLiteral("error"),
        QStringLiteral("errorString"),
        QStringLiteral("eta"),
        QStringLiteral("etaIdle"),
        QStringLiteral("file-count"),
        QStringLiteral("group"),
        QStringLiteral("haveUnchecked"),
        QStringLiteral("haveValid"),
        QStringLiteral("honorsSessionLimits"),
        QStringLiteral("isFinished"),
        QStringLiteral("isPrivate"),
        QStringLiteral("isStalled"),
        QStringLiteral("labels"),
        QStringLiteral("leftUntilDone"),
        QStringLiteral("manualAnnounceTime"),
        QStringLiteral("maxConnectedPeers"),
        QStringLiteral("metadataPercentComplete"),
        QStringLiteral("peer-limit"),
        QStringLiteral("peersConnected"),
        QStringLiteral("peersFrom"),
        QStringLiteral("peersGettingFromUs"),
        QStringLiteral("peersSendingToUs"),
        QStringLiteral("percentDone"),
        QStringLiteral("pieceCount"),
        QStringLiteral("pieceSize"),
        QStringLiteral("queuePosition"),
        QStringLiteral("rateDownload"),
        QStringLiteral("rateUpload"),
        QStringLiteral("recheckProgress"),
        QStringLiteral("secondsDownloading"),
        QStringLiteral("secondsSeeding"),
        QStringLiteral("seedIdleLimit"),
        QStringLiteral("seedIdleMode"),
        QStringLiteral("seedRatioLimit"),
        QStringLiteral("seedRatioMode"),
        QStringLiteral("sizeWhenDone"),
        QStringLiteral("startDate"),
        QStringLiteral("status"),
        QStringLiteral("uploadedEver"),
        QStringLiteral("uploadLimit"),
        QStringLiteral("uploadLimited"),
        QStringLiteral("uploadRatio"),
        QStringLiteral("webseedsSendingToUs")
    };

    if (m_sequentialDownloadSupported) {
        QJsonArray fields = arguments.value(QStringLiteral("fields")).toArray();
        fields.append(QStringLiteral("sequential_download"));
        fields.append(QStringLiteral("sequential_download_from_piece"));
        arguments[QStringLiteral("fields")] = fields;
    }

    postRpc("torrent-get", arguments, RpcRequestType::TorrentDetails);
}

void rpc_client::getTorrentFiles(int id)
{
    if (id < 0 || !prepareTorrentDetailRequest(RpcRequestType::TorrentFiles, id))
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { id };
    arguments[QStringLiteral("fields")] = QJsonArray {
        QStringLiteral("id"), QStringLiteral("downloadDir"),
        QStringLiteral("files"), QStringLiteral("wanted"),
        QStringLiteral("priorities")
    };
    postRpc(QStringLiteral("torrent-get"), arguments, RpcRequestType::TorrentFiles);
}

void rpc_client::getTorrentPeers(int id)
{
    if (id < 0 || !prepareTorrentDetailRequest(RpcRequestType::TorrentPeers, id))
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { id };
    arguments[QStringLiteral("fields")] = QJsonArray {
        QStringLiteral("id"), QStringLiteral("peers")
    };
    postRpc(QStringLiteral("torrent-get"), arguments, RpcRequestType::TorrentPeers);
}

void rpc_client::getTorrentTrackers(int id)
{
    if (id < 0 || !prepareTorrentDetailRequest(RpcRequestType::TorrentTrackers, id))
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { id };
    arguments[QStringLiteral("fields")] = QJsonArray {
        QStringLiteral("id"), QStringLiteral("trackers"),
        QStringLiteral("trackerStats")
    };
    postRpc(QStringLiteral("torrent-get"), arguments, RpcRequestType::TorrentTrackers);
}


void rpc_client::getTorrentPieces(int id)
{
    if (id < 0 || !prepareTorrentDetailRequest(RpcRequestType::TorrentPieces, id))
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { id };

    arguments[QStringLiteral("fields")] = QJsonArray {
        QStringLiteral("id"),
        QStringLiteral("status"),
        QStringLiteral("error"),
        QStringLiteral("errorString"),
        QStringLiteral("isFinished"),
        QStringLiteral("isStalled"),
        QStringLiteral("percentDone"),
        QStringLiteral("metadataPercentComplete"),
        QStringLiteral("recheckProgress"),
        QStringLiteral("pieceCount"),
        QStringLiteral("pieces"),
        QStringLiteral("pieceSize"),
        QStringLiteral("rateDownload"),
        QStringLiteral("rateUpload"),
        QStringLiteral("uploadRatio"),
        QStringLiteral("downloadedEver"),
        QStringLiteral("uploadedEver"),
        QStringLiteral("corruptEver"),
        QStringLiteral("haveValid"),
        QStringLiteral("haveUnchecked"),
        QStringLiteral("desiredAvailable"),
        QStringLiteral("leftUntilDone"),
        QStringLiteral("sizeWhenDone"),
        QStringLiteral("totalSize"),
        QStringLiteral("eta"),
        QStringLiteral("etaIdle"),
        QStringLiteral("secondsDownloading"),
        QStringLiteral("secondsSeeding"),
        QStringLiteral("activityDate"),
        QStringLiteral("doneDate"),
        QStringLiteral("startDate"),
        QStringLiteral("peersConnected"),
        QStringLiteral("peersFrom"),
        QStringLiteral("peersGettingFromUs"),
        QStringLiteral("peersSendingToUs"),
        QStringLiteral("webseedsSendingToUs"),
        QStringLiteral("downloadLimited"),
        QStringLiteral("downloadLimit"),
        QStringLiteral("uploadLimited"),
        QStringLiteral("uploadLimit"),
        QStringLiteral("seedRatioMode"),
        QStringLiteral("seedRatioLimit"),
        QStringLiteral("seedIdleMode"),
        QStringLiteral("seedIdleLimit"),
        QStringLiteral("queuePosition"),
        QStringLiteral("bandwidthPriority"),
        QStringLiteral("honorsSessionLimits")
    };

    postRpc(QStringLiteral("torrent-get"),
            arguments,
            RpcRequestType::TorrentPieces);
}

void rpc_client::getTorrentProperties(int id)
{
    if (id < 0 || !prepareTorrentDetailRequest(RpcRequestType::TorrentProperties, id))
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { id };
    arguments[QStringLiteral("fields")] = QJsonArray {
        QStringLiteral("activityDate"),
        QStringLiteral("addedDate"),
        QStringLiteral("bandwidthPriority"),
        QStringLiteral("comment"),
        QStringLiteral("corruptEver"),
        QStringLiteral("creator"),
        QStringLiteral("dateCreated"),
        QStringLiteral("desiredAvailable"),
        QStringLiteral("doneDate"),
        QStringLiteral("downloadDir"),
        QStringLiteral("downloadedEver"),
        QStringLiteral("downloadLimit"),
        QStringLiteral("downloadLimited"),
        QStringLiteral("editDate"),
        QStringLiteral("error"),
        QStringLiteral("errorString"),
        QStringLiteral("eta"),
        QStringLiteral("etaIdle"),
        QStringLiteral("file-count"),
        QStringLiteral("files"),
        QStringLiteral("group"),
        QStringLiteral("hashString"),
        QStringLiteral("haveUnchecked"),
        QStringLiteral("haveValid"),
        QStringLiteral("honorsSessionLimits"),
        QStringLiteral("id"),
        QStringLiteral("isFinished"),
        QStringLiteral("isPrivate"),
        QStringLiteral("isStalled"),
        QStringLiteral("labels"),
        QStringLiteral("leftUntilDone"),
        QStringLiteral("magnetLink"),
        QStringLiteral("manualAnnounceTime"),
        QStringLiteral("maxConnectedPeers"),
        QStringLiteral("metadataPercentComplete"),
        QStringLiteral("name"),
        QStringLiteral("peer-limit"),
        QStringLiteral("peers"),
        QStringLiteral("peersConnected"),
        QStringLiteral("peersFrom"),
        QStringLiteral("peersGettingFromUs"),
        QStringLiteral("peersSendingToUs"),
        QStringLiteral("percentDone"),
        QStringLiteral("pieceCount"),
        QStringLiteral("pieces"),
        QStringLiteral("pieceSize"),
        QStringLiteral("priorities"),
        QStringLiteral("queuePosition"),
        QStringLiteral("rateDownload"),
        QStringLiteral("rateUpload"),
        QStringLiteral("recheckProgress"),
        QStringLiteral("secondsDownloading"),
        QStringLiteral("secondsSeeding"),
        QStringLiteral("seedIdleLimit"),
        QStringLiteral("seedIdleMode"),
        QStringLiteral("seedRatioLimit"),
        QStringLiteral("seedRatioMode"),
        QStringLiteral("sizeWhenDone"),
        QStringLiteral("startDate"),
        QStringLiteral("status"),
        QStringLiteral("trackers"),
        QStringLiteral("trackerStats"),
        QStringLiteral("totalSize"),
        QStringLiteral("uploadedEver"),
        QStringLiteral("uploadLimit"),
        QStringLiteral("uploadLimited"),
        QStringLiteral("uploadRatio"),
        QStringLiteral("wanted"),
        QStringLiteral("webseeds"),
        QStringLiteral("webseedsSendingToUs")
    };

    if (m_sequentialDownloadSupported) {
        QJsonArray fields = arguments.value(QStringLiteral("fields")).toArray();
        fields.append(QStringLiteral("sequential_download"));
        fields.append(QStringLiteral("sequential_download_from_piece"));
        arguments[QStringLiteral("fields")] = fields;
    }

    postRpc(QStringLiteral("torrent-get"),
            arguments,
            RpcRequestType::TorrentProperties);
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

void rpc_client::setTorrentFilesWantedAndPriority(int torrentId,
                                                  const QList<int> &fileIndices,
                                                  bool wanted,
                                                  int priority)
{
    if (torrentId < 0 || fileIndices.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = QJsonArray { torrentId };

    if (wanted) {
        arguments["files-wanted"] = idsToJsonArray(fileIndices);

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
    } else {
        arguments["files-unwanted"] = idsToJsonArray(fileIndices);
    }

    postRpc("torrent-set", arguments, RpcRequestType::Command);
}


void rpc_client::addTorrentTracker(int torrentId, const QString &announceUrl)
{
    const QString trimmedUrl = announceUrl.trimmed();

    if (torrentId < 0 || trimmedUrl.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { torrentId };
    arguments[QStringLiteral("trackerAdd")] = QJsonArray { trimmedUrl };

    postRpc(QStringLiteral("torrent-set"),
            arguments,
            RpcRequestType::Command);
}

void rpc_client::editTorrentTracker(int torrentId,
                                    int trackerId,
                                    const QString &announceUrl)
{
    const QString trimmedUrl = announceUrl.trimmed();

    if (torrentId < 0 || trackerId < 0 || trimmedUrl.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { torrentId };
    arguments[QStringLiteral("trackerReplace")] = QJsonArray {
        QJsonArray { trackerId, trimmedUrl }
    };

    postRpc(QStringLiteral("torrent-set"),
            arguments,
            RpcRequestType::Command);
}

void rpc_client::removeTorrentTracker(int torrentId, int trackerId)
{
    if (torrentId < 0 || trackerId < 0)
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { torrentId };
    arguments[QStringLiteral("trackerRemove")] = QJsonArray { trackerId };

    postRpc(QStringLiteral("torrent-set"),
            arguments,
            RpcRequestType::Command);
}

void rpc_client::renameTorrentPath(int torrentId,
                                   const QString &path,
                                   const QString &newName)
{
    const QString trimmedPath = path.trimmed();
    const QString trimmedName = newName.trimmed();

    if (torrentId < 0 || trimmedPath.isEmpty() || trimmedName.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { torrentId };
    arguments[QStringLiteral("path")] = trimmedPath;
    arguments[QStringLiteral("name")] = trimmedName;

    postRpc(QStringLiteral("torrent-rename-path"),
            arguments,
            RpcRequestType::Command);
}

void rpc_client::setTorrentProperties(int torrentId,
                                      const QJsonObject &properties)
{
    if (torrentId < 0 || properties.isEmpty())
        return;

    QJsonObject arguments = properties;
    arguments.insert(QStringLiteral("ids"), QJsonArray { torrentId });

    postRpc(QStringLiteral("torrent-set"),
            arguments,
            RpcRequestType::Command);
}

void rpc_client::setTorrentsSequentialDownload(const QList<int> &ids,
                                               bool enabled)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = idsToJsonArray(ids);
    arguments[QStringLiteral("sequential_download")] = enabled;

    postRpc(QStringLiteral("torrent-set"),
            arguments,
            RpcRequestType::Command);
}

void rpc_client::setTorrentsBandwidthPriority(const QList<int> &ids,
                                              int priority)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = idsToJsonArray(ids);
    arguments[QStringLiteral("bandwidthPriority")] = priority;

    postRpc(QStringLiteral("torrent-set"),
            arguments,
            RpcRequestType::Command);
}

void rpc_client::queueMoveTop(const QList<int> &ids)
{
    postIdsCommand(QStringLiteral("queue-move-top"), ids);
}

void rpc_client::queueMoveUp(const QList<int> &ids)
{
    postIdsCommand(QStringLiteral("queue-move-up"), ids);
}

void rpc_client::queueMoveDown(const QList<int> &ids)
{
    postIdsCommand(QStringLiteral("queue-move-down"), ids);
}

void rpc_client::queueMoveBottom(const QList<int> &ids)
{
    postIdsCommand(QStringLiteral("queue-move-bottom"), ids);
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

        "version",
        "rpc-version",
        "rpc-version-minimum",
        "rpc-version-semver",

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

void rpc_client::getSessionStatistics()
{
    postRpc(QStringLiteral("session-stats"),
            QJsonObject(),
            RpcRequestType::SessionStats);
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

void rpc_client::testPortForwarding()
{
    postRpc(QStringLiteral("port-test"), QJsonObject(), RpcRequestType::PortTest);
}

void rpc_client::addTorrentFile(const QString &filePath,
                                const QString &downloadDir,
                                bool paused,
                                const QList<int> &filesUnwanted,
                                const QList<int> &priorityLow,
                                const QList<int> &priorityHigh,
                                bool deleteFileOnSuccess)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        const QString message =
            tr("Could not open torrent file: %1").arg(filePath);

        emit torrentFileAddFailed(filePath, message);
        emit commandFailed(QStringLiteral("torrent-add"), message);
        return;
    }

    const QByteArray data = file.readAll();

    if (data.isEmpty()) {
        const QString message =
            tr("Torrent file is empty: %1").arg(filePath);

        emit torrentFileAddFailed(filePath, message);
        emit commandFailed(QStringLiteral("torrent-add"), message);
        return;
    }

    QJsonObject arguments;
    arguments.insert(QStringLiteral("metainfo"),
                     QString::fromLatin1(data.toBase64()));

    if (!downloadDir.trimmed().isEmpty()) {
        arguments.insert(QStringLiteral("download-dir"),
                         downloadDir.trimmed());
    }

    arguments.insert(QStringLiteral("paused"), paused);

    if (!filesUnwanted.isEmpty())
        arguments.insert(QStringLiteral("files-unwanted"),
                         idsToJsonArray(filesUnwanted));

    if (!priorityLow.isEmpty())
        arguments.insert(QStringLiteral("priority-low"),
                         idsToJsonArray(priorityLow));

    if (!priorityHigh.isEmpty())
        arguments.insert(QStringLiteral("priority-high"),
                         idsToJsonArray(priorityHigh));

    RpcRequestContext context;
    context.method = QStringLiteral("torrent-add");
    context.arguments = arguments;
    context.type = RpcRequestType::Command;
    context.torrentFilePath = filePath;
    context.deleteTorrentFileOnSuccess = deleteFileOnSuccess;

    postRpc(context);
}

void rpc_client::addMagnetLink(const QString &magnetLink,
                               const QString &downloadDir,
                               bool paused)
{
    const QString trimmed = magnetLink.trimmed();

    if (trimmed.isEmpty())
        return;

    QJsonObject arguments;
    arguments.insert(QStringLiteral("filename"), trimmed);

    if (!downloadDir.trimmed().isEmpty()) {
        arguments.insert(QStringLiteral("download-dir"),
                         downloadDir.trimmed());
    }

    arguments.insert(QStringLiteral("paused"), paused);

    postRpc(QStringLiteral("torrent-add"),
            arguments,
            RpcRequestType::Command);
}
