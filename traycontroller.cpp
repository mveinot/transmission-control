#include "traycontroller.h"

#include "settingskeys.h"
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

    // Do not restore the main window from a plain tray icon click.
    // The user-facing restore path is the explicit "Show Planetary" tray menu action.

    updateTrayIconVisibility();
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

    if (m_reallyQuit)
        return false;

    if (shouldCloseToTray()) {
        event->ignore();
        m_window->hide();
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
