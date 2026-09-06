#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QBrush>
#include <QFont>
#include <QIcon>

#include "torrentmodel.h"
#include "appicons.h"

namespace {

QJsonValue makeTorrentValue(int id,
                            const QString &name,
                            int status,
                            double percentDone,
                            qint64 sizeWhenDone,
                            int queuePosition)
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
    object["sizeWhenDone"] = static_cast<double>(sizeWhenDone);
    object["queuePosition"] = queuePosition;
    object["addedDate"] = 1710000000.0;
    object["downloadedEver"] = 4096.0;
    object["uploadedEver"] = 8192.0;
    object["downloadDir"] = "/downloads/linux";
    object["leftUntilDone"] = static_cast<double>(sizeWhenDone * (1.0 - percentDone));
    object["desiredAvailable"] = static_cast<double>(sizeWhenDone * (1.0 - percentDone));
    object["peersConnected"] = 7;
    object["peersSendingToUs"] = 2;
    object["peersGettingFromUs"] = 3;

    QJsonArray trackerStats;
    QJsonObject tracker;
    tracker["host"] = "tracker.example.com:6969";
    tracker["seederCount"] = 9;
    tracker["leecherCount"] = 11;
    trackerStats.append(tracker);
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

} // namespace

class TestTorrentModel : public QObject
{
    Q_OBJECT

private slots:
    void exposesQueueColumn();
    void exposesOptionalColumns();
    void exposesTorrentErrorState();
    void verificationOverridesErrorPresentationAndUsesCheckProgress();
    void indexesRowsByTorrentKey();
    void queuePositionChangeEmitsDataChanged();
    void removesMissingRowsOnUpdate();
    void themeChangeRefreshesDecorations();
};

void TestTorrentModel::themeChangeRefreshesDecorations()
{
    auto &icons = AppIcons::IconManager::instance();
    const QString originalTheme = icons.themeId();
    icons.setThemeId(QString::fromLatin1(AppIcons::GlassTheme));

    TorrentModel model;
    model.applyUpdate(makeTorrentList({
        makeTorrentValue(10, "One", 4, 0.25, 1024, 3),
    }));
    const QModelIndex status = model.index(0, TorrentModel::StatusColumn);
    const QImage glass = status.data(Qt::DecorationRole).value<QIcon>()
                             .pixmap(128, 128).toImage();
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);

    icons.setThemeId(QString::fromLatin1(AppIcons::ClassicTheme));

    QCOMPARE(changedSpy.count(), 2);
    QVERIFY(status.data(Qt::DecorationRole).value<QIcon>()
                .pixmap(128, 128).toImage() != glass);

    icons.setThemeId(originalTheme);
}

void TestTorrentModel::exposesQueueColumn()
{
    TorrentModel model;

    QCOMPARE(model.columnCount(), int(TorrentModel::ColumnCount));
    QVERIFY(TorrentModel::QueueColumn < TorrentModel::ColumnCount);
    QCOMPARE(model.headerData(TorrentModel::QueueColumn,
                              Qt::Horizontal,
                              Qt::DisplayRole).toString(),
             QStringLiteral("Queue"));

    model.applyUpdate(makeTorrentList({
        makeTorrentValue(10, "One", 4, 0.25, 1024, 3),
    }));

    const QModelIndex queueIndex = model.index(0, TorrentModel::QueueColumn);
    QCOMPARE(model.data(queueIndex, Qt::DisplayRole).toInt(), 3);
    QCOMPARE(model.data(queueIndex, Qt::UserRole + 1).toInt(), 3);
}

void TestTorrentModel::verificationOverridesErrorPresentationAndUsesCheckProgress()
{
    TorrentModel model;
    QJsonObject checking =
        makeTorrentValue(10, "Checking", 2, 1.0, 1024, 3).toObject();
    checking.insert(QStringLiteral("error"), 3);
    checking.insert(QStringLiteral("errorString"),
                    QStringLiteral("No data found"));
    checking.insert(QStringLiteral("recheckProgress"), 0.425);
    model.applyUpdate(makeTorrentList({checking}));

    const QModelIndex nameIndex = model.index(0, TorrentModel::NameColumn);
    const QModelIndex statusIndex = model.index(0, TorrentModel::StatusColumn);
    const QModelIndex completedIndex =
        model.index(0, TorrentModel::PercentDoneColumn);

    QCOMPARE(model.data(statusIndex, Qt::DisplayRole).toString(),
             QStringLiteral("Verifying"));
    QVERIFY(model.data(nameIndex, TorrentModel::HasErrorRole).toBool());
    QCOMPARE(model.data(completedIndex, Qt::DisplayRole).toString(),
             QStringLiteral("42.5%"));
    QCOMPARE(model.data(completedIndex, TorrentModel::SortRole).toDouble(),
             42.5);
    QCOMPARE(model.data(completedIndex,
                        TorrentModel::DownloadCompletionRole).toDouble(),
             100.0);
    QVERIFY(model.data(statusIndex, Qt::ToolTipRole).toString().contains(
        QStringLiteral("Previous error: No data found")));
    QVERIFY(!model.data(statusIndex, Qt::FontRole).isValid());
    QVERIFY(!model.data(statusIndex, Qt::ForegroundRole).isValid());
}


void TestTorrentModel::exposesOptionalColumns()
{
    TorrentModel model;

    model.applyUpdate(makeTorrentList({
        makeTorrentValue(10, "One", 4, 0.25, 1024, 3),
    }));

    QCOMPARE(model.headerData(TorrentModel::HealthColumn,
                              Qt::Horizontal,
                              Qt::DisplayRole).toString(),
             QStringLiteral("Health"));
    QCOMPARE(model.headerData(TorrentModel::TrackerColumn,
                              Qt::Horizontal,
                              Qt::DisplayRole).toString(),
             QStringLiteral("Tracker"));
    QCOMPARE(model.headerData(TorrentModel::AddedColumn,
                              Qt::Horizontal,
                              Qt::DisplayRole).toString(),
             QStringLiteral("Added"));
    QCOMPARE(model.headerData(TorrentModel::DownloadedEverColumn,
                              Qt::Horizontal,
                              Qt::DisplayRole).toString(),
             QStringLiteral("Downloaded"));
    QCOMPARE(model.headerData(TorrentModel::UploadedEverColumn,
                              Qt::Horizontal,
                              Qt::DisplayRole).toString(),
             QStringLiteral("Uploaded"));
    QCOMPARE(model.headerData(TorrentModel::DownloadDirColumn,
                              Qt::Horizontal,
                              Qt::DisplayRole).toString(),
             QStringLiteral("Download Folder"));
    QCOMPARE(model.headerData(TorrentModel::SeedsColumn,
                              Qt::Horizontal,
                              Qt::DisplayRole).toString(),
             QStringLiteral("Seeds"));
    QCOMPARE(model.headerData(TorrentModel::PeersConnectedColumn,
                              Qt::Horizontal,
                              Qt::DisplayRole).toString(),
             QStringLiteral("Peers"));

    QCOMPARE(model.data(model.index(0, TorrentModel::HealthColumn), Qt::DisplayRole).toString(),
             QStringLiteral("Excellent"));
    QVERIFY(model.data(model.index(0, TorrentModel::HealthColumn), TorrentModel::SortRole).toInt() >= 85);
    QVERIFY(model.data(model.index(0, TorrentModel::HealthColumn), Qt::ToolTipRole).toString().contains(QStringLiteral("Health score")));
    QCOMPARE(model.data(model.index(0, TorrentModel::TrackerColumn), Qt::DisplayRole).toString(),
             QStringLiteral("tracker.example.com"));
    QCOMPARE(model.data(model.index(0, TorrentModel::AddedColumn), TorrentModel::SortRole).toLongLong(),
             qint64(1710000000));
    QCOMPARE(model.data(model.index(0, TorrentModel::DownloadedEverColumn), TorrentModel::SortRole).toLongLong(),
             qint64(4096));
    QCOMPARE(model.data(model.index(0, TorrentModel::UploadedEverColumn), TorrentModel::SortRole).toLongLong(),
             qint64(8192));
    QCOMPARE(model.data(model.index(0, TorrentModel::DownloadDirColumn), Qt::DisplayRole).toString(),
             QStringLiteral("/downloads/linux"));
    QCOMPARE(model.data(model.index(0, TorrentModel::DownloadDirColumn), TorrentModel::SortRole).toString(),
             QStringLiteral("/downloads/linux"));
    QCOMPARE(model.data(model.index(0, TorrentModel::NameColumn), TorrentModel::DownloadDirRole).toString(),
             QStringLiteral("/downloads/linux"));
    QCOMPARE(model.data(model.index(0, TorrentModel::SeedsColumn), Qt::DisplayRole).toString(),
             QStringLiteral("2/9"));
    QCOMPARE(model.data(model.index(0, TorrentModel::PeersConnectedColumn), Qt::DisplayRole).toString(),
             QStringLiteral("3/11"));
    QVERIFY(model.data(model.index(0, TorrentModel::SeedsColumn), TorrentModel::SortRole).toLongLong() > 0);
    QVERIFY(model.data(model.index(0, TorrentModel::PeersConnectedColumn), TorrentModel::SortRole).toLongLong() > 0);
}

void TestTorrentModel::exposesTorrentErrorState()
{
    TorrentModel model;

    QJsonObject errored = makeTorrentValue(10, "Broken", 0, 0.25, 1024, 3).toObject();
    errored[QStringLiteral("error")] = 3;
    errored[QStringLiteral("errorString")] = QStringLiteral("No data found! Ensure your drives are connected?");

    model.applyUpdate(makeTorrentList({ errored }));

    const QModelIndex nameIndex = model.index(0, TorrentModel::NameColumn);
    const QModelIndex statusIndex = model.index(0, TorrentModel::StatusColumn);

    QCOMPARE(model.data(statusIndex, Qt::DisplayRole).toString(), QStringLiteral("Error"));
    QCOMPARE(model.data(nameIndex, TorrentModel::HasErrorRole).toBool(), true);
    QCOMPARE(model.data(nameIndex, TorrentModel::ErrorStringRole).toString(),
             QStringLiteral("No data found! Ensure your drives are connected?"));
    QCOMPARE(model.data(nameIndex, TorrentModel::StatusValueRole).toInt(), 0);
    QCOMPARE(model.data(statusIndex, Qt::ToolTipRole).toString(),
             QStringLiteral("Error: No data found! Ensure your drives are connected?"));
    QVERIFY(qvariant_cast<QIcon>(model.data(statusIndex, Qt::DecorationRole)).isNull() == false);
    QVERIFY(qvariant_cast<QIcon>(model.data(nameIndex, Qt::DecorationRole)).isNull() == false);
    QVERIFY(qvariant_cast<QBrush>(model.data(statusIndex, Qt::ForegroundRole)).style() != Qt::NoBrush);
    QVERIFY(qvariant_cast<QFont>(model.data(statusIndex, Qt::FontRole)).bold());
}

void TestTorrentModel::indexesRowsByTorrentKey()
{
    TorrentModel model;
    model.applyUpdate(makeTorrentList({
        makeTorrentValue(10, "Ten", 4, 0.25, 1024, 0),
        makeTorrentValue(20, "Twenty", 0, 1.0, 2048, 1),
    }));

    QCOMPARE(model.rowForKey(QStringLiteral("10")), 0);
    QCOMPARE(model.rowForKey(QStringLiteral("20")), 1);
    QCOMPARE(model.rowForKey(QStringLiteral("999")), -1);

    QCOMPARE(model.data(model.index(model.rowForKey(QStringLiteral("20")), TorrentModel::NameColumn)).toString(),
             QStringLiteral("Twenty"));
}

void TestTorrentModel::queuePositionChangeEmitsDataChanged()
{
    TorrentModel model;
    model.applyUpdate(makeTorrentList({
        makeTorrentValue(10, "Ten", 4, 0.25, 1024, 0),
    }));

    QSignalSpy dataChangedSpy(&model, &TorrentModel::dataChanged);

    model.applyUpdate(makeTorrentList({
        makeTorrentValue(10, "Ten", 4, 0.25, 1024, 5),
    }));

    QCOMPARE(dataChangedSpy.count(), 1);
    QCOMPARE(model.data(model.index(0, TorrentModel::QueueColumn), Qt::UserRole + 1).toInt(), 5);
}

void TestTorrentModel::removesMissingRowsOnUpdate()
{
    TorrentModel model;
    model.applyUpdate(makeTorrentList({
        makeTorrentValue(10, "Ten", 4, 0.25, 1024, 0),
        makeTorrentValue(20, "Twenty", 0, 1.0, 2048, 1),
    }));

    QSignalSpy rowsRemovedSpy(&model, &TorrentModel::rowsRemoved);

    model.applyUpdate(makeTorrentList({
        makeTorrentValue(20, "Twenty", 0, 1.0, 2048, 1),
    }));

    QCOMPARE(rowsRemovedSpy.count(), 1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rowForKey(QStringLiteral("10")), -1);
    QCOMPARE(model.rowForKey(QStringLiteral("20")), 0);
}

QTEST_MAIN(TestTorrentModel)
#include "test_torrentmodel.moc"
