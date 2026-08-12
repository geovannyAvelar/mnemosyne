#pragma once

#include <QDateTime>
#include <QString>

#include <optional>

// Append-only, per-device sync log stored as JSON Lines files in
// SyncFolder::dataDirectory(). Each device only ever appends to its own
// file (<deviceId>.jsonl) — two devices can never race on the same bytes,
// since neither ever rewrites what the other wrote. Merging across devices
// (see latestFromOtherDevices) is read-only and just picks the newest
// timestamp per book.
namespace ProgressSyncLog {

struct RemoteEntry
{
    QString bookHash;
    QString title;
    int position = 0;
    qreal zoom = 1.0;
    QDateTime timestamp;
    QString deviceId;
    QString deviceName;
};

// No-op if SyncFolder isn't configured.
void appendEntry(const QString &bookHash, const QString &title, int position, qreal zoom);

// The most recent entry for bookHash written by any device other than
// excludeDeviceId, or nullopt if none exists / sync isn't configured.
std::optional<RemoteEntry> latestFromOtherDevices(const QString &bookHash, const QString &excludeDeviceId);

// Same append/merge logic as above, but against an arbitrary directory of
// <deviceId>.jsonl files instead of SyncFolder::dataDirectory(). Lets other
// sync backends (e.g. GoogleDriveSync's local staging directory) reuse the
// same file format and "newest timestamp wins" merge rule without
// duplicating it. No-op / nullopt if dir or bookHash is empty.
void appendEntryToDirectory(const QString &dir, const QString &deviceId, const QString &deviceName,
                             const QString &bookHash, const QString &title, int position, qreal zoom);
std::optional<RemoteEntry> latestFromDirectory(const QString &dir, const QString &bookHash,
                                                const QString &excludeDeviceId);

} // namespace ProgressSyncLog
