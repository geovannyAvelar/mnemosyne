#include "HighlightSync.h"

#include "DeviceIdentity.h"
#include "HighlightStore.h"
#include "HighlightSyncLog.h"

#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "GoogleDriveHighlightSync.h"
#endif

#include <QHash>

namespace HighlightSync {

namespace {

QDateTime effectiveTime(const Highlight &h)
{
    return h.updatedAt.isValid() ? h.updatedAt : h.createdAt;
}

// Merges entries (already excluding this device's own writes) into local
// storage for bookHash using last-write-wins per highlight id. Returns true
// if anything actually changed on disk.
bool mergeEntries(const QString &bookHash, const QVector<HighlightSyncLog::RemoteEntry> &entries)
{
    if (entries.isEmpty()) {
        return false;
    }

    QHash<QString, Highlight> byId;
    for (const Highlight &h : HighlightStore::highlightsFor(bookHash)) {
        byId.insert(h.id, h);
    }

    // Collapse the pulled entries to the newest one per id first, in case
    // more than one device logged an operation for the same highlight.
    QHash<QString, HighlightSyncLog::RemoteEntry> bestById;
    for (const HighlightSyncLog::RemoteEntry &entry : entries) {
        auto it = bestById.find(entry.id);
        if (it == bestById.end() || entry.timestamp > it->timestamp) {
            bestById.insert(entry.id, entry);
        }
    }

    bool changed = false;
    for (auto it = bestById.constBegin(); it != bestById.constEnd(); ++it) {
        const HighlightSyncLog::RemoteEntry &remote = it.value();
        const auto localIt = byId.find(remote.id);
        const bool localExists = localIt != byId.end();
        if (localExists && remote.timestamp <= effectiveTime(localIt.value())) {
            continue; // local is at least as new; remote is stale
        }

        if (remote.op == HighlightSyncLog::Op::Delete) {
            if (localExists) {
                byId.remove(remote.id);
                changed = true;
            }
            continue;
        }

        byId.insert(remote.id, remote.highlight);
        changed = true;
    }

    if (!changed) {
        return false;
    }

    const QList<Highlight> merged = byId.values();
    HighlightStore::replaceMerged(bookHash, QVector<Highlight>(merged.cbegin(), merged.cend()));
    return true;
}

} // namespace

void pushUpsert(const QString &bookHash, const Highlight &highlight)
{
    HighlightSyncLog::appendUpsert(bookHash, highlight);
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    GoogleDriveHighlightSync::appendUpsert(bookHash, highlight);
#endif
}

void pushDelete(const QString &bookHash, const QString &highlightId)
{
    HighlightSyncLog::appendDelete(bookHash, highlightId);
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    GoogleDriveHighlightSync::appendDelete(bookHash, highlightId);
#endif
}

void pull(const QString &bookHash, std::function<void(bool changed)> callback)
{
    if (bookHash.isEmpty()) {
        return;
    }

    const QString excludeId = DeviceIdentity::id();

    const QVector<HighlightSyncLog::RemoteEntry> localFolderEntries =
        HighlightSyncLog::entriesForBook(bookHash, excludeId);
    callback(mergeEntries(bookHash, localFolderEntries));

#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    GoogleDriveHighlightSync::entriesForBook(
        bookHash, excludeId, [bookHash, callback](QVector<HighlightSyncLog::RemoteEntry> driveEntries) {
            callback(mergeEntries(bookHash, driveEntries));
        });
#endif
}

} // namespace HighlightSync
