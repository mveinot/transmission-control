#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonObject>

#include "torrentmodel.h"
#include "torrentsortproxymodel.h"

namespace {

QJsonValue makeTorrentValue(int id,
                            const QString &name,
                            int status,
                            double percentDone,
                            double rateDownload,
                            qint64 sizeWhenDone,
                            int queuePosition)
{
    QJsonObject object;
    object["id"] = id;
    object["name"] = name;
    object["status"] = status;
    object["percentDone"] = percentDone;
    object["rateDownload"] = rateDownload;
    object["rateUpload"] = 0.0;
    object["uploadRatio"] = 1.0;
    object["eta"] = 0;
    object["sizeWhenDone"] = static_cast<double>(sizeWhenDone);
    object["queuePosition"] = queuePosition;
    return object;
}


QJsonValue makeTorrentValueWithTrackers(int id,
                                        const QString &name,
                                        int status,
                                        double percentDone,
                                        const QStringList &trackerHosts)
{
    QJsonObject object = makeTorrentValue(id,
                                          name,
                                          status,
                                          percentDone,
                                          0.0,
                                          1024,
                                          id).toObject();

    QJsonArray trackerStats;
    for (const QString &host : trackerHosts) {
        QJsonObject tracker;
        tracker[QStringLiteral("host")] = host;
        trackerStats.append(tracker);
    }

    object[QStringLiteral("trackerStats")] = trackerStats;
    return object;
}

QVector<torrent> makeTorrentList(std::initializer_list<QJsonValue> values)
{
    QVector<torrent> result;
    result.reserve(static_cast<int>(values.size()));

    for (const QJsonValue &value : values)
        result.append(torrent(value));

    return result;
}

int torrentIdAtProxyRow(const TorrentSortProxyModel &proxy, int row)
{
    return proxy.index(row, TorrentModel::IdColumn).data(Qt::UserRole).toInt();
}

} // namespace

class TestTorrentSortProxyModel : public QObject
{
    Q_OBJECT

private slots:
    void sortsByQueuePosition();
    void separatesDownloadingAndWaitingTorrents();
    void filtersCompletedTorrents();
    void filtersActiveTorrents();
    void filtersInactiveTorrents();
    void filtersErroredTorrents();
    void filtersByTrackerHost();
    void filtersByDownloadDir();
    void combinesStateAndTrackerFilters();
};

void TestTorrentSortProxyModel::separatesDownloadingAndWaitingTorrents()
{
    TorrentModel sourceModel;
    sourceModel.applyUpdate(makeTorrentList({
        makeTorrentValue(1, "Downloading", 4, 0.25, 0.0, 1024, 0),
        makeTorrentValue(2, "Queued", 3, 0.25, 0.0, 1024, 1),
    }));

    TorrentSortProxyModel proxy;
    proxy.setSourceModel(&sourceModel);
    proxy.setStateFilter(TorrentSortProxyModel::StateFilter::Downloading);

    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(torrentIdAtProxyRow(proxy, 0), 1);

    proxy.setStateFilter(TorrentSortProxyModel::StateFilter::Waiting);

    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(torrentIdAtProxyRow(proxy, 0), 2);
}

void TestTorrentSortProxyModel::sortsByQueuePosition()
{
    TorrentModel sourceModel;
    sourceModel.applyUpdate(makeTorrentList({
        makeTorrentValue(1, "Last", 4, 0.2, 0.0, 1024, 30),
        makeTorrentValue(2, "First", 4, 0.2, 0.0, 1024, 10),
        makeTorrentValue(3, "Middle", 4, 0.2, 0.0, 1024, 20),
    }));

    TorrentSortProxyModel proxy;
    proxy.setSourceModel(&sourceModel);
    proxy.sort(TorrentModel::QueueColumn, Qt::AscendingOrder);

    QCOMPARE(proxy.rowCount(), 3);
    QCOMPARE(torrentIdAtProxyRow(proxy, 0), 2);
    QCOMPARE(torrentIdAtProxyRow(proxy, 1), 3);
    QCOMPARE(torrentIdAtProxyRow(proxy, 2), 1);
}

void TestTorrentSortProxyModel::filtersCompletedTorrents()
{
    TorrentModel sourceModel;
    sourceModel.applyUpdate(makeTorrentList({
        makeTorrentValue(1, "Partial", 4, 0.999, 0.0, 1024, 0),
        makeTorrentValue(2, "Complete", 6, 1.0, 0.0, 1024, 1),
    }));

    TorrentSortProxyModel proxy;
    proxy.setSourceModel(&sourceModel);
    proxy.setStateFilter(TorrentSortProxyModel::StateFilter::Completed);

    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(torrentIdAtProxyRow(proxy, 0), 2);
}

void TestTorrentSortProxyModel::filtersActiveTorrents()
{
    TorrentModel sourceModel;
    sourceModel.applyUpdate(makeTorrentList({
        makeTorrentValue(1, "Downloading", 4, 0.25, 0.0, 1024, 0),
        makeTorrentValue(2, "Seeding", 6, 1.0, 0.0, 1024, 1),
        makeTorrentValue(3, "Verifying", 2, 0.5, 0.0, 1024, 2),
        makeTorrentValue(4, "Paused", 0, 0.5, 0.0, 1024, 3),
    }));

    TorrentSortProxyModel proxy;
    proxy.setSourceModel(&sourceModel);
    proxy.setStateFilter(TorrentSortProxyModel::StateFilter::Active);

    QCOMPARE(proxy.rowCount(), 3);
}

void TestTorrentSortProxyModel::filtersInactiveTorrents()
{
    TorrentModel sourceModel;
    sourceModel.applyUpdate(makeTorrentList({
        makeTorrentValue(1, "Paused", 0, 0.25, 0.0, 1024, 0),
        makeTorrentValue(2, "Queued", 3, 0.25, 0.0, 1024, 1),
        makeTorrentValue(3, "Waiting Verify", 1, 0.25, 0.0, 1024, 2),
        makeTorrentValue(4, "Waiting Seed", 5, 1.0, 0.0, 1024, 3),
        makeTorrentValue(5, "Downloading", 4, 0.25, 0.0, 1024, 4),
    }));

    TorrentSortProxyModel proxy;
    proxy.setSourceModel(&sourceModel);
    proxy.setStateFilter(TorrentSortProxyModel::StateFilter::Inactive);

    QCOMPARE(proxy.rowCount(), 4);
}

void TestTorrentSortProxyModel::filtersErroredTorrents()
{
    TorrentModel sourceModel;

    QJsonObject pausedErrored = makeTorrentValue(1, "Paused with error", 0, 0.25, 0.0, 1024, 0).toObject();
    pausedErrored[QStringLiteral("error")] = 3;
    pausedErrored[QStringLiteral("errorString")] = QStringLiteral("Permission denied");

    QJsonObject downloadingOk = makeTorrentValue(2, "Fine", 4, 0.25, 0.0, 1024, 1).toObject();

    sourceModel.applyUpdate(makeTorrentList({ pausedErrored, downloadingOk }));

    TorrentSortProxyModel proxy;
    proxy.setSourceModel(&sourceModel);
    proxy.setStateFilter(TorrentSortProxyModel::StateFilter::Error);

    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(torrentIdAtProxyRow(proxy, 0), 1);

    proxy.setStateFilter(TorrentSortProxyModel::StateFilter::Stopped);

    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(torrentIdAtProxyRow(proxy, 0), 1);
}

void TestTorrentSortProxyModel::filtersByTrackerHost()
{
    TorrentModel sourceModel;
    sourceModel.applyUpdate(makeTorrentList({
        makeTorrentValueWithTrackers(1, "Ubuntu", 4, 0.25, { QStringLiteral("tracker.example.com") }),
        makeTorrentValueWithTrackers(2, "Debian", 4, 0.25, { QStringLiteral("other.example.com") }),
        makeTorrentValueWithTrackers(3, "Fedora", 4, 0.25, { QStringLiteral("TRACKER.EXAMPLE.COM:6969") }),
    }));

    TorrentSortProxyModel proxy;
    proxy.setSourceModel(&sourceModel);
    proxy.setTrackerFilter(QStringLiteral("tracker.example.com"));

    QCOMPARE(proxy.rowCount(), 2);
    QCOMPARE(torrentIdAtProxyRow(proxy, 0), 1);
    QCOMPARE(torrentIdAtProxyRow(proxy, 1), 3);
}


void TestTorrentSortProxyModel::filtersByDownloadDir()
{
    TorrentModel sourceModel;

    QJsonObject one = makeTorrentValue(1, "Ubuntu", 4, 0.25, 0.0, 1024, 0).toObject();
    one[QStringLiteral("downloadDir")] = QStringLiteral("/downloads/linux");

    QJsonObject two = makeTorrentValue(2, "Movie", 4, 0.25, 0.0, 1024, 1).toObject();
    two[QStringLiteral("downloadDir")] = QStringLiteral("/downloads/media");

    QJsonObject three = makeTorrentValue(3, "Fedora", 4, 0.25, 0.0, 1024, 2).toObject();
    three[QStringLiteral("downloadDir")] = QStringLiteral("/downloads/linux");

    sourceModel.applyUpdate(makeTorrentList({ one, two, three }));

    TorrentSortProxyModel proxy;
    proxy.setSourceModel(&sourceModel);
    proxy.setDownloadDirFilter(QStringLiteral("/downloads/linux"));

    QCOMPARE(proxy.rowCount(), 2);
    QCOMPARE(torrentIdAtProxyRow(proxy, 0), 1);
    QCOMPARE(torrentIdAtProxyRow(proxy, 1), 3);
}

void TestTorrentSortProxyModel::combinesStateAndTrackerFilters()
{
    TorrentModel sourceModel;
    sourceModel.applyUpdate(makeTorrentList({
        makeTorrentValueWithTrackers(1, "Partial Matching Tracker", 4, 0.25, { QStringLiteral("tracker.example.com") }),
        makeTorrentValueWithTrackers(2, "Complete Matching Tracker", 6, 1.0, { QStringLiteral("tracker.example.com") }),
        makeTorrentValueWithTrackers(3, "Complete Other Tracker", 6, 1.0, { QStringLiteral("other.example.com") }),
    }));

    TorrentSortProxyModel proxy;
    proxy.setSourceModel(&sourceModel);
    proxy.setStateFilter(TorrentSortProxyModel::StateFilter::Completed);
    proxy.setTrackerFilter(QStringLiteral("tracker.example.com"));

    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(torrentIdAtProxyRow(proxy, 0), 2);
}

QTEST_MAIN(TestTorrentSortProxyModel)
#include "test_torrentsortproxymodel.moc"
