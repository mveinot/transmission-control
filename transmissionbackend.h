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
    void getTorrentDetails(const TorrentKey &torrentKey) override;
    void getTorrentFiles(const TorrentKey &torrentKey) override;
    void getTorrentPeers(const TorrentKey &torrentKey) override;
    void getTorrentTrackers(const TorrentKey &torrentKey) override;
    void getTorrentPieces(const TorrentKey &torrentKey) override;
    void getTorrentProperties(const TorrentKey &torrentKey) override;
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
    void startTorrents(const QList<TorrentKey> &torrentKeys) override;
    void startAllTorrents() override;
    void startTorrentsNow(const QList<TorrentKey> &torrentKeys) override;
    void stopTorrents(const QList<TorrentKey> &torrentKeys) override;
    void stopAllTorrents() override;
    void removeTorrents(const QList<TorrentKey> &torrentKeys,
                        bool deleteLocalData) override;
    void verifyTorrents(const QList<TorrentKey> &torrentKeys) override;
    void reannounceTorrents(const QList<TorrentKey> &torrentKeys) override;
    void setTorrentLocation(const QList<TorrentKey> &torrentKeys,
                            const QString &location,
                            bool moveData) override;
    void setTorrentFilesWanted(const TorrentKey &torrentKey,
                               const QList<int> &fileIndices,
                               bool wanted) override;

    void setTorrentFilesPriority(const TorrentKey &torrentKey,
                                 const QList<int> &fileIndices,
                                 int priority) override;

    void setTorrentFilesWantedAndPriority(const TorrentKey &torrentKey,
                                          const QList<int> &fileIndices,
                                          bool wanted,
                                          int priority) override;

    void addTorrentTracker(const TorrentKey &torrentKey,
                           const QString &announceUrl) override;
    void editTorrentTracker(const TorrentKey &torrentKey,
                            int trackerId,
                            const QString &announceUrl) override;
    void removeTorrentTracker(const TorrentKey &torrentKey, int trackerId) override;

    void renameTorrentPath(const TorrentKey &torrentKey,
                           const QString &path,
                           const QString &newName) override;

    void setTorrentProperties(const TorrentKey &torrentKey,
                              const TorrentPropertyChanges &properties) override;

    void setTorrentsSequentialDownload(const QList<TorrentKey> &torrentKeys,
                                       bool enabled) override;

    void setTorrentsBandwidthPriority(const QList<TorrentKey> &torrentKeys,
                                      int priority) override;

    void queueMoveTop(const QList<TorrentKey> &torrentKeys) override;
    void queueMoveUp(const QList<TorrentKey> &torrentKeys) override;
    void queueMoveDown(const QList<TorrentKey> &torrentKeys) override;
    void queueMoveBottom(const QList<TorrentKey> &torrentKeys) override;
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
    QHash<TorrentKey, QJsonObject> torrentTrackerMetadata;
    void postRpc(const RpcRequestContext &context);
    void postRpc(const QString &method,
                 const QJsonObject &arguments,
                 RpcRequestType type);
    void postIdsCommand(const QString &method, const QList<TorrentKey> &torrentKeys);
    void postSingleTorrentSet(const TorrentKey &torrentKey, const QJsonObject &arguments);
    bool prepareTorrentDetailRequest(RpcRequestType type, const TorrentKey &torrentKey);
    static bool isTorrentDetailRequest(RpcRequestType type);


public slots:
    void replyFinished(QNetworkReply *reply);
};

#endif // TRANSMISSIONBACKEND_H
