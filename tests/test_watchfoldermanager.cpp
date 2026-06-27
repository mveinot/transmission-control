#include <QtTest/QtTest>

#include "watchfoldermanager.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

class TestWatchFolderManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void retriesFailedPendingTorrentAndStopsAfterProcessed();
};

void TestWatchFolderManager::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("PlanetaryTests"));
    QCoreApplication::setApplicationName(QStringLiteral("WatchFolderManagerTest"));
}

void TestWatchFolderManager::retriesFailedPendingTorrentAndStopsAfterProcessed()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString torrentPath = tempDir.filePath(QStringLiteral("test.torrent"));

    QFile torrentFile(torrentPath);
    QVERIFY(torrentFile.open(QIODevice::WriteOnly));
    QVERIFY(torrentFile.write("d4:infod4:name4:teste4:piece20:aaaaaaaaaaaaaaaaaaaa6:lengthi0eee") > 0);
    torrentFile.close();

    WatchFolderManager manager;
    manager.clearProcessedHistory();
    manager.setWatchFolder(tempDir.path());
    manager.setScanIntervalMs(250);
    manager.setRequiredStableChecks(1);

    QSignalSpy readySpy(&manager, &WatchFolderManager::torrentFileReady);
    QVERIFY(readySpy.isValid());

    manager.setEnabled(true);

    QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 1, 2000);

    const QString emittedPath = readySpy.at(0).at(0).toString();
    QCOMPARE(QDir::cleanPath(emittedPath), QDir::cleanPath(torrentPath));
    QVERIFY(manager.hasPendingTorrentFile(torrentPath));

    manager.retryTorrentFile(torrentPath);
    QVERIFY(!manager.hasPendingTorrentFile(torrentPath));

    QTRY_COMPARE_WITH_TIMEOUT(readySpy.count(), 2, 3000);
    QVERIFY(manager.hasPendingTorrentFile(torrentPath));

    manager.markTorrentFileProcessed(torrentPath);
    QVERIFY(!manager.hasPendingTorrentFile(torrentPath));

    const int countAfterProcessed = readySpy.count();
    QTest::qWait(800);
    QCOMPARE(readySpy.count(), countAfterProcessed);
}

QTEST_MAIN(TestWatchFolderManager)
#include "test_watchfoldermanager.moc"
