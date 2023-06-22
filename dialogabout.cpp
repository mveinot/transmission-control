#include "dialogabout.h"
#include "ui_dialogabout.h"
#include "version.h"

DialogAbout::DialogAbout(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogAbout)
{
    ui->setupUi(this);
    ui->versionValue->setText(__PLANETARY_VERSION__);
    ui->builtValue->setText(QString(__BUILD_TIME__) + " on " + QString(__BUILD_DATE__));
}

DialogAbout::~DialogAbout()
{
    delete ui;
}

void DialogAbout::on_pushButton_clicked()
{
    this->close();
}
