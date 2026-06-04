#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "rpc_client.h"
#include "dialogabout.h"
#include "serverconfig.h"
#include <QTimer>
//#include <QProgressBar>
#include <QActionGroup>
#include <QHeaderView>
#include <QSettings>
//#include "version.h"
#include "torrentsortproxymodel.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    client = new rpc_client(this);
    proxy = new TorrentSortProxyModel(this);
    proxy->setSourceModel(client);

    this->aboutAction = new QAction(0);
    this->aboutAction->setMenuRole(QAction::AboutRole);

    ui->setupUi(this);

    auto *stateGroup = new QActionGroup(this);
    stateGroup->setExclusive(true);

    ui->actionAll->setCheckable(true);
    ui->actionDownloading->setCheckable(true);
    ui->actionCompleted->setCheckable(true);
    ui->actionActive->setCheckable(true);
    ui->actionInactive->setCheckable(true);
    ui->actionStopped->setCheckable(true);
    ui->actionError->setCheckable(true);

    stateGroup->addAction(ui->actionAll);
    stateGroup->addAction(ui->actionDownloading);
    stateGroup->addAction(ui->actionCompleted);
    stateGroup->addAction(ui->actionActive);
    stateGroup->addAction(ui->actionInactive);
    stateGroup->addAction(ui->actionStopped);
    stateGroup->addAction(ui->actionError);

    ui->actionAll->setChecked(true);

    connect(ui->actionAll, &QAction::triggered, this, [this]() {
        proxy->setStateFilter(TorrentSortProxyModel::StateFilter::All);
    });

    connect(ui->actionDownloading, &QAction::triggered, this, [this]() {
        proxy->setStateFilter(TorrentSortProxyModel::StateFilter::Downloading);
    });

    connect(ui->actionCompleted, &QAction::triggered, this, [this]() {
        proxy->setStateFilter(TorrentSortProxyModel::StateFilter::Completed);
    });

    connect(ui->actionActive, &QAction::triggered, this, [this]() {
        proxy->setStateFilter(TorrentSortProxyModel::StateFilter::Active);
    });

    connect(ui->actionInactive, &QAction::triggered, this, [this]() {
        proxy->setStateFilter(TorrentSortProxyModel::StateFilter::Inactive);
    });

    connect(ui->actionStopped, &QAction::triggered, this, [this]() {
        proxy->setStateFilter(TorrentSortProxyModel::StateFilter::Stopped);
    });

    connect(ui->actionError, &QAction::triggered, this, [this]() {
        proxy->setStateFilter(TorrentSortProxyModel::StateFilter::Error);
    });

    connect(ui->actionServer_Setup, &QAction::triggered, this, &MainWindow::onServerSetupTriggered);

    MainWindow::setWindowTitle(QCoreApplication::applicationName());

    this->mainMenu = new QMenu(0);
    this->menuBar()->addMenu(this->mainMenu);
    this->mainMenu->addAction(this->aboutAction);
    this->setMenuBar(this->menuBar());

    timer = new QTimer(this);
    ui->statusbar->showMessage("Connected to " + client->getServer());

    connect(timer, &QTimer::timeout, this, &MainWindow::updateTorrentList);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    connect(client, &rpc_client::updateStarted, this, [this]() {
        ui->statusbar->showMessage("Connected to " + client->getServer() + " (updating)");
    });

    connect(client, &rpc_client::updateFinished, this, [this]() {
        ui->statusbar->showMessage("Connected to " + client->getServer());
    });

    ui->tableView->setModel(proxy);
    ui->tableView->hideColumn(rpc_client::IdColumn);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->sortByColumn(rpc_client::NameColumn, Qt::AscendingOrder);
    restoreTableViewState();

    client->init();
    timer->start(10000);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateTorrentList()
{
    client->getTorrentList();
}

void MainWindow::drawTorrentList()
{
}

void MainWindow::showAbout()
{
    DialogAbout *about = new DialogAbout(this);
    about->show();
}

void MainWindow::on_tableView_clicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid())
        return;

    QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);
    if (!sourceIndex.isValid())
        return;

    const int sourceRow = sourceIndex.row();
    const int torrentId = client->data(
                                    client->index(sourceRow, rpc_client::IdColumn),
                                    Qt::UserRole
                                    ).toInt();

    selected = sourceRow;

    qDebug() << "Selected source row:" << sourceRow
             << "torrent id:" << torrentId
             << "name:" << client->data(
                                     client->index(sourceRow, rpc_client::NameColumn),
                                     Qt::DisplayRole
                                     ).toString();
}

int MainWindow::currentSourceRow() const
{
    const QModelIndex proxyIndex = ui->tableView->currentIndex();

    if (!proxyIndex.isValid())
        return -1;

    const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return -1;

    return sourceIndex.row();
}

int MainWindow::currentTorrentId() const
{
    const QModelIndex proxyIndex = ui->tableView->currentIndex();

    if (!proxyIndex.isValid())
        return -1;

    const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);

    if (!sourceIndex.isValid())
        return -1;

    return sourceIndex.data(Qt::UserRole).toInt();
}

void MainWindow::on_actionDelete_Torrent_triggered()
{
    const int torrentId = currentTorrentId();

    if (torrentId < 0) {
        qDebug() << "No torrent selected";
        return;
    }

    qDebug() << "Delete torrent id:" << torrentId;

    // Later:
    // client->deleteTorrent(torrentId);
}

void MainWindow::saveTableViewState()
{
    QSettings settings;

    settings.setValue(
        "ui/tableView/horizontalHeaderState",
        ui->tableView->horizontalHeader()->saveState()
        );

    settings.setValue(
        "ui/tableView/verticalHeaderState",
        ui->tableView->verticalHeader()->saveState()
        );
}

void MainWindow::restoreTableViewState()
{
    QSettings settings;

    const QByteArray horizontalState =
        settings.value("ui/tableView/horizontalHeaderState").toByteArray();

    if (!horizontalState.isEmpty()) {
        ui->tableView->horizontalHeader()->restoreState(horizontalState);
    }

    const QByteArray verticalState =
        settings.value("ui/tableView/verticalHeaderState").toByteArray();

    if (!verticalState.isEmpty()) {
        ui->tableView->verticalHeader()->restoreState(verticalState);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveTableViewState();

    QMainWindow::closeEvent(event);
}

void MainWindow::onServerSetupTriggered()
{
    ServerConfig *sc = new ServerConfig(this);
    sc->show();
}