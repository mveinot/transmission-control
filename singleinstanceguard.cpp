#include "singleinstanceguard.h"

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

SingleInstanceGuard::SingleInstanceGuard(const QString &serverName, QObject *parent)
    : QObject(parent)
    , serverName(serverName)
{
}

SingleInstanceGuard::~SingleInstanceGuard()
{
    if (server) {
        server->close();
        QLocalServer::removeServer(serverName);
    }
}

bool SingleInstanceGuard::tryStartPrimaryInstance()
{
    server = new QLocalServer(this);

    connect(server, &QLocalServer::newConnection,
            this, &SingleInstanceGuard::handleNewConnection);

    if (server->listen(serverName))
        return true;

    /*
     * If listen failed, first check whether another live instance owns
     * the server name. If yes, this is definitely not the primary.
     */
    if (canContactPrimaryInstance()) {
        server->deleteLater();
        server = nullptr;
        return false;
    }

    /*
     * Only now assume the server name is stale from a crash.
     */
    QLocalServer::removeServer(serverName);

    if (server->listen(serverName))
        return true;

    server->deleteLater();
    server = nullptr;

    return false;
}

bool SingleInstanceGuard::notifyPrimaryInstance(const QString &message)
{
    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::WriteOnly);

    if (!socket.waitForConnected(500))
        return false;

    socket.write(message.toUtf8());
    socket.flush();
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();

    return true;
}

bool SingleInstanceGuard::notifyPrimaryInstance(const QStringList &arguments)
{
    if (arguments.isEmpty())
        return notifyPrimaryInstance();

    QJsonArray argumentsArray;

    for (const QString &argument : arguments)
        argumentsArray.append(argument);

    QJsonObject message;
    message.insert(QStringLiteral("type"), QStringLiteral("open"));
    message.insert(QStringLiteral("arguments"), argumentsArray);

    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);

    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::WriteOnly);

    if (!socket.waitForConnected(500))
        return false;

    socket.write(payload);
    socket.flush();
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();

    return true;
}

void SingleInstanceGuard::handleNewConnection()
{
    while (server && server->hasPendingConnections()) {
        QLocalSocket *socket = server->nextPendingConnection();

        auto processMessage = [this](const QByteArray &message) {
            const QByteArray trimmedMessage = message.trimmed();

            if (trimmedMessage.isEmpty())
                return;

            if (trimmedMessage == "activate") {
                emit activationRequested();
                return;
            }

            const QJsonDocument document = QJsonDocument::fromJson(trimmedMessage);

            if (!document.isObject())
                return;

            const QJsonObject object = document.object();
            const QString type = object.value(QStringLiteral("type")).toString();

            if (type != QStringLiteral("open"))
                return;

            const QJsonArray argumentsArray =
                object.value(QStringLiteral("arguments")).toArray();

            QStringList arguments;
            arguments.reserve(argumentsArray.size());

            for (const QJsonValue &value : argumentsArray) {
                const QString argument = value.toString().trimmed();

                if (!argument.isEmpty())
                    arguments.append(argument);
            }

            if (arguments.isEmpty())
                emit activationRequested();
            else
                emit openRequested(arguments);
        };

        connect(socket, &QLocalSocket::readyRead, this, [socket, processMessage]() {
            processMessage(socket->readAll());
            socket->disconnectFromServer();
        });

        connect(socket, &QLocalSocket::disconnected,
                socket, &QLocalSocket::deleteLater);

        /*
         * In case the message is already available before readyRead fires.
         * Because event timing enjoys being adorable.
         */
        QTimer::singleShot(0, socket, [socket, processMessage]() {
            if (socket->bytesAvailable() > 0) {
                processMessage(socket->readAll());
                socket->disconnectFromServer();
            }
        });
    }
}

bool SingleInstanceGuard::canContactPrimaryInstance() const
{
    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::WriteOnly);

    return socket.waitForConnected(300);
}