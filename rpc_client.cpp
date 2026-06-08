#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
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
    QNetworkRequest request = makeRequest();

    request.setAttribute(
        RpcRequestTypeAttribute,
        static_cast<int>(type)
        );

    request.setAttribute(RpcMethodAttribute, method);

    na_manager->post(
        request,
        makeRpcPayload(method, arguments)
        );
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
    const auto requestType =
        static_cast<RpcRequestType>(
            reply->request()
                .attribute(
                    RpcRequestTypeAttribute,
                    static_cast<int>(RpcRequestType::Command)
                    )
                .toInt()
            );

    const bool isTorrentGet =
        requestType == RpcRequestType::TorrentGet;

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

        if (isTorrentGet) {
            updateInProgress = false;
            getTorrentList();
        }

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

    if (!isTorrentGet) {
        const QString method =
            reply->request().attribute(RpcMethodAttribute).toString();

        if (method == "torrent-add") {
            const bool deleteFileOnSuccess =
                reply->request()
                    .attribute(DeleteTorrentFileOnSuccessAttribute, false)
                    .toBool();

            const QString torrentFilePath =
                reply->request()
                    .attribute(TorrentFilePathAttribute)
                    .toString();

            if (deleteFileOnSuccess && !torrentFilePath.isEmpty()) {
                if (QFile::remove(torrentFilePath)) {
                    qDebug() << "Deleted torrent file after successful add:"
                             << torrentFilePath;
                } else {
                    qWarning() << "Could not delete torrent file after add:"
                               << torrentFilePath;
                }
            }
        }

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

    QVector<torrent> incoming;
    incoming.reserve(newTorrentList.size());

    for (const QJsonValue &obj : newTorrentList) {
        incoming.append(torrent(obj));
    }

    emit torrentsReceived(incoming);

    emit listUpdated();

    finishTorrentGet();
}

/*
bool rpc_client::isClientReady()
{
    return _clientReady;
}
*/
void rpc_client::setSessionToken(QByteArray token)
{
    _session_token = token;
    _clientReady = true;
}

/*
torrent rpc_client::getTorrent(int item)
{
    return rpc_client::torrentVector[item];
}
*/
/*
int rpc_client::countTorrents() const
{
    return torrentVector.count();
}

QJsonArray rpc_client::torrents()
{
    return rpc_client::torrentList;
}
*/
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

    const int currentIndex =
        settings.value("servers/currentIndex", defaultIndex).toInt();

    if (setServerFromSettingsIndex(currentIndex))
        return true;

    if (defaultIndex != currentIndex)
        return setServerFromSettingsIndex(defaultIndex);

    return false;
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

    //clearTorrents();
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
        "files",
        "peers"
    };

    postRpc("torrent-get", arguments, RpcRequestType::TorrentGet);
}
/*
void rpc_client::clearTorrents()
{
    beginResetModel();

    torrentList = {};
    torrentVector.clear();
    m_rowById.clear();

    endResetModel();

    emit listUpdated();
}
*/

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

// QTableView methods
/*
int rpc_client::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : torrentVector.size();
}

int rpc_client::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant rpc_client::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= torrentVector.size()) {
        return {};
    }

    const torrent &t = torrentVector.at(index.row());

    if (role == Qt::DisplayRole)
        switch (index.column()) {
        case IdColumn:           return t.getId();
        case NameColumn:         return t.getName();
        case PercentDoneColumn:  return QString::number(t.getPercentDone(), 'f', 1) + "%";
        case StatusColumn:       return t.getStatus();
        case RateDownloadColumn: return t.getRateDownload();
        case RateUploadColumn:   return t.getRateUpload();
        case UploadRatioColumn:  return t.getUploadRatio();
        case EtaColumn:          return t.getEta();
        case SizeColumn:         return t.getSize();
        default:                 return {};
        }

    if (role == Qt::UserRole) {
        return t.getId();
    }

    if (role == Qt::UserRole + 1) {
        switch (index.column()) {
        case IdColumn:           return t.getId();
        case NameColumn:         return t.getName();
        case PercentDoneColumn:  return t.getPercentDone();
        case StatusColumn:       return t.getStatus();
        case RateDownloadColumn: return t.getRateDownload();
        case RateUploadColumn:   return t.getRateUpload();
        case UploadRatioColumn:  return t.getUploadRatio();
        case EtaColumn:          return t.getEta();
        case SizeColumn:         return t.getSizeBytes();
        default:                 return {};
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case IdColumn:
        case PercentDoneColumn:
        case RateDownloadColumn:
        case RateUploadColumn:
        case UploadRatioColumn:
        case SizeColumn:
        case EtaColumn:
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    return {};
}

QVariant rpc_client::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case IdColumn:           return "ID";
        case NameColumn:         return "Name";
        case PercentDoneColumn:  return "Completed";
        case StatusColumn:       return "Status";
        case RateDownloadColumn: return "Down";
        case RateUploadColumn:   return "Up";
        case UploadRatioColumn:  return "Ratio";
        case EtaColumn:          return "ETA";
        case SizeColumn:         return "Size";
        default:                 return {};
        }
    }
    return section + 1;
}

int rpc_client::rowForId(int id) const
{
    return m_rowById.value(id, -1);
}
*/
/*
bool rpc_client::updateFromJson(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || doc.isNull()) {
        return false;
    }

    QJsonArray array;
    if (doc.isArray()) {
        array = doc.array();
    } else if (doc.isObject()) {
        array = doc.object().value("torrents").toArray();
    } else {
        return false;
    }

    QVector<torrent> incoming;
    incoming.reserve(array.size());
    for (const auto &value : std::as_const(array)) {
        incoming.append(torrent(value));
    }

    applyUpdate(incoming);
    return true;
}
*/
/*
void rpc_client::rebuildIndex()
{
    m_rowById.clear();
    m_rowById.reserve(torrentVector.size());
    for (int row = 0; row < torrentVector.size(); ++row) {
        m_rowById.insert(torrentVector.at(row).getId(), row);
    }
}

void rpc_client::applyUpdate(const QVector<torrent> &incoming)
{
    QHash<int, torrent> incomingById;
    incomingById.reserve(incoming.size());
    QSet<int> incomingIds;
    incomingIds.reserve(incoming.size());

    for (const torrent &t : incoming) {
        incomingById.insert(t.getId(), t);
        incomingIds.insert(t.getId());
    }

    // Remove rows that disappeared. Remove from back to front.
    for (int row = torrentVector.size() - 1; row >= 0; --row) {
        const int id = torrentVector.at(row).getId();
        if (!incomingIds.contains(id)) {
            beginRemoveRows(QModelIndex(), row, row);
            torrentVector.removeAt(row);
            endRemoveRows();
        }
    }

    rebuildIndex();

    // Update existing rows in place.
    for (int row = 0; row < torrentVector.size(); ++row) {
        const int id = torrentVector.at(row).getId();
        auto it = incomingById.constFind(id);
        if (it == incomingById.cend()) {
            continue;
        }

        const torrent &updated = it.value();


        if (!torrentVector.at(row).sameDisplayData(updated)) {
            torrentVector[row] = updated;
            emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
        }


        const bool displayChanged =
            !torrentVector.at(row).sameDisplayData(updated);

        torrentVector[row] = updated;

        if (displayChanged) {
            emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
        }
    }

    rebuildIndex();

    // Insert new rows. Appending is simplest; proxy handles sorted view order.
    for (const torrent &t : incoming) {
        if (!m_rowById.contains(t.getId())) {
            const int row = torrentVector.size();
            beginInsertRows(QModelIndex(), row, row);
            torrentVector.append(t);
            endInsertRows();
            m_rowById.insert(t.getId(), row);
        }
    }

    rebuildIndex();
}
*/

void rpc_client::removeTorrent(int id, bool deleteLocalData)
{
    removeTorrents({ id }, deleteLocalData);
}

void rpc_client::startTorrent(int id)
{
    startTorrents({ id });
}


void rpc_client::stopTorrent(int id)
{
    stopTorrents({ id });
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

    QNetworkRequest request = makeRequest();

    request.setAttribute(
        RpcRequestTypeAttribute,
        static_cast<int>(RpcRequestType::Command)
        );

    request.setAttribute(RpcMethodAttribute, QStringLiteral("torrent-add"));
    request.setAttribute(TorrentFilePathAttribute, filePath);
    request.setAttribute(DeleteTorrentFileOnSuccessAttribute, deleteFileOnSuccess);

    na_manager->post(
        request,
        makeRpcPayload("torrent-add", arguments)
        );
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

void rpc_client::reannounceTorrent(int id)
{
    reannounceTorrents({ id });
}

void rpc_client::verifyTorrent(int id)
{
    verifyTorrents({ id });
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