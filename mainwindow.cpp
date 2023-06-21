#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "rpc_client.h"
#include <QTimer>
#include <QProgressBar>

rpc_client client;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    qDebug() << "create";
    ui->setupUi(this);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTorrentList);
    timer->start(5000);
    client.init();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateTorrentList()
{
    client.getTorrentList();
    qDebug() << client.countTorrents();
    ui->tableWidget->setRowCount(client.countTorrents());
    QJsonArray torrentListing = client.torrents();
    for (int i = 0; i < torrentListing.count(); i++)
    {
        QJsonObject obj = torrentListing[i].toObject();
        QProgressBar *pgbar = new QProgressBar();
        pgbar->setRange(0, 100);
        pgbar->setValue(obj["percentDone"].toInt(0)*100);
        ui->tableWidget->setItem(i, 0, new QTableWidgetItem(obj["name"].toString()));
        ui->tableWidget->setCellWidget(i, 1, pgbar);
    }
}
