#ifndef FAKEHTTPSERVER_H
#define FAKEHTTPSERVER_H

#include <QHash>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

#include <functional>

// Minimal one-request-per-connection HTTP server for backend protocol tests.
// It deliberately implements only framing needed by Qt's network client.
class FakeHttpServer : public QObject
{
public:
    struct Request {
        QByteArray method;
        QByteArray target;
        QHash<QByteArray, QByteArray> headers;
        QByteArray body;
    };

    struct Response {
        int status = 200;
        QByteArray reason = QByteArrayLiteral("OK");
        QHash<QByteArray, QByteArray> headers;
        QByteArray body;
    };

    using Responder = std::function<Response(const Request &)>;

    explicit FakeHttpServer(Responder responder, QObject *parent = nullptr)
        : QObject(parent)
        , m_responder(std::move(responder))
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this,
                        [this, socket]() {
                    m_buffers[socket] += socket->readAll();
                    process(socket);
                });
                connect(socket, &QTcpSocket::disconnected,
                        socket, &QObject::deleteLater);
            }
        });
        m_listening = m_server.listen(QHostAddress::LocalHost);
    }

    bool isListening() const { return m_listening; }

    QUrl url(const QString &path = QString()) const
    {
        QUrl result(QStringLiteral("http://127.0.0.1:%1")
                        .arg(m_server.serverPort()));
        result.setPath(path);
        return result;
    }

private:
    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    Responder m_responder;
    bool m_listening = false;

    void process(QTcpSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;

        const QList<QByteArray> lines =
            buffer.left(headerEnd).split('\n');
        if (lines.isEmpty())
            return;

        Request request;
        const QList<QByteArray> requestLine =
            lines.first().trimmed().split(' ');
        if (requestLine.size() >= 2) {
            request.method = requestLine.at(0);
            request.target = requestLine.at(1);
        }

        qsizetype contentLength = 0;
        for (qsizetype index = 1; index < lines.size(); ++index) {
            const QByteArray line = lines.at(index).trimmed();
            const qsizetype separator = line.indexOf(':');
            if (separator < 0)
                continue;
            const QByteArray name = line.left(separator).trimmed().toLower();
            const QByteArray value = line.mid(separator + 1).trimmed();
            request.headers.insert(name, value);
            if (name == QByteArrayLiteral("content-length"))
                contentLength = value.toLongLong();
        }

        const qsizetype bodyStart = headerEnd + 4;
        if (buffer.size() < bodyStart + contentLength)
            return;
        request.body = buffer.mid(bodyStart, contentLength);

        const Response response = m_responder(request);
        QByteArray wire = QByteArrayLiteral("HTTP/1.1 ")
                          + QByteArray::number(response.status)
                          + QByteArrayLiteral(" ") + response.reason
                          + QByteArrayLiteral("\r\nContent-Length: ")
                          + QByteArray::number(response.body.size())
                          + QByteArrayLiteral("\r\n");
        for (auto it = response.headers.cbegin();
             it != response.headers.cend(); ++it) {
            wire += it.key() + QByteArrayLiteral(": ") + it.value()
                    + QByteArrayLiteral("\r\n");
        }
        wire += QByteArrayLiteral("Connection: close\r\n\r\n")
                + response.body;
        socket->write(wire);
        socket->disconnectFromHost();
        m_buffers.remove(socket);
    }
};

#endif // FAKEHTTPSERVER_H
