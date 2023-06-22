#include "rpc_client.h"

rpc_client::rpc_client()
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
            emit listUpdated();
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

int rpc_client::countTorrents()
{
    return rpc_client::torrentVector.count();
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

void rpc_client::getTorrentList()
{
    QNetworkRequest request = QNetworkRequest(transmissionURL());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(QByteArray("X-Transmission-Session-Id"), QByteArray(rpc_client::_session_token));

    QByteArray data("{\"method\":\"torrent-get\",\"arguments\": {\"fields\":[\"rateDownload\",\"rateUpload\",\"id\",\"percentDone\",\"status\",\"name\",\"uploadRatio\",\"eta\",\"files\",\"peers\"]}}");
    na_manager->post(request, data);
}
