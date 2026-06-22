#include "mainwindow.h"
#include "singleinstanceguard.h"
#include "version.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QTimer>
#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

static QString translationsPath()
{
#ifdef Q_OS_MAC
    QDir dir(QCoreApplication::applicationDirPath());

    // Planetary.app/Contents/MacOS -> Planetary.app/Contents/Resources/translations
    if (dir.dirName() == QStringLiteral("MacOS")) {
        dir.cdUp();

        if (dir.cd(QStringLiteral("Resources"))
            && dir.cd(QStringLiteral("translations"))) {
            return dir.absolutePath();
        }
    }
#endif

    // Normal Linux / Windows / local build layout:
    // beside the executable in ./translations
    QDir appDir(QCoreApplication::applicationDirPath());

    if (appDir.cd(QStringLiteral("translations")))
        return appDir.absolutePath();

    return QString();
}

static QLocale applicationLocale()
{
    const QByteArray forcedLocale = qgetenv("PLANETARY_LOCALE");

    if (!forcedLocale.isEmpty())
        return QLocale(QString::fromUtf8(forcedLocale));

    return QLocale::system();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    const QLocale locale = applicationLocale();

    QTranslator qtTranslator;
    const bool qtTranslationLoaded =
        qtTranslator.load(locale,
                          QStringLiteral("qtbase"),
                          QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath));

    if (qtTranslationLoaded)
        a.installTranslator(&qtTranslator);

    QTranslator appTranslator;
    const bool appTranslationLoaded =
        appTranslator.load(locale,
                           QStringLiteral("planetary"),
                           QStringLiteral("_"),
                           QStringLiteral(":/translations"));

    if (appTranslationLoaded)
        a.installTranslator(&appTranslator);

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
