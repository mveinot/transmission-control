#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <QApplication>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

class rpc_client: public QObject
{
    Q_OBJECT

public:
    rpc_client();
    void init();
    void getTorrentList();
    QJsonArray torrents();
    bool isClientReady();
    int countTorrents();
    QString authString();
private:
    QString username = "vmark";
    QString password = "8kfkfvq9";
    bool _clientReady = false;
    QByteArray _session_token;
    QNetworkAccessManager *na_manager;
    void setSessionToken(QByteArray token);
    QJsonArray torrentList;
public slots:
    void replyFinished(QNetworkReply *reply);
};

#endif // RPC_CLIENT_H
