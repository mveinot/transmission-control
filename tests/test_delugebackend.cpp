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
    QStringList methods;
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
        const bool expireThisRequest =
            method == QStringLiteral("web.connected")
            && expireFirstConnectionCheck
            && methods.count(QStringLiteral("web.connected")) == 1;
        if (method == QStringLiteral("web.connected")) {
            connectionCheckHadCookie =
                headers.contains("Cookie: _session_id=planetary-test");
        }

        QJsonObject response{
            {QStringLiteral("id"), request.value(QStringLiteral("id"))}
        };
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
    void expiredAuthenticationIsRetriedOnce();
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
    QVERIFY(backend.loadCurrentServerFromSettings());
    QSignalSpy completed(&backend, &TorrentBackend::updateFinished);
    QSignalSpy failed(&backend, &TorrentBackend::updateFailed);

    backend.init();

    QVERIFY(completed.wait());
    QCOMPARE(failed.count(), 0);
    QCOMPARE(server.methods,
             QStringList({QStringLiteral("auth.login"),
                          QStringLiteral("web.connected")}));
    QVERIFY(server.connectionCheckHadCookie);
}

void TestDelugeBackend::rejectedPasswordReportsAuthenticationFailure()
{
    FakeDelugeWeb server;
    server.acceptPassword = false;
    configureServer(server.url(), QStringLiteral("wrong"));
    DelugeBackend backend;
    QVERIFY(backend.loadCurrentServerFromSettings());
    QSignalSpy failed(&backend, &TorrentBackend::updateFailed);

    backend.init();

    QVERIFY(failed.wait());
    QVERIFY(failed.first().first().toString().contains(
        QStringLiteral("authentication failed"), Qt::CaseInsensitive));
    QCOMPARE(server.methods, QStringList({QStringLiteral("auth.login")}));
}

void TestDelugeBackend::expiredAuthenticationIsRetriedOnce()
{
    FakeDelugeWeb server;
    server.expireFirstConnectionCheck = true;
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.loadCurrentServerFromSettings());
    QSignalSpy completed(&backend, &TorrentBackend::updateFinished);
    QSignalSpy failed(&backend, &TorrentBackend::updateFailed);

    backend.init();

    QVERIFY(completed.wait());
    QCOMPARE(failed.count(), 0);
    QCOMPARE(server.methods,
             QStringList({QStringLiteral("auth.login"),
                          QStringLiteral("web.connected"),
                          QStringLiteral("auth.login"),
                          QStringLiteral("web.connected")}));
}

void TestDelugeBackend::disconnectedDaemonHasDistinctFailure()
{
    FakeDelugeWeb server;
    server.daemonConnected = false;
    configureServer(server.url(), QStringLiteral("correct"));
    DelugeBackend backend;
    QVERIFY(backend.loadCurrentServerFromSettings());
    QSignalSpy failed(&backend, &TorrentBackend::updateFailed);

    backend.init();

    QVERIFY(failed.wait());
    const QString message = failed.first().first().toString();
    QVERIFY(message.contains(QStringLiteral("authenticated"),
                             Qt::CaseInsensitive));
    QVERIFY(message.contains(QStringLiteral("not connected"),
                             Qt::CaseInsensitive));
}

QTEST_MAIN(TestDelugeBackend)
#include "test_delugebackend.moc"
