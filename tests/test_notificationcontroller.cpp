#include <QtTest>

#include "notificationcontroller.h"
#include "settingskeys.h"
#include "torrent.h"

#include <QJsonObject>
#include <QSettings>

class TestNotificationController : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void completionTransitionUsesStatusEnum();
    void directAddSuppressesRefreshDuplicate();
    void disabledDirectAddStillSuppressesRefreshDuplicate();
};

namespace {

torrent makeTorrent(int id,
                    const QString &name,
                    double percentDone,
                    torrent::Status status,
                    bool stalled = false,
                    int error = 0,
                    const QString &errorString = {})
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("percentDone"), percentDone);
    object.insert(QStringLiteral("status"), static_cast<int>(status));
    object.insert(QStringLiteral("isStalled"), stalled);
    object.insert(QStringLiteral("error"), error);
    object.insert(QStringLiteral("errorString"), errorString);
    return torrent(object);
}

}

void TestNotificationController::init()
{
    QCoreApplication::setOrganizationName(QStringLiteral("PlanetaryTests"));
    QCoreApplication::setApplicationName(QStringLiteral("NotificationController"));
    QSettings().clear();
}

void TestNotificationController::completionTransitionUsesStatusEnum()
{
    QStringList titles;
    NotificationController controller(
        nullptr,
        [&titles](const QString &title, const QString &, int) {
            titles.append(title);
            return true;
        });

    controller.processTorrentList({
        makeTorrent(1, QStringLiteral("Example"), 0.5, torrent::Status::Downloading)
    });
    controller.processTorrentList({
        makeTorrent(1, QStringLiteral("Example"), 0.5, torrent::Status::Seeding)
    });

    QCOMPARE(titles, QStringList({QStringLiteral("Torrent finished")}));
}

void TestNotificationController::directAddSuppressesRefreshDuplicate()
{
    QStringList titles;
    NotificationController controller(
        nullptr,
        [&titles](const QString &title, const QString &, int) {
            titles.append(title);
            return true;
        });

    controller.processTorrentList({});
    controller.handleTorrentAdded(7, QStringLiteral("Example"));
    controller.processTorrentList({
        makeTorrent(7, QStringLiteral("Example"), 0.0, torrent::Status::Downloading)
    });

    QCOMPARE(titles.count(QStringLiteral("Torrent added")), 1);
}

void TestNotificationController::disabledDirectAddStillSuppressesRefreshDuplicate()
{
    QSettings().setValue(SettingsKeys::NotifyTorrentAdded, false);

    QStringList titles;
    NotificationController controller(
        nullptr,
        [&titles](const QString &title, const QString &, int) {
            titles.append(title);
            return true;
        });

    controller.processTorrentList({});
    controller.handleTorrentAdded(9, QStringLiteral("Example"));

    QSettings().setValue(SettingsKeys::NotifyTorrentAdded, true);
    controller.processTorrentList({
        makeTorrent(9, QStringLiteral("Example"), 0.0, torrent::Status::Downloading)
    });

    QVERIFY(titles.isEmpty());
}

QTEST_MAIN(TestNotificationController)
#include "test_notificationcontroller.moc"
