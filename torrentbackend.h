#ifndef TORRENTBACKEND_H
#define TORRENTBACKEND_H

#include "torrent.h"
#include "torrentdomain.h"
#include "torrentkey.h"

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QVector>

// Feature discovery keeps optional backend operations out of UI type checks.
// Values may change after a backend has completed its version handshake.
struct TorrentBackendCapabilities
{
    bool forceStart = false;
    bool queueManagement = false;
    bool sequentialDownload = false;
    bool labels = false;
    bool groups = false;
    bool torrentProperties = false;
    bool torrentSpeedLimits = false;
    bool torrentShareLimits = false;
    bool torrentBandwidthPriority = false;
    bool torrentSessionLimitOverride = false;
    bool torrentQueuePosition = false;
    bool torrentPeerLimit = false;
    bool filePriorities = false;
    bool fileLowPriority = false;
    bool trackerEditing = false;
    bool pathRenaming = false;
    bool sessionSettings = false;
    bool sessionEncryptionDisable = false;
    bool sessionStatistics = false;
    bool freeSpaceQuery = false;
    bool portTest = false;
    bool blocklistUpdate = false;
};

// Semantic contract between Planetary's presentation layer and a remote
// torrent service. Implementations own authentication, transport, request
// deduplication, and conversion from their native API into Planetary's current
// torrent/detail snapshots.
class TorrentBackend : public QObject
{
    Q_OBJECT

public:
    explicit TorrentBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~TorrentBackend() override = default;

    virtual QString backendName() const = 0;
    virtual QString serverDisplayName() const = 0;
    virtual QString endpointUrl() const = 0;
    virtual TorrentBackendCapabilities capabilities() const = 0;

    virtual bool loadCurrentServerFromSettings() = 0;
    virtual bool setServerFromSettingsIndex(int index) = 0;
    virtual void init() = 0;

    virtual void getTorrentList() = 0;
    virtual void getTorrentTrackerMetadata() = 0;
    virtual void getTorrentDetails(const TorrentKey &torrentKey) = 0;
    virtual void getTorrentFiles(const TorrentKey &torrentKey) = 0;
    virtual void getTorrentPeers(const TorrentKey &torrentKey) = 0;
    virtual void getTorrentTrackers(const TorrentKey &torrentKey) = 0;
    virtual void getTorrentPieces(const TorrentKey &torrentKey) = 0;
    virtual void getTorrentProperties(const TorrentKey &torrentKey) = 0;
    virtual void cancelTorrentDetailRequests() = 0;

    virtual void addTorrentFromFile(const QString &filePath,
                                    bool deleteFileOnSuccess) = 0;
    virtual void addTorrentFromMagnet(const QString &magnetLink) = 0;
    virtual void addTorrentFile(const QString &filePath,
                                const QString &downloadDir = QString(),
                                bool paused = false,
                                const QList<int> &filesUnwanted = {},
                                const QList<int> &priorityLow = {},
                                const QList<int> &priorityHigh = {},
                                bool deleteFileOnSuccess = false) = 0;
    virtual void addMagnetLink(const QString &magnetLink,
                               const QString &downloadDir = QString(),
                               bool paused = false) = 0;

    virtual void startTorrents(const QList<TorrentKey> &torrentKeys) = 0;
    virtual void startAllTorrents() = 0;
    virtual void startTorrentsNow(const QList<TorrentKey> &torrentKeys) = 0;
    virtual void stopTorrents(const QList<TorrentKey> &torrentKeys) = 0;
    virtual void stopAllTorrents() = 0;
    virtual void removeTorrents(const QList<TorrentKey> &torrentKeys,
                                bool deleteLocalData) = 0;
    virtual void verifyTorrents(const QList<TorrentKey> &torrentKeys) = 0;
    virtual void reannounceTorrents(const QList<TorrentKey> &torrentKeys) = 0;
    virtual void setTorrentLocation(const QList<TorrentKey> &torrentKeys,
                                    const QString &location,
                                    bool moveData) = 0;
    virtual void setTorrentFilesWanted(const TorrentKey &torrentKey,
                                       const QList<int> &fileIndices,
                                       bool wanted) = 0;
    virtual void setTorrentFilesPriority(const TorrentKey &torrentKey,
                                         const QList<int> &fileIndices,
                                         int priority) = 0;
    virtual void setTorrentFilesWantedAndPriority(
        const TorrentKey &torrentKey,
        const QList<int> &fileIndices,
        bool wanted,
        int priority) = 0;

    virtual void addTorrentTracker(const TorrentKey &torrentKey,
                                   const QString &announceUrl) = 0;
    virtual void editTorrentTracker(const TorrentKey &torrentKey,
                                    int trackerId,
                                    const QString &announceUrl) = 0;
    virtual void removeTorrentTracker(const TorrentKey &torrentKey,
                                      int trackerId) = 0;
    virtual void renameTorrentPath(const TorrentKey &torrentKey,
                                   const QString &path,
                                   const QString &newName) = 0;
    virtual void setTorrentProperties(const TorrentKey &torrentKey,
                                      const TorrentPropertyChanges &properties) = 0;
    virtual void setTorrentsSequentialDownload(
        const QList<TorrentKey> &torrentKeys,
                                               bool enabled) = 0;
    virtual void setTorrentsBandwidthPriority(
        const QList<TorrentKey> &torrentKeys,
                                              int priority) = 0;
    virtual void queueMoveTop(const QList<TorrentKey> &torrentKeys) = 0;
    virtual void queueMoveUp(const QList<TorrentKey> &torrentKeys) = 0;
    virtual void queueMoveDown(const QList<TorrentKey> &torrentKeys) = 0;
    virtual void queueMoveBottom(const QList<TorrentKey> &torrentKeys) = 0;

    virtual void getSessionSettings() = 0;
    virtual void getSessionStatistics() = 0;
    virtual void setSessionSettings(const QJsonObject &settings) = 0;
    virtual void getFreeSpace(const QString &path) = 0;
    virtual void testPortForwarding() = 0;
    virtual void updateBlocklist(const QJsonObject &changedSettings) = 0;

signals:
    void updateStarted();
    void updateFinished();
    void updateFailed(const QString &message);
    void torrentsReceived(const QVector<torrent> &torrents);
    void torrentTrackerMetadataUpdated();
    void torrentDetailsReceived(const TorrentDetails &details);
    void torrentFilesReceived(const TorrentFiles &files);
    void torrentPeersReceived(const TorrentPeers &peers);
    void torrentTrackersReceived(const TorrentTrackers &trackers);
    void torrentPiecesReceived(const TorrentPieces &pieces);
    void torrentPropertiesReceived(const TorrentProperties &properties);
    void commandSucceeded(const QString &method);
    void commandFailed(const QString &method, const QString &message);
    void torrentFileAddSucceeded(const QString &filePath);
    void torrentAdded(const TorrentKey &torrentKey, const QString &name);
    void torrentFileAddFailed(const QString &filePath,
                              const QString &message);
    void serverChanged();
    void capabilitiesChanged(const TorrentBackendCapabilities &capabilities);
    void sessionSettingsReceived(const QJsonObject &settings);
    void sessionStatisticsReceived(const QJsonObject &statistics);
    void sessionStatisticsFailed(const QString &message);
    void freeSpaceReceived(const QString &path, qint64 sizeBytes);
    void portTestFinished(bool portIsOpen, const QString &ipProtocol);
    void portTestFailed(const QString &message);
    void blocklistUpdateFinished(int ruleCount);
    void blocklistUpdateFailed(const QString &message);
};

#endif // TORRENTBACKEND_H
