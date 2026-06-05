#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include "rpc_client.h"

rpc_client::rpc_client(QObject *parent)
    : QAbstractTableModel(parent)
//   : QObject{parent}
{
}

void rpc_client::init()
{
    na_manager = new QNetworkAccessManager;
    QObject::connect(na_manager,&QNetworkAccessManager::finished, this, &rpc_client::replyFinished);

    // make request
    QNetworkRequest request = QNetworkRequest(transmissionURL());
    request.setRawHeader("Authorization", rpc_client::authString().toLocal8Bit());
    na_manager->get(request);
}

QString rpc_client::authString()
{
    QString concatenated = username+':'+password;
    QByteArray data = concatenated.toLocal8Bit().toBase64();
    QString headerData = "Basic " + data;

    return headerData;
}

void rpc_client::replyFinished(QNetworkReply * reply)
{
    if(reply->error() == QNetworkReply::NoError || reply->error() == QNetworkReply::ContentConflictError){
        //qDebug() << "Network reply OK";
        //qDebug() << reply->error();
        if (_session_token.isEmpty())
        {
            QByteArray _token = reply->rawHeader("X-Transmission-Session-Id");
            rpc_client::setSessionToken(_token);
            updateInProgress = false;
            getTorrentList();
        } else {
            QString contents = QString::fromUtf8(reply->readAll());
            QJsonDocument doc = QJsonDocument::fromJson(contents.toUtf8());
            QJsonValue dObj = doc["arguments"];
            QJsonValue torrentsObj = dObj["torrents"];
            const QJsonArray newTorrentList = dObj["torrents"].toArray();

            QVector<torrent> incoming;
            incoming.reserve(newTorrentList.size());

            rpc_client::torrentList = torrentsObj.toArray();

            for (const auto &obj : newTorrentList)
            {
                incoming.append(torrent(obj));
            }

            torrentList = newTorrentList;
            applyUpdate(incoming);

            //qDebug() << "List updated";
            updateInProgress = false;
            emit listUpdated();
            emit updateFinished();
        }
        } else {
        if (reply->error() != QNetworkReply::NoError) {
            const QString message = reply->errorString();
            qDebug() << "Network reply ERROR:" << message;

            emit updateFailed(message);
            updateInProgress = false;
            emit updateFinished();

            reply->deleteLater();
            return;
        }
    }
}

bool rpc_client::isClientReady()
{
    return _clientReady;
}

void rpc_client::setSessionToken(QByteArray token)
{
    _session_token = token;
    _clientReady = true;
}

torrent rpc_client::getTorrent(int item)
{
    return rpc_client::torrentVector[item];
}

int rpc_client::countTorrents() const
{
    return torrentVector.count();
}

QJsonArray rpc_client::torrents()
{
    return rpc_client::torrentList;
}

QUrl rpc_client::transmissionURL()
{
    QString URL;
    if (useSSL)
    {
        URL += "https://";
    } else {
        URL += "http://";
    }

    URL += server + ":" + QString::number(port) + serverPath;

    return QUrl(URL);
}

QString rpc_client::getServer()
{
    return server + ":" + QString::number(port);
}

void rpc_client::getTorrentList()
{
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

    na_manager->post(
        makeRequest(),
        makeRpcPayload("torrent-get", arguments)
        );
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

    if (!_session_token.isEmpty()) {
        request.setRawHeader(
            "X-Transmission-Session-Id",
            _session_token
            );
    }

    if (!username.isEmpty() || !password.isEmpty()) {
        const QByteArray auth =
            QString("%1:%2").arg(username, password).toUtf8().toBase64();

        request.setRawHeader("Authorization", "Basic " + auth);
    }

    return request;
}

// QTableView methods
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
    // "Name" << "Completed" << "Status" << "Download" << "Upload" << "Ratio" << "ETA";
}

int rpc_client::rowForId(int id) const
{
    return m_rowById.value(id, -1);
}

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

void rpc_client::removeTorrent(int id, bool deleteLocalData)
{
    QJsonObject arguments;
    arguments["ids"] = QJsonArray { id };
    arguments["delete-local-data"] = deleteLocalData;

    na_manager->post(
        makeRequest(),
        makeRpcPayload("torrent-remove", arguments)
        );
}

void rpc_client::startTorrent(int id)
{
    QJsonObject arguments;
    arguments["ids"] = QJsonArray { id };

    na_manager->post(
        makeRequest(),
        makeRpcPayload("torrent-start", arguments)
        );
}

void rpc_client::stopTorrent(int id)
{
    QJsonObject arguments;
    arguments["ids"] = QJsonArray { id };

    na_manager->post(
        makeRequest(),
        makeRpcPayload("torrent-stop", arguments)
        );
}

void rpc_client::addTorrentFromFile(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open torrent file:" << filePath << file.errorString();
        return;
    }

    const QByteArray torrentData = file.readAll();
    file.close();

    if (torrentData.isEmpty()) {
        qWarning() << "Torrent file is empty:" << filePath;
        return;
    }

    QJsonObject arguments;
    arguments["metainfo"] = QString::fromLatin1(torrentData.toBase64());

    na_manager->post(
        makeRequest(),
        makeRpcPayload("torrent-add", arguments)
        );
}

void rpc_client::addTorrentFromMagnet(const QString &magnetLink)
{
    if (magnetLink.trimmed().isEmpty()) {
        qWarning() << "Empty magnet link";
        return;
    }

    QJsonObject arguments;
    arguments["filename"] = magnetLink.trimmed();

    na_manager->post(
        makeRequest(),
        makeRpcPayload("torrent-add", arguments)
        );
}