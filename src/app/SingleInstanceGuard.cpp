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
    // Probe for a live primary first. When there's no primary, this returns
    // almost immediately (the OS refuses the connection outright -- there's
    // no socket/pipe to find), so a generous timeout here doesn't add any
    // latency to the common case of being the first instance; it only
    // matters for the much rarer case of a primary that's genuinely slow to
    // accept (seen in CI on macOS/Windows, where connecting right after the
    // primary's listen() call needs more than a couple hundred ms to land).
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (socket.waitForConnected(2000)) {
        QByteArray payload;
        for (const QString &path : filesToOpen) {
            payload += path.toUtf8();
            payload += '\n';
        }
        socket.write(payload);
        socket.waitForBytesWritten(2000);
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
