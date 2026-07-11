#include "watchfoldermanager.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace {

QString processedFingerprintsSettingsKey()
{
    return QStringLiteral("watchFolder/processedFingerprints");
}

constexpr qsizetype MaxStoredFingerprints = 1000;

} // namespace

WatchFolderManager::WatchFolderManager(QObject *parent)
    : QObject(parent)
{
    m_scanTimer.setInterval(m_scanIntervalMs);

    connect(&m_scanTimer, &QTimer::timeout,
            this, &WatchFolderManager::scanWatchFolder);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &WatchFolderManager::handleDirectoryChanged);

    loadProcessedFingerprints();
}

void WatchFolderManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;

    if (m_enabled)
        restartWatcher();
    else
        stopWatcher();
}

bool WatchFolderManager::isEnabled() const
{
    return m_enabled;
}

void WatchFolderManager::setWatchFolder(const QString &folderPath)
{
    const QString trimmedPath = folderPath.trimmed();
    const QString cleanedPath = trimmedPath.isEmpty()
        ? QString()
        : QDir::cleanPath(trimmedPath);

    if (m_watchFolder == cleanedPath)
        return;

    m_watchFolder = cleanedPath;
    m_candidates.clear();
    m_pendingFingerprintsByPath.clear();

    if (m_enabled)
        restartWatcher();
}

QString WatchFolderManager::watchFolder() const
{
    return m_watchFolder;
}

void WatchFolderManager::setScanIntervalMs(int intervalMs)
{
    const int normalizedInterval = qMax(250, intervalMs);

    if (m_scanIntervalMs == normalizedInterval)
        return;

    m_scanIntervalMs = normalizedInterval;
    m_scanTimer.setInterval(m_scanIntervalMs);
}

int WatchFolderManager::scanIntervalMs() const
{
    return m_scanIntervalMs;
}

void WatchFolderManager::setRequiredStableChecks(int checks)
{
    m_requiredStableChecks = qMax(1, checks);
}

int WatchFolderManager::requiredStableChecks() const
{
    return m_requiredStableChecks;
}

void WatchFolderManager::clearProcessedHistory()
{
    m_processedFingerprints.clear();
    m_candidates.clear();
    m_pendingFingerprintsByPath.clear();
    saveProcessedFingerprints();

    if (m_enabled)
        QTimer::singleShot(0, this, &WatchFolderManager::scanWatchFolder);
}

bool WatchFolderManager::hasPendingTorrentFile(const QString &filePath) const
{
    return !pendingFingerprintForFile(filePath).isEmpty();
}

void WatchFolderManager::markTorrentFileProcessed(const QString &filePath)
{
    const QString fingerprint = pendingFingerprintForFile(filePath);

    if (fingerprint.isEmpty())
        return;

    forgetPending(filePath);
    m_candidates.remove(QDir::cleanPath(QFileInfo(filePath).absoluteFilePath()));
    markProcessed(fingerprint);
}

void WatchFolderManager::retryTorrentFile(const QString &filePath)
{
    const QString cleanedPath =
        QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());

    if (cleanedPath.isEmpty())
        return;

    forgetPending(cleanedPath);
    m_candidates.remove(cleanedPath);

    /*
     * Nudge a retry without waiting for another filesystem event.
     * Use the scan interval instead of an immediate retry so a persistent
     * authentication or server error does not create a tight retry loop.
     */
    QTimer::singleShot(m_scanIntervalMs,
                       this,
                       &WatchFolderManager::scanWatchFolder);
}

void WatchFolderManager::restartWatcher()
{
    stopWatcher();

    if (!m_enabled)
        return;

    if (m_watchFolder.isEmpty()) {
        emit warningMessage(tr("Watch folder is enabled but no folder is configured."));
        return;
    }

    const QFileInfo folderInfo(m_watchFolder);

    if (!folderInfo.exists() || !folderInfo.isDir()) {
        emit warningMessage(
            tr("Watch folder does not exist: %1").arg(m_watchFolder)
            );
        return;
    }

    if (!m_watcher.addPath(m_watchFolder)) {
        emit warningMessage(
           tr("Could not watch folder: %1").arg(m_watchFolder)
            );
        return;
    }

    emit statusMessage(
        tr("Watching folder for torrents: %1").arg(m_watchFolder)
        );

    m_scanTimer.start();

    /*
     * Initial scan is deliberate. Existing files that have not been processed
     * will be picked up. Files already seen in a previous run are ignored by
     * persistent fingerprint history.
     */
    scanWatchFolder();
}

void WatchFolderManager::stopWatcher()
{
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());

    m_scanTimer.stop();
    m_candidates.clear();
    m_pendingFingerprintsByPath.clear();
}

void WatchFolderManager::handleDirectoryChanged(const QString &path)
{
    Q_UNUSED(path)

    /*
     * QFileSystemWatcher can coalesce events, miss bursts, or fire while a file
     * is still being copied. We just use it as a nudge; the timer does the real
     * stability checking. Because filesystems, much like users, do not promise
     * to behave.
     */
    scanWatchFolder();
}

QStringList WatchFolderManager::torrentFilesInWatchFolder() const
{
    if (m_watchFolder.isEmpty())
        return {};

    QDir dir(m_watchFolder);

    const QFileInfoList entries = dir.entryInfoList(
        QStringList { QStringLiteral("*.torrent") },
        QDir::Files | QDir::Readable | QDir::NoSymLinks,
        QDir::Name
        );

    QStringList result;
    result.reserve(entries.size());

    for (const QFileInfo &entry : entries)
        result.append(entry.absoluteFilePath());

    return result;
}

bool WatchFolderManager::isTorrentFile(const QString &filePath) const
{
    const QFileInfo info(filePath);

    return info.exists()
           && info.isFile()
           && info.isReadable()
           && info.suffix().compare(QStringLiteral("torrent"),
                                    Qt::CaseInsensitive) == 0;
}

void WatchFolderManager::scanWatchFolder()
{
    if (!m_enabled)
        return;

    if (m_watchFolder.isEmpty())
        return;

    const QFileInfo folderInfo(m_watchFolder);

    if (!folderInfo.exists() || !folderInfo.isDir()) {
        emit warningMessage(
            tr("Watch folder is no longer available: %1").arg(m_watchFolder)
            );
        stopWatcher();
        return;
    }

    const QStringList torrentFiles = torrentFilesInWatchFolder();

    QSet<QString> currentlySeen;

    for (const QString &filePath : torrentFiles) {
        currentlySeen.insert(filePath);
        observeCandidate(filePath);
    }

    /*
     * Remove candidates that disappeared before becoming stable.
     */
    const QList<QString> candidatePaths = m_candidates.keys();

    for (const QString &candidatePath : candidatePaths) {
        if (!currentlySeen.contains(candidatePath))
            m_candidates.remove(candidatePath);
    }
}

void WatchFolderManager::observeCandidate(const QString &filePath)
{
    if (!isTorrentFile(filePath))
        return;

    const QFileInfo fileInfo(filePath);
    const QString fingerprint = fingerprintForFile(filePath, fileInfo);

    if (alreadyProcessed(fingerprint) || isPending(fileInfo.absoluteFilePath(), fingerprint))
        return;

    CandidateFile candidate = m_candidates.value(filePath);

    if (!candidateIsStable(filePath, fileInfo, candidate)) {
        m_candidates.insert(filePath, candidate);
        return;
    }

    m_candidates.remove(filePath);
    markPending(fileInfo.absoluteFilePath(), fingerprint);

    emit statusMessage(
        tr("Watch folder found torrent: %1").arg(fileInfo.fileName())
        );

    emit torrentFileReady(fileInfo.absoluteFilePath());
}

bool WatchFolderManager::candidateIsStable(const QString &filePath,
                                           const QFileInfo &fileInfo,
                                           CandidateFile &candidate)
{
    Q_UNUSED(filePath)

    const qint64 currentSize = fileInfo.size();
    const QDateTime currentModified = fileInfo.lastModified();

    if (currentSize <= 0)
        return false;

    const bool unchanged =
        candidate.size == currentSize
        && candidate.lastModified == currentModified;

    if (unchanged) {
        ++candidate.stableChecks;
    } else {
        candidate.size = currentSize;
        candidate.lastModified = currentModified;
        candidate.stableChecks = 0;
    }

    return candidate.stableChecks >= m_requiredStableChecks;
}

QString WatchFolderManager::fingerprintForFile(const QString &filePath,
                                               const QFileInfo &fileInfo) const
{
    /*
     * This avoids re-adding the same file on app restart if the .torrent is
     * left in the watch folder. It is not cryptographic, nor does it need to be.
     */
    return QStringLiteral("%1|%2|%3")
        .arg(QDir::cleanPath(fileInfo.absoluteFilePath()),
             QString::number(fileInfo.size()),
             QString::number(fileInfo.lastModified().toMSecsSinceEpoch()));
}

bool WatchFolderManager::alreadyProcessed(const QString &fingerprint) const
{
    return m_processedFingerprints.contains(fingerprint);
}

bool WatchFolderManager::isPending(const QString &filePath,
                                   const QString &fingerprint) const
{
    const QString cleanedPath =
        QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());

    return m_pendingFingerprintsByPath.value(cleanedPath) == fingerprint;
}

void WatchFolderManager::markPending(const QString &filePath,
                                     const QString &fingerprint)
{
    if (fingerprint.isEmpty())
        return;

    const QString cleanedPath =
        QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());

    if (cleanedPath.isEmpty())
        return;

    m_pendingFingerprintsByPath.insert(cleanedPath, fingerprint);
}

void WatchFolderManager::forgetPending(const QString &filePath)
{
    const QString cleanedPath =
        QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());

    if (!cleanedPath.isEmpty())
        m_pendingFingerprintsByPath.remove(cleanedPath);
}

QString WatchFolderManager::pendingFingerprintForFile(const QString &filePath) const
{
    const QString cleanedPath =
        QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());

    if (cleanedPath.isEmpty())
        return {};

    return m_pendingFingerprintsByPath.value(cleanedPath);
}

void WatchFolderManager::markProcessed(const QString &fingerprint)
{
    if (fingerprint.isEmpty())
        return;

    m_processedFingerprints.insert(fingerprint);

    while (m_processedFingerprints.size() > MaxStoredFingerprints) {
        auto iterator = m_processedFingerprints.begin();

        if (iterator == m_processedFingerprints.end())
            break;

        m_processedFingerprints.erase(iterator);
    }

    saveProcessedFingerprints();
}

void WatchFolderManager::loadProcessedFingerprints()
{
    QSettings settings;

    const QStringList values =
        settings.value(processedFingerprintsSettingsKey()).toStringList();

    m_processedFingerprints =
        QSet<QString>(values.begin(), values.end());
}

void WatchFolderManager::saveProcessedFingerprints() const
{
    QSettings settings;

    const QStringList values =
        QStringList(m_processedFingerprints.begin(),
                    m_processedFingerprints.end());

    settings.setValue(processedFingerprintsSettingsKey(), values);
}