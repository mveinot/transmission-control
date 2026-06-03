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
            getTorrentList();
        } else {
            QString contents = QString::fromUtf8(reply->readAll());
            QJsonDocument doc = QJsonDocument::fromJson(contents.toUtf8());
            QJsonValue dObj = doc["arguments"];
            QJsonValue torrentsObj = dObj["torrents"];
            rpc_client::torrentList = torrentsObj.toArray();
            rpc_client::torrentVector.clear();
            foreach (const QJsonValue &obj, rpc_client::torrentList)
            {
                torrent tor(obj);
                rpc_client::torrentVector.append(tor);
            }
            qDebug() << "List updated";
            emit listUpdated();
            QModelIndex topLeft = createIndex(0,0);
            QModelIndex bottomRight = createIndex(8,8);
            emit dataChanged(topLeft,bottomRight);
        }
        } else {
        QString err = reply->errorString();
        QString contents = QString::fromUtf8(reply->readAll());
        qDebug() << "Network reply ERROR";
        qDebug() << err;
        qDebug() << contents;
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
    //qDebug() << _session_token;
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
    QNetworkRequest request = QNetworkRequest(transmissionURL());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(QByteArray("X-Transmission-Session-Id"), QByteArray(rpc_client::_session_token));

    QByteArray data("{\"method\":\"torrent-get\",\"arguments\": {\"fields\":[\"rateDownload\",\"rateUpload\",\"id\",\"percentDone\",\"status\",\"name\",\"uploadRatio\",\"eta\",\"files\",\"peers\"]}}");
    na_manager->post(request, data);
}


// QTableView methods
int rpc_client::rowCount(const QModelIndex & /*parent*/) const
{
    qDebug() << "rowCount" << countTorrents();
    return 20;
}


int rpc_client::columnCount(const QModelIndex & /*parent*/) const
{
    return 8;
}

QVariant rpc_client::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole)
        return QString("Row%1, Column%2")
            .arg(index.row() + 1)
            .arg(index.column() +1);

    return QVariant();
}

QVariant rpc_client::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            return QString("Name");
        case 1:
            return QString("Completed");
        case 2:
            return QString("Status");
        case 3:
            return QString("Download");
        case 4:
            return QString("Upload");
        case 5:
            return QString("Ratio");
        case 6:
            return QString("ETA");
        case 7:
            return QString("ID");
        }
    }
    return QVariant();
    // "Name" << "Completed" << "Status" << "Download" << "Upload" << "Ratio" << "ETA";
}
