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
}


void MainWindow::on_torrentListing_viewportEntered()
{
}

