#include <QtTest/QtTest>
#include <QSignalSpy>

#include "torrentmodel.h"

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
    void indexesRowsByTorrentId();
    void queuePositionChangeEmitsDataChanged();
    void removesMissingRowsOnUpdate();
};

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

void TestTorrentModel::indexesRowsByTorrentId()
{
    TorrentModel model;
    model.applyUpdate(makeTorrentList({
        makeTorrentValue(10, "Ten", 4, 0.25, 1024, 0),
        makeTorrentValue(20, "Twenty", 0, 1.0, 2048, 1),
    }));

    QCOMPARE(model.rowForId(10), 0);
    QCOMPARE(model.rowForId(20), 1);
    QCOMPARE(model.rowForId(999), -1);

    QCOMPARE(model.data(model.index(model.rowForId(20), TorrentModel::NameColumn)).toString(),
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
    QCOMPARE(model.rowForId(10), -1);
    QCOMPARE(model.rowForId(20), 0);
}

QTEST_MAIN(TestTorrentModel)
#include "test_torrentmodel.moc"
