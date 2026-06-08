#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <QApplication>
//#include <QAbstractTableModel>
#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include "torrent.h"

class rpc_client: public QObject
{
    Q_OBJECT

public:
    /*
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
*/
    struct TransmissionServer
    {
        QString name;
        QString rpcUrl;
        QString username;
        QString password;

        bool isValid() const
        {
            return !rpcUrl.trimmed().isEmpty();
        }
    };

    bool loadCurrentServerFromSettings();
    bool setServerFromSettingsIndex(int index);
    void setServer(const TransmissionServer &server);
    rpc_client(QObject *parent);
    void init();
    void getTorrentList();
    //QJsonArray torrents();
    //bool isClientReady();
    //int countTorrents() const;
    //torrent getTorrent(int item);
    QString getServer();
    //int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    //int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    //QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    //QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    //int rowForId(int id) const;
    //bool updateFromJson(const QByteArray &json);
    void removeTorrent(int id, bool deleteLocalData);
    void startTorrent(int id);
    void stopTorrent(int id);
    void addTorrentFromFile(const QString &filePath, bool deleteFileOnSuccess);
    void addTorrentFromMagnet(const QString &magnetLink);
    void reannounceTorrent(int id);
    void verifyTorrent(int id);
    void startTorrents(const QList<int> &ids);
    void stopTorrents(const QList<int> &ids);
    void removeTorrents(const QList<int> &ids, bool deleteLocalData);
    void verifyTorrents(const QList<int> &ids);
    void reannounceTorrents(const QList<int> &ids);

signals:
    void listUpdated();
    void updateStarted();
    void updateFinished();
    void updateFailed(const QString &message);
    void torrentsReceived(const QVector<torrent> &torrents);
    void serverChanged();

private:
    enum class RpcRequestType {
        TorrentGet,
        Command
    };

    QString username;
    QString password;
    bool _clientReady = false;
    bool updateInProgress = false;
    QByteArray _session_token;
    QNetworkAccessManager *na_manager;
    //QJsonArray torrentList;
    //QVector<torrent> torrentVector;
    //QHash<int, int> m_rowById;
    QString serverName;
    QString rpcUrl;

    void setSessionToken(QByteArray token);
    //void rebuildIndex();
    //void applyUpdate(const QVector<torrent> &incoming);

    static TransmissionServer readServerFromSettings(int index, bool *ok = nullptr);
    //void clearTorrents();

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
