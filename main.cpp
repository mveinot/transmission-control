#include "mainwindow.h"
#include "singleinstanceguard.h"
#include "version.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("mvgrafx");
    QCoreApplication::setOrganizationDomain("mvgrafx.net");
    QCoreApplication::setApplicationName(QString("Planetary"));
    QCoreApplication::setApplicationVersion(__PLANETARY_VERSION__);

    a.setWindowIcon(QIcon(":/icons/planetary.icns"));
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "transmission-control_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    QApplication::setQuitOnLastWindowClosed(false);

    const QString instanceServerName =
        QStringLiteral("com.mvgrafx.Planetary.singleInstance");

    SingleInstanceGuard instanceGuard(instanceServerName);

    if (!instanceGuard.tryStartPrimaryInstance()) {
        instanceGuard.notifyPrimaryInstance();
        return 0;
    }

    MainWindow w;

    QObject::connect(&instanceGuard, &SingleInstanceGuard::activationRequested,
                     &w, &MainWindow::bringToFront);

    w.show();

    return a.exec();
}
