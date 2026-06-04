#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "rpc_client.h"
#include "dialogabout.h"
#include <QTimer>
#include <QProgressBar>
#include "version.h"
#include "torrentsortproxtmodel.h"
#include "progressbardelegate.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    client = new rpc_client(this);
    proxy = new TorrentSortProxtModel(this);
    proxy->setSourceModel(client);

    this->aboutAction = new QAction(0);
    this->aboutAction->setMenuRole(QAction::AboutRole);

    ui->setupUi(this);

    MainWindow::setWindowTitle(QCoreApplication::applicationName());

    this->mainMenu = new QMenu(0);
    this->menuBar()->addMenu(this->mainMenu);
    this->mainMenu->addAction(this->aboutAction);
    this->setMenuBar(this->menuBar());

    timer = new QTimer(this);

    connect(timer, &QTimer::timeout, this, &MainWindow::updateTorrentList);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    timer->start(5000);
    client->init();
    ui->statusbar->showMessage("Planetary " + QString(__PLANETARY_VERSION__) + " connected to " + client->getServer());
    ui->tableView->setModel(proxy);
    ui->tableView->hideColumn(rpc_client::IdColumn);

    ui->tableView->setItemDelegateForColumn(
                     rpc_client::PercentDoneColumn,
                     new ProgressBarDelegate(ui->tableView)
        );

    ui->tableView->setSortingEnabled(true);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->sortByColumn(rpc_client::NameColumn, Qt::AscendingOrder);
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

void MainWindow::on_tableWidget_cellClicked(int row, int column)
{
    //selected = row;
    //qDebug() << client->getTorrent(row).getName() << client->getTorrent(row).getId();
}


void MainWindow::on_actionDelete_Torrent_triggered()
{
    //qDebug() << client->getTorrent(ui->tableWidget->currentRow()).getId();
}

