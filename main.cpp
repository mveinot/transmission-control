#include "mainwindow.h"
#include "singleinstanceguard.h"
#include "version.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QTimer>

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

    const QStringList launchArguments =
        QCoreApplication::arguments().mid(1);

    if (!instanceGuard.tryStartPrimaryInstance()) {
        instanceGuard.notifyPrimaryInstance(launchArguments);
        return 0;
    }

    MainWindow w;

    a.installEventFilter(&w);

    QObject::connect(&instanceGuard, &SingleInstanceGuard::activationRequested,
                     &w, &MainWindow::bringToFront);

    QObject::connect(&instanceGuard, &SingleInstanceGuard::openRequested,
                     &w, &MainWindow::handleLaunchArguments);

    w.show();

    if (!launchArguments.isEmpty()) {
        QTimer::singleShot(0, &w, [&w, launchArguments]() {
            w.handleLaunchArguments(launchArguments);
        });
    }

    return a.exec();
}
