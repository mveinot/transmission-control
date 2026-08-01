#include "serverconnectionprobe.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

class FakeConnectionServer : public QObject
{
    Q_OBJECT

public:
    enum class Mode {
        TransmissionCorrection,
        HtmlOnly,
        AuthenticationFailure,
        QBittorrent,
        Deluge
    };

    explicit FakeConnectionServer(Mode mode, QObject *parent = nullptr)
        : QObject(parent)
        , m_mode(mode)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead,
                        this, [this, socket]() { process(socket); });
                connect(socket, &QTcpSocket::disconnected,
                        socket, &QObject::deleteLater);
            }
        });
        QVERIFY(m_server.listen(QHostAddress::LocalHost));
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1")
                        .arg(m_server.serverPort()));
    }

    int requestCount() const { return m_requestCount; }

private:
    QTcpServer m_server;
    Mode m_mode;
    int m_requestCount = 0;
    QHash<QTcpSocket *, QByteArray> m_buffers;

    void process(QTcpSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        buffer += socket->readAll();
        const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;

        const QByteArray headers = buffer.left(headerEnd);
        qsizetype contentLength = 0;
        for (const QByteArray &line : headers.split('\n')) {
            if (line.toLower().startsWith("content-length:"))
                contentLength = line.mid(15).trimmed().toLongLong();
        }
        const qsizetype bodyOffset = headerEnd + 4;
        if (buffer.size() < bodyOffset + contentLength)
            return;

        const QByteArray body = buffer.mid(bodyOffset, contentLength);
        const QByteArray requestLine = headers.left(headers.indexOf("\r\n"));
        ++m_requestCount;

        int status = 200;
        QByteArray response;
        QByteArray extraHeaders;
        if (m_mode == Mode::AuthenticationFailure) {
            status = 401;
        } else if (m_mode == Mode::HtmlOnly) {
            response = QByteArrayLiteral("<html>not an RPC response</html>");
        } else if (m_mode == Mode::TransmissionCorrection) {
            if (!requestLine.contains("/transmission/rpc")) {
                response = QByteArrayLiteral("<html>server home</html>");
            } else if (!headers.contains("X-Transmission-Session-Id: token")) {
                status = 409;
                extraHeaders = QByteArrayLiteral(
                    "X-Transmission-Session-Id: token\r\n");
            } else {
                response = QByteArrayLiteral(
                    "{\"result\":\"success\",\"arguments\":{}}");
            }
        } else if (m_mode == Mode::QBittorrent) {
            response = requestLine.contains("/api/v2/auth/login")
                           ? QByteArrayLiteral("Ok.")
                           : QByteArrayLiteral("unexpected");
        } else {
            const QString method = QJsonDocument::fromJson(body)
                                       .object()
                                       .value(QStringLiteral("method"))
                                       .toString();
            const bool validMethod = method == QStringLiteral("auth.login")
                                     || method == QStringLiteral("web.connected");
            response = QJsonDocument(
                           QJsonObject{{QStringLiteral("result"), validMethod},
                                       {QStringLiteral("id"), 1}})
                           .toJson(QJsonDocument::Compact);
        }

        const QByteArray reason = status == 200 ? QByteArrayLiteral("OK")
                                  : status == 401 ? QByteArrayLiteral("Unauthorized")
                                                  : QByteArrayLiteral("Conflict");
        const QByteArray httpResponse =
            QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status) + ' '
            + reason + QByteArrayLiteral("\r\nContent-Type: application/json\r\n")
            + extraHeaders + QByteArrayLiteral("Content-Length: ")
            + QByteArray::number(response.size())
            + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + response;
        socket->write(httpResponse);
        socket->disconnectFromHost();
        m_buffers.remove(socket);
    }
};

class TestServerConnectionProbe : public QObject
{
    Q_OBJECT

private slots:
    void transmissionCandidates();
    void transmissionPartialPath();
    void qbittorrentCandidates();
    void qbittorrentApiEndpointCandidate();
    void delugeCandidates();
    void preservesExplicitPorts();
    void rejectsUnsupportedUrls();
    void correctsTransmissionPathAfterValidatedFailure();
    void rejectsTransmissionHtmlFalsePositive();
    void doesNotGuessAfterAuthenticationFailure();
    void validatesQBittorrentLogin();
    void validatesDelugeLoginAndDaemon();
};

void TestServerConnectionProbe::transmissionCandidates()
{
    const QList<QUrl> candidates = ServerConnectionProbe::candidateUrls(
        QStringLiteral("transmission"), QUrl(QStringLiteral("http://nas")));

    QCOMPARE(candidates,
             QList<QUrl>({QUrl(QStringLiteral("http://nas")),
                          QUrl(QStringLiteral("http://nas/transmission/rpc")),
                          QUrl(QStringLiteral("http://nas:9091")),
                          QUrl(QStringLiteral(
                              "http://nas:9091/transmission/rpc"))}));
}

void TestServerConnectionProbe::transmissionPartialPath()
{
    const QList<QUrl> candidates = ServerConnectionProbe::candidateUrls(
        QStringLiteral("transmission"),
        QUrl(QStringLiteral("https://nas/transmission")));

    QCOMPARE(candidates.at(1),
             QUrl(QStringLiteral("https://nas/transmission/rpc")));
    QCOMPARE(candidates.at(2), QUrl(QStringLiteral("https://nas:9091/transmission")));
    QCOMPARE(candidates.at(3),
             QUrl(QStringLiteral("https://nas:9091/transmission/rpc")));
}

void TestServerConnectionProbe::qbittorrentCandidates()
{
    const QList<QUrl> candidates = ServerConnectionProbe::candidateUrls(
        QStringLiteral("qbittorrent"),
        QUrl(QStringLiteral("http://downloads")));

    QCOMPARE(candidates,
             QList<QUrl>({QUrl(QStringLiteral("http://downloads")),
                          QUrl(QStringLiteral("http://downloads:8080"))}));
}

void TestServerConnectionProbe::qbittorrentApiEndpointCandidate()
{
    const QList<QUrl> candidates = ServerConnectionProbe::candidateUrls(
        QStringLiteral("qbittorrent"),
        QUrl(QStringLiteral(
            "http://downloads/api/v2/auth/login")));

    QVERIFY(candidates.contains(QUrl(QStringLiteral("http://downloads"))));
    QVERIFY(candidates.contains(QUrl(QStringLiteral("http://downloads:8080"))));
}

void TestServerConnectionProbe::delugeCandidates()
{
    const QList<QUrl> candidates = ServerConnectionProbe::candidateUrls(
        QStringLiteral("deluge"), QUrl(QStringLiteral("http://seedbox")));

    QCOMPARE(candidates,
             QList<QUrl>({QUrl(QStringLiteral("http://seedbox")),
                          QUrl(QStringLiteral("http://seedbox:8112"))}));
}

void TestServerConnectionProbe::preservesExplicitPorts()
{
    const QList<QUrl> candidates = ServerConnectionProbe::candidateUrls(
        QStringLiteral("transmission"),
        QUrl(QStringLiteral("https://seedbox:443/custom/rpc")));

    QCOMPARE(candidates.size(), 2);
    QCOMPARE(candidates.constFirst(),
             QUrl(QStringLiteral("https://seedbox:443/custom/rpc")));
    QCOMPARE(candidates.constLast(),
             QUrl(QStringLiteral(
                 "https://seedbox:443/custom/rpc/transmission/rpc")));
}

void TestServerConnectionProbe::rejectsUnsupportedUrls()
{
    QVERIFY(ServerConnectionProbe::candidateUrls(
                QStringLiteral("transmission"),
                QUrl(QStringLiteral("ftp://nas/transmission/rpc")))
                .isEmpty());
    QVERIFY(ServerConnectionProbe::candidateUrls(
                QStringLiteral("transmission"),
                QUrl(QStringLiteral("http:///transmission/rpc")))
                .isEmpty());
}

void TestServerConnectionProbe::correctsTransmissionPathAfterValidatedFailure()
{
    FakeConnectionServer server(FakeConnectionServer::Mode::TransmissionCorrection);
    ServerConnectionProbe probe;
    QSignalSpy success(&probe, &ServerConnectionProbe::connectionSucceeded);

    probe.start(QStringLiteral("transmission"), server.url(), {}, {});

    QVERIFY(success.wait());
    QCOMPARE(success.constFirst().at(0).toUrl(),
             QUrl(server.url().toString() + QStringLiteral("/transmission/rpc")));
    QVERIFY(success.constFirst().at(1).toBool());
    QCOMPARE(server.requestCount(), 3);
}

void TestServerConnectionProbe::rejectsTransmissionHtmlFalsePositive()
{
    FakeConnectionServer server(FakeConnectionServer::Mode::HtmlOnly);
    ServerConnectionProbe probe;
    QSignalSpy success(&probe, &ServerConnectionProbe::connectionSucceeded);
    QSignalSpy failure(&probe, &ServerConnectionProbe::connectionFailed);

    probe.start(QStringLiteral("transmission"), server.url(), {}, {});

    QVERIFY(failure.wait());
    QCOMPARE(success.count(), 0);
    QCOMPARE(server.requestCount(), 2);
}

void TestServerConnectionProbe::doesNotGuessAfterAuthenticationFailure()
{
    FakeConnectionServer server(FakeConnectionServer::Mode::AuthenticationFailure);
    ServerConnectionProbe probe;
    QSignalSpy failure(&probe, &ServerConnectionProbe::connectionFailed);

    probe.start(QStringLiteral("transmission"), server.url(),
                QStringLiteral("user"), QStringLiteral("wrong"));

    QVERIFY(failure.wait());
    QCOMPARE(server.requestCount(), 1);
    QCOMPARE(failure.constFirst().at(1).value<ServerConnectionProbe::FailureKind>(),
             ServerConnectionProbe::FailureKind::Authentication);
}

void TestServerConnectionProbe::validatesQBittorrentLogin()
{
    FakeConnectionServer server(FakeConnectionServer::Mode::QBittorrent);
    ServerConnectionProbe probe;
    QSignalSpy success(&probe, &ServerConnectionProbe::connectionSucceeded);

    probe.start(QStringLiteral("qbittorrent"), server.url(),
                QStringLiteral("user"), QStringLiteral("password"));

    QVERIFY(success.wait());
    QVERIFY(!success.constFirst().at(1).toBool());
}

void TestServerConnectionProbe::validatesDelugeLoginAndDaemon()
{
    FakeConnectionServer server(FakeConnectionServer::Mode::Deluge);
    ServerConnectionProbe probe;
    QSignalSpy success(&probe, &ServerConnectionProbe::connectionSucceeded);

    probe.start(QStringLiteral("deluge"), server.url(), {},
                QStringLiteral("password"));

    QVERIFY(success.wait());
    QVERIFY(!success.constFirst().at(1).toBool());
    QCOMPARE(server.requestCount(), 2);
}

QTEST_MAIN(TestServerConnectionProbe)
#include "test_serverconnectionprobe.moc"
