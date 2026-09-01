#include "SingleInstanceGuard.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QTimer>

namespace {
Q_LOGGING_CATEGORY(lcSingleInstance, "mnemosyne.singleinstance")

// QLocalServer already scopes local sockets per-user on all three platforms
// (a Unix domain socket under a user-private runtime directory on
// Linux/macOS, a named pipe namespaced to the caller's session on Windows),
// so this can't collide across different users on the same machine.
//
// The name is hashed down to a short, fixed-length string rather than used
// verbatim: on Unix this becomes a filesystem path under the OS temp
// directory, whose sun_path field caps out at 104 bytes on macOS. A literal
// "MnemosyneSingleInstance-<applicationName>" name blew past that once
// combined with macOS's unusually deep per-app $TMPDIR (seen in CI as
// SingleInstanceGuardTest's longer applicationName pushing bind() over the
// limit) -- listen() failed silently there (see the check in
// tryBecomePrimary() below) so the "primary" was never actually reachable.
// Hashing keeps the total path short regardless of platform or app name,
// while QCoreApplication::applicationName() still keeps this from colliding
// with (or stealing file-open requests from) an actual Mnemosyne instance
// when SingleInstanceGuardTest sets a different application name (same
// trick AppPersistenceTest uses for QSettings).
QString serverName()
{
    const QByteArray hash = QCryptographicHash::hash(QCoreApplication::applicationName().toUtf8(),
                                                       QCryptographicHash::Sha1);
    return QStringLiteral("mnemosyne-si-") + QString::fromLatin1(hash.toHex().left(16));
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
    const bool connected = socket.waitForConnected(2000);
    if (connected) {
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
    // Temporary diagnostic: this codepath has been silently failing in
    // Windows CI (SingleInstanceGuardTest) with no clue why -- surfacing
    // the actual socket error should tell us whether it's a real "nothing
    // is listening" versus something else entirely.
    qCWarning(lcSingleInstance) << "connect probe to" << serverName() << "did not connect, error"
                                 << socket.error() << "-" << socket.errorString();
    // Cancel the pending connect attempt explicitly rather than leaving it
    // for `socket`'s destructor to unwind -- on Windows in particular, an
    // outstanding overlapped connect left dangling has been unreliable.
    socket.abort();

    // No primary answered. Either this is the first instance, or a previous
    // one crashed and left its socket file behind (Linux/macOS only --
    // Windows named pipes are cleaned up by the OS when the owning process
    // dies). removeServer() clears any such stale file so listen() below
    // doesn't fail against a socket nothing is actually listening on.
    QLocalServer::removeServer(serverName());

    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstanceGuard::handleNewConnection);
    if (m_server->listen(serverName())) {
        // On Windows, arming the pipe to actually accept an incoming
        // connection finishes via the event loop rather than fully
        // synchronously inside listen() -- a client connecting right after
        // this call returns (as SingleInstanceGuardTest does, and as two
        // near-simultaneous real launches could) has been seen blocking for
        // that client's entire waitForConnected() timeout instead of
        // connecting immediately. A single non-blocking processEvents()
        // call didn't help (nothing is queued yet at the instant it runs),
        // so this actually waits briefly, giving that arming a real chance
        // to complete before a client can be turned away.
        QEventLoop loop;
        QTimer::singleShot(100, &loop, &QEventLoop::quit);
        loop.exec();
    } else {
        // E.g. a permissions issue on the runtime directory -- fall back to
        // running unguarded rather than refusing to open at all. Logged
        // rather than silently swallowed: a listen() failure here means
        // this process never actually becomes reachable as a primary, which
        // is otherwise indistinguishable from a working one until a second
        // launch mysteriously opens its own window instead of forwarding to
        // this one.
        qCWarning(lcSingleInstance) << "failed to listen on" << serverName() << "-" << m_server->errorString();
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
