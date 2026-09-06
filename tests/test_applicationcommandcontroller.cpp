#include <QtTest>

#include "applicationcommandcontroller.h"
#include "iconthememanager.h"
#include "iconthemeregistry.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>

class TestApplicationCommandController : public QObject
{
    Q_OBJECT

private slots:
    void routesCommandsOnce();
    void reconcilesBackendAndSessionState();
    void iconThemesProvideDistinctArtwork();
    void registeredThemeCanReplaceItsArtworkLive();
};

namespace {

struct CommandFixture
{
    QMainWindow window;
    QMenu transfers {QStringLiteral("Transfers"), &window};
    QMenu help {QStringLiteral("Help"), &window};
    QAction openTorrent {&window};
    QAction addMagnet {&window};
    QAction startSelected {&window};
    QAction stopSelected {&window};
    QAction startAll {&window};
    QAction stopAll {&window};
    QAction forceStart {&window};
    QAction verify {&window};
    QAction reannounce {&window};
    QAction deleteTorrent {&window};
    QAction queueTop {&window};
    QAction queueUp {&window};
    QAction queueDown {&window};
    QAction queueBottom {&window};
    QAction closeWindow {&window};
    QAction applicationSettings {&window};
    QAction manageServers {&window};
    QAction serverSettings {&window};
    QAction alternativeSpeed {&window};
    QAction statistics {&window};
    QAction about {&window};
    QAction quit {&window};
    QAction checkForUpdates {&window};
    QAction exportSettings {&window};
    QAction importSettings {&window};

    CommandFixture()
    {
        window.menuBar()->addMenu(&transfers);
        window.menuBar()->addMenu(&help);
        help.addAction(&checkForUpdates);
        help.addAction(&about);
        alternativeSpeed.setCheckable(true);
    }

    ApplicationCommandController::Actions actions()
    {
        return {
            &openTorrent,
            &addMagnet,
            &startSelected,
            &stopSelected,
            &startAll,
            &stopAll,
            &forceStart,
            &verify,
            &reannounce,
            &deleteTorrent,
            &queueTop,
            &queueUp,
            &queueDown,
            &queueBottom,
            &closeWindow,
            &applicationSettings,
            &manageServers,
            &serverSettings,
            &alternativeSpeed,
            &statistics,
            &about,
            &quit,
            &checkForUpdates,
            &exportSettings,
            &importSettings
        };
    }

    ApplicationCommandController::Menus menus()
    {
        return {window.menuBar(), &transfers, &help};
    }
};

} // namespace

void TestApplicationCommandController::routesCommandsOnce()
{
    CommandFixture fixture;
    int addRequests = 0;
    int startRequests = 0;
    int settingsRequests = 0;
    int exportRequests = 0;
    int importRequests = 0;

    ApplicationCommandController::Handlers handlers;
    handlers.openTorrent = [&]() { ++addRequests; };
    handlers.startSelected = [&]() { ++startRequests; };
    handlers.applicationSettings = [&]() { ++settingsRequests; };
    handlers.exportSettings = [&]() { ++exportRequests; };
    handlers.importSettings = [&]() { ++importRequests; };

    ApplicationCommandController controller(
        &fixture.window,
        fixture.actions(),
        fixture.menus(),
        handlers);
    controller.setup();

    fixture.openTorrent.trigger();
    fixture.startSelected.trigger();
    fixture.applicationSettings.trigger();
    fixture.exportSettings.trigger();
    fixture.importSettings.trigger();

    QCOMPARE(addRequests, 1);
    QCOMPARE(startRequests, 1);
    QCOMPARE(settingsRequests, 1);
    QCOMPARE(exportRequests, 1);
    QCOMPARE(importRequests, 1);
    QCOMPARE(fixture.openTorrent.shortcut(), QKeySequence::Open);
    QVERIFY(!fixture.openTorrent.icon().isNull());
    QCOMPARE(fixture.applicationSettings.text(), QStringLiteral("Settings…"));
#ifdef Q_OS_MACOS
    QCOMPARE(fixture.applicationSettings.menuRole(),
             QAction::ApplicationSpecificRole);
    QCOMPARE(fixture.applicationSettings.shortcut(),
             QKeySequence::Preferences);
#endif
    QCOMPARE(fixture.window.menuBar()->actions().at(0)->text(),
             QStringLiteral("Edit"));
}

void TestApplicationCommandController::reconcilesBackendAndSessionState()
{
    CommandFixture fixture;
    int alternativeSpeedRequests = 0;
    bool requestedState = false;
    ApplicationCommandController::Handlers handlers;
    handlers.alternativeSpeed = [&](bool enabled) {
        ++alternativeSpeedRequests;
        requestedState = enabled;
    };

    ApplicationCommandController controller(
        &fixture.window,
        fixture.actions(),
        fixture.menus(),
        handlers);
    controller.setup();

    TorrentBackendCapabilities capabilities;
    controller.setBackendState(QStringLiteral("qBittorrent"), capabilities);
    QVERIFY(!fixture.serverSettings.isEnabled());
    QVERIFY(!fixture.statistics.isVisible());

    capabilities.sessionSettings = true;
    capabilities.sessionStatistics = true;
    controller.setBackendState(QStringLiteral("qBittorrent"), capabilities);
    QCOMPARE(fixture.serverSettings.text(),
             QStringLiteral("qBittorrent Settings..."));
    QVERIFY(fixture.serverSettings.isEnabled());
    QVERIFY(fixture.statistics.isVisible());

    controller.setAlternativeSpeedState(true, true);
    QCOMPARE(alternativeSpeedRequests, 0);
    QVERIFY(fixture.alternativeSpeed.isChecked());
    QVERIFY(fixture.alternativeSpeed.isEnabled());

    fixture.alternativeSpeed.trigger();
    QCOMPARE(alternativeSpeedRequests, 1);
    QVERIFY(!requestedState);
}

void TestApplicationCommandController::iconThemesProvideDistinctArtwork()
{
    auto &icons = AppIcons::IconThemeManager::instance();
    const QString originalTheme = icons.themeId();
    icons.setThemeId(QString::fromLatin1(AppIcons::GlassTheme));

    CommandFixture fixture;
    ApplicationCommandController controller(
        &fixture.window,
        fixture.actions(),
        fixture.menus(),
        {});
    controller.setup();

    const QImage glassAction = fixture.startSelected.icon()
                                   .pixmap(128, 128).toImage();
    QSignalSpy themeChangedSpy(&icons, &AppIcons::IconThemeManager::themeChanged);
    icons.setThemeId(QString::fromLatin1(AppIcons::ClassicTheme));

    const QIcon classic = icons.icon(
        AppIcons::Id::ActionStart,
        QString::fromLatin1(AppIcons::ClassicTheme));
    const QIcon glass = icons.icon(
        AppIcons::Id::ActionStart,
        QString::fromLatin1(AppIcons::GlassTheme));

    QVERIFY(!classic.isNull());
    QVERIFY(!glass.isNull());
    QVERIFY(classic.pixmap(128, 128).toImage()
            != glass.pixmap(128, 128).toImage());
    QCOMPARE(themeChangedSpy.count(), 1);
    QCOMPARE(fixture.startSelected.property("planetaryIconId").toInt(),
             static_cast<int>(AppIcons::Id::ActionStart));
    QVERIFY(fixture.startSelected.icon().pixmap(128, 128).toImage()
            != glassAction);
    QCOMPARE(AppIcons::IconThemeRegistry::instance()
                 .resolvedThemeId(QStringLiteral("unknown")),
             QString::fromLatin1(AppIcons::GlassTheme));

    icons.setThemeId(originalTheme);
}

void TestApplicationCommandController::registeredThemeCanReplaceItsArtworkLive()
{
    auto &registry = AppIcons::IconThemeRegistry::instance();
    auto &icons = AppIcons::IconThemeManager::instance();
    const QString originalTheme = icons.themeId();
    const QString customThemeId = QStringLiteral("test-external");
    const AppIcons::IconTheme::IconFiles files {
        {AppIcons::Id::ActionStart, QStringLiteral("action-start.png")}
    };

    QVERIFY(registry.registerTheme(AppIcons::IconTheme(
        customThemeId,
        QStringLiteral("Test External"),
        QStringLiteral(":/icons/ui/classic"),
        files,
        QString::fromLatin1(AppIcons::GlassTheme))));
    QVERIFY(registry.contains(customThemeId));

    QAction action;
    icons.setThemeId(customThemeId);
    icons.bindAction(&action, AppIcons::Id::ActionStart);
    const QImage classicArtwork = action.icon().pixmap(128, 128).toImage();

    // An omitted semantic icon falls back to the theme's declared fallback.
    QCOMPARE(icons.icon(AppIcons::Id::ActionStop).pixmap(128, 128).toImage(),
             icons.icon(AppIcons::Id::ActionStop,
                        QString::fromLatin1(AppIcons::GlassTheme))
                 .pixmap(128, 128).toImage());

    QSignalSpy themeChangedSpy(&icons, &AppIcons::IconThemeManager::themeChanged);
    QVERIFY(registry.registerTheme(AppIcons::IconTheme(
        customThemeId,
        QStringLiteral("Test External"),
        QStringLiteral(":/icons/ui/glass"),
        files,
        QString::fromLatin1(AppIcons::GlassTheme))));

    QCOMPARE(themeChangedSpy.count(), 1);
    QVERIFY(action.icon().pixmap(128, 128).toImage() != classicArtwork);

    QVERIFY(registry.unregisterTheme(customThemeId));
    QCOMPARE(icons.themeId(), QString::fromLatin1(AppIcons::GlassTheme));
    QCOMPARE(themeChangedSpy.count(), 2);
    icons.setThemeId(originalTheme);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication application(argc, argv);
    TestApplicationCommandController test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_applicationcommandcontroller.moc"
