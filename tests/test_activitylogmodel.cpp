#include "activitylogmodel.h"

#include <QSortFilterProxyModel>
#include <QtTest>

class TestActivityLogModel : public QObject
{
    Q_OBJECT

private slots:
    void storesLevelsAndDisplaysLevelColumn();
    void filtersActivityAndWarningsErrors();
};

void TestActivityLogModel::storesLevelsAndDisplaysLevelColumn()
{
    ActivityLogModel model;
    model.addEvent(QStringLiteral("Connected"), QStringLiteral("RPC ready"),
                   QStringLiteral("Local"), ActivityLevel::Info);
    model.addEvent(QStringLiteral("Torrent finished"), QStringLiteral("Linux ISO"),
                   QStringLiteral("Local"), ActivityLevel::Torrent);
    model.addEvent(QStringLiteral("RPC failed"), QStringLiteral("System error"),
                   QStringLiteral("Local"), ActivityLevel::Error);

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.data(model.index(0, ActivityLogModel::LevelColumn)).toString(),
             QStringLiteral("Info"));
    QCOMPARE(model.data(model.index(1, ActivityLogModel::LevelColumn)).toString(),
             QStringLiteral("Torrent"));
    QCOMPARE(model.data(model.index(2, ActivityLogModel::LevelColumn)).toString(),
             QStringLiteral("Error"));
}

void TestActivityLogModel::filtersActivityAndWarningsErrors()
{
    ActivityLogModel model;
    model.addEvent(QStringLiteral("Connected"), {}, {}, ActivityLevel::Info);
    model.addEvent(QStringLiteral("Torrent added"), {}, {}, ActivityLevel::Torrent);
    model.addEvent(QStringLiteral("Torrent stalled"), {}, {}, ActivityLevel::Warning);
    model.addEvent(QStringLiteral("RPC failed"), {}, {}, ActivityLevel::Error);

    ActivityLogFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    QCOMPARE(proxy.rowCount(), 4);
    proxy.setFilter(ActivityLogFilterProxyModel::Filter::Activity);
    QCOMPARE(proxy.rowCount(), 2);
    proxy.setFilter(ActivityLogFilterProxyModel::Filter::WarningsAndErrors);
    QCOMPARE(proxy.rowCount(), 2);
    proxy.setFilter(ActivityLogFilterProxyModel::Filter::All);
    QCOMPARE(proxy.rowCount(), 4);
}

QTEST_MAIN(TestActivityLogModel)
#include "test_activitylogmodel.moc"
