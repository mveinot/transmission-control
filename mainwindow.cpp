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
    QStringList tableHeaders;
    tableHeaders << "Name" << "Percent" << "Status";
    ui->tableWidget->setHorizontalHeaderLabels(tableHeaders);
    ui->tableWidget->setColumnWidth(0, 450);
    ui->tableWidget->setColumnWidth(1,100);
    ui->tableWidget->setColumnWidth(2, 100);
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

        QString torrentName = obj["name"].toString();
        double torrentPct = obj["percentDone"].toDouble()*100;
        int torrentStatus = obj["status"].toInt(0);
        QString torrentStatusStr = QString::number(torrentStatus);

        qDebug() << torrentName << " : " << torrentPct << " : " << torrentStatus;

        QProgressBar *pgbar = new QProgressBar();
        pgbar->setRange(0, 100);
        pgbar->setValue((int) torrentPct);

        ui->tableWidget->setItem(i, 0, new QTableWidgetItem(torrentName));
        ui->tableWidget->setCellWidget(i, 1, pgbar);
        ui->tableWidget->setItem(i, 2, new QTableWidgetItem(torrentStatusStr));

    }
}
