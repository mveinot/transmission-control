#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QSettings>
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
#include "rpc_client.h"
#include "torrentsortproxymodel.h"
#include "torrentmodel.h"
#include "geoipservice.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void showAbout();
    void loadServerCombo();
    void saveSelectedServerFromCombo();
    QSystemTrayIcon *trayIcon = nullptr;
    QMenu *trayMenu = nullptr;
    bool reallyQuit = false;

    void setupTrayIcon();
    void showMainWindow();
    void quitApplication();

public slots:
    void bringToFront();

private slots:
    void updateTorrentList();
    void drawTorrentList();
    void on_tableView_clicked(const QModelIndex &index);
    void on_actionDelete_Torrent_triggered();
    void onServerSetupTriggered();
    void showTorrentContextMenu(const QPoint &pos);
    void on_actionStart_Torrent_triggered();
    void on_actionStop_Torrent_triggered();
    void on_action_Open_Torrent_triggered();
    void on_actionAdd_Torrent_from_Magnet_Link_triggered();
    void on_actionReannounce_triggered();
    void on_actionAbout_triggered();
    void on_actionVerify_Torrent_triggered();

    void on_actionSettings_triggered();

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    QMenu *mainMenu;
    QAction *aboutAction;
    QMenuBar *mainMenuBar;
    int selected = 0;
    QSettings settings;
    TorrentModel *torrentModel = nullptr;
    rpc_client *client = nullptr;
    TorrentSortProxyModel *proxy = nullptr;
    GeoIpService *geoIpService = nullptr;
    enum FileTreeColumn {
        FileNameColumn = 0,
        FileSizeColumn,
        FileDoneColumn,
        FilePercentColumn,
        FileColumnCount
    };

    int currentTorrentId() const;
    QList<int> selectedTorrentIds() const;
    QStringList selectedTorrentNames() const;
    QString currentTorrentName() const;
    QLabel *connectionStatusLabel = nullptr;
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
    void populateFileTree(const QJsonArray &files);
    void populatePeerTable(const QJsonArray &peers);
    void reannounceSelectedTorrent();
    void verifySelectedTorrent();
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
    bool hideApplicationIconEnabled() const;

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

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
};
#endif // MAINWINDOW_H
