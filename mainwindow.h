#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLocale>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPoint>
#include <QSystemTrayIcon>
#include <QEvent>
#include <QLabel>
#include <QSet>
#include <QHash>
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
class TorrentAddController;
class UpdateChecker;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QSystemTrayIcon *trayIcon = nullptr;
    QMenu *trayMenu = nullptr;
    bool reallyQuit = false;

    void showAbout();
    void loadServerCombo();
    void saveSelectedServerFromCombo();
    void setupTrayIcon();
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
    void showTorrentContextMenu(const QPoint &pos);
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
    enum FileTreeColumn {
        FileNameColumn = 0,
        FilePriorityColumn,
        FileSizeColumn,
        FileDoneColumn,
        FilePercentColumn,
        FileColumnCount
    };

    enum FileTreeRole {
        FileKindRole = Qt::UserRole,
        FileIndexRole,
        FileWantedRole,
        FilePriorityRole
    };

    int currentTorrentId() const;
    QList<int> selectedTorrentIds() const;
    QStringList selectedTorrentNames() const;
    QLabel *connectionStatusLabel = nullptr;
    TorrentAddController *torrentAddController = nullptr;
    void startSelectedTorrent();
    void stopSelectedTorrent();
    void addTorrentFromFile();
    void addTorrentFromMagnet();
    void saveTableViewState();
    void restoreTableViewState();
    void setTorrentStateFilter(TorrentSortProxyModel::StateFilter filter);
    QTreeWidgetItem *findOrCreateChild(QTreeWidgetItem *parent,
                                       const QString &name,
                                       bool isFolder);
    QTreeWidgetItem *findOrCreateTopLevelItem(const QString &name);
    void populateFileTree(const QJsonArray &files,
                          const QJsonArray &wanted,
                          const QJsonArray &priorities);
    void populatePeerTable(const QJsonArray &peers);
    void reannounceSelectedTorrent();
    void verifySelectedTorrent();
    void forceStartSelectedTorrents();
    void setSelectedTorrentsLocation();
    void updateTorrentActionState();
    int updateIntervalMs() const;
    void applyUpdateInterval();
    void setupConnectionStatusIndicator();
    void populateGeneralTab(const QJsonObject &details);
    void populateTrackerTable(const QJsonObject &details);
    void clearGeneralTab();
    void clearTrackerTable();
    bool trayIconEnabled() const;
    bool trayNotificationsEnabled() const;
    void updateFolderPriorityStates();
    void updateFolderPriorityState(QTreeWidgetItem *item);
    QList<int> fileIndicesForItem(QTreeWidgetItem *item) const;
    QList<int> selectedFileIndicesForContextItem(QTreeWidgetItem *item) const;
    void showFileContextMenu(const QPoint &pos);
    void showTrackerContextMenu(const QPoint &pos);
    void copySelectedTorrentMagnetLink();
    void copySelectedTorrentHash();
    void copyTextToClipboard(const QString &text,
                             const QString &statusMessage);
    void openFileFromContextMenu(const QList<int> &fileIndices);
    void openContainingFolderFromContextMenu(const QList<int> &fileIndices);
    bool resolveMappedLocalPathForSingleFile(const QList<int> &fileIndices,
                                            const QString &dialogTitle,
                                            QString *localPath,
                                            QString *remotePath = nullptr,
                                            bool requireFileExists = true);
    QList<FolderMapping> currentServerFolderMappings() const;
    QString mapRemotePathToLocalPath(const QString &remotePath,
                                     const QList<FolderMapping> &mappings) const;
    void setSelectedFilesPriorityState(int priority, bool wanted);
    void queueMoveSelectedTop();
    void queueMoveSelectedUp();
    void queueMoveSelectedDown();
    void queueMoveSelectedBottom();
    void applyAppSettings();
    void updateTrayIconVisibility();
    void showTrayNotification(const QString &title,
                              const QString &message,
                              QSystemTrayIcon::MessageIcon icon =
                              QSystemTrayIcon::Information,
                              int millisecondsTimeoutHint = 5000);

    QSet<int> knownCompletedTorrentIds;
    void processFinishedTorrentNotifications(const QVector<torrent> &torrents);
    static bool isTorrentCompleteForNotification(const torrent &torrentItem);
    bool completedTorrentNotificationBaselineLoaded = false;
    bool openSessionSettingsWhenReceived = false;
    QString remoteDownloadDir;
    QString currentTorrentDownloadDir;
    int currentDetailsTorrentId = -1;
    QString currentTorrentHashString;
    QString currentTorrentMagnetLink;
    QHash<int, QString> currentTorrentFilePaths;
    qint64 remoteFreeSpaceBytes = -1;
    int lastTorrentCount = 0;

    void updateConnectionStatus(int torrentCount);
    QString formatBytes(qint64 bytes) const;
    void setupWatchFolderManager();
    void loadWatchFolderSettings();

    enum class TorrentFilterItemType {
        Status,
        Tracker
    };

    void rebuildTorrentFilterList(const QVector<torrent> &torrents);
    void addStatusFilterItems();
    void addTrackerFilterItems(const QStringList &trackerHosts);
    UpdateChecker *updateChecker = nullptr;

    void setupUpdateChecker();
    void maybeCheckForUpdates();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
};
#endif // MAINWINDOW_H
