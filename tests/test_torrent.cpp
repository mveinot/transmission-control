#include <QtTest/QtTest>

#include "torrent.h"

namespace {

QJsonValue makeTorrentValue(int id,
                            const QString &name,
                            int status,
                            double percentDone,
                            int queuePosition)
{
    QJsonObject object;
    object["id"] = id;
    object["name"] = name;
    object["status"] = status;
    object["percentDone"] = percentDone;
    object["rateDownload"] = 2048.0;
    object["rateUpload"] = 1024.0;
    object["uploadRatio"] = 1.5;
    object["eta"] = 125;
    object["sizeWhenDone"] = 4096.0;
    object["queuePosition"] = queuePosition;

    QJsonArray files;
    files.append(QJsonObject{{"name", "file-one.bin"}});
    object["files"] = files;

    QJsonArray peers;
    peers.append(QJsonObject{{"address", "192.0.2.10"}});
    object["peers"] = peers;

    return object;
}

} // namespace

class TestTorrent : public QObject
{
    Q_OBJECT

private slots:
    void parsesBasicFields();
    void mapsTransmissionStatuses();
    void sameDisplayDataIncludesQueuePosition();
};

void TestTorrent::parsesBasicFields()
{
    const torrent item(makeTorrentValue(42, "Ubuntu ISO", 4, 0.625, 7));

    QCOMPARE(item.getId(), 42);
    QCOMPARE(item.getName(), QStringLiteral("Ubuntu ISO"));
    QCOMPARE(item.getStatus(), QStringLiteral("Downloading"));
    QCOMPARE(item.getPercentDone(), 62.5);
    QCOMPARE(item.getSizeBytes(), qint64(4096));
    QCOMPARE(item.getQueuePosition(), 7);
    QCOMPARE(item.getFiles().size(), 1);
    QCOMPARE(item.getPeers().size(), 1);
}

void TestTorrent::mapsTransmissionStatuses()
{
    const QVector<QPair<int, QString>> cases = {
        {0, QStringLiteral("Paused")},
        {1, QStringLiteral("Waiting to Verify")},
        {2, QStringLiteral("Verifying")},
        {3, QStringLiteral("Queued")},
        {4, QStringLiteral("Downloading")},
        {5, QStringLiteral("Waiting to Seed")},
        {6, QStringLiteral("Seeding")},
        {999, QStringLiteral("Unknown")},
    };

    for (const auto &testCase : cases) {
        const torrent item(makeTorrentValue(1, "Test", testCase.first, 0.0, 0));
        QCOMPARE(item.getStatus(), testCase.second);
    }
}

void TestTorrent::sameDisplayDataIncludesQueuePosition()
{
    const torrent first(makeTorrentValue(1, "Same", 4, 0.5, 1));
    const torrent second(makeTorrentValue(1, "Same", 4, 0.5, 2));

    QVERIFY(!first.sameDisplayData(second));
}

QTEST_MAIN(TestTorrent)
#include "test_torrent.moc"
