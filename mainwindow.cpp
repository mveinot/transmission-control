#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "rpc_client.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    qDebug() << "create";
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_torrentListing_clicked(const QModelIndex &index)
{
    qDebug() << "click!";
    qDebug() <<
    QListWidgetItem *newItem = new QListWidgetItem;
    newItem->setText("testing");
    ui->torrentListing->insertItem(0,newItem);
}


void MainWindow::on_torrentListing_viewportEntered()
{
    qDebug() << "click!";

    QListWidgetItem *newItem = new QListWidgetItem;
    newItem->setText("testing");
    ui->torrentListing->insertItem(0,newItem);
}

