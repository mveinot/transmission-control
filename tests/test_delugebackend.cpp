#include "delugebackend.h"
#include "settingskeys.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

class FakeDelugeWeb : public QObject
{
    Q_OBJECT

public:
    explicit FakeDelugeWeb(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                    buffers[socket] += socket->readAll();
                    process(socket);
                });
                connect(socket, &QTcpSocket::disconnected,
                        socket, &QObject::deleteLater);
            }
        });
        QVERIFY(server.listen(QHostAddress::LocalHost));
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1")
                        .arg(server.serverPort()));
    }

    bool acceptPassword = true;
    bool daemonConnected = true;
    bool expireFirstConnectionCheck = false;
    bool expireFirstTorrentRequest = false;
    QJsonObject torrents;
    QJsonObject torrentStatus;
    QJsonObject sessionConfig;
    QJsonObject sessionStatus;
    qint64 freeSpace = 0;
    bool portOpen = false;
    QString mismatchMethod;
    QString addedTorrentId = QStringLiteral("added-torrent-id");
    QStringList methods;
    QHash<QString, QJsonArray> parametersByMethod;
    QHash<QString, QList<QJsonArray>> parameterHistory;
    bool connectionCheckHadCookie = false;

private:
    QTcpServer server;
    QHash<QTcpSocket *, QByteArray> buffers;

    void process(QTcpSocket *socket)
    {
        QByteArray &buffer = buffers[socket];
        const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;

        const QByteArray headers = buffer.left(headerEnd);
        qsizetype contentLength = 0;
        for (const QByteArray &line : headers.split('\n')) {
            if (line.toLower().startsWith("content-length:"))
                contentLength = line.mid(15).trimmed().toLongLong();
        }
        const qsizetype bodyStart = headerEnd + 4;
        if (buffer.size() < bodyStart + contentLength)
            return;

        const QJsonObject request =
            QJsonDocument::fromJson(buffer.mid(bodyStart, contentLength))
                .object();
        const QString method = request.value(QStringLiteral("method")).toString();
        methods.append(method);
        const QJsonArray parameters =
            request.value(QStringLiteral("params")).toArray();
        parametersByMethod.insert(method, parameters);
        parameterHistory[method].append(parameters);
        const bool expireThisRequest =
            (method == QStringLiteral("web.connected")
             && expireFirstConnectionCheck
             && methods.count(QStringLiteral("web.connected")) == 1)
            || (method == QStringLiteral("core.get_torrents_status")
                && expireFirstTorrentRequest
                && methods.count(
                    QStringLiteral("core.get_torrents_status")) == 1);
        if (method == QStringLiteral("web.connected")) {
            connectionCheckHadCookie =
                headers.contains("Cookie: _session_id=planetary-test");
        }

        QJsonObject response{
            {QStringLiteral("id"), request.value(QStringLiteral("id"))}
        };
        if (method == mismatchMethod) {
            response.insert(
                QStringLiteral("id"),
                request.value(QStringLiteral("id")).toInt() + 1);
        }
        if (expireThisRequest) {
            response.insert(
                QStringLiteral("error"),
                QJsonObject{
                    {QStringLiteral("code"), 1},
                    {QStringLiteral("message"),
                     QStringLiteral("Not authenticated")}
                });
            response.insert(QStringLiteral("result"), QJsonValue::Null);
        } else {
            response.insert(QStringLiteral("error"), QJsonValue::Null);
            if (method == QStringLiteral("auth.login"))
                response.insert(QStringLiteral("result"), acceptPassword);
            else if (method == QStringLiteral("core.get_torrents_status"))
                response.insert(QStringLiteral("result"), torrents);
            else if (method == QStringLiteral("core.get_torrent_status"))
                response.insert(QStringLiteral("result"), torrentStatus);
            else if (method == QStringLiteral("core.get_config"))
                response.insert(QStringLiteral("result"), sessionConfig);
            else if (method == QStringLiteral("core.get_session_status"))
                response.insert(QStringLiteral("result"), sessionStatus);
            else if (method == QStringLiteral("core.get_free_space"))
                response.insert(QStringLiteral("result"), freeSpace);
            else if (method == QStringLiteral("core.test_listen_port"))
                response.insert(QStringLiteral("result"), portOpen);
            else if (method == QStringLiteral("core.add_torrent_magnet")
                     || method
                            == QStringLiteral("core.add_torrent_file_async")) {
                response.insert(QStringLiteral("result"), addedTorrentId);
            } else if (method == QStringLiteral("core.remove_torrents")) {
                response.insert(QStringLiteral("result"), QJsonArray{});
            }
            else
                response.insert(QStringLiteral("result"), daemonConnected);
        }

        const QByteArray body =
            QJsonDocument(response).toJson(QJsonDocument::Compact);
        QByteArray wire = QByteArrayLiteral("HTTP/1.1 200 OK\r\n")
                          + QByteArrayLiteral("Content-Type: application/json\r\n")
                          + QByteArrayLiteral("Content-Length: ")
                          + QByteArray::number(body.size())
                          + QByteArrayLiteral("\r\n");
        if (method == QStringLiteral("auth.login") && acceptPassword) {
            wire += QByteArrayLiteral(
                "Set-Cookie: _session_id=planetary-test; Path=/; HttpOnly\r\n");
        }
        wire += QByteArrayLiteral("Connection: close\r\n\r\n") + body;
        socket->write(wire);
        socket->disconnectFromHost();
        buffers.remove(socket);
    }
};

class TestDelugeBackend : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void authenticationChecksDaemonAndReusesCookie();
    void torrentListIsNormalized();
    void expiredAuthenticationIsRetriedOnce();
    void expiredAuthenticationDuringTorrentListIsRetriedOnce();
    void coreTorrentControlsUseExpectedRpcMethods();
    void magnetAndFileAddsSendOptionsAndReportSuccess();
    void selectedTorrentDetailsAreNormalized();
    void torrentMutationsUseDelugeCoreMethods();
    void sessionOperationsAreNormalized();
    void duplicateDetailRequestsAreCoalesced();
    void disconnectedDaemonRejectsQueuedCommands();
    void mismatchedCommandReplyReportsFailure();
    void rejectedPasswordReportsAuthenticationFailure();
    void disconnectedDaemonHasDistinctFailure();

private:
    QTemporaryDir settingsDirectory;
    void configureServer(const QUrl &url, const QString &password);
};

void TestDelugeBackend::initTestCase()
{
    QVERIFY(settingsDirectory.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("PlanetaryTests"));
    QCoreApplication::setApplicationName(QStringLiteral("DelugeBackend"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDirectory.path());
}

void TestDelugeBackend::configureServer(const QUrl &url,
                                        const QString &password)
{
    QSettings settings;
    settings.clear();
    settings.beginWriteArray(SettingsKeys::ServersArray);
    settings.setArrayIndex(0);
    settings.setValue(SettingsKeys::ServerBackendType,
                      QStringLiteral("deluge"));
    settings.setValue(SettingsKeys::ServerName,
                      QStringLiteral("Test Deluge"));
    settings.setValue(SettingsKeys::ServerRpcUrl, url.toString());
    settings.setValue(SettingsKeys::ServerPassword, password);
    settings.endArray();
    settings.setValue(SettingsKeys::ServersDefaultIndex, 0);
    settings.sync();
}

void TestDelugeBackend::authenticationChecksDaemonAndReusesCookie()
{
    FakeDelugeWeb server;
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy completed(&backend, &TorrentBackend::updateFinished);
    QSignalSpy failed(&backend, &TorrentBackend::updateFailed);

    backend.init();

    QVERIFY(completed.wait());
    QCOMPARE(failed.count(), 0);
    QCOMPARE(server.methods,
             QStringList({QStringLiteral("auth.login"),
                          QStringLiteral("web.connected"),
                          QStringLiteral("core.get_torrents_status")}));
    QVERIFY(server.connectionCheckHadCookie);
}

void TestDelugeBackend::torrentListIsNormalized()
{
    FakeDelugeWeb server;
    server.torrents.insert(
        QStringLiteral("0123456789abcdef"),
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Linux image")},
            {QStringLiteral("state"), QStringLiteral("Downloading")},
            {QStringLiteral("progress"), 25.0},
            {QStringLiteral("total_wanted"), 1000},
            {QStringLiteral("total_remaining"), 750},
            {QStringLiteral("all_time_download"), 300},
            {QStringLiteral("total_uploaded"), 100},
            {QStringLiteral("download_payload_rate"), 50000},
            {QStringLiteral("upload_payload_rate"), 4000},
            {QStringLiteral("ratio"), 0.33},
            {QStringLiteral("eta"), 15},
            {QStringLiteral("num_seeds"), 2},
            {QStringLiteral("total_seeds"), 10},
            {QStringLiteral("num_peers"), 3},
            {QStringLiteral("total_peers"), 20},
            {QStringLiteral("time_added"), 1700000000},
            {QStringLiteral("download_location"), QStringLiteral("/downloads")},
            {QStringLiteral("queue"), 4},
            {QStringLiteral("tracker"),
             QStringLiteral("https://tracker.example/announce")},
            {QStringLiteral("tracker_host"),
             QStringLiteral("tracker.example")},
            {QStringLiteral("label"), QStringLiteral("Linux")},
            {QStringLiteral("message"), QStringLiteral("OK")},
            {QStringLiteral("is_finished"), false}
        });
    server.torrents.insert(
        QStringLiteral("fedcba9876543210"),
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("Queued seed")},
            {QStringLiteral("state"), QStringLiteral("Queued")},
            {QStringLiteral("progress"), 100.0},
            {QStringLiteral("total_wanted"), 2000},
            {QStringLiteral("total_remaining"), 0},
            {QStringLiteral("is_finished"), true},
            {QStringLiteral("message"), QStringLiteral("OK")}
        });

    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QVector<torrent> snapshot;
    connect(&backend, &TorrentBackend::torrentsReceived,
            this, [&snapshot](const QVector<torrent> &torrents) {
                snapshot = torrents;
            });

    backend.init();

    QTRY_COMPARE(snapshot.size(), 2);
    const auto downloading = std::find_if(
        snapshot.cbegin(), snapshot.cend(), [](const torrent &item) {
            return item.getKey() == QStringLiteral("0123456789abcdef");
        });
    QVERIFY(downloading != snapshot.cend());
    QCOMPARE(downloading->getName(), QStringLiteral("Linux image"));
    QCOMPARE(downloading->getStatusValue(),
             static_cast<int>(torrent::Status::Downloading));
    QCOMPARE(downloading->getPercentDone(), 25.0);
    QCOMPARE(downloading->getSizeBytes(), 1000);
    QCOMPARE(downloading->getRateDownloadBytesPerSecond(), 50000.0);
    QCOMPARE(downloading->getConnectedSeeds(), 2);
    QCOMPARE(downloading->getTotalSeeds(), 10);
    QCOMPARE(downloading->getConnectedPeers(), 3);
    QCOMPARE(downloading->getTotalPeers(), 20);
    QCOMPARE(downloading->getDownloadDir(), QStringLiteral("/downloads"));
    QCOMPARE(downloading->getPrimaryTrackerHost(),
             QStringLiteral("tracker.example"));
    QCOMPARE(downloading->getLabels(), QStringList({QStringLiteral("Linux")}));

    const auto queuedSeed = std::find_if(
        snapshot.cbegin(), snapshot.cend(), [](const torrent &item) {
            return item.getKey() == QStringLiteral("fedcba9876543210");
        });
    QVERIFY(queuedSeed != snapshot.cend());
    QCOMPARE(queuedSeed->getStatusValue(),
             static_cast<int>(torrent::Status::WaitingToSeed));
}

void TestDelugeBackend::rejectedPasswordReportsAuthenticationFailure()
{
    FakeDelugeWeb server;
    server.acceptPassword = false;
    configureServer(server.url(), QStringLiteral("wrong"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy failed(&backend, &TorrentBackend::updateFailed);
    QSignalSpy completed(&backend, &TorrentBackend::updateFinished);

    backend.init();

    QVERIFY(failed.wait());
    QVERIFY(failed.first().first().toString().contains(
        QStringLiteral("authentication failed"), Qt::CaseInsensitive));
    QCOMPARE(completed.count(), 0);
    QCOMPARE(server.methods, QStringList({QStringLiteral("auth.login")}));
}

void TestDelugeBackend::expiredAuthenticationIsRetriedOnce()
{
    FakeDelugeWeb server;
    server.expireFirstConnectionCheck = true;
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy completed(&backend, &TorrentBackend::updateFinished);
    QSignalSpy failed(&backend, &TorrentBackend::updateFailed);

    backend.init();

    QVERIFY(completed.wait());
    QCOMPARE(failed.count(), 0);
    QCOMPARE(server.methods,
             QStringList({QStringLiteral("auth.login"),
                          QStringLiteral("web.connected"),
                          QStringLiteral("auth.login"),
                          QStringLiteral("web.connected"),
                          QStringLiteral("core.get_torrents_status")}));
}

void TestDelugeBackend::expiredAuthenticationDuringTorrentListIsRetriedOnce()
{
    FakeDelugeWeb server;
    server.expireFirstTorrentRequest = true;
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy completed(&backend, &TorrentBackend::updateFinished);
    QSignalSpy failed(&backend, &TorrentBackend::updateFailed);

    backend.init();

    QVERIFY(completed.wait());
    QCOMPARE(failed.count(), 0);
    QCOMPARE(server.methods,
             QStringList({QStringLiteral("auth.login"),
                          QStringLiteral("web.connected"),
                          QStringLiteral("core.get_torrents_status"),
                          QStringLiteral("auth.login"),
                          QStringLiteral("web.connected"),
                          QStringLiteral("core.get_torrents_status")}));
}

void TestDelugeBackend::coreTorrentControlsUseExpectedRpcMethods()
{
    FakeDelugeWeb server;
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy ready(&backend, &TorrentBackend::updateFinished);
    QSignalSpy succeeded(&backend, &TorrentBackend::commandSucceeded);
    backend.init();
    QVERIFY(ready.wait());

    const QList<TorrentKey> keys{
        QStringLiteral("first-id"),
        QStringLiteral("second-id")
    };
    backend.startTorrents(keys);
    backend.stopTorrents(keys);
    backend.removeTorrents(keys, true);
    backend.verifyTorrents(keys);
    backend.reannounceTorrents(keys);

    QTRY_COMPARE(succeeded.count(), 5);
    QVERIFY(server.methods.contains(QStringLiteral("core.resume_torrents")));
    QVERIFY(server.methods.contains(QStringLiteral("core.pause_torrents")));
    QVERIFY(server.methods.contains(QStringLiteral("core.remove_torrents")));
    QVERIFY(server.methods.contains(QStringLiteral("core.force_recheck")));
    QVERIFY(server.methods.contains(QStringLiteral("core.force_reannounce")));
    QCOMPARE(
        server.parametersByMethod.value(
            QStringLiteral("core.resume_torrents")).first().toArray(),
        QJsonArray({QStringLiteral("first-id"),
                    QStringLiteral("second-id")}));
    const QJsonArray removeParameters =
        server.parametersByMethod.value(
            QStringLiteral("core.remove_torrents"));
    QVERIFY(removeParameters.at(1).toBool());
}

void TestDelugeBackend::magnetAndFileAddsSendOptionsAndReportSuccess()
{
    FakeDelugeWeb server;
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy ready(&backend, &TorrentBackend::updateFinished);
    QSignalSpy added(&backend, &TorrentBackend::torrentAdded);
    QSignalSpy fileAdded(&backend,
                         &TorrentBackend::torrentFileAddSucceeded);
    backend.init();
    QVERIFY(ready.wait());

    const QString magnet =
        QStringLiteral("magnet:?xt=urn:btih:0123456789abcdef");
    backend.addMagnetLink(magnet, QStringLiteral("/remote/downloads"), true);
    QTRY_COMPARE(added.count(), 1);

    const QJsonArray magnetParameters =
        server.parametersByMethod.value(
            QStringLiteral("core.add_torrent_magnet"));
    QCOMPARE(magnetParameters.at(0).toString(), magnet);
    const QJsonObject magnetOptions = magnetParameters.at(1).toObject();
    QVERIFY(magnetOptions.value(QStringLiteral("add_paused")).toBool());
    QCOMPARE(magnetOptions.value(QStringLiteral("download_location")).toString(),
             QStringLiteral("/remote/downloads"));

    QTemporaryDir sourceDirectory;
    QVERIFY(sourceDirectory.isValid());
    const QString torrentPath =
        sourceDirectory.filePath(QStringLiteral("sample.torrent"));
    QFile torrentFile(torrentPath);
    QVERIFY(torrentFile.open(QIODevice::WriteOnly));
    const QByteArray torrentBytes("test torrent payload");
    QCOMPARE(torrentFile.write(torrentBytes), torrentBytes.size());
    torrentFile.close();

    backend.addTorrentFile(torrentPath, QStringLiteral("/remote/files"),
                           false, {}, {}, {}, true);
    QTRY_COMPARE(fileAdded.count(), 1);
    QTRY_COMPARE(added.count(), 2);
    QVERIFY(!QFileInfo::exists(torrentPath));

    const QJsonArray fileParameters =
        server.parametersByMethod.value(
            QStringLiteral("core.add_torrent_file_async"));
    QCOMPARE(fileParameters.at(0).toString(),
             QStringLiteral("sample.torrent"));
    QCOMPARE(QByteArray::fromBase64(
                 fileParameters.at(1).toString().toLatin1()),
             torrentBytes);
    QCOMPARE(fileParameters.at(2).toObject()
                 .value(QStringLiteral("download_location")).toString(),
             QStringLiteral("/remote/files"));
}

void TestDelugeBackend::selectedTorrentDetailsAreNormalized()
{
    FakeDelugeWeb server;
    server.torrentStatus = QJsonObject{
        {QStringLiteral("name"), QStringLiteral("Linux ISO")},
        {QStringLiteral("hash"), QStringLiteral("torrent-hash")},
        {QStringLiteral("comment"), QStringLiteral("Release image")},
        {QStringLiteral("creator"), QStringLiteral("mktool")},
        {QStringLiteral("download_location"), QStringLiteral("/srv/downloads")},
        {QStringLiteral("total_size"), 3000},
        {QStringLiteral("time_created"), 12345},
        {QStringLiteral("progress"), 50.0},
        {QStringLiteral("pieces"), QJsonArray{3, 1, 2, 0, 3}},
        {QStringLiteral("files"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("index"), 0},
                 {QStringLiteral("path"), QStringLiteral("image.iso")},
                 {QStringLiteral("size"), 3000}
             }
         }},
        {QStringLiteral("file_progress"), QJsonArray{0.5}},
        {QStringLiteral("file_priorities"), QJsonArray{7}},
        {QStringLiteral("peers"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("ip"), QStringLiteral("192.0.2.4:51413")},
                 {QStringLiteral("client"), QStringLiteral("Peer Client")},
                 {QStringLiteral("progress"), 75.0},
                 {QStringLiteral("down_speed"), 1200},
                 {QStringLiteral("up_speed"), 300}
             }
         }},
        {QStringLiteral("trackers"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("tier"), 0},
                 {QStringLiteral("url"),
                  QStringLiteral("https://tracker.example/announce")}
             }
         }},
        {QStringLiteral("queue"), 4},
        {QStringLiteral("max_connections"), 80},
        {QStringLiteral("max_download_speed"), 250},
        {QStringLiteral("max_upload_speed"), -1},
        {QStringLiteral("stop_ratio"), 2.0},
        {QStringLiteral("label"), QStringLiteral("Linux")}
    };
    configureServer(server.url(), QStringLiteral("correct"));

    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy detailsSpy(&backend, &TorrentBackend::torrentDetailsReceived);
    QSignalSpy filesSpy(&backend, &TorrentBackend::torrentFilesReceived);
    QSignalSpy peersSpy(&backend, &TorrentBackend::torrentPeersReceived);
    QSignalSpy trackersSpy(&backend, &TorrentBackend::torrentTrackersReceived);
    QSignalSpy piecesSpy(&backend, &TorrentBackend::torrentPiecesReceived);
    QSignalSpy propertiesSpy(
        &backend, &TorrentBackend::torrentPropertiesReceived);

    backend.getTorrentDetails(QStringLiteral("torrent-hash"));
    backend.getTorrentFiles(QStringLiteral("torrent-hash"));
    backend.getTorrentPeers(QStringLiteral("torrent-hash"));
    backend.getTorrentTrackers(QStringLiteral("torrent-hash"));
    backend.getTorrentPieces(QStringLiteral("torrent-hash"));
    backend.getTorrentProperties(QStringLiteral("torrent-hash"));

    QTRY_COMPARE(detailsSpy.size(), 1);
    QTRY_COMPARE(filesSpy.size(), 1);
    QTRY_COMPARE(peersSpy.size(), 1);
    QTRY_COMPARE(trackersSpy.size(), 1);
    QTRY_COMPARE(piecesSpy.size(), 1);
    QTRY_COMPARE(propertiesSpy.size(), 1);

    const TorrentDetails details =
        qvariant_cast<TorrentDetails>(detailsSpy.first().first());
    QCOMPARE(details.key, QStringLiteral("torrent-hash"));
    QCOMPARE(details.name, QStringLiteral("Linux ISO"));
    QCOMPARE(details.downloadDirectory, QStringLiteral("/srv/downloads"));
    QCOMPARE(details.totalSize, 3000);

    const TorrentFiles files =
        qvariant_cast<TorrentFiles>(filesSpy.first().first());
    QCOMPARE(files.files.size(), 1);
    QCOMPARE(files.files.first().bytesCompleted, 1500);
    QCOMPARE(files.files.first().priority, 1);

    const TorrentPeers peers =
        qvariant_cast<TorrentPeers>(peersSpy.first().first());
    QCOMPARE(peers.peers.size(), 1);
    QCOMPARE(peers.peers.first().address, QStringLiteral("192.0.2.4"));
    QCOMPARE(peers.peers.first().port, 51413);
    QCOMPARE(peers.peers.first().progress, 0.75);

    const TorrentTrackers trackers =
        qvariant_cast<TorrentTrackers>(trackersSpy.first().first());
    QCOMPARE(trackers.trackers.first().host,
             QStringLiteral("tracker.example"));

    const TorrentPieces pieces =
        qvariant_cast<TorrentPieces>(piecesSpy.first().first());
    QCOMPARE(pieces.pieceCount, 5);
    QCOMPARE(static_cast<uchar>(pieces.completedPieces.at(0)),
             static_cast<uchar>(0x88));
    QCOMPARE(pieces.percentDone, 0.5);

    const TorrentProperties properties =
        qvariant_cast<TorrentProperties>(propertiesSpy.first().first());
    QCOMPARE(properties.peerLimit, 80);
    QVERIFY(properties.downloadLimited);
    QVERIFY(!properties.uploadLimited);
    QCOMPARE(properties.labels, QStringList{QStringLiteral("Linux")});
}

void TestDelugeBackend::torrentMutationsUseDelugeCoreMethods()
{
    FakeDelugeWeb server;
    server.torrentStatus = QJsonObject{
        {QStringLiteral("files"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("index"), 0},
                 {QStringLiteral("path"), QStringLiteral("folder/a.bin")},
                 {QStringLiteral("size"), 100}
             },
             QJsonObject{
                 {QStringLiteral("index"), 1},
                 {QStringLiteral("path"), QStringLiteral("folder/b.bin")},
                 {QStringLiteral("size"), 200}
             }
         }},
        {QStringLiteral("file_progress"), QJsonArray{1.0, 0.5}},
        {QStringLiteral("file_priorities"), QJsonArray{4, 4}},
        {QStringLiteral("trackers"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("tier"), 0},
                 {QStringLiteral("url"),
                  QStringLiteral("https://old.example/announce")}
             }
         }},
        {QStringLiteral("name"), QStringLiteral("Test")},
        {QStringLiteral("hash"), QStringLiteral("torrent-hash")},
        {QStringLiteral("max_connections"), 50},
        {QStringLiteral("max_download_speed"), -1},
        {QStringLiteral("max_upload_speed"), -1},
        {QStringLiteral("stop_ratio"), -1}
    };
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy filesSpy(&backend, &TorrentBackend::torrentFilesReceived);
    QSignalSpy trackersSpy(&backend, &TorrentBackend::torrentTrackersReceived);
    QSignalSpy propertiesSpy(
        &backend, &TorrentBackend::torrentPropertiesReceived);
    QSignalSpy succeeded(&backend, &TorrentBackend::commandSucceeded);

    backend.getTorrentFiles(QStringLiteral("torrent-hash"));
    backend.getTorrentTrackers(QStringLiteral("torrent-hash"));
    backend.getTorrentProperties(QStringLiteral("torrent-hash"));
    QTRY_COMPARE(filesSpy.size(), 1);
    QTRY_COMPARE(trackersSpy.size(), 1);
    QTRY_COMPARE(propertiesSpy.size(), 1);

    backend.setTorrentFilesWanted(QStringLiteral("torrent-hash"), {1}, false);
    backend.setTorrentFilesPriority(QStringLiteral("torrent-hash"), {0}, 1);
    backend.setTorrentLocation({QStringLiteral("torrent-hash")},
                               QStringLiteral("/new/location"), true);
    backend.editTorrentTracker(QStringLiteral("torrent-hash"), 0,
                               QStringLiteral("https://new.example/announce"));
    backend.renameTorrentPath(QStringLiteral("torrent-hash"),
                              QStringLiteral("folder/a.bin"),
                              QStringLiteral("renamed.bin"));
    backend.setTorrentsSequentialDownload(
        {QStringLiteral("torrent-hash")}, true);
    backend.queueMoveTop({QStringLiteral("torrent-hash")});

    TorrentPropertyChanges changes;
    changes.peerLimit = 75;
    changes.downloadLimited = true;
    changes.downloadLimit = 500;
    changes.uploadLimited = false;
    changes.seedRatioMode = 1;
    changes.seedRatioLimit = 2.5;
    backend.setTorrentProperties(QStringLiteral("torrent-hash"), changes);

    QTRY_COMPARE(succeeded.size(), 8);
    QVERIFY(server.methods.contains(QStringLiteral("core.move_storage")));
    QVERIFY(server.methods.contains(
        QStringLiteral("core.set_torrent_trackers")));
    QVERIFY(server.methods.contains(QStringLiteral("core.rename_files")));
    QVERIFY(server.methods.contains(QStringLiteral("core.queue_top")));

    const QList<QJsonArray> optionCalls =
        server.parameterHistory.value(
            QStringLiteral("core.set_torrent_options"));
    QCOMPARE(optionCalls.size(), 4);
    bool sawUnwanted = false;
    bool sawHighPriority = false;
    bool sawSequential = false;
    bool sawProperties = false;
    for (const QJsonArray &call : optionCalls) {
        const QJsonObject options = call.at(1).toObject();
        const QJsonArray priorities =
            options.value(QStringLiteral("file_priorities")).toArray();
        sawUnwanted |= priorities == QJsonArray({4, 0});
        sawHighPriority |= priorities == QJsonArray({7, 0});
        sawSequential |=
            options.value(QStringLiteral("sequential_download")).toBool();
        sawProperties |=
            options.value(QStringLiteral("max_connections")).toInt() == 75;
    }
    QVERIFY(sawUnwanted);
    QVERIFY(sawHighPriority);
    QVERIFY(sawSequential);
    QVERIFY(sawProperties);

    const QJsonArray renameParameters =
        server.parametersByMethod.value(QStringLiteral("core.rename_files"));
    QCOMPARE(renameParameters.at(1).toArray().first().toArray(),
             QJsonArray({0, QStringLiteral("folder/renamed.bin")}));
}

void TestDelugeBackend::sessionOperationsAreNormalized()
{
    FakeDelugeWeb server;
    server.sessionConfig = QJsonObject{
        {QStringLiteral("listen_ports"), QJsonArray{52000, 52000}},
        {QStringLiteral("random_port"), false},
        {QStringLiteral("upnp"), true},
        {QStringLiteral("dht"), true},
        {QStringLiteral("utpex"), true},
        {QStringLiteral("lsd"), false},
        {QStringLiteral("max_connections_global"), 300},
        {QStringLiteral("max_connections_per_torrent"), 60},
        {QStringLiteral("max_download_speed"), 1200.0},
        {QStringLiteral("max_upload_speed"), -1.0},
        {QStringLiteral("max_active_downloading"), 4},
        {QStringLiteral("max_active_seeding"), -1},
        {QStringLiteral("download_location"), QStringLiteral("/downloads")},
        {QStringLiteral("add_paused"), true},
        {QStringLiteral("enc_in_policy"), 0},
        {QStringLiteral("enc_out_policy"), 0}
    };
    server.sessionStatus = QJsonObject{
        {QStringLiteral("total_payload_download"), 123456},
        {QStringLiteral("total_payload_upload"), 654321}
    };
    server.freeSpace = 987654321;
    server.portOpen = true;
    configureServer(server.url(), QStringLiteral("correct"));

    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy settingsSpy(
        &backend, &TorrentBackend::sessionSettingsReceived);
    QSignalSpy statisticsSpy(
        &backend, &TorrentBackend::sessionStatisticsReceived);
    QSignalSpy freeSpaceSpy(&backend, &TorrentBackend::freeSpaceReceived);
    QSignalSpy portSpy(&backend, &TorrentBackend::portTestFinished);
    QSignalSpy succeeded(&backend, &TorrentBackend::commandSucceeded);

    backend.getSessionSettings();
    backend.getSessionStatistics();
    backend.getFreeSpace(QStringLiteral("/downloads"));
    backend.testPortForwarding();
    QTRY_COMPARE(settingsSpy.size(), 1);
    QTRY_COMPARE(statisticsSpy.size(), 1);
    QTRY_COMPARE(freeSpaceSpy.size(), 1);
    QTRY_COMPARE(portSpy.size(), 1);

    const QJsonObject settings =
        settingsSpy.first().first().toJsonObject();
    QCOMPARE(settings.value(QStringLiteral("peer-port")).toInt(), 52000);
    QCOMPARE(settings.value(QStringLiteral("download-dir")).toString(),
             QStringLiteral("/downloads"));
    QVERIFY(!settings.value(
        QStringLiteral("start-added-torrents")).toBool());
    QCOMPARE(settings.value(QStringLiteral("encryption")).toString(),
             QStringLiteral("required"));
    QVERIFY(settings.value(
        QStringLiteral("speed-limit-down-enabled")).toBool());
    QVERIFY(!settings.value(
        QStringLiteral("speed-limit-up-enabled")).toBool());

    const QJsonObject statistics =
        statisticsSpy.first().first().toJsonObject();
    QCOMPARE(statistics.value(QStringLiteral("current-stats")).toObject()
                 .value(QStringLiteral("downloadedBytes"))
                 .toInt(),
             123456);
    QCOMPARE(freeSpaceSpy.first().at(0).toString(),
             QStringLiteral("/downloads"));
    QCOMPARE(freeSpaceSpy.first().at(1).toLongLong(), 987654321);
    QVERIFY(portSpy.first().at(0).toBool());

    backend.setSessionSettings(
        QJsonObject{
            {QStringLiteral("peer-port"), 53000},
            {QStringLiteral("start-added-torrents"), true},
            {QStringLiteral("speed-limit-down-enabled"), false},
            {QStringLiteral("download-queue-enabled"), false},
            {QStringLiteral("encryption"), QStringLiteral("disabled")}
        });
    QTRY_COMPARE(succeeded.size(), 1);
    const QJsonObject native =
        server.parametersByMethod.value(QStringLiteral("core.set_config"))
            .first().toObject();
    QCOMPARE(native.value(QStringLiteral("listen_ports")).toArray(),
             QJsonArray({53000, 53000}));
    QVERIFY(!native.value(QStringLiteral("add_paused")).toBool());
    QCOMPARE(native.value(QStringLiteral("max_download_speed")).toDouble(),
             -1.0);
    QCOMPARE(native.value(QStringLiteral("max_active_downloading")).toInt(),
             -1);
    QCOMPARE(native.value(QStringLiteral("enc_in_policy")).toInt(), 2);
}

void TestDelugeBackend::duplicateDetailRequestsAreCoalesced()
{
    FakeDelugeWeb server;
    server.torrentStatus = QJsonObject{
        {QStringLiteral("pieces"), QJsonArray{3, 0}},
        {QStringLiteral("progress"), 50.0}
    };
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy piecesSpy(&backend, &TorrentBackend::torrentPiecesReceived);

    backend.getTorrentPieces(QStringLiteral("torrent-hash"));
    backend.getTorrentPieces(QStringLiteral("torrent-hash"));

    QTRY_COMPARE(piecesSpy.size(), 1);
    QCOMPARE(server.methods.count(
                 QStringLiteral("core.get_torrent_status")),
             1);
}

void TestDelugeBackend::disconnectedDaemonRejectsQueuedCommands()
{
    FakeDelugeWeb server;
    server.daemonConnected = false;
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy failed(&backend, &TorrentBackend::commandFailed);

    backend.startTorrents({QStringLiteral("torrent-hash")});

    QTRY_COMPARE(failed.size(), 1);
    QCOMPARE(failed.first().at(0).toString(),
             QStringLiteral("torrent-start"));
    QVERIFY(!server.methods.contains(
        QStringLiteral("core.resume_torrents")));
}

void TestDelugeBackend::mismatchedCommandReplyReportsFailure()
{
    FakeDelugeWeb server;
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy ready(&backend, &TorrentBackend::updateFinished);
    QSignalSpy failed(&backend, &TorrentBackend::commandFailed);
    backend.init();
    QVERIFY(ready.wait());

    server.mismatchMethod = QStringLiteral("core.resume_torrents");
    backend.startTorrents({QStringLiteral("torrent-hash")});

    QTRY_COMPARE(failed.size(), 1);
    QCOMPARE(failed.first().at(0).toString(),
             QStringLiteral("torrent-start"));
}

void TestDelugeBackend::disconnectedDaemonHasDistinctFailure()
{
    FakeDelugeWeb server;
    server.daemonConnected = false;
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.setServerProfile(
        ServerProfileRepository().profileAtSettingsIndex(0)));
    QSignalSpy failed(&backend, &TorrentBackend::updateFailed);
    QSignalSpy completed(&backend, &TorrentBackend::updateFinished);

    backend.init();

    QVERIFY(failed.wait());
    const QString message = failed.first().first().toString();
    QVERIFY(message.contains(QStringLiteral("authenticated"),
                             Qt::CaseInsensitive));
    QVERIFY(message.contains(QStringLiteral("not connected"),
                             Qt::CaseInsensitive));
    QCOMPARE(completed.count(), 0);
}

QTEST_MAIN(TestDelugeBackend)
#include "test_delugebackend.moc"
