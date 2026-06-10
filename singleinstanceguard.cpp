#include "singleinstanceguard.h"

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

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
     * A stale local-server socket can be left behind after a crash.
     * Try removing it, then listen again. If another app is genuinely
     * running, this second listen should still fail.
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

void SingleInstanceGuard::handleNewConnection()
{
    while (server && server->hasPendingConnections()) {
        QLocalSocket *socket = server->nextPendingConnection();

        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            const QByteArray message = socket->readAll();

            if (message.trimmed() == "activate")
                emit activationRequested();
        });

        connect(socket, &QLocalSocket::disconnected,
                socket, &QLocalSocket::deleteLater);

        /*
         * In case the message is already available before readyRead fires.
         * Because event timing enjoys being adorable.
         */
        QTimer::singleShot(0, socket, [this, socket]() {
            if (socket->bytesAvailable() > 0) {
                const QByteArray message = socket->readAll();

                if (message.trimmed() == "activate")
                    emit activationRequested();
            }

            socket->disconnectFromServer();
        });
    }
}