#include "traycontroller.h"

#include "settingskeys.h"
#include "torrent.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QSettings>

TrayController::TrayController(QMainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
}

void TrayController::setup()
{
    if (!m_window)
        return;

    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_trayIcon = new QSystemTrayIcon(this);

    QIcon menuBarIcon(QStringLiteral(":/icons/planetary_menu.png"));
    menuBarIcon.setIsMask(true);

    m_trayIcon->setIcon(menuBarIcon);
    m_trayIcon->setToolTip(QApplication::applicationName());

    m_trayMenu = new QMenu(m_window);

    QAction *showAction = m_trayMenu->addAction(tr("Show Planetary"));
    QAction *quitAction = m_trayMenu->addAction(tr("Quit"));

    connect(showAction, &QAction::triggered,
            this, &TrayController::showMainWindow);

    connect(quitAction, &QAction::triggered,
            this, &TrayController::quitApplication);

    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick) {
                    showMainWindow();
                }
            });

    updateTrayIconVisibility();
}

void TrayController::applySettings()
{
    updateTrayIconVisibility();
}

void TrayController::showMainWindow()
{
    if (!m_window)
        return;

    m_window->show();
    m_window->setWindowState(m_window->windowState() & ~Qt::WindowMinimized);
    m_window->raise();
    m_window->activateWindow();
}

void TrayController::quitApplication()
{
    m_reallyQuit = true;

    if (m_trayIcon)
        m_trayIcon->hide();

    if (m_window)
        m_window->close();

    qApp->quit();
}

bool TrayController::handleCloseEvent(QCloseEvent *event)
{
    if (!event || !m_window)
        return false;

    if (!m_reallyQuit && m_trayIcon && m_trayIcon->isVisible()) {
        event->ignore();
        m_window->hide();
        return true;
    }

    return false;
}

bool TrayController::isTrayAvailable() const
{
    return m_trayIcon != nullptr;
}

bool TrayController::isTrayVisible() const
{
    return m_trayIcon && m_trayIcon->isVisible();
}

bool TrayController::trayIconEnabled() const
{
    QSettings settings;
    return settings.value(SettingsKeys::ShowTrayIcon, true).toBool();
}

bool TrayController::trayNotificationsEnabled() const
{
    QSettings settings;

    return trayIconEnabled() &&
           settings.value(SettingsKeys::ShowTrayNotifications, true).toBool();
}

void TrayController::updateTrayIconVisibility()
{
    if (!m_trayIcon)
        return;

    if (trayIconEnabled()) {
        m_trayIcon->show();
    } else {
        m_trayIcon->hide();
    }
}

void TrayController::showNotification(const QString &title,
                                      const QString &message,
                                      QSystemTrayIcon::MessageIcon icon,
                                      int millisecondsTimeoutHint)
{
    if (!m_trayIcon || !m_trayIcon->isVisible())
        return;

    if (!trayNotificationsEnabled())
        return;

    m_trayIcon->showMessage(title, message, icon, millisecondsTimeoutHint);
}

bool TrayController::isTorrentCompleteForNotification(const torrent &torrentItem)
{
    const QString status = torrentItem.getStatus();

    return torrentItem.getPercentDone() >= 99.9 ||
           status == QStringLiteral("Seeding") ||
           status == QStringLiteral("Waiting to Seed");
}

void TrayController::processTorrentList(const QVector<torrent> &torrents)
{
    QSet<int> currentlyCompleted;

    for (const torrent &torrentItem : torrents) {
        if (isTorrentCompleteForNotification(torrentItem))
            currentlyCompleted.insert(torrentItem.getId());
    }

    if (!m_completedTorrentNotificationBaselineLoaded) {
        m_knownCompletedTorrentIds = currentlyCompleted;
        m_completedTorrentNotificationBaselineLoaded = true;
        return;
    }

    for (const torrent &torrentItem : torrents) {
        const int id = torrentItem.getId();

        if (!currentlyCompleted.contains(id))
            continue;

        if (m_knownCompletedTorrentIds.contains(id))
            continue;

        showNotification(
            tr("Torrent finished"),
            torrentItem.getName(),
            QSystemTrayIcon::Information,
            5000
            );
    }

    m_knownCompletedTorrentIds = currentlyCompleted;
}
