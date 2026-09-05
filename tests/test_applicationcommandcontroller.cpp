#include <QtTest>

#include "applicationcommandcontroller.h"
#include "appicons.h"

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
    const QIcon classic = AppIcons::icon(
        AppIcons::Icon::ActionStart,
        QString::fromLatin1(AppIcons::ClassicTheme));
    const QIcon glass = AppIcons::icon(
        AppIcons::Icon::ActionStart,
        QString::fromLatin1(AppIcons::GlassTheme));

    QVERIFY(!classic.isNull());
    QVERIFY(!glass.isNull());
    QVERIFY(classic.pixmap(128, 128).toImage()
            != glass.pixmap(128, 128).toImage());
    QCOMPARE(AppIcons::normalizedThemeId(QStringLiteral("unknown")),
             QString::fromLatin1(AppIcons::GlassTheme));
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication application(argc, argv);
    TestApplicationCommandController test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_applicationcommandcontroller.moc"
