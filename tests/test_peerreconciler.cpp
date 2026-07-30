#include "peerreconciler.h"
#include "torrentpeerscontroller.h"

#include <QTableWidget>
#include <QTest>

#include <algorithm>

class TestPeerReconciler : public QObject
{
    Q_OBJECT

private slots:
    void unchangedSnapshotProducesNoOperations();
    void updatesReportOnlyChangedFields();
    void endpointsDriveInsertionsAndRemovals();
    void duplicateEndpointsRemainDistinct();
    void clearForcesFreshInsertions();
    void tableItemsSurviveRateOnlyUpdates();
};

namespace {

TorrentPeer peer(const QString &address, int port, qint64 downloadRate = 0)
{
    TorrentPeer result;
    result.address = address;
    result.port = port;
    result.clientName = QStringLiteral("Test client");
    result.progress = 0.5;
    result.downloadRate = downloadRate;
    return result;
}

int countKind(const QVector<PeerRowChange> &changes, PeerRowChange::Kind kind)
{
    return std::count_if(
        changes.cbegin(),
        changes.cend(),
        [kind](const PeerRowChange &change) {
            return change.kind == kind;
        });
}

}

void TestPeerReconciler::unchangedSnapshotProducesNoOperations()
{
    PeerSnapshotReconciler reconciler;
    const QVector<TorrentPeer> snapshot {
        peer(QStringLiteral(" 192.0.2.1 "), 51413),
    };

    const QVector<PeerRowChange> initial = reconciler.reconcile(snapshot);
    QCOMPARE(countKind(initial, PeerRowChange::Kind::Insert), 1);
    QCOMPARE(initial.constFirst().key.address, QStringLiteral("192.0.2.1"));

    QVERIFY(reconciler.reconcile(snapshot).isEmpty());
}

void TestPeerReconciler::updatesReportOnlyChangedFields()
{
    PeerSnapshotReconciler reconciler;
    TorrentPeer original = peer(QStringLiteral("192.0.2.2"), 51413, 100);
    reconciler.reconcile({ original });

    TorrentPeer updated = original;
    updated.downloadRate = 200;
    updated.progress = 0.75;

    const QVector<PeerRowChange> changes = reconciler.reconcile({ updated });
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.constFirst().kind, PeerRowChange::Kind::Update);
    QCOMPARE(changes.constFirst().fields,
             PeerFields(PeerField::DownloadRate) | PeerField::Progress);
}

void TestPeerReconciler::endpointsDriveInsertionsAndRemovals()
{
    PeerSnapshotReconciler reconciler;
    reconciler.reconcile({
        peer(QStringLiteral("192.0.2.3"), 51413),
        peer(QStringLiteral("192.0.2.4"), 51413),
    });

    const QVector<PeerRowChange> changes = reconciler.reconcile({
        peer(QStringLiteral("192.0.2.4"), 51413),
        peer(QStringLiteral("192.0.2.5"), 51413),
    });

    QCOMPARE(countKind(changes, PeerRowChange::Kind::Remove), 1);
    QCOMPARE(countKind(changes, PeerRowChange::Kind::Insert), 1);
    QCOMPARE(countKind(changes, PeerRowChange::Kind::Update), 0);
}

void TestPeerReconciler::duplicateEndpointsRemainDistinct()
{
    PeerSnapshotReconciler reconciler;
    const TorrentPeer first = peer(QStringLiteral("2001:db8::1"), 51413);
    TorrentPeer second = first;
    second.clientName = QStringLiteral("Second client");

    const QVector<PeerRowChange> initial =
        reconciler.reconcile({ first, second });
    QCOMPARE(countKind(initial, PeerRowChange::Kind::Insert), 2);

    second.uploadRate = 42;
    const QVector<PeerRowChange> changes =
        reconciler.reconcile({ first, second });
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.constFirst().key.occurrence, 1);
    QCOMPARE(changes.constFirst().fields, PeerFields(PeerField::UploadRate));
}

void TestPeerReconciler::clearForcesFreshInsertions()
{
    PeerSnapshotReconciler reconciler;
    const QVector<TorrentPeer> snapshot {
        peer(QStringLiteral("192.0.2.6"), 51413),
    };
    reconciler.reconcile(snapshot);
    reconciler.clear();

    const QVector<PeerRowChange> changes = reconciler.reconcile(snapshot);
    QCOMPARE(countKind(changes, PeerRowChange::Kind::Insert), 1);
}

void TestPeerReconciler::tableItemsSurviveRateOnlyUpdates()
{
    QTableWidget table;
    TorrentPeersController controller(&table, nullptr);
    controller.setup();

    TorrentPeers initial;
    initial.peers = {
        peer(QStringLiteral("192.0.2.7"), 51413, 100),
    };
    controller.populate(initial);

    QTableWidgetItem *addressItem = table.item(0, 1);
    QTableWidgetItem *clientItem = table.item(0, 4);
    QTableWidgetItem *downloadItem = table.item(0, 6);
    table.selectRow(0);

    TorrentPeers updated = initial;
    updated.peers[0].downloadRate = 200;
    controller.populate(updated);

    const int row = table.row(addressItem);
    QVERIFY(row >= 0);
    QCOMPARE(table.item(row, 1), addressItem);
    QCOMPARE(table.item(row, 4), clientItem);
    QCOMPARE(table.item(row, 6), downloadItem);
    QCOMPARE(downloadItem->data(Qt::UserRole).toLongLong(), 200);
    QVERIFY(table.item(row, 1)->isSelected());
}

QTEST_MAIN(TestPeerReconciler)
#include "test_peerreconciler.moc"
