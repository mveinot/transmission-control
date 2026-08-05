#include "fakehttpserver.h"
#include "qbittorrentbackend.h"
#include "settingskeys.h"

#include <QJsonDocument>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestQBittorrentBackend : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void protocolIsIdentified();
    void login204IsAcceptedAndListIsNormalized();
    void expiredReadAuthenticationIsRecovered();
    void commandsUseFormEncodedHashes();
    void selectedTorrentEndpointsAreProjected();

private:
    QTemporaryDir m_settingsDirectory;
    void configureServer(const QUrl &url);
};

void TestQBittorrentBackend::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("PlanetaryTests"));
    QCoreApplication::setApplicationName(QStringLiteral("QBittorrentBackend"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       m_settingsDirectory.path());
}

void TestQBittorrentBackend::protocolIsIdentified()
{
    QBittorrentBackend backend;
    QCOMPARE(backend.protocolDescription(),
             QStringLiteral("qBittorrent Web API v2"));
}

void TestQBittorrentBackend::configureServer(const QUrl &url)
{
    QSettings settings;
    settings.clear();
    settings.beginWriteArray(SettingsKeys::ServersArray);
    settings.setArrayIndex(0);
    settings.setValue(SettingsKeys::ServerBackendType,
                      QStringLiteral("qbittorrent"));
    settings.setValue(SettingsKeys::ServerName, QStringLiteral("Test qBt"));
    settings.setValue(SettingsKeys::ServerRpcUrl, url.toString());
    settings.setValue(SettingsKeys::ServerUsername, QStringLiteral("user"));
    settings.setValue(SettingsKeys::ServerPassword, QStringLiteral("secret"));
    settings.endArray();
    settings.setValue(SettingsKeys::ServersDefaultIndex, 0);
    settings.sync();
}

void TestQBittorrentBackend::login204IsAcceptedAndListIsNormalized()
{
    int loginCount = 0;
    QByteArray loginBody;
    bool listHadCookie = false;
    FakeHttpServer server([&](const FakeHttpServer::Request &request) {
        if (request.target.startsWith("/api/v2/auth/login")) {
            ++loginCount;
            loginBody = request.body;
            return FakeHttpServer::Response{
                204, "No Content", {{"Set-Cookie", "SID=test-sid; Path=/"}}, {}};
        }
        if (request.target.startsWith("/api/v2/torrents/info")) {
            listHadCookie =
                request.headers.value("cookie").contains("SID=test-sid");
            const QJsonArray torrents{
                QJsonObject{
                    {"hash", "hash-one"}, {"name", "Linux ISO"},
                    {"state", "downloading"}, {"progress", 0.4},
                    {"dlspeed", 2048}, {"upspeed", 128},
                    {"num_seeds", 5}, {"num_complete", 70},
                    {"num_leechs", 3}, {"num_incomplete", 20},
                    {"total_size", 100000}
                }
            };
            return FakeHttpServer::Response{
                200, "OK", {{"Content-Type", "application/json"}},
                QJsonDocument(torrents).toJson(QJsonDocument::Compact)};
        }
        return FakeHttpServer::Response{404, "Not Found", {}, {}};
    });
    QVERIFY(server.isListening());
    configureServer(server.url());

    QBittorrentBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy received(&backend, &TorrentBackend::torrentsReceived);
    backend.getTorrentList();

    QTRY_COMPARE(received.size(), 1);
    QCOMPARE(loginCount, 1);
    QVERIFY(loginBody.contains("username=user"));
    QVERIFY(loginBody.contains("password=secret"));
    QVERIFY(listHadCookie);
    const QVector<torrent> torrents =
        qvariant_cast<QVector<torrent>>(received.first().first());
    QCOMPARE(torrents.first().getKey(), QStringLiteral("hash-one"));
    QCOMPARE(torrents.first().getConnectedSeeds(), 5);
    QCOMPARE(torrents.first().getTotalSeeds(), 70);
}

void TestQBittorrentBackend::expiredReadAuthenticationIsRecovered()
{
    int loginCount = 0;
    int listCount = 0;
    FakeHttpServer server([&](const FakeHttpServer::Request &request) {
        if (request.target.startsWith("/api/v2/auth/login")) {
            ++loginCount;
            return FakeHttpServer::Response{
                200, "OK",
                {{"Set-Cookie",
                  QByteArrayLiteral("SID=sid-")
                      + QByteArray::number(loginCount)
                      + QByteArrayLiteral("; Path=/")}},
                "Ok."};
        }
        if (request.target.startsWith("/api/v2/torrents/info")) {
            ++listCount;
            if (listCount == 1)
                return FakeHttpServer::Response{403, "Forbidden", {}, {}};
            return FakeHttpServer::Response{
                200, "OK", {{"Content-Type", "application/json"}}, "[]"};
        }
        return FakeHttpServer::Response{404, "Not Found", {}, {}};
    });
    QVERIFY(server.isListening());
    configureServer(server.url());

    QBittorrentBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy received(&backend, &TorrentBackend::torrentsReceived);
    backend.getTorrentList();

    QTRY_COMPARE(received.size(), 1);
    QCOMPARE(loginCount, 2);
    QCOMPARE(listCount, 2);
}

void TestQBittorrentBackend::commandsUseFormEncodedHashes()
{
    QList<FakeHttpServer::Request> commands;
    FakeHttpServer server([&](const FakeHttpServer::Request &request) {
        if (request.target.startsWith("/api/v2/auth/login")) {
            return FakeHttpServer::Response{
                200, "OK", {{"Set-Cookie", "SID=test; Path=/"}}, "Ok."};
        }
        commands.append(request);
        return FakeHttpServer::Response{204, "No Content", {}, {}};
    });
    QVERIFY(server.isListening());
    configureServer(server.url());

    QBittorrentBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy succeeded(&backend, &TorrentBackend::commandSucceeded);
    backend.startTorrents({QStringLiteral("alpha"),
                           QStringLiteral("beta")});
    backend.removeTorrents({QStringLiteral("alpha")}, true);

    QTRY_COMPARE(succeeded.size(), 2);
    QCOMPARE(commands.size(), 2);
    const auto commandFor = [&commands](const QByteArray &path) {
        for (const FakeHttpServer::Request &request : commands) {
            if (request.target.contains(path))
                return request;
        }
        return FakeHttpServer::Request{};
    };
    const FakeHttpServer::Request start =
        commandFor("/api/v2/torrents/start");
    const FakeHttpServer::Request remove =
        commandFor("/api/v2/torrents/delete");
    QVERIFY(!start.target.isEmpty());
    QVERIFY(!remove.target.isEmpty());
    QVERIFY(start.body.contains("hashes=alpha%7Cbeta"));
    QVERIFY(remove.body.contains("deleteFiles=true"));
}

void TestQBittorrentBackend::selectedTorrentEndpointsAreProjected()
{
    FakeHttpServer server([](const FakeHttpServer::Request &request) {
        if (request.target.startsWith("/api/v2/auth/login")) {
            return FakeHttpServer::Response{
                200, "OK", {{"Set-Cookie", "SID=test; Path=/"}}, "Ok."};
        }

        QByteArray body;
        if (request.target.startsWith("/api/v2/torrents/info")) {
            body = QJsonDocument(
                       QJsonArray{QJsonObject{
                           {"hash", "hash-one"}, {"name", "Linux ISO"},
                           {"progress", 0.5}, {"save_path", "/downloads"},
                           {"total_size", 1000}}})
                       .toJson(QJsonDocument::Compact);
        } else if (request.target.startsWith(
                       "/api/v2/torrents/properties")) {
            body = R"({"comment":"Release","created_by":"tool"})";
        } else if (request.target.startsWith(
                       "/api/v2/torrents/pieceStates")) {
            body = "[2,0,2]";
        } else if (request.target.startsWith(
                       "/api/v2/torrents/files")) {
            body = QJsonDocument(
                       QJsonArray{QJsonObject{
                           {"index", 0}, {"name", "image.iso"},
                           {"size", 1000}, {"progress", 0.5},
                           {"priority", 1}}})
                       .toJson(QJsonDocument::Compact);
        } else if (request.target.startsWith(
                       "/api/v2/sync/torrentPeers")) {
            body = QJsonDocument(
                       QJsonObject{{"peers",
                           QJsonObject{{"peer", QJsonObject{
                               {"ip", "192.0.2.10"}, {"port", 51413},
                               {"client", "Peer"}, {"progress", 0.25},
                               {"dl_speed", 10}, {"up_speed", 20},
                               {"flags", "E"}}}}}})
                       .toJson(QJsonDocument::Compact);
        } else if (request.target.startsWith(
                       "/api/v2/torrents/trackers")) {
            body = QJsonDocument(
                       QJsonArray{QJsonObject{
                           {"url", "https://tracker.example/announce"},
                           {"tier", 0}, {"status", 2},
                           {"num_seeds", 7}, {"num_leeches", 3}}})
                       .toJson(QJsonDocument::Compact);
        }
        return FakeHttpServer::Response{
            200, "OK", {{"Content-Type", "application/json"}}, body};
    });
    QVERIFY(server.isListening());
    configureServer(server.url());

    QBittorrentBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy list(&backend, &TorrentBackend::torrentsReceived);
    QSignalSpy details(&backend, &TorrentBackend::torrentDetailsReceived);
    QSignalSpy files(&backend, &TorrentBackend::torrentFilesReceived);
    QSignalSpy peers(&backend, &TorrentBackend::torrentPeersReceived);
    QSignalSpy trackers(&backend, &TorrentBackend::torrentTrackersReceived);
    QSignalSpy pieces(&backend, &TorrentBackend::torrentPiecesReceived);

    backend.getTorrentList();
    QTRY_COMPARE(list.size(), 1);
    backend.getTorrentDetails(QStringLiteral("hash-one"));
    backend.getTorrentFiles(QStringLiteral("hash-one"));
    backend.getTorrentPeers(QStringLiteral("hash-one"));
    backend.getTorrentTrackers(QStringLiteral("hash-one"));
    backend.getTorrentPieces(QStringLiteral("hash-one"));

    QTRY_COMPARE(details.size(), 1);
    QTRY_COMPARE(files.size(), 1);
    QTRY_COMPARE(peers.size(), 1);
    QTRY_COMPARE(trackers.size(), 1);
    QTRY_COMPARE(pieces.size(), 1);
    const TorrentPieces pieceSnapshot =
        qvariant_cast<TorrentPieces>(pieces.first().first());
    QCOMPARE(pieceSnapshot.pieceCount, 3);
    QCOMPARE(static_cast<uchar>(pieceSnapshot.completedPieces.at(0)),
             static_cast<uchar>(0xa0));
}

QTEST_MAIN(TestQBittorrentBackend)
#include "test_qbittorrentbackend.moc"
