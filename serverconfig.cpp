#include "serverconfig.h"
#include "ui_serverconfig.h"

ServerConfig::ServerConfig(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ServerConfig)
{
    ui->setupUi(this);
}

ServerConfig::~ServerConfig()
{
    delete ui;
}
