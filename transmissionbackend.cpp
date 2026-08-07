#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QFile>
#include <QFileInfo>
#include "transmissionbackend.h"

#include <algorithm>
#include <utility>

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

static QJsonArray keysToJsonArray(const QList<TorrentKey> &torrentKeys)
{
    QJsonArray array;

    for (const TorrentKey &torrentKey : torrentKeys)
        array.append(torrentKey);

    return array;
}

// File indices are subordinate backend values, not torrent identities.
static QJsonArray indicesToJsonArray(const QList<int> &indices)
{
    QJsonArray array;

    for (int index : indices)
        array.append(index);

    return array;
}

static TorrentKey torrentKeyFromObject(const QJsonObject &object)
{
    TorrentKey key =
        object.value(QStringLiteral("hashString")).toString().trimmed();

    if (!key.isEmpty())
        return key;

    const QJsonValue legacyId = object.value(QStringLiteral("id"));
    if (legacyId.isString())
        return legacyId.toString().trimmed();

    const int numericId = legacyId.toInt(-1);
    return numericId >= 0 ? QString::number(numericId) : QString();
}

static TorrentDetails torrentDetailsFromObject(const QJsonObject &object)
{
    TorrentDetails details;
    details.key = torrentKeyFromObject(object);
    details.name = object.value(QStringLiteral("name")).toString();
    details.comment = object.value(QStringLiteral("comment")).toString();
    details.creator = object.value(QStringLiteral("creator")).toString();
    details.downloadDirectory =
        object.value(QStringLiteral("downloadDir")).toString();
    details.hashString =
        object.value(QStringLiteral("hashString")).toString();
    details.magnetLink =
        object.value(QStringLiteral("magnetLink")).toString();
    details.totalSize =
        object.value(QStringLiteral("totalSize")).toVariant().toLongLong();
    details.creationTime =
        object.value(QStringLiteral("dateCreated")).toVariant().toLongLong();

    const QString sequentialKey =
        object.contains(QStringLiteral("sequential_download"))
            ? QStringLiteral("sequential_download")
            : QStringLiteral("sequentialDownload");
    details.hasSequentialDownload = object.contains(sequentialKey);
    details.sequentialDownload =
        object.value(sequentialKey).toVariant().toBool();
    details.hasBandwidthPriority =
        object.contains(QStringLiteral("bandwidthPriority"));
    details.bandwidthPriority =
        object.value(QStringLiteral("bandwidthPriority")).toInt();
    details.fields = object.toVariantMap();
    return details;
}

static TorrentFiles torrentFilesFromObject(const QJsonObject &object)
{
    TorrentFiles snapshot;
    snapshot.key = torrentKeyFromObject(object);
    snapshot.downloadDirectory =
        object.value(QStringLiteral("downloadDir")).toString();

    const QJsonArray files = object.value(QStringLiteral("files")).toArray();
    const QJsonArray wanted = object.value(QStringLiteral("wanted")).toArray();
    const QJsonArray priorities =
        object.value(QStringLiteral("priorities")).toArray();
    snapshot.files.reserve(files.size());

    for (int index = 0; index < files.size(); ++index) {
        const QJsonObject file = files.at(index).toObject();
        TorrentFile normalized;
        normalized.index = index;
        normalized.path = file.value(QStringLiteral("name")).toString();
        normalized.length =
            file.value(QStringLiteral("length")).toVariant().toLongLong();
        normalized.bytesCompleted =
            file.value(QStringLiteral("bytesCompleted")).toVariant().toLongLong();
        normalized.wanted =
            index >= wanted.size() || wanted.at(index).toVariant().toBool();
        normalized.priority =
            index < priorities.size() ? priorities.at(index).toInt() : 0;
        snapshot.files.append(normalized);
    }

    return snapshot;
}

static TorrentPeers torrentPeersFromObject(const QJsonObject &object)
{
    TorrentPeers snapshot;
    snapshot.key = torrentKeyFromObject(object);
    const QJsonArray peers = object.value(QStringLiteral("peers")).toArray();
    snapshot.peers.reserve(peers.size());

    for (const QJsonValue &value : peers) {
        const QJsonObject peer = value.toObject();
        TorrentPeer normalized;
        normalized.address = peer.value(QStringLiteral("address")).toString();
        normalized.port = peer.value(QStringLiteral("port")).toInt();
        normalized.clientName =
            peer.value(QStringLiteral("clientName")).toString();
        normalized.progress =
            peer.value(QStringLiteral("progress")).toDouble();
        normalized.downloadRate =
            peer.value(QStringLiteral("rateToClient")).toVariant().toLongLong();
        normalized.uploadRate =
            peer.value(QStringLiteral("rateToPeer")).toVariant().toLongLong();
        normalized.encrypted =
            peer.value(QStringLiteral("isEncrypted")).toBool();
        normalized.incoming =
            peer.value(QStringLiteral("isIncoming")).toBool();
        snapshot.peers.append(normalized);
    }

    return snapshot;
}

static TorrentTrackers torrentTrackersFromObject(const QJsonObject &object)
{
    TorrentTrackers snapshot;
    snapshot.key = torrentKeyFromObject(object);
    const QJsonArray stats =
        object.value(QStringLiteral("trackerStats")).toArray();
    const QJsonArray trackers =
        object.value(QStringLiteral("trackers")).toArray();
    snapshot.trackers.reserve(stats.size());

    for (int index = 0; index < stats.size(); ++index) {
        const QJsonObject tracker = stats.at(index).toObject();
        TorrentTracker normalized;
        normalized.id = tracker.value(QStringLiteral("id")).toInt(-1);
        normalized.tier = tracker.value(QStringLiteral("tier")).toInt(-1);
        normalized.host = tracker.value(QStringLiteral("host")).toString();
        normalized.siteName =
            tracker.value(QStringLiteral("sitename")).toString();
        normalized.announceUrl =
            tracker.value(QStringLiteral("announce")).toString();
        normalized.scrapeUrl =
            tracker.value(QStringLiteral("scrape")).toString();

        if (normalized.id < 0 && index < trackers.size())
            normalized.id = trackers.at(index).toObject()
                                .value(QStringLiteral("id")).toInt(-1);

        if (normalized.id < 0) {
            for (const QJsonValue &value : trackers) {
                const QJsonObject candidate = value.toObject();
                if (candidate.value(QStringLiteral("announce")).toString()
                    == normalized.announceUrl) {
                    normalized.id =
                        candidate.value(QStringLiteral("id")).toInt(-1);
                    break;
                }
            }
        }

        normalized.announceState =
            tracker.value(QStringLiteral("announceState")).toInt(-1);
        normalized.scrapeState =
            tracker.value(QStringLiteral("scrapeState")).toInt(-1);
        normalized.seederCount =
            tracker.value(QStringLiteral("seederCount")).toInt(-1);
        normalized.leecherCount =
            tracker.value(QStringLiteral("leecherCount")).toInt(-1);
        normalized.downloadCount =
            tracker.value(QStringLiteral("downloadCount")).toInt(-1);
        normalized.lastAnnounceTime =
            tracker.value(QStringLiteral("lastAnnounceTime")).toVariant().toLongLong();
        normalized.nextAnnounceTime =
            tracker.value(QStringLiteral("nextAnnounceTime")).toVariant().toLongLong();
        normalized.lastScrapeTime =
            tracker.value(QStringLiteral("lastScrapeTime")).toVariant().toLongLong();
        normalized.nextScrapeTime =
            tracker.value(QStringLiteral("nextScrapeTime")).toVariant().toLongLong();
        normalized.lastAnnounceResult =
            tracker.value(QStringLiteral("lastAnnounceResult")).toString();
        normalized.lastScrapeResult =
            tracker.value(QStringLiteral("lastScrapeResult")).toString();
        normalized.lastAnnounceSucceeded =
            tracker.value(QStringLiteral("lastAnnounceSucceeded")).toBool();
        normalized.lastAnnounceTimedOut =
            tracker.value(QStringLiteral("lastAnnounceTimedOut")).toBool();
        normalized.lastScrapeSucceeded =
            tracker.value(QStringLiteral("lastScrapeSucceeded")).toBool();
        normalized.lastScrapeTimedOut =
            tracker.value(QStringLiteral("lastScrapeTimedOut")).toBool();
        snapshot.trackers.append(normalized);
    }

    return snapshot;
}

static TorrentPieces torrentPiecesFromObject(const QJsonObject &object)
{
    TorrentPieces snapshot;
    snapshot.key = torrentKeyFromObject(object);
    snapshot.pieceCount =
        object.value(QStringLiteral("pieceCount")).toInt();
    snapshot.completedPieces = QByteArray::fromBase64(
        object.value(QStringLiteral("pieces")).toString().toLatin1());
    snapshot.percentDone =
        object.value(QStringLiteral("percentDone")).toDouble();
    return snapshot;
}

static TorrentProperties torrentPropertiesFromObject(const QJsonObject &object)
{
    TorrentProperties properties;
    properties.key = torrentKeyFromObject(object);
    properties.name = object.value(QStringLiteral("name")).toString();
    properties.hashString =
        object.value(QStringLiteral("hashString")).toString();
    properties.bandwidthPriority =
        object.value(QStringLiteral("bandwidthPriority")).toInt();
    properties.honorsSessionLimits =
        object.value(QStringLiteral("honorsSessionLimits")).toBool(true);
    properties.queuePosition =
        object.value(QStringLiteral("queuePosition")).toInt();
    properties.peerLimit =
        object.value(QStringLiteral("peer-limit")).toInt(-1);
    properties.downloadLimited =
        object.value(QStringLiteral("downloadLimited")).toBool();
    properties.downloadLimit =
        object.value(QStringLiteral("downloadLimit")).toInt();
    properties.uploadLimited =
        object.value(QStringLiteral("uploadLimited")).toBool();
    properties.uploadLimit =
        object.value(QStringLiteral("uploadLimit")).toInt();
    properties.seedRatioMode =
        object.value(QStringLiteral("seedRatioMode")).toInt();
    properties.seedRatioLimit =
        object.value(QStringLiteral("seedRatioLimit")).toDouble();
    properties.seedIdleMode =
        object.value(QStringLiteral("seedIdleMode")).toInt();
    properties.seedIdleLimit =
        object.value(QStringLiteral("seedIdleLimit")).toInt();
    properties.group = object.value(QStringLiteral("group")).toString();
    properties.hasGroup = object.contains(QStringLiteral("group"));

    const QJsonArray labels = object.value(QStringLiteral("labels")).toArray();
    for (const QJsonValue &label : labels)
        properties.labels.append(label.toString());

    properties.fields = object.toVariantMap();
    return properties;
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

TransmissionBackend::TransmissionBackend(QObject *parent)
    : TorrentBackend(parent)
    , na_manager(new QNetworkAccessManager(this))
    , m_protocol(TransmissionProtocol::createLegacy())
{
    connect(na_manager, &QNetworkAccessManager::finished,
            this, &TransmissionBackend::replyFinished);
}

void TransmissionBackend::init()
{
    if (rpcUrl.trimmed().isEmpty()) {
        qWarning() << "No valid Transmission server configured.";
        emit updateFailed(tr("No valid Transmission server configured."));
        return;
    }

    beginProtocolNegotiation();
    getTorrentList();
}

void TransmissionBackend::postRpc(const QString &method,
                         const QJsonObject &arguments,
                         RpcRequestType type)
{
    RpcRequestContext context;
    context.method = method;
    context.arguments = arguments;
    context.type = type;

    postRpc(context);
}

void TransmissionBackend::postRpc(const RpcRequestContext &sourceContext)
{
    if (!m_protocolReady
        && sourceContext.type != RpcRequestType::ProtocolProbe) {
        m_requestsPendingAfterProtocol.append(sourceContext);
        beginProtocolNegotiation();
        return;
    }

    RpcRequestContext context = sourceContext;
    if (context.requestId == 0)
        context.requestId = m_nextRequestId++;

    QNetworkRequest request = makeRequest();

    request.setAttribute(
        RpcRequestTypeAttribute,
        static_cast<int>(context.type)
        );

    request.setAttribute(RpcMethodAttribute, context.method);

    QNetworkReply *reply = na_manager->post(
        request,
        m_protocol->encodeRequest(context.method, context.arguments,
                                  context.requestId)
        );

    // Reply identity is the correlation key; the context also preserves retry
    // metadata that is not encoded in QNetworkRequest attributes.
    pendingRequests.insert(reply, context);
}


void TransmissionBackend::postIdsCommand(
    const QString &method,
    const QList<TorrentKey> &torrentKeys)
{
    if (torrentKeys.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = keysToJsonArray(torrentKeys);

    postRpc(method, arguments, RpcRequestType::Command);
}

void TransmissionBackend::postSingleTorrentSet(
    const TorrentKey &torrentKey,
    const QJsonObject &arguments)
{
    if (!isValidTorrentKey(torrentKey) || arguments.isEmpty())
        return;

    QJsonObject payload = arguments;
    payload[QStringLiteral("ids")] = QJsonArray { torrentKey };

    postRpc(QStringLiteral("torrent-set"), payload, RpcRequestType::Command);
}

bool TransmissionBackend::isTorrentDetailRequest(RpcRequestType type)
{
    return type == RpcRequestType::TorrentDetails
        || type == RpcRequestType::TorrentFiles
        || type == RpcRequestType::TorrentPeers
        || type == RpcRequestType::TorrentTrackers
        || type == RpcRequestType::TorrentPieces;
}

bool TransmissionBackend::prepareTorrentDetailRequest(
    RpcRequestType type,
    const TorrentKey &torrentKey)
{
    for (const RpcRequestContext &queued :
         std::as_const(m_requestsPendingAfterProtocol)) {
        if (queued.type != type)
            continue;
        const QJsonArray ids =
            queued.arguments.value(QStringLiteral("ids")).toArray();
        if (ids.size() == 1 && ids.first().toString() == torrentKey)
            return false;
    }
    m_requestsPendingAfterProtocol.erase(
        std::remove_if(m_requestsPendingAfterProtocol.begin(),
                       m_requestsPendingAfterProtocol.end(),
                       [type](const RpcRequestContext &queued) {
                           return queued.type == type;
                       }),
        m_requestsPendingAfterProtocol.end());

    QList<QNetworkReply *> obsoleteReplies;

    for (auto it = pendingRequests.cbegin(); it != pendingRequests.cend(); ++it) {
        if (it.value().type != type)
            continue;

        const QJsonArray ids = it.value().arguments.value(QStringLiteral("ids")).toArray();
        if (ids.size() == 1 && ids.first().toString() == torrentKey)
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

void TransmissionBackend::cancelTorrentDetailRequests()
{
    m_requestsPendingAfterProtocol.erase(
        std::remove_if(m_requestsPendingAfterProtocol.begin(),
                       m_requestsPendingAfterProtocol.end(),
                       [](const RpcRequestContext &queued) {
                           return isTorrentDetailRequest(queued.type);
                       }),
        m_requestsPendingAfterProtocol.end());

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

void TransmissionBackend::replyFinished(QNetworkReply *reply)
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
        if (requestType == RpcRequestType::ProtocolProbe) {
            failProtocolNegotiation(message);
            return;
        }

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

        if (context.updateBlocklistAfterSuccess
            || context.method == QStringLiteral("blocklist-update")) {
            emit blocklistUpdateFailed(message);
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

    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (requestType == RpcRequestType::ProtocolProbe
        && isDefiniteModernProtocolRejection(httpStatus)) {
        reply->deleteLater();
        activateProtocol(TransmissionProtocol::createLegacy());
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
    if (requestType == RpcRequestType::ProtocolProbe
        && root.value(QStringLiteral("jsonrpc")).toString()
               != QStringLiteral("2.0")) {
        // A valid legacy envelope is definitive evidence that the endpoint
        // expects Transmission's original RPC dialect.
        if (root.value(QStringLiteral("result")).isString()) {
            activateProtocol(TransmissionProtocol::createLegacy());
            return;
        }
        failProtocolNegotiation(
            tr("Transmission returned an unrecognized RPC response."));
        return;
    }

    const TransmissionProtocolReply protocolReply =
        m_protocol->decodeReply(root, context.requestId,
                                context.method);

    if (requestType == RpcRequestType::ProtocolProbe) {
        // Even a structured JSON-RPC error proves which envelope the endpoint
        // speaks. Subsequent operation errors should be reported normally,
        // not obscured by retrying them through the legacy dialect.
        if (protocolReply.valid) {
            activateProtocol(TransmissionProtocol::createJsonRpc2());
            return;
        }

        failProtocolNegotiation(
            protocolReply.error.isEmpty()
                ? tr("Transmission protocol negotiation failed.")
                : protocolReply.error);
        return;
    }

    if (!protocolReply.valid || !protocolReply.success) {
        const QString message =
            protocolReply.error.isEmpty()
                ? tr("Transmission RPC call failed.")
                : protocolReply.error;

        qDebug() << "Transmission RPC error:" << message;

        emitRequestFailed(message);
        finishTorrentGet();
        return;
    }

    if (requestType == RpcRequestType::SessionGet) {
        const QJsonObject arguments = protocolReply.result;

        const bool sequentialDownloadWasSupported =
            m_sequentialDownloadSupported;
        m_sequentialDownloadSupported =
            sessionSupportsSequentialDownload(arguments);
        const int rpcVersion = arguments.value(QStringLiteral("rpc-version")).toInt(
            arguments.value(QStringLiteral("rpc_version")).toInt());
        const bool labelsWereSupported = m_torrentLabelsSupported;
        const bool groupsWereSupported = m_torrentGroupsSupported;

        // Labels entered the RPC protocol in version 16 and bandwidth groups
        // in version 17. Gate list fields so older servers retain a working
        // core torrent refresh instead of rejecting an unknown field.
        m_torrentLabelsSupported = rpcVersion >= 16;
        m_torrentGroupsSupported = rpcVersion >= 17;

        emit sessionSettingsReceived(arguments);

        const bool listFieldsChanged =
            labelsWereSupported != m_torrentLabelsSupported
            || groupsWereSupported != m_torrentGroupsSupported;
        const bool capabilitiesDidChange =
            listFieldsChanged
            || sequentialDownloadWasSupported != m_sequentialDownloadSupported;

        if (capabilitiesDidChange)
            emit capabilitiesChanged(capabilities());

        if (listFieldsChanged)
            getTorrentList();
        return;
    }

    if (requestType == RpcRequestType::SessionStats) {
        emit sessionStatisticsReceived(
            protocolReply.result);
        return;
    }

    if (requestType == RpcRequestType::FreeSpace) {
        const QJsonObject arguments = protocolReply.result;

        const QString path =
            arguments.value(QStringLiteral("path")).toString();

        const qint64 sizeBytes =
            static_cast<qint64>(arguments.value(QStringLiteral("size-bytes")).toDouble());

        emit freeSpaceReceived(path, sizeBytes);
        return;
    }

    if (requestType == RpcRequestType::PortTest) {
        const QJsonObject arguments = protocolReply.result;

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
        if (context.updateBlocklistAfterSuccess) {
            // The update must observe the URL currently displayed in the
            // dialog, so start it only after session-set has succeeded.
            postRpc(QStringLiteral("blocklist-update"), QJsonObject(),
                    RpcRequestType::Command);
        }

        if (context.method == QStringLiteral("blocklist-update")) {
            const QJsonObject arguments = protocolReply.result;
            const int ruleCount =
                arguments.value(QStringLiteral("blocklist-size")).toInt(
                    arguments.value(QStringLiteral("blocklist_size")).toInt(-1));
            emit blocklistUpdateFinished(ruleCount);
        }

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
            const QJsonObject arguments = protocolReply.result;
            const QJsonObject added =
                arguments.value(QStringLiteral("torrent-added")).toObject();

            if (!added.isEmpty()) {
                emit torrentAdded(
                    torrentKeyFromObject(added),
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

    const QJsonObject arguments = protocolReply.result;

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
        QSet<TorrentKey> receivedKeys;

        for (const QJsonValue &value : newTorrentList) {
            const QJsonObject object = value.toObject();
            const TorrentKey torrentKey = torrentKeyFromObject(object);
            if (!isValidTorrentKey(torrentKey))
                continue;

            receivedKeys.insert(torrentKey);
            torrentTrackerMetadata.insert(torrentKey, object);
        }

        // Drop metadata for torrents removed since the previous slow poll.
        const QList<TorrentKey> cachedKeys = torrentTrackerMetadata.keys();
        for (const TorrentKey &torrentKey : cachedKeys) {
            if (!receivedKeys.contains(torrentKey))
                torrentTrackerMetadata.remove(torrentKey);
        }

        emit torrentTrackerMetadataUpdated();
        return;
    }

    if (requestType == RpcRequestType::TorrentDetails) {
        if (!newTorrentList.isEmpty()) {
            const QJsonObject detail = newTorrentList.first().toObject();
            const TorrentKey torrentKey = torrentKeyFromObject(detail);

            if (isValidTorrentKey(torrentKey))
                emit torrentDetailsReceived(torrentDetailsFromObject(detail));
        }

        return;
    }

    if (requestType == RpcRequestType::TorrentFiles
        || requestType == RpcRequestType::TorrentPeers
        || requestType == RpcRequestType::TorrentTrackers) {
        if (!newTorrentList.isEmpty()) {
            const QJsonObject detail = newTorrentList.first().toObject();
            const TorrentKey torrentKey = torrentKeyFromObject(detail);

            if (isValidTorrentKey(torrentKey)) {
                if (requestType == RpcRequestType::TorrentFiles)
                    emit torrentFilesReceived(torrentFilesFromObject(detail));
                else if (requestType == RpcRequestType::TorrentPeers)
                    emit torrentPeersReceived(torrentPeersFromObject(detail));
                else
                    emit torrentTrackersReceived(torrentTrackersFromObject(detail));
            }
        }

        return;
    }

    if (requestType == RpcRequestType::TorrentPieces) {
        if (!newTorrentList.isEmpty()) {
            const QJsonObject detail = newTorrentList.first().toObject();
            const TorrentKey torrentKey = torrentKeyFromObject(detail);

            if (isValidTorrentKey(torrentKey))
                emit torrentPiecesReceived(torrentPiecesFromObject(detail));
        }

        return;
    }

    if (requestType == RpcRequestType::TorrentProperties) {
        if (!newTorrentList.isEmpty()) {
            const QJsonObject detail = newTorrentList.first().toObject();
            const TorrentKey torrentKey = torrentKeyFromObject(detail);

            if (isValidTorrentKey(torrentKey))
                emit torrentPropertiesReceived(torrentPropertiesFromObject(detail));
        }

        return;
    }

    if (requestType == RpcRequestType::TorrentGet) {
        QVector<torrent> incoming;
        incoming.reserve(newTorrentList.size());

        for (const QJsonValue &value : newTorrentList) {
            QJsonObject object = value.toObject();
            const TorrentKey torrentKey = torrentKeyFromObject(object);
            const QJsonObject metadata = torrentTrackerMetadata.value(torrentKey);

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

void TransmissionBackend::setSessionToken(QByteArray token)
{
    _session_token = token;
    _clientReady = true;
}

bool TransmissionBackend::setServerProfile(const ServerProfile &profile)
{
    if (profile.backendType != QStringLiteral("transmission")
        || !profile.isValid()) {
        return false;
    }
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

    serverName = profile.name;
    rpcUrl = profile.rpcUrl;
    username = profile.username;
    password = profile.password;

    _session_token.clear();
    _clientReady = false;
    updateInProgress = false;
    updateRequestedWhileInProgress = false;
    m_sequentialDownloadSupported = false;
    m_torrentLabelsSupported = false;
    m_torrentGroupsSupported = false;
    m_protocol = TransmissionProtocol::createLegacy();
    m_protocolReady = false;
    m_protocolNegotiationInProgress = false;
    m_requestsPendingAfterProtocol.clear();
    m_nextRequestId = 1;

    emit serverChanged();
    return true;
}

QString TransmissionBackend::backendName() const
{
    return QStringLiteral("Transmission");
}

QString TransmissionBackend::serverDisplayName() const
{
    if (!serverName.isEmpty())
        return serverName;

    if (!rpcUrl.isEmpty())
        return rpcUrl;

    return tr("No server configured");
}

QString TransmissionBackend::endpointUrl() const
{
    return rpcUrl;
}

QString TransmissionBackend::protocolDescription() const
{
    if (!m_protocolReady)
        return tr("Negotiating");

    return m_protocol->dialect() == TransmissionProtocolDialect::JsonRpc2
               ? QStringLiteral("Transmission JSON-RPC 2.0")
               : QStringLiteral("Transmission legacy RPC");
}

void TransmissionBackend::beginProtocolNegotiation()
{
    if (m_protocolReady || m_protocolNegotiationInProgress
        || rpcUrl.trimmed().isEmpty()) {
        return;
    }

    m_protocolNegotiationInProgress = true;
    m_protocol = TransmissionProtocol::createJsonRpc2();

    RpcRequestContext probe;
    probe.method = QStringLiteral("session-get");
    probe.type = RpcRequestType::ProtocolProbe;
    postRpc(probe);
}

void TransmissionBackend::activateProtocol(
    std::unique_ptr<TransmissionProtocol> protocol)
{
    m_protocol = std::move(protocol);
    m_protocolReady = true;
    m_protocolNegotiationInProgress = false;

    const QList<RpcRequestContext> pending =
        std::exchange(m_requestsPendingAfterProtocol, {});
    for (const RpcRequestContext &request : pending)
        postRpc(request);
}

void TransmissionBackend::failProtocolNegotiation(const QString &message)
{
    m_protocolNegotiationInProgress = false;
    m_protocolReady = false;
    const QList<RpcRequestContext> pending =
        std::exchange(m_requestsPendingAfterProtocol, {});
    const bool torrentListWasPending = std::any_of(
        pending.cbegin(), pending.cend(), [](const RpcRequestContext &request) {
            return request.type == RpcRequestType::TorrentGet;
        });

    if (torrentListWasPending) {
        updateInProgress = false;
        updateRequestedWhileInProgress = false;
        emit updateFailed(message);
        emit updateFinished();
    } else {
        emit commandFailed(QStringLiteral("protocol-negotiation"), message);
    }
}

bool TransmissionBackend::isDefiniteModernProtocolRejection(int httpStatus)
{
    return httpStatus == 400 || httpStatus == 404 || httpStatus == 405
           || httpStatus == 415 || httpStatus == 501;
}

TorrentBackendCapabilities TransmissionBackend::capabilities() const
{
    TorrentBackendCapabilities result;
    result.forceStart = true;
    result.queueManagement = true;
    result.sequentialDownload = m_sequentialDownloadSupported;
    result.labels = m_torrentLabelsSupported;
    result.groups = m_torrentGroupsSupported;
    result.torrentProperties = true;
    result.torrentSpeedLimits = true;
    result.torrentShareLimits = true;
    result.torrentBandwidthPriority = true;
    result.torrentSessionLimitOverride = true;
    result.torrentQueuePosition = true;
    result.torrentPeerLimit = true;
    result.filePriorities = true;
    result.fileLowPriority = true;
    result.trackerEditing = true;
    result.pathRenaming = true;
    result.torrentLocation = true;
    result.torrentLocationModeSelection = true;
    result.addTorrentFileSelection = true;
    result.sessionSettings = true;
    result.sessionStatistics = true;
    result.freeSpaceQuery = true;
    result.portTest = true;
    result.blocklistUpdate = true;
    return result;
}


void TransmissionBackend::getTorrentList()
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
    QJsonArray fields {
        "id",
        "name",
        "hashString",
        "percentDone",
        "recheckProgress",
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
    if (m_torrentLabelsSupported)
        fields.append(QStringLiteral("labels"));
    if (m_torrentGroupsSupported)
        fields.append(QStringLiteral("group"));
    arguments[QStringLiteral("fields")] = fields;

    postRpc("torrent-get", arguments, RpcRequestType::TorrentGet);
}

void TransmissionBackend::getTorrentTrackerMetadata()
{
    for (auto it = pendingRequests.cbegin(); it != pendingRequests.cend(); ++it) {
        if (it.value().type == RpcRequestType::TorrentListTrackers)
            return;
    }

    QJsonObject arguments;
    arguments[QStringLiteral("fields")] = QJsonArray {
        QStringLiteral("id"), QStringLiteral("hashString"),
        QStringLiteral("trackers"),
        QStringLiteral("trackerStats")
    };
    postRpc(QStringLiteral("torrent-get"), arguments,
            RpcRequestType::TorrentListTrackers);
}

QNetworkRequest TransmissionBackend::makeRequest() const
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

void TransmissionBackend::addTorrentFromFile(const QString &filePath,
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

void TransmissionBackend::addTorrentFromMagnet(const QString &magnetLink)
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

void TransmissionBackend::startTorrents(const QList<TorrentKey> &ids)
{
    postIdsCommand(QStringLiteral("torrent-start"), ids);
}

void TransmissionBackend::startAllTorrents()
{
    postRpc(QStringLiteral("torrent-start"), QJsonObject(), RpcRequestType::Command);
}

void TransmissionBackend::startTorrentsNow(const QList<TorrentKey> &ids)
{
    postIdsCommand(QStringLiteral("torrent-start-now"), ids);
}

void TransmissionBackend::stopTorrents(const QList<TorrentKey> &ids)
{
    postIdsCommand(QStringLiteral("torrent-stop"), ids);
}

void TransmissionBackend::stopAllTorrents()
{
    postRpc(QStringLiteral("torrent-stop"), QJsonObject(), RpcRequestType::Command);
}

void TransmissionBackend::removeTorrents(const QList<TorrentKey> &ids, bool deleteLocalData)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = keysToJsonArray(ids);
    arguments["delete-local-data"] = deleteLocalData;

    postRpc("torrent-remove", arguments, RpcRequestType::Command);
}

void TransmissionBackend::verifyTorrents(const QList<TorrentKey> &ids)
{
    postIdsCommand(QStringLiteral("torrent-verify"), ids);
}

void TransmissionBackend::reannounceTorrents(const QList<TorrentKey> &ids)
{
    postIdsCommand(QStringLiteral("torrent-reannounce"), ids);
}

void TransmissionBackend::setTorrentLocation(const QList<TorrentKey> &ids,
                                    const QString &location,
                                    bool moveData)
{
    const QString trimmedLocation = location.trimmed();

    if (ids.isEmpty() || trimmedLocation.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = keysToJsonArray(ids);
    arguments[QStringLiteral("location")] = trimmedLocation;
    arguments[QStringLiteral("move")] = moveData;

    postRpc(QStringLiteral("torrent-set-location"),
            arguments,
            RpcRequestType::Command);
}

void TransmissionBackend::getTorrentDetails(const TorrentKey &id)
{
    if (!isValidTorrentKey(id)
        || !prepareTorrentDetailRequest(RpcRequestType::TorrentDetails, id))
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

void TransmissionBackend::getTorrentFiles(const TorrentKey &id)
{
    if (!isValidTorrentKey(id)
        || !prepareTorrentDetailRequest(RpcRequestType::TorrentFiles, id))
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { id };
    arguments[QStringLiteral("fields")] = QJsonArray {
        QStringLiteral("id"), QStringLiteral("hashString"),
        QStringLiteral("downloadDir"),
        QStringLiteral("files"), QStringLiteral("wanted"),
        QStringLiteral("priorities")
    };
    postRpc(QStringLiteral("torrent-get"), arguments, RpcRequestType::TorrentFiles);
}

void TransmissionBackend::getTorrentPeers(const TorrentKey &id)
{
    if (!isValidTorrentKey(id)
        || !prepareTorrentDetailRequest(RpcRequestType::TorrentPeers, id))
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { id };
    arguments[QStringLiteral("fields")] = QJsonArray {
        QStringLiteral("id"), QStringLiteral("hashString"),
        QStringLiteral("peers")
    };
    postRpc(QStringLiteral("torrent-get"), arguments, RpcRequestType::TorrentPeers);
}

void TransmissionBackend::getTorrentTrackers(const TorrentKey &id)
{
    if (!isValidTorrentKey(id)
        || !prepareTorrentDetailRequest(RpcRequestType::TorrentTrackers, id))
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { id };
    arguments[QStringLiteral("fields")] = QJsonArray {
        QStringLiteral("id"), QStringLiteral("hashString"),
        QStringLiteral("trackers"),
        QStringLiteral("trackerStats")
    };
    postRpc(QStringLiteral("torrent-get"), arguments, RpcRequestType::TorrentTrackers);
}


void TransmissionBackend::getTorrentPieces(const TorrentKey &id)
{
    if (!isValidTorrentKey(id)
        || !prepareTorrentDetailRequest(RpcRequestType::TorrentPieces, id))
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { id };

    arguments[QStringLiteral("fields")] = QJsonArray {
        QStringLiteral("id"),
        QStringLiteral("hashString"),
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

void TransmissionBackend::getTorrentProperties(const TorrentKey &id)
{
    if (!isValidTorrentKey(id)
        || !prepareTorrentDetailRequest(RpcRequestType::TorrentProperties, id))
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

void TransmissionBackend::setTorrentFilesWanted(const TorrentKey &torrentId,
                                       const QList<int> &fileIndices,
                                       bool wanted)
{
    if (!isValidTorrentKey(torrentId) || fileIndices.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = QJsonArray { torrentId };

    if (wanted)
        arguments["files-wanted"] = indicesToJsonArray(fileIndices);
    else
        arguments["files-unwanted"] = indicesToJsonArray(fileIndices);

    postRpc("torrent-set", arguments, RpcRequestType::Command);
}

void TransmissionBackend::setTorrentFilesPriority(const TorrentKey &torrentId,
                                         const QList<int> &fileIndices,
                                         int priority)
{
    if (!isValidTorrentKey(torrentId) || fileIndices.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = QJsonArray { torrentId };

    switch (priority) {
    case 1:
        arguments["priority-high"] = indicesToJsonArray(fileIndices);
        break;
    case -1:
        arguments["priority-low"] = indicesToJsonArray(fileIndices);
        break;
    case 0:
    default:
        arguments["priority-normal"] = indicesToJsonArray(fileIndices);
        break;
    }

    postRpc("torrent-set", arguments, RpcRequestType::Command);
}

void TransmissionBackend::setTorrentFilesWantedAndPriority(const TorrentKey &torrentId,
                                                  const QList<int> &fileIndices,
                                                  bool wanted,
                                                  int priority)
{
    if (!isValidTorrentKey(torrentId) || fileIndices.isEmpty())
        return;

    QJsonObject arguments;
    arguments["ids"] = QJsonArray { torrentId };

    if (wanted) {
        arguments["files-wanted"] = indicesToJsonArray(fileIndices);

        switch (priority) {
        case 1:
            arguments["priority-high"] = indicesToJsonArray(fileIndices);
            break;
        case -1:
            arguments["priority-low"] = indicesToJsonArray(fileIndices);
            break;
        case 0:
        default:
            arguments["priority-normal"] = indicesToJsonArray(fileIndices);
            break;
        }
    } else {
        arguments["files-unwanted"] = indicesToJsonArray(fileIndices);
    }

    postRpc("torrent-set", arguments, RpcRequestType::Command);
}


void TransmissionBackend::addTorrentTracker(const TorrentKey &torrentId, const QString &announceUrl)
{
    const QString trimmedUrl = announceUrl.trimmed();

    if (!isValidTorrentKey(torrentId) || trimmedUrl.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { torrentId };
    arguments[QStringLiteral("trackerAdd")] = QJsonArray { trimmedUrl };

    postRpc(QStringLiteral("torrent-set"),
            arguments,
            RpcRequestType::Command);
}

void TransmissionBackend::editTorrentTracker(const TorrentKey &torrentId,
                                    int trackerId,
                                    const QString &announceUrl)
{
    const QString trimmedUrl = announceUrl.trimmed();

    if (!isValidTorrentKey(torrentId) || trackerId < 0 || trimmedUrl.isEmpty())
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

void TransmissionBackend::removeTorrentTracker(const TorrentKey &torrentId, int trackerId)
{
    if (!isValidTorrentKey(torrentId) || trackerId < 0)
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { torrentId };
    arguments[QStringLiteral("trackerRemove")] = QJsonArray { trackerId };

    postRpc(QStringLiteral("torrent-set"),
            arguments,
            RpcRequestType::Command);
}

void TransmissionBackend::renameTorrentPath(const TorrentKey &torrentId,
                                   const QString &path,
                                   const QString &newName)
{
    const QString trimmedPath = path.trimmed();
    const QString trimmedName = newName.trimmed();

    if (!isValidTorrentKey(torrentId) || trimmedPath.isEmpty() || trimmedName.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = QJsonArray { torrentId };
    arguments[QStringLiteral("path")] = trimmedPath;
    arguments[QStringLiteral("name")] = trimmedName;

    postRpc(QStringLiteral("torrent-rename-path"),
            arguments,
            RpcRequestType::Command);
}

void TransmissionBackend::setTorrentProperties(const TorrentKey &torrentId,
                                      const TorrentPropertyChanges &properties)
{
    if (!isValidTorrentKey(torrentId))
        return;

    QJsonObject arguments;
    arguments.insert(QStringLiteral("ids"), QJsonArray { torrentId });
    arguments.insert(QStringLiteral("bandwidthPriority"), properties.bandwidthPriority);
    arguments.insert(QStringLiteral("honorsSessionLimits"), properties.honorsSessionLimits);
    arguments.insert(QStringLiteral("queuePosition"), properties.queuePosition);
    arguments.insert(QStringLiteral("peer-limit"), properties.peerLimit);
    arguments.insert(QStringLiteral("downloadLimited"), properties.downloadLimited);
    arguments.insert(QStringLiteral("downloadLimit"), properties.downloadLimit);
    arguments.insert(QStringLiteral("uploadLimited"), properties.uploadLimited);
    arguments.insert(QStringLiteral("uploadLimit"), properties.uploadLimit);
    arguments.insert(QStringLiteral("seedRatioMode"), properties.seedRatioMode);
    arguments.insert(QStringLiteral("seedRatioLimit"), properties.seedRatioLimit);
    arguments.insert(QStringLiteral("seedIdleMode"), properties.seedIdleMode);
    arguments.insert(QStringLiteral("seedIdleLimit"), properties.seedIdleLimit);

    QJsonArray labels;
    for (const QString &label : properties.labels)
        labels.append(label);
    arguments.insert(QStringLiteral("labels"), labels);

    if (properties.setGroup)
        arguments.insert(QStringLiteral("group"), properties.group);

    postRpc(QStringLiteral("torrent-set"),
            arguments,
            RpcRequestType::Command);
}

void TransmissionBackend::setTorrentsSequentialDownload(const QList<TorrentKey> &ids,
                                               bool enabled)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = keysToJsonArray(ids);
    arguments[QStringLiteral("sequential_download")] = enabled;

    postRpc(QStringLiteral("torrent-set"),
            arguments,
            RpcRequestType::Command);
}

void TransmissionBackend::setTorrentsBandwidthPriority(const QList<TorrentKey> &ids,
                                              int priority)
{
    if (ids.isEmpty())
        return;

    QJsonObject arguments;
    arguments[QStringLiteral("ids")] = keysToJsonArray(ids);
    arguments[QStringLiteral("bandwidthPriority")] = priority;

    postRpc(QStringLiteral("torrent-set"),
            arguments,
            RpcRequestType::Command);
}

void TransmissionBackend::queueMoveTop(const QList<TorrentKey> &ids)
{
    postIdsCommand(QStringLiteral("queue-move-top"), ids);
}

void TransmissionBackend::queueMoveUp(const QList<TorrentKey> &ids)
{
    postIdsCommand(QStringLiteral("queue-move-up"), ids);
}

void TransmissionBackend::queueMoveDown(const QList<TorrentKey> &ids)
{
    postIdsCommand(QStringLiteral("queue-move-down"), ids);
}

void TransmissionBackend::queueMoveBottom(const QList<TorrentKey> &ids)
{
    postIdsCommand(QStringLiteral("queue-move-bottom"), ids);
}

void TransmissionBackend::getSessionSettings()
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
        "blocklist-enabled",
        "blocklist-url",

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

void TransmissionBackend::getSessionStatistics()
{
    postRpc(QStringLiteral("session-stats"),
            QJsonObject(),
            RpcRequestType::SessionStats);
}

void TransmissionBackend::setSessionSettings(const QJsonObject &settings)
{
    if (settings.isEmpty())
        return;

    postRpc("session-set", settings, RpcRequestType::Command);
}

void TransmissionBackend::updateBlocklist(const QJsonObject &changedSettings)
{
    if (changedSettings.isEmpty()) {
        postRpc(QStringLiteral("blocklist-update"), QJsonObject(),
                RpcRequestType::Command);
        return;
    }

    RpcRequestContext context;
    context.method = QStringLiteral("session-set");
    context.type = RpcRequestType::Command;
    context.arguments = changedSettings;
    context.updateBlocklistAfterSuccess = true;
    postRpc(context);
}

void TransmissionBackend::getFreeSpace(const QString &path)
{
    if (path.trimmed().isEmpty())
        return;

    QJsonObject arguments;
    arguments["path"] = path;

    postRpc(QStringLiteral("free-space"), arguments, RpcRequestType::FreeSpace);
}

void TransmissionBackend::testPortForwarding()
{
    postRpc(QStringLiteral("port-test"), QJsonObject(), RpcRequestType::PortTest);
}

void TransmissionBackend::addTorrentFile(const QString &filePath,
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
                         indicesToJsonArray(filesUnwanted));

    if (!priorityLow.isEmpty())
        arguments.insert(QStringLiteral("priority-low"),
                         indicesToJsonArray(priorityLow));

    if (!priorityHigh.isEmpty())
        arguments.insert(QStringLiteral("priority-high"),
                         indicesToJsonArray(priorityHigh));

    RpcRequestContext context;
    context.method = QStringLiteral("torrent-add");
    context.arguments = arguments;
    context.type = RpcRequestType::Command;
    context.torrentFilePath = filePath;
    context.deleteTorrentFileOnSuccess = deleteFileOnSuccess;

    postRpc(context);
}

void TransmissionBackend::addMagnetLink(const QString &magnetLink,
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
