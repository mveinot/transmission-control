#ifndef TORRENTBACKEND_H
#define TORRENTBACKEND_H

#include "torrent.h"

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
    bool trackerEditing = false;
    bool pathRenaming = false;
    bool sessionSettings = false;
    bool sessionStatistics = false;
    bool freeSpaceQuery = false;
    bool portTest = false;
    bool blocklistUpdate = false;
};

// Semantic contract between Planetary's presentation layer and a remote
// torrent service. Implementations own authentication, transport, request
// deduplication, and conversion from their native API into Planetary's current
// torrent/detail payloads.
//
// Detail and settings payloads remain QJsonObject during the first migration
// pass. Their keys form Planetary's compatibility schema rather than granting
// controllers access to an implementation's raw wire response.
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
    virtual void getTorrentDetails(int id) = 0;
    virtual void getTorrentFiles(int id) = 0;
    virtual void getTorrentPeers(int id) = 0;
    virtual void getTorrentTrackers(int id) = 0;
    virtual void getTorrentPieces(int id) = 0;
    virtual void getTorrentProperties(int id) = 0;
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

    virtual void startTorrents(const QList<int> &ids) = 0;
    virtual void startAllTorrents() = 0;
    virtual void startTorrentsNow(const QList<int> &ids) = 0;
    virtual void stopTorrents(const QList<int> &ids) = 0;
    virtual void stopAllTorrents() = 0;
    virtual void removeTorrents(const QList<int> &ids,
                                bool deleteLocalData) = 0;
    virtual void verifyTorrents(const QList<int> &ids) = 0;
    virtual void reannounceTorrents(const QList<int> &ids) = 0;
    virtual void setTorrentLocation(const QList<int> &ids,
                                    const QString &location,
                                    bool moveData) = 0;
    virtual void setTorrentFilesWanted(int torrentId,
                                       const QList<int> &fileIndices,
                                       bool wanted) = 0;
    virtual void setTorrentFilesPriority(int torrentId,
                                         const QList<int> &fileIndices,
                                         int priority) = 0;
    virtual void setTorrentFilesWantedAndPriority(
        int torrentId,
        const QList<int> &fileIndices,
        bool wanted,
        int priority) = 0;

    virtual void addTorrentTracker(int torrentId,
                                   const QString &announceUrl) = 0;
    virtual void editTorrentTracker(int torrentId,
                                    int trackerId,
                                    const QString &announceUrl) = 0;
    virtual void removeTorrentTracker(int torrentId, int trackerId) = 0;
    virtual void renameTorrentPath(int torrentId,
                                   const QString &path,
                                   const QString &newName) = 0;
    virtual void setTorrentProperties(int torrentId,
                                      const QJsonObject &properties) = 0;
    virtual void setTorrentsSequentialDownload(const QList<int> &ids,
                                               bool enabled) = 0;
    virtual void setTorrentsBandwidthPriority(const QList<int> &ids,
                                              int priority) = 0;
    virtual void queueMoveTop(const QList<int> &ids) = 0;
    virtual void queueMoveUp(const QList<int> &ids) = 0;
    virtual void queueMoveDown(const QList<int> &ids) = 0;
    virtual void queueMoveBottom(const QList<int> &ids) = 0;

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
    void torrentDetailsReceived(int torrentId,
                                const QJsonObject &torrentDetails);
    void torrentFilesReceived(int torrentId,
                              const QJsonObject &torrentDetails);
    void torrentPeersReceived(int torrentId,
                              const QJsonObject &torrentDetails);
    void torrentTrackersReceived(int torrentId,
                                 const QJsonObject &torrentDetails);
    void torrentPiecesReceived(int torrentId,
                               const QJsonObject &pieceDetails);
    void torrentPropertiesReceived(int torrentId,
                                   const QJsonObject &properties);
    void commandSucceeded(const QString &method);
    void commandFailed(const QString &method, const QString &message);
    void torrentFileAddSucceeded(const QString &filePath);
    void torrentAdded(int torrentId, const QString &name);
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
