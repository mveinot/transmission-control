#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <QApplication>
#include <QAbstractTableModel>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include "torrent.h"

class rpc_client: public QAbstractTableModel
{
    Q_OBJECT

public:
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
    enum Column
    {
        IdColumn,
        NameColumn,
        PercentDoneColumn,
        StatusColumn,
        RateDownloadColumn,
        RateUploadColumn,
        UploadRatioColumn,
        EtaColumn,
        ColumnCount
    };

signals:
    void listUpdated();
    void updateStarted();
    void updateFinished();
    void updateFailed(const QString &message);

private:
    QString username = "vmark";
    QString password = "8kfkfvq9";
    bool _clientReady = false;
    bool updateInProgress = false;
    QByteArray _session_token;
    QNetworkAccessManager *na_manager;
    void setSessionToken(QByteArray token);
    QJsonArray torrentList;
    QVector<torrent> torrentVector;
    void rebuildIndex();
    void applyUpdate(const QVector<torrent> &incoming);
    QHash<int, int> m_rowById;
    bool useSSL = false;
    QString server = "nas2.mvgrafx.net";
    int port = 9091;
    QString serverPath = "/transmission/rpc";
    QUrl transmissionURL();// = QUrl("http://nas2.mvgrafx.net:9091/transmission/rpc");


public slots:
    void replyFinished(QNetworkReply *reply);
};

#endif // RPC_CLIENT_H
