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
#include <Qt>
#include "foldermapping.h"
#include "rpc_client.h"
#include "torrentsortproxymodel.h"
#include "torrentmodel.h"
#include "geoipservice.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QActionGroup;
class ActivityLogModel;
class QDockWidget;
class QTableView;
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
class StatusBarController;
class NotificationController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void showAbout();
    void showDiagnostics();
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
    void on_actionStart_All_Torrents_triggered();
    void on_actionStop_All_Torrents_triggered();
    void on_action_Open_Torrent_triggered();
    void on_actionAdd_Torrent_from_Magnet_Link_triggered();
    void on_actionReannounce_triggered();
    void on_actionVerify_Torrent_triggered();
    void on_actionSettings_triggered();
    void handleTorrentsReceived(const QVector<torrent> &torrents);
    void showSessionSettings();
    void showStatistics();
    void handleSessionSettingsReceived(const QJsonObject &settings);
    void toggleAlternativeSpeedMode(bool enabled);
    void refreshRemoteFreeSpace();
    void showQuickSpeedLimitsDialog();
    void on_actionAbout_triggered();
    void on_actionQuit_triggered();
    void exportSettings();
    void importSettings();
    void setToolBarVisibleFromAction(bool visible);
    void setStatusBarVisibleFromAction(bool visible);
    void setToolBarButtonStyleFromAction(QAction *action);

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    TorrentModel *torrentModel = nullptr;
    rpc_client *client = nullptr;
    TorrentSortProxyModel *proxy = nullptr;
    GeoIpService *geoIpService = nullptr;
    WatchFolderManager *watchFolderManager = nullptr;
    WatchFolderController *watchFolderController = nullptr;

    int currentTorrentId() const;
    bool currentTabWantsLiveTorrentDetails() const;
    void refreshCurrentTorrentLiveDetailsIfNeeded();
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
    void updateAlternativeSpeedAction(bool enabled, bool available);
    void setupViewMenu();
    void setupEditMenu();
    void setupActivityDock();
    void copyFromFocusedWidget();
    void selectAllInFocusedWidget();
    void focusTorrentSearch();
    void focusFileSearch();
    void recordActivity(const QString &event, const QString &details, const QString &server);
    void setupPlatformMenus();
    void restoreViewSettings();
    void saveViewSettings() const;
    void applyToolBarButtonStyle(Qt::ToolButtonStyle style);
    bool openSessionSettingsWhenReceived = false;
    bool openQuickSpeedLimitsWhenReceived = false;
    bool alternativeSpeedSettingsAvailable = false;
    bool confirmedAlternativeSpeedEnabled = false;
    QString remoteDownloadDir;
    QJsonObject cachedSessionSettings;

    UpdateCheckController *updateCheckController = nullptr;
    TorrentGeneralController *torrentGeneralController = nullptr;
    TorrentFilesController *torrentFilesController = nullptr;
    TorrentPeersController *torrentPeersController = nullptr;
    TorrentTrackersController *torrentTrackersController = nullptr;
    TorrentListController *torrentListController = nullptr;
    TorrentFilterController *torrentFilterController = nullptr;
    TrayController *trayController = nullptr;
    StatusBarController *statusBarController = nullptr;
    NotificationController *notificationController = nullptr;
    QMenu *viewMenu = nullptr;
    QAction *showToolBarAction = nullptr;
    QAction *showStatusBarAction = nullptr;
    QActionGroup *toolBarStyleActionGroup = nullptr;
    QDockWidget *activityDock = nullptr;
    QTableView *activityTable = nullptr;
    ActivityLogModel *activityLogModel = nullptr;
    bool activityConnectionEstablished = false;
    bool activityConnectionFailed = false;

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    QMenu *createPopupMenu() override;
};
#endif // MAINWINDOW_H
