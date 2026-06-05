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
#include "rpc_client.h"
#include "torrentsortproxymodel.h"

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

private slots:
    void updateTorrentList();
    void drawTorrentList();
    void on_tableView_clicked(const QModelIndex &index);
    void on_actionDelete_Torrent_triggered();
    void onServerSetupTriggered();
    void showTorrentContextMenu(const QPoint &pos);

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    QMenu *mainMenu;
    QAction *aboutAction;
    QMenuBar *mainMenuBar;
    rpc_client *client = nullptr;
    TorrentSortProxyModel *proxy = nullptr;
    int currentTorrentId() const;
    int currentSourceRow() const;
    int selected = 0;
    QSettings settings;
    void saveTableViewState();
    void restoreTableViewState();
    void setTorrentStateFilter(TorrentSortProxyModel::StateFilter filter);
    QTreeWidgetItem *findOrCreateChild(QTreeWidgetItem *parent,
                                       const QString &name,
                                       bool isFolder);
    QTreeWidgetItem *findOrCreateTopLevelItem(const QString &name);
    void populateFileTree(const QJsonArray &files);
    void populatePeerTable(const QJsonArray &peers);
    enum FileTreeColumn {
        FileNameColumn = 0,
        FileSizeColumn,
        FileDoneColumn,
        FilePercentColumn,
        FileColumnCount
    };


protected:
    void closeEvent(QCloseEvent *event) override;
};
#endif // MAINWINDOW_H
