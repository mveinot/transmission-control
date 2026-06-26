#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "torrent.h"

class rpc_client: public QObject
{
    Q_OBJECT

public:
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
    void getTorrentDetails(int id);
    void getTorrentPieces(int id);
    void getTorrentProperties(int id);
    QString getServer();
    void addTorrentFromFile(const QString &filePath, bool deleteFileOnSuccess);
    void addTorrentFromMagnet(const QString &magnetLink);
    void addTorrentFile(const QString &filePath,
                        const QString &downloadDir = QString(),
                        bool paused = false,
                        const QList<int> &filesUnwanted = {},
                        const QList<int> &priorityLow = {},
                        const QList<int> &priorityHigh = {},
                        bool deleteFileOnSuccess = false);

    void addMagnetLink(const QString &magnetLink,
                       const QString &downloadDir = QString(),
                       bool paused = false);
    void startTorrents(const QList<int> &ids);
    void startTorrentsNow(const QList<int> &ids);
    void stopTorrents(const QList<int> &ids);
    void removeTorrents(const QList<int> &ids, bool deleteLocalData);
    void verifyTorrents(const QList<int> &ids);
    void reannounceTorrents(const QList<int> &ids);
    void setTorrentLocation(const QList<int> &ids,
                            const QString &location,
                            bool moveData);
    void setTorrentFilesWanted(int torrentId,
                               const QList<int> &fileIndices,
                               bool wanted);

    void setTorrentFilesPriority(int torrentId,
                                 const QList<int> &fileIndices,
                                 int priority);

    void setTorrentFilesWantedAndPriority(int torrentId,
                                          const QList<int> &fileIndices,
                                          bool wanted,
                                          int priority);

    void addTorrentTracker(int torrentId, const QString &announceUrl);
    void editTorrentTracker(int torrentId, int trackerId, const QString &announceUrl);
    void removeTorrentTracker(int torrentId, int trackerId);

    void renameTorrentPath(int torrentId,
                           const QString &path,
                           const QString &newName);

    void setTorrentProperties(int torrentId,
                              const QJsonObject &properties);

    void queueMoveTop(const QList<int> &ids);
    void queueMoveUp(const QList<int> &ids);
    void queueMoveDown(const QList<int> &ids);
    void queueMoveBottom(const QList<int> &ids);
    void getSessionSettings();
    void setSessionSettings(const QJsonObject &settings);
    void getFreeSpace(const QString &path);

signals:
    void updateStarted();
    void updateFinished();
    void updateFailed(const QString &message);
    void torrentsReceived(const QVector<torrent> &torrents);
    void torrentDetailsReceived(int torrentId, const QJsonObject &torrentDetails);
    void torrentPiecesReceived(int torrentId, const QJsonObject &pieceDetails);
    void torrentPropertiesReceived(int torrentId, const QJsonObject &properties);
    void commandSucceeded(const QString &method);
    void commandFailed(const QString &method, const QString &message);
    void serverChanged();
    void sessionSettingsReceived(const QJsonObject &settings);
    void freeSpaceReceived(const QString &path, qint64 sizeBytes);

private:
    enum class RpcRequestType {
        TorrentGet,
        TorrentDetails,
        TorrentPieces,
        TorrentProperties,
        Command,
        SessionGet,
        FreeSpace
    };

    struct RpcRequestContext
    {
        QString method;
        QJsonObject arguments;
        RpcRequestType type;
        QString torrentFilePath;
        bool deleteTorrentFileOnSuccess = false;
        bool retriedAfterAuth = false;
    };

    QString username;
    QString password;
    bool _clientReady = false;
    bool updateInProgress = false;
    QByteArray _session_token;
    QNetworkAccessManager *na_manager;
    QString serverName;
    QString rpcUrl;

    void setSessionToken(QByteArray token);

    static TransmissionServer readServerFromSettings(int index, bool *ok = nullptr);

    QByteArray makeRpcPayload(const QString &method,
                              const QJsonObject &arguments = {}) const;
    QNetworkRequest makeRequest() const;
    QHash<QNetworkReply *, RpcRequestContext> pendingRequests;
    void postRpc(const RpcRequestContext &context);
    void postRpc(const QString &method,
                 const QJsonObject &arguments,
                 RpcRequestType type);


public slots:
    void replyFinished(QNetworkReply *reply);
};

#endif // RPC_CLIENT_H
