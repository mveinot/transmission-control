#include "applicationappearance.h"
#include "mainwindow.h"
#include "settingskeys.h"
#include "singleinstanceguard.h"
#include "version.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QSettings>
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

// if the user launched with a locale override use that, otherwise system locale
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

    // determine the language to load
    const QLocale locale = applicationLocale();

    // QT's built-in strings translator
    QTranslator qtTranslator;
    const bool qtTranslationLoaded =
        qtTranslator.load(locale,
                          QStringLiteral("qtbase"),
                          QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath));

    if (qtTranslationLoaded)
        a.installTranslator(&qtTranslator);

    // App string translator
    QTranslator appTranslator;
    const bool appTranslationLoaded =
        appTranslator.load(locale,
                           QStringLiteral("planetary"),
                           QStringLiteral("_"),
                           QStringLiteral(":/translations"));

    if (appTranslationLoaded)
        a.installTranslator(&appTranslator);

    // application details
    QCoreApplication::setOrganizationName("mvgrafx");
    QCoreApplication::setOrganizationDomain("mvgrafx.net");
    QCoreApplication::setApplicationName(QString("Planetary"));
    QCoreApplication::setApplicationVersion(__PLANETARY_VERSION__);

    // Apply the saved override before constructing any application windows.
    QSettings settings;
    ApplicationAppearance::apply(
        settings.value(SettingsKeys::Appearance,
                       QString::fromLatin1(ApplicationAppearance::FollowSystem)).toString());

    a.setWindowIcon(QIcon(":/icons/planetary.icns"));

    QApplication::setQuitOnLastWindowClosed(false);

    // some magic to ensure the app only runs one instance at a time,
    // but new launches will send any relevant data to the running instance
    const QString instanceServerName =
        QStringLiteral("com.mvgrafx.Planetary.singleInstance");

    SingleInstanceGuard instanceGuard(instanceServerName);

    const QStringList launchArguments =
        QCoreApplication::arguments().mid(1);

    if (!instanceGuard.tryStartPrimaryInstance()) {
        instanceGuard.notifyPrimaryInstance(launchArguments);
        return 0;
    }

    // start the main app window
    MainWindow w;
    a.installEventFilter(&w);

    QObject::connect(&instanceGuard, &SingleInstanceGuard::activationRequested,
                     &w, &MainWindow::bringToFront);

    QObject::connect(&instanceGuard, &SingleInstanceGuard::openRequested,
                     &w, &MainWindow::handleLaunchArguments);

    w.show();

    // First-run setup is presented only after the main window is visible.
    // Launch-time magnet/file arguments remain pending until a server exists.
    QTimer::singleShot(0, &w, [&w, launchArguments]() {
        const bool forceWizard =
            QApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier);
        w.runFirstTimeServerSetup(launchArguments, forceWizard);
    });

    return a.exec();
}
