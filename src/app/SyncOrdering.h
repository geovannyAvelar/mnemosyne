#pragma once

#include <QDateTime>
#include <QString>
#include <QtGlobal>

// A (Lamport, wall-clock) pair used to order sync events across devices --
// see LamportClock.h for where the logical value comes from. lamport == 0
// means "no Lamport info" (an entry/record written before this field
// existed, or by an app version that doesn't set it yet); a real tick is
// never 0 (LamportClock::tick() starts at 1), so 0 can never collide with
// a genuine value.
struct SyncClock
{
    quint64 lamport = 0;
    QDateTime timestamp;
};

// True if `a` (from device `deviceIdA`) should be treated as newer than
// `b` (from device `deviceIdB`) when merging two REMOTE entries against
// each other -- e.g. two devices' competing edits of the same highlight,
// or the local-folder vs Google Drive backends' answers for the same
// book. Falls back to comparing `timestamp` whenever either side lacks a
// Lamport value. When both have one and they're numerically equal (two
// devices' independent counters happening to coincide), the higher
// deviceId wins -- arbitrary but fixed, so every device computes the same
// answer and the merge converges.
inline bool isNewerEntry(const SyncClock &a, const QString &deviceIdA, const SyncClock &b, const QString &deviceIdB)
{
    if (a.lamport != 0 && b.lamport != 0) {
        if (a.lamport != b.lamport) {
            return a.lamport > b.lamport;
        }
        return deviceIdA > deviceIdB;
    }
    return a.timestamp > b.timestamp;
}

// True if `remote` should overwrite whatever's already stored locally.
// Falls back to `timestamp` whenever either side lacks a Lamport value.
// Ties (equal Lamport values) favor local, matching this system's existing
// "remote must be strictly newer to overwrite" rule.
inline bool isNewerThanLocal(const SyncClock &remote, const SyncClock &local)
{
    if (remote.lamport != 0 && local.lamport != 0) {
        return remote.lamport > local.lamport;
    }
    return remote.timestamp > local.timestamp;
}
