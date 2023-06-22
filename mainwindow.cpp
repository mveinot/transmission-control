#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "rpc_client.h"
#include "dialogabout.h"
#include <QTimer>
#include <QProgressBar>

rpc_client client;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
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
    connect(&client, &rpc_client::listUpdated, this, &MainWindow::drawTorrentList);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    timer->start(5000);
    client.init();
    QStringList tableHeaders;
    tableHeaders << "Name" << "Completed" << "Status" << "Download" << "Upload" << "Ratio" << "ETA";
    ui->tableWidget->setHorizontalHeaderLabels(tableHeaders);
    ui->tableWidget->setColumnWidth(0, 450);
    ui->tableWidget->setColumnWidth(1,100);
    ui->tableWidget->setColumnWidth(2, 100);
    QHeaderView *verticalHeader = ui->tableWidget->verticalHeader();
    verticalHeader->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(16);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateTorrentList()
{
    client.getTorrentList();
}

void MainWindow::drawTorrentList()
{
    //qDebug() << "reload";
    ui->tableWidget->setRowCount(client.countTorrents());
    for (int i = 0; i < client.countTorrents(); i++)
    {
        QProgressBar *pgbar = new QProgressBar();
        pgbar->setRange(0, 100);
        pgbar->setValue((int) client.getTorrent(i).getPercentDone());

        ui->tableWidget->setItem(i, 0, new QTableWidgetItem(client.getTorrent(i).getName()));
        ui->tableWidget->setCellWidget(i, 1, pgbar);
        ui->tableWidget->setItem(i, 2, new QTableWidgetItem(client.getTorrent(i).getStatus()));
        ui->tableWidget->setItem(i, 3, new QTableWidgetItem(client.getTorrent(i).getRateDownload()));
        ui->tableWidget->setItem(i, 4, new QTableWidgetItem(client.getTorrent(i).getRateUpload()));
        ui->tableWidget->setItem(i, 5, new QTableWidgetItem(client.getTorrent(i).getUploadRatio()));
        ui->tableWidget->setItem(i, 6, new QTableWidgetItem(client.getTorrent(i).getEta()));
        ui->tableWidget->resizeColumnToContents(0);
    }
}

void MainWindow::showAbout()
{
    DialogAbout *about = new DialogAbout(this);
    about->show();
}

void MainWindow::on_tableWidget_cellClicked(int row, int column)
{
    qDebug() << client.getTorrent(row).getName();
}

