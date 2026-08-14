#include "watchfoldercontroller.h"

#include "torrentbackend.h"
#include "settingskeys.h"
#include "torrentaddcontroller.h"
#include "watchfoldermanager.h"

#include <QFileInfo>
#include <QSettings>
#include <QTimer>

WatchFolderController::WatchFolderController(WatchFolderManager *manager,
                                             TorrentAddController *torrentAddController,
                                             TorrentBackend *client,
                                             QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_torrentAddController(torrentAddController)
    , m_client(client)
{
}

void WatchFolderController::setup()
{
    if (!m_manager || !m_torrentAddController || !m_client)
        return;

    connect(m_manager, &WatchFolderManager::torrentFileReady,
            this, [this](const QString &filePath) {
                const QFileInfo fileInfo(filePath);

                emit activityEventRequested(
                    tr("Torrent file detected"),
                    fileInfo.absoluteFilePath());

                emit activityEventRequested(
                    tr("Sending torrent to %1").arg(m_client->backendName()),
                    fileInfo.fileName());

                m_torrentAddController->addTorrentFileUsingDefaults(filePath);
            });

    connect(m_client, &TorrentBackend::torrentFileAddSucceeded,
            m_manager, &WatchFolderManager::markTorrentFileProcessed);

    connect(m_client, &TorrentBackend::torrentFileAddFailed,
            this, &WatchFolderController::handleTorrentFileAddFailed);

    connect(m_manager, &WatchFolderManager::statusMessage,
            this, [this](const QString &message) {
                emit statusMessageRequested(message, 3000);
            });

    connect(m_manager, &WatchFolderManager::warningMessage,
            this, [this](const QString &message) {
                emit statusMessageRequested(message, 5000);
            });

    connect(m_manager, &WatchFolderManager::torrentFileReady,
            this, [this](const QString &) {
                /*
                 * The add controller emits addStarted too, but this delayed refresh
                 * keeps watch-folder behavior resilient when the backend accepts
                 * the add before the next normal polling interval.
                 */
                QTimer::singleShot(1000, this, [this]() {
                    emit torrentListRefreshRequested();
                });
            });
}

void WatchFolderController::loadSettings()
{
    if (!m_manager)
        return;

    QSettings settings;

    const bool enabled =
        settings.value(QString::fromLatin1(SettingsKeys::WatchFolderEnabled),
                       false).toBool();

    const QString folderPath =
        settings.value(QString::fromLatin1(SettingsKeys::WatchFolderPath))
            .toString();

    const int scanIntervalMs =
        settings.value(QString::fromLatin1(SettingsKeys::WatchFolderScanIntervalMs),
                       1000).toInt();

    const int stableChecks =
        settings.value(QString::fromLatin1(SettingsKeys::WatchFolderStableChecks),
                       2).toInt();

    m_manager->setScanIntervalMs(scanIntervalMs);
    m_manager->setRequiredStableChecks(stableChecks);
    m_manager->setWatchFolder(folderPath);
    m_manager->setEnabled(enabled);
}

void WatchFolderController::handleTorrentFileAddFailed(const QString &filePath,
                                                       const QString &message)
{
    if (!m_manager)
        return;

    if (!m_manager->hasPendingTorrentFile(filePath))
        return;

    if (shouldTreatFailureAsProcessed(message)) {
        m_manager->markTorrentFileProcessed(filePath);
        emit statusMessageRequested(
            tr("Watch folder skipped duplicate torrent: %1")
                .arg(QFileInfo(filePath).fileName()),
            5000
            );
        return;
    }

    m_manager->retryTorrentFile(filePath);
    emit statusMessageRequested(
        tr("Watch folder add failed; will retry %1: %2")
            .arg(QFileInfo(filePath).fileName(), message),
        5000
        );
}

bool WatchFolderController::shouldTreatFailureAsProcessed(const QString &message) const
{
    // Duplicate is terminal: the server already owns the torrent.
    return message.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive);
}
