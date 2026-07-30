#include "pollingcoordinator.h"

#include <QDateTime>
#include <QTimer>

#include <utility>

PollingCoordinator::PollingCoordinator(
    Requests requests,
    QObject *parent,
    int commandDebounceMs,
    qint64 slowRefreshIntervalMs)
    : QObject(parent)
    , m_requests(std::move(requests))
    , m_pollTimer(new QTimer(this))
    , m_commandRefreshTimer(new QTimer(this))
    , m_slowRefreshIntervalMs(qMax<qint64>(1, slowRefreshIntervalMs))
{
    m_commandRefreshTimer->setSingleShot(true);
    m_commandRefreshTimer->setInterval(qMax(0, commandDebounceMs));

    connect(m_pollTimer, &QTimer::timeout,
            this, [this]() {
                requestTorrentList(ListRefreshMode::ShowLoading);
            });
    connect(m_commandRefreshTimer, &QTimer::timeout, this, [this]() {
        requestTorrentList();

        if (m_pendingCommandDetailsRefresh
            && isValidTorrentKey(m_selectedTorrent)
            && m_requests.torrentDetails) {
            m_requests.torrentDetails(m_selectedTorrent);
        }

        m_pendingCommandDetailsRefresh = false;
    });
}

void PollingCoordinator::setPollingInterval(int intervalMs)
{
    m_pollTimer->setInterval(qMax(1, intervalMs));
}

int PollingCoordinator::pollingInterval() const
{
    return m_pollTimer->interval();
}

void PollingCoordinator::startPolling()
{
    if (!m_pollTimer->isActive())
        m_pollTimer->start();
}

void PollingCoordinator::requestTorrentList(ListRefreshMode mode)
{
    if (mode != ListRefreshMode::Silent) {
        emit torrentListRefreshStarted(
            mode == ListRefreshMode::ServerChanged);
    }
    if (m_requests.torrentList)
        m_requests.torrentList();
}

void PollingCoordinator::scheduleCommandRefresh(bool refreshDetails)
{
    // OR accumulation preserves a detail refresh requested by any command in
    // the burst while restarting the debounce window for later completions.
    m_pendingCommandDetailsRefresh |= refreshDetails;
    m_commandRefreshTimer->start();
}

void PollingCoordinator::setSelectedTorrent(const TorrentKey &torrentKey)
{
    if (m_selectedTorrent == torrentKey)
        return;

    if (m_requests.cancelTorrentDetails)
        m_requests.cancelTorrentDetails();
    m_selectedTorrent = torrentKey;
}

void PollingCoordinator::setDetailsPaneVisible(bool visible)
{
    if (m_detailsPaneVisible == visible)
        return;

    m_detailsPaneVisible = visible;
    if (!visible && m_requests.cancelTorrentDetails)
        m_requests.cancelTorrentDetails();
}

void PollingCoordinator::setDetailView(DetailView view)
{
    m_detailView = view;
}

void PollingCoordinator::requestSelectedTorrent(bool includeSummary)
{
    if (!m_detailsPaneVisible || !isValidTorrentKey(m_selectedTorrent))
        return;

    if (includeSummary && m_requests.torrentDetails)
        m_requests.torrentDetails(m_selectedTorrent);
    requestVisibleTorrentData();
}

void PollingCoordinator::handleTorrentListReceived()
{
    refreshSlowData();
    requestVisibleTorrentData();
}

void PollingCoordinator::resetForServerChange()
{
    m_commandRefreshTimer->stop();
    m_pendingCommandDetailsRefresh = false;
    m_lastFreeSpaceRefreshMs = 0;
    m_lastTrackerMetadataRefreshMs = 0;
    m_remoteDownloadDirectory.clear();
    m_selectedTorrent.clear();
    if (m_requests.cancelTorrentDetails)
        m_requests.cancelTorrentDetails();
}

void PollingCoordinator::setRemoteDownloadDirectory(
    const QString &path,
    bool refreshFreeSpace)
{
    m_remoteDownloadDirectory = path;
    if (refreshFreeSpace)
        requestFreeSpaceNow();
}

QString PollingCoordinator::remoteDownloadDirectory() const
{
    return m_remoteDownloadDirectory;
}

bool PollingCoordinator::requestFreeSpaceNow()
{
    if (!m_requests.freeSpace
        || !m_requests.freeSpaceSupported
        || !m_requests.freeSpaceSupported()
        || m_remoteDownloadDirectory.trimmed().isEmpty()) {
        return false;
    }

    m_lastFreeSpaceRefreshMs = QDateTime::currentMSecsSinceEpoch();
    m_requests.freeSpace(m_remoteDownloadDirectory);
    return true;
}

void PollingCoordinator::refreshSlowData(bool force)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (force
        || now - m_lastTrackerMetadataRefreshMs
               >= m_slowRefreshIntervalMs) {
        m_lastTrackerMetadataRefreshMs = now;
        if (m_requests.trackerMetadata)
            m_requests.trackerMetadata();
    }

    if (m_requests.freeSpaceSupported
        && m_requests.freeSpaceSupported()
        && !m_remoteDownloadDirectory.isEmpty()
        && (force
            || now - m_lastFreeSpaceRefreshMs
                   >= m_slowRefreshIntervalMs)) {
        requestFreeSpaceNow();
    }
}

void PollingCoordinator::requestVisibleTorrentData()
{
    if (!m_detailsPaneVisible || !isValidTorrentKey(m_selectedTorrent))
        return;

    switch (m_detailView) {
    case DetailView::GeneralLive:
        if (m_requests.torrentPieces)
            m_requests.torrentPieces(m_selectedTorrent);
        break;
    case DetailView::Files:
        if (m_requests.torrentFiles)
            m_requests.torrentFiles(m_selectedTorrent);
        break;
    case DetailView::Peers:
        if (m_requests.torrentPeers)
            m_requests.torrentPeers(m_selectedTorrent);
        break;
    case DetailView::Trackers:
        if (m_requests.torrentTrackers)
            m_requests.torrentTrackers(m_selectedTorrent);
        break;
    case DetailView::None:
        break;
    }
}
