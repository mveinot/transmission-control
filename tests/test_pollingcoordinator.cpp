#include <QtTest>

#include "pollingcoordinator.h"

class TestPollingCoordinator : public QObject
{
    Q_OBJECT

private slots:
    void commandRefreshesAreCoalesced();
    void delayedDetailsUseCurrentSelection();
    void serverResetCancelsPendingCommandRefresh();
    void requestsOnlyVisibleDetailCategory();
    void slowRequestsRespectCadenceAndCapability();
    void periodicPollingAnnouncesRefresh();
};

void TestPollingCoordinator::commandRefreshesAreCoalesced()
{
    int listRequests = 0;
    QList<TorrentKey> detailRequests;
    PollingCoordinator::Requests requests;
    requests.torrentList = [&]() { ++listRequests; };
    requests.torrentDetails =
        [&](const TorrentKey &key) { detailRequests.append(key); };

    PollingCoordinator coordinator(requests, nullptr, 10);
    coordinator.setSelectedTorrent(QStringLiteral("alpha"));
    coordinator.scheduleCommandRefresh(false);
    coordinator.scheduleCommandRefresh(true);
    coordinator.scheduleCommandRefresh(false);

    QTRY_COMPARE(listRequests, 1);
    QCOMPARE(detailRequests, QList<TorrentKey> {QStringLiteral("alpha")});
}

void TestPollingCoordinator::delayedDetailsUseCurrentSelection()
{
    QList<TorrentKey> detailRequests;
    PollingCoordinator::Requests requests;
    requests.torrentDetails =
        [&](const TorrentKey &key) { detailRequests.append(key); };

    PollingCoordinator coordinator(requests, nullptr, 10);
    coordinator.setSelectedTorrent(QStringLiteral("old-selection"));
    coordinator.scheduleCommandRefresh(true);
    coordinator.setSelectedTorrent(QStringLiteral("new-selection"));

    QTRY_COMPARE(
        detailRequests,
        QList<TorrentKey> {QStringLiteral("new-selection")});
}

void TestPollingCoordinator::serverResetCancelsPendingCommandRefresh()
{
    int listRequests = 0;
    int detailRequests = 0;
    int cancellations = 0;
    PollingCoordinator::Requests requests;
    requests.torrentList = [&]() { ++listRequests; };
    requests.torrentDetails =
        [&](const TorrentKey &) { ++detailRequests; };
    requests.cancelTorrentDetails = [&]() { ++cancellations; };

    PollingCoordinator coordinator(requests, nullptr, 20);
    coordinator.setSelectedTorrent(QStringLiteral("old-server-torrent"));
    coordinator.scheduleCommandRefresh(true);
    coordinator.resetForServerChange();
    QTest::qWait(40);

    QCOMPARE(listRequests, 0);
    QCOMPARE(detailRequests, 0);
    QCOMPARE(cancellations, 2);
}

void TestPollingCoordinator::requestsOnlyVisibleDetailCategory()
{
    QStringList requestsMade;
    PollingCoordinator::Requests requests;
    requests.torrentDetails =
        [&](const TorrentKey &) { requestsMade.append(QStringLiteral("summary")); };
    requests.torrentFiles =
        [&](const TorrentKey &) { requestsMade.append(QStringLiteral("files")); };
    requests.torrentPeers =
        [&](const TorrentKey &) { requestsMade.append(QStringLiteral("peers")); };
    requests.cancelTorrentDetails =
        [&]() { requestsMade.append(QStringLiteral("cancel")); };

    PollingCoordinator coordinator(requests);
    coordinator.setSelectedTorrent(QStringLiteral("selected"));
    requestsMade.clear();
    coordinator.setDetailView(PollingCoordinator::DetailView::Files);
    coordinator.requestSelectedTorrent(true);
    QCOMPARE(requestsMade,
             QStringList({QStringLiteral("summary"),
                          QStringLiteral("files")}));

    requestsMade.clear();
    coordinator.setDetailView(PollingCoordinator::DetailView::Peers);
    coordinator.handleTorrentListReceived();
    QCOMPARE(requestsMade, QStringList({QStringLiteral("peers")}));

    requestsMade.clear();
    coordinator.setDetailsPaneVisible(false);
    coordinator.handleTorrentListReceived();
    QCOMPARE(requestsMade, QStringList({QStringLiteral("cancel")}));
}

void TestPollingCoordinator::slowRequestsRespectCadenceAndCapability()
{
    int trackerRequests = 0;
    QStringList freeSpacePaths;
    bool freeSpaceSupported = true;
    PollingCoordinator::Requests requests;
    requests.trackerMetadata = [&]() { ++trackerRequests; };
    requests.freeSpace =
        [&](const QString &path) { freeSpacePaths.append(path); };
    requests.freeSpaceSupported = [&]() { return freeSpaceSupported; };

    PollingCoordinator coordinator(requests, nullptr, 10, 1000);
    coordinator.setRemoteDownloadDirectory(QStringLiteral("/downloads"), true);
    QCOMPARE(freeSpacePaths.size(), 1);

    coordinator.refreshSlowData();
    coordinator.refreshSlowData();
    QCOMPARE(trackerRequests, 1);
    QCOMPARE(freeSpacePaths.size(), 1);

    coordinator.refreshSlowData(true);
    QCOMPARE(trackerRequests, 2);
    QCOMPARE(freeSpacePaths.size(), 2);

    freeSpaceSupported = false;
    coordinator.refreshSlowData(true);
    QCOMPARE(trackerRequests, 3);
    QCOMPARE(freeSpacePaths.size(), 2);
}

void TestPollingCoordinator::periodicPollingAnnouncesRefresh()
{
    int listRequests = 0;
    PollingCoordinator::Requests requests;
    requests.torrentList = [&]() { ++listRequests; };

    PollingCoordinator coordinator(requests);
    QSignalSpy started(
        &coordinator,
        &PollingCoordinator::torrentListRefreshStarted);
    coordinator.setPollingInterval(10);
    coordinator.startPolling();

    QTRY_VERIFY(listRequests >= 1);
    QVERIFY(started.size() >= 1);
    QCOMPARE(started.first().first().toBool(), false);
}

QTEST_GUILESS_MAIN(TestPollingCoordinator)

#include "test_pollingcoordinator.moc"
