#include "applicationcommandcontroller.h"

#include "iconthememanager.h"

#include <QAction>
#include <QDesktopServices>
#include <QKeySequence>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QUrl>

#include <utility>

namespace {
template<typename Callback>
void invoke(const Callback &callback)
{
    if (callback)
        callback();
}
} // namespace

ApplicationCommandController::ApplicationCommandController(
    QMainWindow *window,
    const Actions &actions,
    const Menus &menus,
    Handlers handlers,
    QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_actions(actions)
    , m_menus(menus)
    , m_handlers(std::move(handlers))
{
}

void ApplicationCommandController::setup()
{
    setupActionAppearance();
    setupPlatformMenus();
    setupEditMenu();
    setupHelpMenu();
    connectCommands();
}

void ApplicationCommandController::setupActionAppearance()
{
    auto &icons = AppIcons::IconThemeManager::instance();
    icons.bindAction(m_actions.openTorrent, AppIcons::Id::ActionAddTorrent);
    icons.bindAction(m_actions.addMagnet, AppIcons::Id::ActionAddMagnet);
    icons.bindAction(m_actions.startSelected, AppIcons::Id::ActionStart);
    icons.bindAction(m_actions.stopSelected, AppIcons::Id::ActionStop);
    icons.bindAction(m_actions.startAll, AppIcons::Id::ActionStartAll);
    icons.bindAction(m_actions.stopAll, AppIcons::Id::ActionStopAll);
    icons.bindAction(m_actions.forceStart, AppIcons::Id::ActionForceStart);
    icons.bindAction(m_actions.verify, AppIcons::Id::ActionVerify);
    icons.bindAction(m_actions.reannounce, AppIcons::Id::ActionReannounce);
    icons.bindAction(m_actions.deleteTorrent, AppIcons::Id::ActionDelete);
    icons.bindAction(m_actions.queueTop, AppIcons::Id::QueueTop);
    icons.bindAction(m_actions.queueUp, AppIcons::Id::QueueUp);
    icons.bindAction(m_actions.queueDown, AppIcons::Id::QueueDown);
    icons.bindAction(m_actions.queueBottom, AppIcons::Id::QueueBottom);

    if (m_actions.openTorrent)
        m_actions.openTorrent->setShortcut(QKeySequence::Open);
#ifdef Q_OS_MACOS
    if (m_actions.addMagnet) {
        m_actions.addMagnet->setShortcut(
            QKeySequence(QStringLiteral("Meta+Shift+O")));
    }
#else
    if (m_actions.addMagnet) {
        m_actions.addMagnet->setShortcut(
            QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    }
#endif
}

void ApplicationCommandController::setupPlatformMenus()
{
    if (m_actions.closeWindow) {
        m_actions.closeWindow->setShortcut(QKeySequence::Close);
        m_actions.closeWindow->setMenuRole(QAction::NoRole);
    }

#ifdef Q_OS_MACOS
    // Qt relocates these Designer actions into the standard application menu.
    if (m_actions.about)
        m_actions.about->setMenuRole(QAction::AboutRole);
    if (m_actions.quit)
        m_actions.quit->setMenuRole(QAction::QuitRole);
    if (m_actions.applicationSettings) {
        m_actions.applicationSettings->setText(tr("Settings…"));
        // Qt's Cocoa plugin renders PreferencesRole as "Preferences...".
        // Modern macOS uses "Settings…", so preserve our label while still
        // placing this action in the standard application menu.
        m_actions.applicationSettings->setMenuRole(
            QAction::ApplicationSpecificRole);
        m_actions.applicationSettings->setShortcut(QKeySequence::Preferences);
    }
    if (m_actions.manageServers) {
        m_actions.manageServers->setMenuRole(
            QAction::ApplicationSpecificRole);
    }
    if (m_actions.serverSettings) {
        m_actions.serverSettings->setMenuRole(
            QAction::ApplicationSpecificRole);
    }
#endif
}

void ApplicationCommandController::setupEditMenu()
{
    if (!m_window || !m_menus.menuBar || !m_menus.transfers)
        return;

    auto *editMenu = new QMenu(tr("Edit"), m_window);
    m_menus.menuBar->insertMenu(
        m_menus.transfers->menuAction(), editMenu);

    QAction *copyAction = editMenu->addAction(tr("Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setShortcutContext(Qt::WindowShortcut);
    connect(copyAction, &QAction::triggered,
            this, [this]() { invoke(m_handlers.copy); });

    QAction *selectAllAction = editMenu->addAction(tr("Select All"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    selectAllAction->setShortcutContext(Qt::WindowShortcut);
    connect(selectAllAction, &QAction::triggered,
            this, [this]() { invoke(m_handlers.selectAll); });

    editMenu->addSeparator();

    QAction *findAction = editMenu->addAction(tr("Find Torrents"));
    findAction->setShortcut(QKeySequence::Find);
    findAction->setShortcutContext(Qt::WindowShortcut);
    connect(findAction, &QAction::triggered,
            this, [this]() { invoke(m_handlers.findTorrents); });

    QAction *findFilesAction = editMenu->addAction(tr("Find Files"));
#ifdef Q_OS_MACOS
    findFilesAction->setShortcut(
        QKeySequence(QStringLiteral("Meta+Alt+F")));
#else
    findFilesAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+Alt+F")));
#endif
    findFilesAction->setShortcutContext(Qt::WindowShortcut);
    connect(findFilesAction, &QAction::triggered,
            this, [this]() { invoke(m_handlers.findFiles); });
}

void ApplicationCommandController::setupHelpMenu()
{
    if (!m_window || !m_menus.help)
        return;

    QAction *websiteAction =
        new QAction(tr("Planetary Website..."), m_window);
    m_menus.help->insertAction(
        m_actions.checkForUpdates, websiteAction);
    connect(websiteAction, &QAction::triggered, this, [this]() {
        if (!QDesktopServices::openUrl(
                QUrl(QStringLiteral("https://planetary.mvgrafx.net/")))) {
            showStatus(tr("Could not open the Planetary website."), 5000);
        }
    });

    QAction *supportAction =
        new QAction(tr("Contact Support..."), m_window);
    m_menus.help->insertAction(m_actions.about, supportAction);
    connect(supportAction, &QAction::triggered, this, [this]() {
        if (!QDesktopServices::openUrl(
                QUrl(QStringLiteral("mailto:planetary@mvgrafx.net")))) {
            showStatus(
                tr("Could not open the default email application. "
                   "Contact planetary@mvgrafx.net directly."),
                8000);
        }
    });

    QAction *diagnosticsAction =
        new QAction(tr("Diagnostics..."), m_window);
    m_menus.help->insertAction(m_actions.about, diagnosticsAction);
    m_menus.help->insertSeparator(m_actions.about);
    connect(diagnosticsAction, &QAction::triggered,
            this, [this]() { invoke(m_handlers.diagnostics); });
}

void ApplicationCommandController::connectCommands()
{
    const auto connectAction =
        [this](QAction *action, const std::function<void()> &handler) {
            if (!action)
                return;
            connect(action, &QAction::triggered,
                    this, [handler]() { invoke(handler); });
        };

    connectAction(m_actions.openTorrent, m_handlers.openTorrent);
    connectAction(m_actions.addMagnet, m_handlers.addMagnet);
    connectAction(m_actions.startSelected, m_handlers.startSelected);
    connectAction(m_actions.stopSelected, m_handlers.stopSelected);
    connectAction(m_actions.startAll, m_handlers.startAll);
    connectAction(m_actions.stopAll, m_handlers.stopAll);
    connectAction(m_actions.verify, m_handlers.verify);
    connectAction(m_actions.reannounce, m_handlers.reannounce);
    connectAction(m_actions.deleteTorrent, m_handlers.deleteTorrent);
    connectAction(m_actions.applicationSettings,
                  m_handlers.applicationSettings);
    connectAction(m_actions.manageServers, m_handlers.manageServers);
    connectAction(m_actions.serverSettings, m_handlers.serverSettings);
    connectAction(m_actions.statistics, m_handlers.statistics);
    connectAction(m_actions.closeWindow, m_handlers.closeWindow);
    connectAction(m_actions.about, m_handlers.about);
    connectAction(m_actions.quit, m_handlers.quit);
    connectAction(m_actions.checkForUpdates, m_handlers.checkForUpdates);
    connectAction(m_actions.exportSettings, m_handlers.exportSettings);
    connectAction(m_actions.importSettings, m_handlers.importSettings);

    if (m_actions.alternativeSpeed) {
        connect(m_actions.alternativeSpeed, &QAction::triggered,
                this, [this](bool checked) {
                    if (m_handlers.alternativeSpeed)
                        m_handlers.alternativeSpeed(checked);
                });
    }
}

void ApplicationCommandController::setBackendState(
    const QString &backendName,
    const TorrentBackendCapabilities &capabilities)
{
    const QString displayName = backendName.trimmed();
    const bool settingsAvailable =
        !displayName.isEmpty() && capabilities.sessionSettings;

    if (m_actions.serverSettings) {
        m_actions.serverSettings->setText(
            settingsAvailable
                ? tr("%1 Settings...").arg(displayName)
                : tr("Server Settings..."));
        m_actions.serverSettings->setEnabled(settingsAvailable);
    }
    if (m_actions.statistics)
        m_actions.statistics->setVisible(capabilities.sessionStatistics);
}

void ApplicationCommandController::setAlternativeSpeedState(
    bool available,
    bool enabled)
{
    if (!m_actions.alternativeSpeed)
        return;

    // Session reconciliation must not issue another session-set command.
    const QSignalBlocker blocker(m_actions.alternativeSpeed);
    m_actions.alternativeSpeed->setEnabled(available);
    m_actions.alternativeSpeed->setChecked(enabled);
}

void ApplicationCommandController::showStatus(
    const QString &message,
    int timeoutMs) const
{
    if (m_handlers.statusMessage)
        m_handlers.statusMessage(message, timeoutMs);
}
