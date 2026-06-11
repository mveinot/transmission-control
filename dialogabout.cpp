#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMouseEvent>
#include "dialogabout.h"
#include "ui_dialogabout.h"
#include "version.h"

static QString readTextFile(const QString &path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&file);
    return stream.readAll();
}

static QString bundledResourcePath(const QString &relativePath)
{
    return QDir::cleanPath(
        QCoreApplication::applicationDirPath()
        + "/../Resources/"
        + relativePath
        );
}

DialogAbout::DialogAbout(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogAbout)
{
    ui->setupUi(this);

    setWindowTitle("About Planetary");

    ui->textAbout->setOpenExternalLinks(true);
    ui->textCredits->setOpenExternalLinks(true);
    ui->textLicense->setReadOnly(true);

    ui->textAbout->setHtml(QString(
                               "<h2>Planetary</h2>"
                               "<p>A native Qt Transmission remote client.</p>"
                               "<p>Version: %1</p>"
                               "<p>Build date: %2</p>"
                               "<p>Copyright © Mark Veinot</p>"
                               ).arg(QCoreApplication::applicationVersion(), QStringLiteral(__BUILD_DATE__)));

    const QString creditsPath =
        bundledResourcePath("licenses/THIRD_PARTY_NOTICES.txt");

    QString credits = readTextFile(creditsPath);

    if (credits.isEmpty()) {
        credits =
            "Planetary uses Qt, libmaxminddb, and DB-IP Lite country data.\n\n"
            "libmaxminddb is licensed under the Apache License, Version 2.0.\n"
            "DB-IP Lite data is licensed under Creative Commons Attribution 4.0 "
            "and requires attribution to DB-IP.com.";
    }

    ui->textCredits->setPlainText(credits);

    const QString licensePath =
        bundledResourcePath("licenses/LICENSE");

    QString licenseText = readTextFile(licensePath);

    if (licenseText.isEmpty()) {
        licenseText =
            "The GPL license text could not be loaded from the application bundle.";
    }

    ui->textLicense->setPlainText(licenseText);

    connect(ui->aboutButtonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);

    connect(ui->aboutButtonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    ui->image->installEventFilter(this);
    //ui->image->setCursor(Qt::PointingHandCursor);
    //ui->image->setToolTip(QStringLiteral("Planetary"));
}

DialogAbout::~DialogAbout()
{
    delete ui;
}

bool DialogAbout::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->image &&
        event->type() == QEvent::MouseButtonDblClick) {
            triggerEasterEgg();
            return true;
    }

    return QDialog::eventFilter(watched, event);
}

void DialogAbout::triggerEasterEgg()
{
    ui->textAbout->setHtml(QString(
                               "<h2>Diagnostics:</h2>"
                               "<p>Qt: %1</p>"
                               "<p>Build: %2 %3</p>"
                               "<p>Caffeine level: Unhealthy</p>"
                               )
                               .arg(QStringLiteral(QT_VERSION_STR),
                                    QStringLiteral(__DATE__),
                                    QStringLiteral(__TIME__)));
}