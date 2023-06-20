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

class rpc_client
{
public:
    rpc_client();
    void init();
    void getTorrentList();
    bool isClientReady();
private:
    bool _clientReady = false;
    QByteArray _session_token;
    QNetworkAccessManager m_manager;
    void setSessionToken(QByteArray token);
    QJsonArray torrentList;
};

#endif // RPC_CLIENT_H
