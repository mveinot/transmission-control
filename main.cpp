#include "applicationappearance.h"
#include "applicationlocale.h"
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
#include <QLibraryInfo>

// The environment override remains useful for translation development and
// takes precedence over the persisted user preference.
static QLocale applicationLocale()
{
    const QString preference =
        QSettings().value(
            SettingsKeys::ApplicationLocale,
            QString::fromLatin1(ApplicationLocale::SystemDefault)).toString();
    return ApplicationLocale::resolve(preference, qgetenv("PLANETARY_LOCALE"));
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // QSettings identity must be established before resolving the saved
    // language preference or constructing any translated UI.
    QCoreApplication::setOrganizationName(QStringLiteral("mvgrafx"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("mvgrafx.net"));
    QCoreApplication::setApplicationName(QStringLiteral("Planetary"));
    QCoreApplication::setApplicationVersion(__PLANETARY_VERSION__);

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
