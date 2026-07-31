#ifndef POLLINGCOORDINATOR_H
#define POLLINGCOORDINATOR_H

#include "torrentkey.h"

#include <QObject>

#include <functional>

class QTimer;

// Centralizes when read-only RPC projections are refreshed. The coordinator
// owns cadence and coalescing policy; callers retain presentation concerns and
// provide backend operations through the request callbacks.
class PollingCoordinator : public QObject
{
    Q_OBJECT

public:
    enum class DetailView {
        None,
        GeneralLive,
        Files,
        Peers,
        Trackers
    };

    enum class ListRefreshMode {
        Silent,
        ShowLoading,
        ServerChanged
    };

    struct Requests {
        std::function<void()> torrentList;
        std::function<void()> trackerMetadata;
        std::function<void(const TorrentKey &)> torrentDetails;
        std::function<void(const TorrentKey &)> torrentFiles;
        std::function<void(const TorrentKey &)> torrentPeers;
        std::function<void(const TorrentKey &)> torrentTrackers;
        std::function<void(const TorrentKey &)> torrentPieces;
        std::function<void()> cancelTorrentDetails;
        std::function<void(const QString &)> freeSpace;
        std::function<bool()> freeSpaceSupported;
    };

    explicit PollingCoordinator(
        Requests requests,
        QObject *parent = nullptr,
        int commandDebounceMs = 200,
        qint64 slowRefreshIntervalMs = 60 * 1000);

    void setPollingInterval(int intervalMs);
    int pollingInterval() const;
    void startPolling();

    void requestTorrentList(
        ListRefreshMode mode = ListRefreshMode::Silent);
    void scheduleCommandRefresh(bool refreshDetails);
    void setSelectedTorrent(const TorrentKey &torrentKey);
    void setDetailsPaneVisible(bool visible);
    void setDetailView(DetailView view);
    void setFileRefreshSuppressed(bool suppressed);
    void requestSelectedTorrent(bool includeSummary);
    void handleTorrentListReceived();

    void resetForServerChange();
    void setRemoteDownloadDirectory(const QString &path,
                                    bool refreshFreeSpace);
    QString remoteDownloadDirectory() const;
    bool requestFreeSpaceNow();
    void refreshSlowData(bool force = false);

signals:
    void torrentListRefreshStarted(bool serverChanged);

private:
    Requests m_requests;
    QTimer *m_pollTimer = nullptr;
    QTimer *m_commandRefreshTimer = nullptr;
    TorrentKey m_selectedTorrent;
    DetailView m_detailView = DetailView::None;
    QString m_remoteDownloadDirectory;
    qint64 m_slowRefreshIntervalMs = 0;
    qint64 m_lastFreeSpaceRefreshMs = 0;
    qint64 m_lastTrackerMetadataRefreshMs = 0;
    bool m_detailsPaneVisible = true;
    bool m_fileRefreshSuppressed = false;
    bool m_pendingCommandDetailsRefresh = false;

    void requestVisibleTorrentData();
};

#endif // POLLINGCOORDINATOR_H
