#include "dialogabout.h"
#include "ui_dialogabout.h"
#include "version.h"
#include <QImageReader>
#include <QtDebug>
#include <QPixmap>
#include <QFile>

DialogAbout::DialogAbout(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogAbout)
{
    QFile f(":/icons/planetary.png");
    qDebug() << f.exists();
    QPixmap pix(":/icons/planetary.png");
    qDebug() << pix.isNull();
    ui->setupUi(this);
    ui->versionValue->setText(__PLANETARY_VERSION__);
    ui->builtValue->setText(QString(__BUILD_TIME__) + " on " + QString(__BUILD_DATE__));
    //ui->image->setPixmap(pix);
    ui->image->show();
    ui->image->setVisible(true);
    qDebug() << "pix null:" << pix.isNull();
    qDebug() << "pix size:" << pix.size();
    qDebug() << "label size before:" << ui->image->size();

    //ui->label->setPixmap(pix);

    //qDebug() << "label has pixmap:" << (ui->image->pixmap() && !ui->image->pixmap().isNull());
    qDebug() << "label size after:" << ui->image->size();
    qDebug() << "label visible:" << ui->image->isVisible();
    qDebug() << "label hidden:" << ui->image->isHidden();
}

DialogAbout::~DialogAbout()
{
    delete ui;
}

void DialogAbout::on_pushButton_clicked()
{
    this->close();
}
