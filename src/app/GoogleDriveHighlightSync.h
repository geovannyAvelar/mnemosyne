#pragma once

#include "HighlightSyncLog.h"

#include <QString>

#include <functional>
#include <memory>

class IHttpClient;

// Second, optional sync backend alongside SyncFolder/HighlightSyncLog: syncs
// the same per-device highlight JSONL log through the signed-in Google
// account's Drive "appDataFolder", the same way GoogleDriveSync does for
// reading progress — see that header for the rationale. Kept as a separate
// module (own file names, own remote-file-id/modified-time caches) rather
// than folded into GoogleDriveSync, so the two logs' upload/download
// bookkeeping can't cross-contaminate.
//
// Every function here is a safe no-op when GoogleAuth::isSignedIn() is
// false.
namespace GoogleDriveHighlightSync {

bool isEnabled();

void appendUpsert(const QString &bookHash, const Highlight &highlight);
void appendDelete(const QString &bookHash, const QString &highlightId);

// Entries logged by other devices for bookHash. Empty (via the callback) if
// disabled or nothing new since the last check.
void entriesForBook(const QString &bookHash, const QString &excludeDeviceId,
                     std::function<void(QVector<HighlightSyncLog::RemoteEntry>)> callback);

// Replaces the QNetworkAccessManager-backed HTTP layer with a test double.
// Passing nullptr restores the default.
void setHttpClientForTesting(std::unique_ptr<IHttpClient> client);

} // namespace GoogleDriveHighlightSync
