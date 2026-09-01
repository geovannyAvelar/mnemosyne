#include "SingleInstanceGuard.h"

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>

namespace {
// QLocalServer already scopes local sockets per-user on all three platforms
// (a Unix domain socket under a user-private runtime directory on
// Linux/macOS, a named pipe namespaced to the caller's session on Windows),
// so this can't collide across different users on the same machine.
// Mixing in QCoreApplication::applicationName() keeps it from colliding with
// a real running Mnemosyne either -- SingleInstanceGuardTest sets a
// different application name (same trick AppPersistenceTest uses for
// QSettings) precisely so it never talks to, or steals files from, an
// actual instance on the machine running the test.
QString serverName()
{
    return QStringLiteral("MnemosyneSingleInstance-") + QCoreApplication::applicationName();
}
}

SingleInstanceGuard::SingleInstanceGuard(QObject *parent)
    : QObject(parent)
{
}

bool SingleInstanceGuard::tryBecomePrimary(const QStringList &filesToOpen)
{
    // Probe for a live primary first -- a short timeout is plenty since this
    // is a local socket with nothing but another process' event loop on the
    // other end.
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (socket.waitForConnected(250)) {
        QByteArray payload;
        for (const QString &path : filesToOpen) {
            payload += path.toUtf8();
            payload += '\n';
        }
        socket.write(payload);
        socket.waitForBytesWritten(250);
        socket.disconnectFromServer();
        return false;
    }

    // No primary answered. Either this is the first instance, or a previous
    // one crashed and left its socket file behind (Linux/macOS only --
    // Windows named pipes are cleaned up by the OS when the owning process
    // dies). removeServer() clears any such stale file so listen() below
    // doesn't fail against a socket nothing is actually listening on.
    QLocalServer::removeServer(serverName());

    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstanceGuard::handleNewConnection);
    if (!m_server->listen(serverName())) {
        // Extremely unlikely (e.g. a permissions issue on the runtime
        // directory) -- fall back to running unguarded rather than refusing
        // to open at all.
        delete m_server;
        m_server = nullptr;
    }
    return true;
}

void SingleInstanceGuard::handleNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QLocalSocket *socket = m_server->nextPendingConnection();

        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            m_pendingData[socket] += socket->readAll();
        });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            const QByteArray data = m_pendingData.take(socket);
            socket->deleteLater();

            QStringList paths;
            for (const QByteArray &line : data.split('\n')) {
                if (!line.isEmpty()) {
                    paths << QString::fromUtf8(line);
                }
            }
            emit filesReceived(paths);
        });
    }
}
