#include "watchfoldercontroller.h"

#include "rpc_client.h"
#include "settingskeys.h"
#include "torrentaddcontroller.h"
#include "watchfoldermanager.h"

#include <QFileInfo>
#include <QSettings>
#include <QTimer>
#include <QDebug>

WatchFolderController::WatchFolderController(WatchFolderManager *manager,
                                             TorrentAddController *torrentAddController,
                                             rpc_client *client,
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
            m_torrentAddController, &TorrentAddController::addTorrentFileUsingDefaults);

    connect(m_client, &rpc_client::torrentFileAddSucceeded,
            m_manager, &WatchFolderManager::markTorrentFileProcessed);

    connect(m_client, &rpc_client::torrentFileAddFailed,
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
                 * keeps watch-folder behavior resilient when Transmission accepts
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

    qDebug() << "Watch folder settings:"
             << "enabled=" << enabled
             << "path=" << folderPath
             << "stableChecks=" << stableChecks;
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
    return message.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive);
}
