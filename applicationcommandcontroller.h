#ifndef APPLICATIONCOMMANDCONTROLLER_H
#define APPLICATIONCOMMANDCONTROLLER_H

#include "torrentbackend.h"

#include <QObject>

#include <functional>

class QAction;
class QMainWindow;
class QMenu;
class QMenuBar;

// Owns application-level action configuration, routing, and capability state.
// Selection-specific torrent policy remains with TorrentListController, while
// layout actions remain with WindowLayoutController.
class ApplicationCommandController : public QObject
{
    Q_OBJECT

public:
    struct Actions {
        QAction *openTorrent = nullptr;
        QAction *addMagnet = nullptr;
        QAction *startSelected = nullptr;
        QAction *stopSelected = nullptr;
        QAction *startAll = nullptr;
        QAction *stopAll = nullptr;
        QAction *forceStart = nullptr;
        QAction *verify = nullptr;
        QAction *reannounce = nullptr;
        QAction *deleteTorrent = nullptr;
        QAction *queueTop = nullptr;
        QAction *queueUp = nullptr;
        QAction *queueDown = nullptr;
        QAction *queueBottom = nullptr;
        QAction *closeWindow = nullptr;
        QAction *applicationSettings = nullptr;
        QAction *manageServers = nullptr;
        QAction *serverSettings = nullptr;
        QAction *alternativeSpeed = nullptr;
        QAction *statistics = nullptr;
        QAction *about = nullptr;
        QAction *quit = nullptr;
        QAction *checkForUpdates = nullptr;
        QAction *exportSettings = nullptr;
        QAction *importSettings = nullptr;
    };

    struct Menus {
        QMenuBar *menuBar = nullptr;
        QMenu *transfers = nullptr;
        QMenu *help = nullptr;
    };

    struct Handlers {
        std::function<void()> copy;
        std::function<void()> selectAll;
        std::function<void()> findTorrents;
        std::function<void()> findFiles;
        std::function<void()> openTorrent;
        std::function<void()> addMagnet;
        std::function<void()> startSelected;
        std::function<void()> stopSelected;
        std::function<void()> startAll;
        std::function<void()> stopAll;
        std::function<void()> verify;
        std::function<void()> reannounce;
        std::function<void()> deleteTorrent;
        std::function<void()> applicationSettings;
        std::function<void()> manageServers;
        std::function<void()> serverSettings;
        std::function<void(bool)> alternativeSpeed;
        std::function<void()> statistics;
        std::function<void()> closeWindow;
        std::function<void()> about;
        std::function<void()> quit;
        std::function<void()> diagnostics;
        std::function<void()> checkForUpdates;
        std::function<void()> exportSettings;
        std::function<void()> importSettings;
        std::function<void(const QString &, int)> statusMessage;
    };

    ApplicationCommandController(QMainWindow *window,
                                 const Actions &actions,
                                 const Menus &menus,
                                 Handlers handlers,
                                 QObject *parent = nullptr);

    void setup();
    void setBackendState(
        const QString &backendName,
        const TorrentBackendCapabilities &capabilities);
    void setAlternativeSpeedState(bool available, bool enabled);

private:
    QMainWindow *m_window = nullptr;
    Actions m_actions;
    Menus m_menus;
    Handlers m_handlers;

    void setupActionAppearance();
    void setupPlatformMenus();
    void setupEditMenu();
    void setupHelpMenu();
    void connectCommands();
    void showStatus(const QString &message, int timeoutMs) const;
};

#endif // APPLICATIONCOMMANDCONTROLLER_H
