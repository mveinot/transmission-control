#include <QtTest/QtTest>

#include "torrentfiltercontroller.h"
#include "torrentmodel.h"
#include "torrentsortproxymodel.h"

#include <QAction>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>

namespace {

QJsonValue makeTorrentValue(int id,
                            const QString &name,
                            int status,
                            double percentDone,
                            const QStringList &trackerHosts,
                            const QString &downloadDir = QString())
{
    QJsonObject object;
    object["id"] = id;
    object["name"] = name;
    object["status"] = status;
    object["percentDone"] = percentDone;
    object["rateDownload"] = 0.0;
    object["rateUpload"] = 0.0;
    object["uploadRatio"] = 1.0;
    object["eta"] = 0;
    object["sizeWhenDone"] = 1024.0;
    object["queuePosition"] = id;
    object["downloadDir"] = downloadDir;

    QJsonArray trackerStats;
    for (const QString &host : trackerHosts) {
        QJsonObject tracker;
        tracker["host"] = host;
        trackerStats.append(tracker);
    }
    object["trackerStats"] = trackerStats;

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

QListWidgetItem *findItemByText(QListWidget &list, const QString &text)
{
    for (int row = 0; row < list.count(); ++row) {
        QListWidgetItem *item = list.item(row);
        if (item && item->text() == text)
            return item;
    }

    return nullptr;
}

void makeCheckable(QAction &action)
{
    action.setCheckable(true);
}

} // namespace

class TestTorrentFilterController : public QObject
{
    Q_OBJECT

private slots:
    void setupBuildsConsistentIconListAndTrackerSelection();
};

void TestTorrentFilterController::setupBuildsConsistentIconListAndTrackerSelection()
{
    QListWidget list;
    QLineEdit searchEdit;
    TorrentModel sourceModel;
    TorrentSortProxyModel proxy;
    proxy.setSourceModel(&sourceModel);

    QAction all;
    QAction downloading;
    QAction completed;
    QAction active;
    QAction inactive;
    QAction stopped;
    QAction error;

    for (QAction *action : { &all, &downloading, &completed, &active, &inactive, &stopped, &error })
        makeCheckable(*action);

    TorrentFilterController::Actions actions;
    actions.all = &all;
    actions.downloading = &downloading;
    actions.completed = &completed;
    actions.active = &active;
    actions.inactive = &inactive;
    actions.stopped = &stopped;
    actions.error = &error;

    TorrentFilterController controller(&list, &searchEdit, &proxy, actions);
    controller.setup();

    QVERIFY(findItemByText(list, QStringLiteral("All")) != nullptr);
    QVERIFY(!findItemByText(list, QStringLiteral("All"))->icon().isNull());
    QCOMPARE(proxy.stateFilter(), TorrentSortProxyModel::StateFilter::All);
    QVERIFY(proxy.trackerFilter().isEmpty());
    QVERIFY(proxy.downloadDirFilter().isEmpty());

    const QVector<torrent> torrents = makeTorrentList({
        makeTorrentValue(1, QStringLiteral("Ubuntu"), 4, 0.25,
                         { QStringLiteral("tracker.example.com") },
                         QStringLiteral("/downloads/linux")),
        makeTorrentValue(2, QStringLiteral("Debian"), 6, 1.0,
                         { QStringLiteral("other.example.com") },
                         QStringLiteral("/downloads/archive")),
    });

    sourceModel.applyUpdate(torrents);
    controller.rebuild(torrents);

    QListWidgetItem *trackerItem = findItemByText(list, QStringLiteral("tracker.example.com"));
    QVERIFY(trackerItem != nullptr);
    QVERIFY(!trackerItem->icon().isNull());

    list.setCurrentItem(trackerItem);
    QCOMPARE(proxy.stateFilter(), TorrentSortProxyModel::StateFilter::All);
    QCOMPARE(proxy.trackerFilter(), QStringLiteral("tracker.example.com"));
    QVERIFY(proxy.downloadDirFilter().isEmpty());
    QCOMPARE(proxy.rowCount(), 1);

    QListWidgetItem *folderItem = findItemByText(list, QStringLiteral("/downloads/archive"));
    QVERIFY(folderItem != nullptr);
    QVERIFY(!folderItem->icon().isNull());

    list.setCurrentItem(folderItem);
    QCOMPARE(proxy.stateFilter(), TorrentSortProxyModel::StateFilter::All);
    QVERIFY(proxy.trackerFilter().isEmpty());
    QCOMPARE(proxy.downloadDirFilter(), QStringLiteral("/downloads/archive"));
    QCOMPARE(proxy.rowCount(), 1);

    QListWidgetItem *completeItem = findItemByText(list, QStringLiteral("Complete"));
    QVERIFY(completeItem != nullptr);
    QVERIFY(!completeItem->icon().isNull());

    list.setCurrentItem(completeItem);
    QCOMPARE(proxy.stateFilter(), TorrentSortProxyModel::StateFilter::Completed);
    QVERIFY(proxy.trackerFilter().isEmpty());
    QVERIFY(proxy.downloadDirFilter().isEmpty());
    QCOMPARE(proxy.rowCount(), 1);
}

QTEST_MAIN(TestTorrentFilterController)
#include "test_torrentfiltercontroller.moc"
