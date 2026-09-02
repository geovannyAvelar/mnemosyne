#pragma once

#include "core/Highlight.h"

#include <QDateTime>
#include <QString>
#include <QVector>

// Append-only, per-device sync log for highlights/notes, stored as JSON
// Lines files in SyncFolder::dataDirectory() — same file-per-device,
// append-only-so-no-write-write-races design as ProgressSyncLog, but keyed
// per highlight id rather than per book, since a book's highlight set is
// edited a piece at a time (add/edit/delete) rather than replaced wholesale.
// See HighlightSync for how these entries get merged into local state.
namespace HighlightSyncLog {

enum class Op { Upsert, Delete };

struct RemoteEntry
{
    QString id; // the highlight's stable id
    QString bookHash;
    Op op = Op::Upsert;
    Highlight highlight; // valid fields only when op == Upsert
    QDateTime timestamp; // when this operation happened, for last-write-wins merging
    QString deviceId;
    QString deviceName;
};

// No-op if SyncFolder isn't configured.
void appendUpsert(const QString &bookHash, const Highlight &highlight);
void appendDelete(const QString &bookHash, const QString &highlightId);

// All entries logged for bookHash by any device other than excludeDeviceId,
// across every <deviceId>-highlights.jsonl file in SyncFolder::dataDirectory().
// Empty if sync isn't configured. Entries are not collapsed per id here —
// see HighlightSync for the last-write-wins merge.
QVector<RemoteEntry> entriesForBook(const QString &bookHash, const QString &excludeDeviceId);

// Same append/read logic as above, but against an arbitrary directory of
// <deviceId>-highlights.jsonl files instead of SyncFolder::dataDirectory().
// Lets GoogleDriveHighlightSync reuse the same file format without
// duplicating it, the same way ProgressSyncLog::appendEntryToDirectory does
// for GoogleDriveSync. highlight's fields are only written for op == Upsert;
// pass a default-constructed Highlight for a delete (only id/bookHash matter).
void appendEntryToDirectory(const QString &dir, const QString &deviceId, const QString &deviceName,
                             const QString &bookHash, const QString &highlightId, Op op, const Highlight &highlight);
QVector<RemoteEntry> entriesFromDirectory(const QString &dir, const QString &bookHash,
                                           const QString &excludeDeviceId);

} // namespace HighlightSyncLog
