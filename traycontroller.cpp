#include "traycontroller.h"

#include "settingskeys.h"
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QSettings>
#include <QTimer>

#if defined(Q_OS_MACOS)
#include "macapplicationbackend.h"
#endif

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

    m_totalCountAction = new QAction(this);
    m_downloadingCountAction = new QAction(this);
    m_seedingCountAction = new QAction(this);
    m_totalCountAction->setEnabled(false);
    m_downloadingCountAction->setEnabled(false);
    m_seedingCountAction->setEnabled(false);
    updateTorrentCountActions();

    m_showAction = new QAction(tr("Show Planetary"), this);
    m_quitAction = new QAction(tr("Quit"), this);

    connect(m_showAction, &QAction::triggered,
            this, &TrayController::showMainWindow);

    connect(m_quitAction, &QAction::triggered,
            this, &TrayController::quitApplication);

    rebuildTrayMenu();
    m_trayIcon->setContextMenu(m_trayMenu);

    // Do not restore the main window from a plain tray icon click.
    // The user-facing restore path is the explicit "Show Planetary" tray menu action.

    updateTrayIconVisibility();
}


void TrayController::setTorrentGlobalActions(QAction *startAllAction, QAction *stopAllAction)
{
    m_startAllAction = startAllAction;
    m_stopAllAction = stopAllAction;
    rebuildTrayMenu();
}

void TrayController::setTorrentCounts(int totalCount,
                                      int downloadingCount,
                                      int seedingCount)
{
    m_totalCount = qMax(0, totalCount);
    m_downloadingCount = qMax(0, downloadingCount);
    m_seedingCount = qMax(0, seedingCount);
    updateTorrentCountActions();
}

void TrayController::applySettings()
{
    updateTrayIconVisibility();

    // If the tray feature is disabled while the main window is hidden, make
    // the window visible again so the app cannot become an unreachable
    // background process with no tray icon and no visible window.
    if (!trayIconEnabled() && m_window && !m_window->isVisible())
        showMainWindow();
}

void TrayController::showMainWindow()
{
    if (!m_window)
        return;

#if defined(Q_OS_MACOS)
    // Restore regular application mode before creating a visible window so
    // macOS reinstates both the Dock icon and application menu.
    setMacApplicationDockIconVisible(true);
#endif

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
    // Consuming close hides rather than destroys the composition root, keeping
    // polling and notification services alive.
    if (!event || !m_window)
        return false;

    if (m_reallyQuit)
        return false;

    if (shouldCloseToTray()) {
        event->ignore();
        m_window->hide();
#if defined(Q_OS_MACOS)
        // The status item remains available in accessory mode, allowing the
        // application to run without occupying the Dock while its window is hidden.
        setMacApplicationDockIconVisible(false);
#endif
        return true;
    }

    // QApplication::setQuitOnLastWindowClosed(false) is used so closing to
    // the tray can keep the app alive. When close-to-tray is not available
    // or disabled, Planetary is a single-window app, so the window close
    // button should quit the application instead of leaving an unreachable
    // dock/taskbar process behind.
    m_reallyQuit = true;
    event->accept();
    qApp->quit();
    return true;
}

bool TrayController::isTrayAvailable() const
{
    return m_trayIcon != nullptr;
}

bool TrayController::isTrayVisible() const
{
    return m_trayIcon && m_trayIcon->isVisible();
}

bool TrayController::showNotification(const QString &title,
                                      const QString &message,
                                      int millisecondsTimeoutHint)
{
#if defined(Q_OS_WIN)
    if (!m_trayIcon || !QSystemTrayIcon::supportsMessages())
        return false;

    const bool wasVisible = m_trayIcon->isVisible();
    if (!wasVisible)
        m_trayIcon->show();

    m_trayIcon->showMessage(title,
                            message,
                            QSystemTrayIcon::Information,
                            millisecondsTimeoutHint);

    if (!wasVisible) {
        const int hideDelay = qMax(millisecondsTimeoutHint, 1000) + 500;
        QTimer::singleShot(hideDelay, this, [this]() {
            if (m_trayIcon && !trayIconEnabled())
                m_trayIcon->hide();
        });
    }

    return true;
#else
    Q_UNUSED(title)
    Q_UNUSED(message)
    Q_UNUSED(millisecondsTimeoutHint)
    return false;
#endif
}

bool TrayController::trayIconEnabled() const
{
    QSettings settings;
    return settings.value(SettingsKeys::ShowTrayIcon, true).toBool();
}

bool TrayController::shouldCloseToTray() const
{
    return trayIconEnabled() &&
           m_trayIcon &&
           m_trayIcon->isVisible();
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

void TrayController::updateTorrentCountActions()
{
    if (m_totalCountAction)
        m_totalCountAction->setText(tr("Torrents: %1").arg(m_totalCount));

    if (m_downloadingCountAction) {
        m_downloadingCountAction->setText(
            tr("Downloading: %1").arg(m_downloadingCount));
    }

    if (m_seedingCountAction)
        m_seedingCountAction->setText(tr("Seeding: %1").arg(m_seedingCount));
}

void TrayController::rebuildTrayMenu()
{
    if (!m_trayMenu)
        return;

    m_trayMenu->clear();

    if (m_totalCountAction)
        m_trayMenu->addAction(m_totalCountAction);

    if (m_downloadingCountAction)
        m_trayMenu->addAction(m_downloadingCountAction);

    if (m_seedingCountAction)
        m_trayMenu->addAction(m_seedingCountAction);

    if (m_totalCountAction || m_downloadingCountAction || m_seedingCountAction)
        m_trayMenu->addSeparator();

    if (m_showAction)
        m_trayMenu->addAction(m_showAction);

    if (m_startAllAction || m_stopAllAction) {
        m_trayMenu->addSeparator();

        if (m_startAllAction)
            m_trayMenu->addAction(m_startAllAction);

        if (m_stopAllAction)
            m_trayMenu->addAction(m_stopAllAction);
    }

    if (m_quitAction) {
        m_trayMenu->addSeparator();
        m_trayMenu->addAction(m_quitAction);
    }
}
