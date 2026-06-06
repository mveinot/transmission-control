#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <QApplication>
#include <QAbstractTableModel>
#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include "torrent.h"

class rpc_client: public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        IdColumn,
        NameColumn,
        SizeColumn,
        PercentDoneColumn,
        StatusColumn,
        RateDownloadColumn,
        RateUploadColumn,
        UploadRatioColumn,
        EtaColumn,
        ColumnCount
    };
    rpc_client(QObject *parent);
    void init();
    void getTorrentList();
    QJsonArray torrents();
    bool isClientReady();
    int countTorrents() const;
    QString authString();
    torrent getTorrent(int item);
    QString getServer();
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    int rowForId(int id) const;
    bool updateFromJson(const QByteArray &json);
    void removeTorrent(int id, bool deleteLocalData);
    void startTorrent(int id);
    void stopTorrent(int id);
    void addTorrentFromFile(const QString &filePath);
    void addTorrentFromMagnet(const QString &magnetLink);
    void reannounceTorrent(int id);
    void verifyTorrent(int id);

signals:
    void listUpdated();
    void updateStarted();
    void updateFinished();
    void updateFailed(const QString &message);

private:
    struct TransmissionServer
    {
        QString name;
        QString rpcUrl;
        QString username;
        QString password;
    };

    enum class RpcRequestType {
        TorrentGet,
        Command
    };

    QString username = "vmark";
    QString password = "8kfkfvq9";
    bool _clientReady = false;
    bool updateInProgress = false;
    QByteArray _session_token;
    QNetworkAccessManager *na_manager;
    QJsonArray torrentList;
    QVector<torrent> torrentVector;
    QHash<int, int> m_rowById;
    bool useSSL = false;
    QString server = "nas2.mvgrafx.net";
    int port = 9091;
    QString serverPath = "/transmission/rpc";
    QString rpcUrl = "http://nas2.mvgrafx.net:9091/transmission/rpc";

    void setSessionToken(QByteArray token);
    void rebuildIndex();
    void applyUpdate(const QVector<torrent> &incoming);
    QUrl transmissionURL();
    QByteArray makeRpcPayload(const QString &method,
                              const QJsonObject &arguments = {}) const;
    QNetworkRequest makeRequest() const;
    void postRpc(const QString &method,
                 const QJsonObject &arguments,
                 RpcRequestType type);


public slots:
    void replyFinished(QNetworkReply *reply);
};

#endif // RPC_CLIENT_H
