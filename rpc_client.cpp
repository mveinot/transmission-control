#include "rpc_client.h"

rpc_client::rpc_client()
{
}

void rpc_client::init()
{
    na_manager = new QNetworkAccessManager;
    QObject::connect(na_manager,&QNetworkAccessManager::finished, this, &rpc_client::replyFinished);

    // make request
    QNetworkRequest request = QNetworkRequest(QUrl("http://nas2.mvgrafx.net:9091/transmission/rpc"));
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
    //if(reply->error() == QNetworkReply::NoError){
        qDebug() << "Network reply OK";
    qDebug() << reply->error();
        if (_session_token.isEmpty())
        {
            QByteArray _token = reply->rawHeader("X-Transmission-Session-Id");
            rpc_client::setSessionToken(_token);
        } else {
            QString contents = QString::fromUtf8(reply->readAll());
            QJsonDocument doc = QJsonDocument::fromJson(contents.toUtf8());
            QJsonValue dObj = doc["arguments"];
            QJsonValue torrentsObj = dObj["torrents"];
            rpc_client::torrentList = torrentsObj.toArray();
            qDebug() << rpc_client::torrentList;
        }
        /*} else {
        QString err = reply->errorString();
        QString contents = QString::fromUtf8(reply->readAll());
        qDebug() << "Network reply ERROR";
        qDebug() << err;
        qDebug() << contents;
    }*/
}

bool rpc_client::isClientReady()
{
    return _clientReady;
}

void rpc_client::setSessionToken(QByteArray token)
{
    _session_token = token;
    _clientReady = true;
    qDebug() << _session_token;
}

int rpc_client::countTorrents()
{
    return torrentList.count();
}

QJsonArray rpc_client::torrents()
{
    return rpc_client::torrentList;
}

void rpc_client::getTorrentList()
{
    QNetworkRequest request = QNetworkRequest(QUrl("http://nas2.mvgrafx.net:9091/transmission/rpc"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(QByteArray("X-Transmission-Session-Id"), QByteArray(rpc_client::_session_token));
    qDebug() << _session_token;

    QByteArray data("{\"method\":\"torrent-get\",\"arguments\": {\"fields\":[\"rateDownload\",\"rateUpload\",\"id\",\"percentDone\",\"status\",\"name\"]}}");
    na_manager->post(request, data);
}
