#ifndef TRANSMISSIONBACKEND_H
#define TRANSMISSIONBACKEND_H

#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "torrentbackend.h"

// Transmission implementation of the backend contract. Request contexts
// preserve semantic routing across authentication retries; QObject ownership
// governs all replies.
class TransmissionBackend : public TorrentBackend
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

    bool loadCurrentServerFromSettings() override;
    bool setServerFromSettingsIndex(int index) override;
    void setServer(const TransmissionServer &server);
    explicit TransmissionBackend(QObject *parent = nullptr);
    QString backendName() const override;
    QString serverDisplayName() const override;
    QString endpointUrl() const override;
    TorrentBackendCapabilities capabilities() const override;
    void init() override;
    void getTorrentList() override;
    void getTorrentTrackerMetadata() override;
    void getTorrentDetails(int id) override;
    void getTorrentFiles(int id) override;
    void getTorrentPeers(int id) override;
    void getTorrentTrackers(int id) override;
    void getTorrentPieces(int id) override;
    void getTorrentProperties(int id) override;
    void cancelTorrentDetailRequests() override;
    void addTorrentFromFile(const QString &filePath,
                            bool deleteFileOnSuccess) override;
    void addTorrentFromMagnet(const QString &magnetLink) override;
    void addTorrentFile(const QString &filePath,
                        const QString &downloadDir = QString(),
                        bool paused = false,
                        const QList<int> &filesUnwanted = {},
                        const QList<int> &priorityLow = {},
                        const QList<int> &priorityHigh = {},
                        bool deleteFileOnSuccess = false) override;

    void addMagnetLink(const QString &magnetLink,
                       const QString &downloadDir = QString(),
                       bool paused = false) override;
    void startTorrents(const QList<int> &ids) override;
    void startAllTorrents() override;
    void startTorrentsNow(const QList<int> &ids) override;
    void stopTorrents(const QList<int> &ids) override;
    void stopAllTorrents() override;
    void removeTorrents(const QList<int> &ids,
                        bool deleteLocalData) override;
    void verifyTorrents(const QList<int> &ids) override;
    void reannounceTorrents(const QList<int> &ids) override;
    void setTorrentLocation(const QList<int> &ids,
                            const QString &location,
                            bool moveData) override;
    void setTorrentFilesWanted(int torrentId,
                               const QList<int> &fileIndices,
                               bool wanted) override;

    void setTorrentFilesPriority(int torrentId,
                                 const QList<int> &fileIndices,
                                 int priority) override;

    void setTorrentFilesWantedAndPriority(int torrentId,
                                          const QList<int> &fileIndices,
                                          bool wanted,
                                          int priority) override;

    void addTorrentTracker(int torrentId,
                           const QString &announceUrl) override;
    void editTorrentTracker(int torrentId,
                            int trackerId,
                            const QString &announceUrl) override;
    void removeTorrentTracker(int torrentId, int trackerId) override;

    void renameTorrentPath(int torrentId,
                           const QString &path,
                           const QString &newName) override;

    void setTorrentProperties(int torrentId,
                              const QJsonObject &properties) override;

    void setTorrentsSequentialDownload(const QList<int> &ids,
                                       bool enabled) override;

    void setTorrentsBandwidthPriority(const QList<int> &ids,
                                      int priority) override;

    void queueMoveTop(const QList<int> &ids) override;
    void queueMoveUp(const QList<int> &ids) override;
    void queueMoveDown(const QList<int> &ids) override;
    void queueMoveBottom(const QList<int> &ids) override;
    void getSessionSettings() override;
    void getSessionStatistics() override;
    void setSessionSettings(const QJsonObject &settings) override;
    void getFreeSpace(const QString &path) override;
    void testPortForwarding() override;
    void updateBlocklist(const QJsonObject &changedSettings) override;

private:
    enum class RpcRequestType {
        TorrentGet,
        TorrentListTrackers,
        TorrentDetails,
        TorrentFiles,
        TorrentPeers,
        TorrentTrackers,
        TorrentPieces,
        TorrentProperties,
        Command,
        SessionGet,
        SessionStats,
        FreeSpace,
        PortTest
    };

    struct RpcRequestContext
    {
        QString method;
        QJsonObject arguments;
        RpcRequestType type;
        QString torrentFilePath;
        bool deleteTorrentFileOnSuccess = false;
        bool retriedAfterAuth = false;
        bool updateBlocklistAfterSuccess = false;
    };

    QString username;
    QString password;
    bool _clientReady = false;
    bool updateInProgress = false;
    bool updateRequestedWhileInProgress = false;
    bool m_sequentialDownloadSupported = false;
    bool m_torrentLabelsSupported = false;
    bool m_torrentGroupsSupported = false;
    QByteArray _session_token;
    QNetworkAccessManager *na_manager = nullptr;
    QString serverName;
    QString rpcUrl;

    void setSessionToken(QByteArray token);

    static TransmissionServer readServerFromSettings(int index, bool *ok = nullptr);

    QByteArray makeRpcPayload(const QString &method,
                              const QJsonObject &arguments = {}) const;
    QNetworkRequest makeRequest() const;
    QHash<QNetworkReply *, RpcRequestContext> pendingRequests;
    // Tracker arrays change much less frequently than rates/status. Cache them
    // separately and merge them into fast list snapshots client-side.
    QHash<int, QJsonObject> torrentTrackerMetadata;
    void postRpc(const RpcRequestContext &context);
    void postRpc(const QString &method,
                 const QJsonObject &arguments,
                 RpcRequestType type);
    void postIdsCommand(const QString &method, const QList<int> &ids);
    void postSingleTorrentSet(int torrentId, const QJsonObject &arguments);
    bool prepareTorrentDetailRequest(RpcRequestType type, int torrentId);
    static bool isTorrentDetailRequest(RpcRequestType type);


public slots:
    void replyFinished(QNetworkReply *reply);
};

#endif // TRANSMISSIONBACKEND_H
