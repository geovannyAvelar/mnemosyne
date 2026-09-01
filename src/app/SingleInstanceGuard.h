#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QStringList>

class QLocalServer;
class QLocalSocket;

// Keeps Mnemosyne to a single running process. The first instance to launch
// becomes the "primary" and listens on a local socket; any instance that
// launches after that finds the primary alive, hands it its command-line
// file paths over that socket, and should exit immediately instead of
// opening a second window (see tryBecomePrimary()'s return value).
//
// This only covers launches that go through argv (a second `mnemosyne
// some.pdf` invocation, or a file manager's "open with" on Linux/Windows).
// A already-running macOS app receiving a Finder "Open With" instead gets a
// QFileOpenEvent with no second process involved at all -- see main.cpp's
// Application::event() override for that path.
class SingleInstanceGuard : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstanceGuard(QObject *parent = nullptr);

    // Tries to claim the primary-instance role. Returns true if this
    // process should proceed to start up normally (it became the primary,
    // or claiming it failed and running unguarded is the safest fallback).
    // Returns false if a primary is already running -- filesToOpen (which
    // may be empty, for a plain re-launch/activation request) has already
    // been forwarded to it, and the caller should exit without creating a
    // window.
    bool tryBecomePrimary(const QStringList &filesToOpen);

signals:
    // A later launch forwarded these file paths (empty means it just wants
    // the primary window brought to front).
    void filesReceived(const QStringList &filePaths);

private:
    void handleNewConnection();

    QLocalServer *m_server = nullptr;
    QHash<QLocalSocket *, QByteArray> m_pendingData;
};
