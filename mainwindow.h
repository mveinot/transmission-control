#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QLocale>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPoint>
#include <QEvent>
#include <QLabel>
#include "foldermapping.h"
#include "rpc_client.h"
#include "torrentsortproxymodel.h"
#include "torrentmodel.h"
#include "geoipservice.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TorrentAddController;
class WatchFolderManager;
class WatchFolderController;
class UpdateCheckController;
class TrayController;
class TorrentGeneralController;
class TorrentFilesController;
class TorrentPeersController;
class TorrentTrackersController;
class TorrentListController;
class TorrentFilterController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void showAbout();
    void loadServerCombo();
    void saveSelectedServerFromCombo();
    void showMainWindow();
    void quitApplication();
    void handleLaunchArguments(const QStringList &arguments);

public slots:
    void bringToFront();

private slots:
    void updateTorrentList();
    void on_tableView_clicked(const QModelIndex &index);
    void on_actionDelete_Torrent_triggered();
    void onServerSetupTriggered();
    void on_actionStart_Torrent_triggered();
    void on_actionStop_Torrent_triggered();
    void on_action_Open_Torrent_triggered();
    void on_actionAdd_Torrent_from_Magnet_Link_triggered();
    void on_actionReannounce_triggered();
    void on_actionVerify_Torrent_triggered();
    void on_actionSettings_triggered();
    void handleTorrentsReceived(const QVector<torrent> &torrents);
    void showSessionSettings();
    void handleSessionSettingsReceived(const QJsonObject &settings);
    void on_actionAbout_triggered();
    void on_actionQuit_triggered();
    void exportSettings();
    void importSettings();
    //void showFilesContextMenu(const QPoint &position);
    //void openSelectedTorrentFile();

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    QMenu *mainMenu;
    QAction *aboutAction;
    TorrentModel *torrentModel = nullptr;
    rpc_client *client = nullptr;
    TorrentSortProxyModel *proxy = nullptr;
    GeoIpService *geoIpService = nullptr;
    WatchFolderManager *watchFolderManager = nullptr;
    WatchFolderController *watchFolderController = nullptr;

    int currentTorrentId() const;
    bool currentTabWantsLiveTorrentDetails() const;
    void refreshCurrentTorrentLiveDetailsIfNeeded();
    QLabel *connectionStatusLabel = nullptr;
    TorrentAddController *torrentAddController = nullptr;
    void addTorrentFromFile();
    void addTorrentFromMagnet();
    void saveTableViewState();
    void restoreTableViewState();
    int updateIntervalMs() const;
    void applyUpdateInterval();
    void setupConnectionStatusIndicator();
    void clearGeneralTab();
    QList<FolderMapping> currentServerFolderMappings() const;
    void applyAppSettings();
    bool openSessionSettingsWhenReceived = false;
    QString remoteDownloadDir;
    qint64 remoteFreeSpaceBytes = -1;
    int lastTorrentCount = 0;

    void updateConnectionStatus(int torrentCount);
    QString formatBytes(qint64 bytes) const;

    UpdateCheckController *updateCheckController = nullptr;
    TorrentGeneralController *torrentGeneralController = nullptr;
    TorrentFilesController *torrentFilesController = nullptr;
    TorrentPeersController *torrentPeersController = nullptr;
    TorrentTrackersController *torrentTrackersController = nullptr;
    TorrentListController *torrentListController = nullptr;
    TorrentFilterController *torrentFilterController = nullptr;
    TrayController *trayController = nullptr;

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
};
#endif // MAINWINDOW_H
