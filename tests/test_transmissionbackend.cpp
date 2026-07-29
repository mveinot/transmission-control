#include "fakehttpserver.h"
#include "transmissionbackend.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

class TestTransmissionBackend : public QObject
{
    Q_OBJECT

private slots:
    void sessionChallengeIsRetriedAndListIsNormalized();
    void commandsUseBackendNeutralKeys();
    void sessionDataIsProjected();
};

void TestTransmissionBackend::sessionChallengeIsRetriedAndListIsNormalized()
{
    QList<QJsonObject> requests;
    int challenges = 0;
    FakeHttpServer server([&](const FakeHttpServer::Request &request) {
        if (request.headers.value("x-transmission-session-id") != "token-1") {
            ++challenges;
            return FakeHttpServer::Response{
                409, "Conflict",
                {{"X-Transmission-Session-Id", "token-1"}}, {}};
        }
        const QJsonObject rpc = QJsonDocument::fromJson(request.body).object();
        requests.append(rpc);
        const QJsonObject torrent{
            {"id", 42}, {"hashString", "hash-42"}, {"name", "Test torrent"},
            {"status", 4}, {"percentDone", 0.25}, {"eta", 120},
            {"rateDownload", 4096}, {"rateUpload", 512},
            {"sizeWhenDone", 10000}, {"totalSize", 10000}
        };
        const QJsonObject response{
            {"result", "success"},
            {"arguments", QJsonObject{{"torrents", QJsonArray{torrent}}}}
        };
        return FakeHttpServer::Response{
            200, "OK", {{"Content-Type", "application/json"}},
            QJsonDocument(response).toJson(QJsonDocument::Compact)};
    });
    QVERIFY(server.isListening());

    TransmissionBackend backend;
    ServerProfile profile;
    profile.name = QStringLiteral("Test Transmission");
    profile.rpcUrl =
        server.url(QStringLiteral("/transmission/rpc")).toString();
    QVERIFY(backend.setServerProfile(profile));
    QSignalSpy received(&backend, &TorrentBackend::torrentsReceived);
    QSignalSpy finished(&backend, &TorrentBackend::updateFinished);
    backend.getTorrentList();

    QTRY_COMPARE(received.size(), 1);
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(challenges, 1);
    QCOMPARE(requests.size(), 1);
    QCOMPARE(requests.first().value("method").toString(),
             QStringLiteral("torrent-get"));
    const QVector<torrent> torrents =
        qvariant_cast<QVector<torrent>>(received.first().first());
    QCOMPARE(torrents.size(), 1);
    QCOMPARE(torrents.first().getKey(), QStringLiteral("hash-42"));
    QCOMPARE(torrents.first().getName(), QStringLiteral("Test torrent"));
}

void TestTransmissionBackend::commandsUseBackendNeutralKeys()
{
    QList<QJsonObject> requests;
    FakeHttpServer server([&](const FakeHttpServer::Request &request) {
        const QJsonObject rpc = QJsonDocument::fromJson(request.body).object();
        requests.append(rpc);
        const QJsonObject arguments =
            rpc.value("method").toString() == QStringLiteral("torrent-get")
                ? QJsonObject{{"torrents", QJsonArray{}}}
                : QJsonObject{};
        const QJsonObject response{{"result", "success"},
                                   {"arguments", arguments}};
        return FakeHttpServer::Response{
            200, "OK", {{"Content-Type", "application/json"}},
            QJsonDocument(response).toJson(QJsonDocument::Compact)};
    });
    QVERIFY(server.isListening());

    TransmissionBackend backend;
    ServerProfile profile;
    profile.name = QStringLiteral("Test");
    profile.rpcUrl = server.url("/rpc").toString();
    QVERIFY(backend.setServerProfile(profile));
    QSignalSpy succeeded(&backend, &TorrentBackend::commandSucceeded);
    backend.startTorrents({QStringLiteral("alpha"), QStringLiteral("beta")});
    backend.removeTorrents({QStringLiteral("alpha")}, true);

    QTRY_COMPARE(succeeded.size(), 2);
    QTRY_VERIFY(requests.size() >= 2);
    const auto requestFor = [&requests](const QString &method) {
        for (const QJsonObject &request : requests) {
            if (request.value("method").toString() == method)
                return request;
        }
        return QJsonObject{};
    };
    const QJsonObject start = requestFor(QStringLiteral("torrent-start"));
    const QJsonObject remove = requestFor(QStringLiteral("torrent-remove"));
    QVERIFY(!start.isEmpty());
    QVERIFY(!remove.isEmpty());
    QCOMPARE(start.value("arguments").toObject()
                 .value("ids").toArray(),
             QJsonArray({"alpha", "beta"}));
    QVERIFY(remove.value("arguments").toObject()
                .value("delete-local-data").toBool());
}

void TestTransmissionBackend::sessionDataIsProjected()
{
    FakeHttpServer server([](const FakeHttpServer::Request &request) {
        const QString method =
            QJsonDocument::fromJson(request.body).object()
                .value("method").toString();
        QJsonObject arguments;
        if (method == QStringLiteral("session-get"))
            arguments = QJsonObject{{"rpc-version", 18},
                                    {"download-dir", "/downloads"}};
        else if (method == QStringLiteral("session-stats"))
            arguments = QJsonObject{{"activeTorrentCount", 3}};
        else if (method == QStringLiteral("free-space"))
            arguments = QJsonObject{{"path", "/downloads"},
                                    {"size-bytes", 123456789}};
        const QJsonObject response{{"result", "success"},
                                   {"arguments", arguments}};
        return FakeHttpServer::Response{
            200, "OK", {{"Content-Type", "application/json"}},
            QJsonDocument(response).toJson(QJsonDocument::Compact)};
    });
    QVERIFY(server.isListening());

    TransmissionBackend backend;
    ServerProfile profile;
    profile.name = QStringLiteral("Test");
    profile.rpcUrl = server.url("/rpc").toString();
    QVERIFY(backend.setServerProfile(profile));
    QSignalSpy settings(&backend, &TorrentBackend::sessionSettingsReceived);
    QSignalSpy statistics(&backend,
                          &TorrentBackend::sessionStatisticsReceived);
    QSignalSpy freeSpace(&backend, &TorrentBackend::freeSpaceReceived);
    backend.getSessionSettings();
    backend.getSessionStatistics();
    backend.getFreeSpace(QStringLiteral("/downloads"));

    QTRY_COMPARE(settings.size(), 1);
    QTRY_COMPARE(statistics.size(), 1);
    QTRY_COMPARE(freeSpace.size(), 1);
    QVERIFY(backend.capabilities().sequentialDownload);
    QVERIFY(backend.capabilities().labels);
    QVERIFY(backend.capabilities().groups);
    QCOMPARE(freeSpace.first().at(1).toLongLong(), 123456789);
}

QTEST_MAIN(TestTransmissionBackend)
#include "test_transmissionbackend.moc"
