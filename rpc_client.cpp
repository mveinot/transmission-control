#include "rpc_client.h"

rpc_client::rpc_client()
{
}

void rpc_client::init()
{
    na_manager = new QNetworkAccessManager;
    QObject::connect(na_manager,&QNetworkAccessManager::finished, this, &rpc_client::replyFinished);

    QString concatenated = "vmark:8kfkfvq9"; //username:password
    QByteArray data = concatenated.toLocal8Bit().toBase64();
    QString headerData = "Basic " + data;

    // make request
    QNetworkRequest request = QNetworkRequest(QUrl("http://nas2.mvgrafx.net:9091/transmission/rpc"));
    request.setRawHeader("Authorization", headerData.toLocal8Bit());
    na_manager->get(request);
}

void rpc_client::replyFinished(QNetworkReply * reply)
{
    QByteArray _token = reply->rawHeader("X-Transmission-Session-Id");
    //qDebug() << "debug: " << _token;
    rpc_client::setSessionToken(_token);
    //reply->deleteLater(); // make sure to clean up
    //getTorrentList();
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

void rpc_client::getTorrentList()
{
    //"{\"method\":\"torrent-get\",\"arguments\": {\"fields\":[\"rateDownload\",\"rateUpload\",\"id\",\"percentDone\",\"status\",\"name\"]}}"
    QNetworkRequest request = QNetworkRequest(QUrl("http://nas2.mvgrafx.net:9091/transmission/rpc"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(QByteArray("X-Transmission-Session-Id"), QByteArray(rpc_client::_session_token));
    qDebug() << _session_token;

    QByteArray data("{\"method\":\"torrent-get\",\"arguments\": {\"fields\":[\"rateDownload\",\"rateUpload\",\"id\",\"percentDone\",\"status\",\"name\"]}}");
    QNetworkReply *reply = na_manager->post(request, data);

    QObject::connect(reply, &QNetworkReply::finished, [=](){
        if(reply->error() == QNetworkReply::NoError){
            qDebug() << "OK";
            QString contents = QString::fromUtf8(reply->readAll());
            QJsonDocument doc = QJsonDocument::fromJson(contents.toUtf8());
            //QJsonObject obj = doc.object();
            QJsonValue dObj = doc["arguments"];
            QJsonValue torrentsObj = dObj["torrents"];
            //QJsonArray torrentsArray = torrentsObj.toArray();
            rpc_client::torrentList = torrentsObj.toArray();
            //["torrents"].toArray();
            //qDebug() << obj;
            //qDebug() << torrentsArray;
            // Iterate over the array and print each value
            /*
            for (const QJsonValueRef item : torrentsArray)
            {
                qDebug()<< item.toObject().value("name").toString();
            }
            */
        }
        else{
            QString err = reply->errorString();
            QString contents = QString::fromUtf8(reply->readAll());
            qDebug() << "ERROR";
            qDebug() << err;
            qDebug() << contents;
        }
        reply->deleteLater();
    });
}
