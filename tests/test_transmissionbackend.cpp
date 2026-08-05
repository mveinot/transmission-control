#include "fakehttpserver.h"
#include "transmissionbackend.h"
#include "transmissionprotocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

class TestTransmissionBackend : public QObject
{
    Q_OBJECT

private slots:
    void legacyProtocolPreservesWireContract();
    void jsonRpcProtocolTranslatesWireContract();
    void jsonRpcProtocolNormalizesSessionResults();
    void jsonRpcProtocolRejectsErrorsAndMismatchedReplies();
    void sessionChallengeIsRetriedAndListIsNormalized();
    void modernProtocolIsNegotiatedAndListIsNormalized();
    void rejectedModernProbeFallsBackToLegacy();
    void commandsUseBackendNeutralKeys();
    void sessionDataIsProjected();
};

void TestTransmissionBackend::legacyProtocolPreservesWireContract()
{
    const std::unique_ptr<TransmissionProtocol> protocol =
        TransmissionProtocol::createLegacy();
    const QJsonObject encoded = QJsonDocument::fromJson(
        protocol->encodeRequest(
            QStringLiteral("torrent-get"),
            QJsonObject{{QStringLiteral("fields"),
                         QJsonArray{QStringLiteral("id")}}},
            27)).object();

    QCOMPARE(encoded,
             QJsonObject({
                 {QStringLiteral("method"), QStringLiteral("torrent-get")},
                 {QStringLiteral("arguments"),
                  QJsonObject{{QStringLiteral("fields"),
                               QJsonArray{QStringLiteral("id")}}}}
             }));

    const TransmissionProtocolReply reply = protocol->decodeReply(
        QJsonObject{
            {QStringLiteral("result"), QStringLiteral("success")},
            {QStringLiteral("arguments"),
             QJsonObject{{QStringLiteral("torrents"), QJsonArray{}}}}
        }, 27, QStringLiteral("torrent-get"));
    QVERIFY(reply.valid);
    QVERIFY(reply.success);
    QVERIFY(reply.result.value(QStringLiteral("torrents")).isArray());
}

void TestTransmissionBackend::jsonRpcProtocolTranslatesWireContract()
{
    const std::unique_ptr<TransmissionProtocol> protocol =
        TransmissionProtocol::createJsonRpc2();
    QCOMPARE(protocol->dialect(), TransmissionProtocolDialect::JsonRpc2);
    const QJsonObject encoded = QJsonDocument::fromJson(
        protocol->encodeRequest(
            QStringLiteral("torrent-get"),
            QJsonObject{
                {QStringLiteral("ids"), QJsonArray{42}},
                {QStringLiteral("fields"),
                 QJsonArray{QStringLiteral("hashString"),
                            QStringLiteral("percentDone"),
                            QStringLiteral("peer-limit")}}
            }, 27)).object();

    QCOMPARE(encoded.value(QStringLiteral("jsonrpc")).toString(),
             QStringLiteral("2.0"));
    QCOMPARE(encoded.value(QStringLiteral("method")).toString(),
             QStringLiteral("torrent_get"));
    QCOMPARE(encoded.value(QStringLiteral("id")).toInt(), 27);
    QCOMPARE(encoded.value(QStringLiteral("params")).toObject()
                 .value(QStringLiteral("fields")).toArray(),
             QJsonArray({QStringLiteral("hash_string"),
                         QStringLiteral("percent_done"),
                         QStringLiteral("peer_limit")}));

    const TransmissionProtocolReply reply = protocol->decodeReply(
        QJsonObject{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), 27},
            {QStringLiteral("result"),
             QJsonObject{{QStringLiteral("torrents"),
                          QJsonArray{QJsonObject{
                              {QStringLiteral("hash_string"),
                               QStringLiteral("abc")},
                              {QStringLiteral("percent_done"), 0.5},
                              {QStringLiteral("peer_limit"), 80}
                          }}}}}
        }, 27, QStringLiteral("torrent-get"));
    QVERIFY(reply.valid);
    QVERIFY(reply.success);
    const QJsonObject torrent = reply.result
                                    .value(QStringLiteral("torrents"))
                                    .toArray().first().toObject();
    QCOMPARE(torrent.value(QStringLiteral("hashString")).toString(),
             QStringLiteral("abc"));
    QCOMPARE(torrent.value(QStringLiteral("percentDone")).toDouble(), 0.5);
    QCOMPARE(torrent.value(QStringLiteral("peer-limit")).toInt(), 80);
}

void TestTransmissionBackend::jsonRpcProtocolNormalizesSessionResults()
{
    const std::unique_ptr<TransmissionProtocol> protocol =
        TransmissionProtocol::createJsonRpc2();
    const TransmissionProtocolReply settings = protocol->decodeReply(
        QJsonObject{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), 31},
            {QStringLiteral("result"),
             QJsonObject{
                 {QStringLiteral("rpc_version"), 18},
                 {QStringLiteral("download_dir"), QStringLiteral("/data")},
                 {QStringLiteral("seed_ratio_limited"), true},
                 {QStringLiteral("seed_ratio_limit"), 2.0}
             }}
        }, 31, QStringLiteral("session-get"));
    QVERIFY(settings.success);
    QCOMPARE(settings.result.value(QStringLiteral("rpc-version")).toInt(), 18);
    QCOMPARE(settings.result.value(QStringLiteral("download-dir")).toString(),
             QStringLiteral("/data"));
    QVERIFY(settings.result.value(QStringLiteral("seedRatioLimited")).toBool());
    QCOMPARE(settings.result.value(QStringLiteral("seedRatioLimit")).toDouble(),
             2.0);

    const TransmissionProtocolReply statistics = protocol->decodeReply(
        QJsonObject{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), 32},
            {QStringLiteral("result"),
             QJsonObject{
                 {QStringLiteral("current_stats"),
                  QJsonObject{{QStringLiteral("downloaded_bytes"), 1024}}},
                 {QStringLiteral("cumulative_stats"),
                  QJsonObject{{QStringLiteral("session_count"), 4}}}
             }}
        }, 32, QStringLiteral("session-stats"));
    QVERIFY(statistics.success);
    QCOMPARE(statistics.result.value(QStringLiteral("current-stats"))
                 .toObject().value(QStringLiteral("downloadedBytes")).toInt(),
             1024);
    QCOMPARE(statistics.result.value(QStringLiteral("cumulative-stats"))
                 .toObject().value(QStringLiteral("sessionCount")).toInt(),
             4);
}

void TestTransmissionBackend::jsonRpcProtocolRejectsErrorsAndMismatchedReplies()
{
    const std::unique_ptr<TransmissionProtocol> protocol =
        TransmissionProtocol::createJsonRpc2();
    const TransmissionProtocolReply error = protocol->decodeReply(
        QJsonObject{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), 8},
            {QStringLiteral("error"),
             QJsonObject{{QStringLiteral("code"), -32602},
                         {QStringLiteral("message"),
                          QStringLiteral("Invalid params")}}}
        }, 8, QStringLiteral("torrent-get"));
    QVERIFY(error.valid);
    QVERIFY(!error.success);
    QCOMPARE(error.error, QStringLiteral("Invalid params"));

    const TransmissionProtocolReply mismatch = protocol->decodeReply(
        QJsonObject{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), 9},
            {QStringLiteral("result"), QJsonObject{}}
        }, 8, QStringLiteral("torrent-get"));
    QVERIFY(!mismatch.valid);
    QVERIFY(mismatch.error.contains(QStringLiteral("mismatched")));
}

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
    QCOMPARE(requests.size(), 2);
    QCOMPARE(requests.first().value("jsonrpc").toString(),
             QStringLiteral("2.0"));
    QCOMPARE(requests.first().value("method").toString(),
             QStringLiteral("session_get"));
    QCOMPARE(requests.last().value("method").toString(),
             QStringLiteral("torrent-get"));
    QCOMPARE(backend.protocolDescription(),
             QStringLiteral("Transmission legacy RPC"));
    const QVector<torrent> torrents =
        qvariant_cast<QVector<torrent>>(received.first().first());
    QCOMPARE(torrents.size(), 1);
    QCOMPARE(torrents.first().getKey(), QStringLiteral("hash-42"));
    QCOMPARE(torrents.first().getName(), QStringLiteral("Test torrent"));
}

void TestTransmissionBackend::modernProtocolIsNegotiatedAndListIsNormalized()
{
    QList<QJsonObject> requests;
    FakeHttpServer server([&](const FakeHttpServer::Request &request) {
        const QJsonObject rpc = QJsonDocument::fromJson(request.body).object();
        requests.append(rpc);
        const QString method = rpc.value(QStringLiteral("method")).toString();
        QJsonObject result;
        if (method == QStringLiteral("session_get")) {
            result.insert(QStringLiteral("rpc_version"), 18);
        } else if (method == QStringLiteral("torrent_get")) {
            result.insert(
                QStringLiteral("torrents"),
                QJsonArray{QJsonObject{
                    {QStringLiteral("id"), 42},
                    {QStringLiteral("hash_string"), QStringLiteral("hash-42")},
                    {QStringLiteral("name"), QStringLiteral("Modern torrent")},
                    {QStringLiteral("status"), 4},
                    {QStringLiteral("percent_done"), 0.75},
                    {QStringLiteral("rate_download"), 8192},
                    {QStringLiteral("rate_upload"), 1024},
                    {QStringLiteral("size_when_done"), 20000},
                    {QStringLiteral("total_size"), 20000}
                }});
        }
        const QJsonObject response{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), rpc.value(QStringLiteral("id"))},
            {QStringLiteral("result"), result}
        };
        return FakeHttpServer::Response{
            200, "OK", {{"Content-Type", "application/json"}},
            QJsonDocument(response).toJson(QJsonDocument::Compact)};
    });
    QVERIFY(server.isListening());

    TransmissionBackend backend;
    ServerProfile profile;
    profile.name = QStringLiteral("Modern Transmission");
    profile.rpcUrl = server.url(QStringLiteral("/rpc")).toString();
    QVERIFY(backend.setServerProfile(profile));
    QSignalSpy received(&backend, &TorrentBackend::torrentsReceived);
    backend.getTorrentList();

    QTRY_COMPARE(received.size(), 1);
    QCOMPARE(requests.size(), 2);
    QCOMPARE(requests.first().value(QStringLiteral("method")).toString(),
             QStringLiteral("session_get"));
    QCOMPARE(requests.last().value(QStringLiteral("method")).toString(),
             QStringLiteral("torrent_get"));
    QCOMPARE(backend.protocolDescription(),
             QStringLiteral("Transmission JSON-RPC 2.0"));
    const QVector<torrent> torrents =
        qvariant_cast<QVector<torrent>>(received.first().first());
    QCOMPARE(torrents.size(), 1);
    QCOMPARE(torrents.first().getKey(), QStringLiteral("hash-42"));
    QCOMPARE(torrents.first().getName(), QStringLiteral("Modern torrent"));
    QCOMPARE(torrents.first().getPercentDone(), 75.0);
}

void TestTransmissionBackend::rejectedModernProbeFallsBackToLegacy()
{
    QStringList methods;
    FakeHttpServer server([&](const FakeHttpServer::Request &request) {
        const QJsonObject rpc = QJsonDocument::fromJson(request.body).object();
        const QString method = rpc.value(QStringLiteral("method")).toString();
        methods.append(method);
        if (method == QStringLiteral("session_get"))
            return FakeHttpServer::Response{404, "Not Found", {}, {}};

        const QJsonObject response{
            {QStringLiteral("result"), QStringLiteral("success")},
            {QStringLiteral("arguments"),
             QJsonObject{{QStringLiteral("torrents"), QJsonArray{}}}}
        };
        return FakeHttpServer::Response{
            200, "OK", {{"Content-Type", "application/json"}},
            QJsonDocument(response).toJson(QJsonDocument::Compact)};
    });
    QVERIFY(server.isListening());

    TransmissionBackend backend;
    ServerProfile profile;
    profile.name = QStringLiteral("Legacy Transmission");
    profile.rpcUrl = server.url(QStringLiteral("/rpc")).toString();
    QVERIFY(backend.setServerProfile(profile));
    QSignalSpy received(&backend, &TorrentBackend::torrentsReceived);
    backend.getTorrentList();

    QTRY_COMPARE(received.size(), 1);
    QCOMPARE(methods,
             QStringList({QStringLiteral("session_get"),
                          QStringLiteral("torrent-get")}));
    QCOMPARE(backend.protocolDescription(),
             QStringLiteral("Transmission legacy RPC"));
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
