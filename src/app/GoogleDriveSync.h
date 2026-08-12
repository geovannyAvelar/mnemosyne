#pragma once

#include "ProgressSyncLog.h"

#include <QString>

#include <functional>
#include <memory>
#include <optional>

class IHttpClient;

// Second, optional sync backend alongside SyncFolder/ProgressSyncLog: syncs
// the same per-device reading-progress JSONL log through the signed-in
// Google account's Drive "appDataFolder" — hidden from the user's normal
// Drive, reachable only with the narrow drive.appdata scope — instead of a
// locally-synced folder. See GoogleAuth for sign-in.
//
// Every function here is a safe no-op when GoogleAuth::isSignedIn() is
// false, so call sites can call these unconditionally alongside the
// existing local-folder ProgressSyncLog calls.
namespace GoogleDriveSync {

bool isEnabled();

void appendEntry(const QString &bookHash, const QString &title, int position, qreal zoom);

void latestFromOtherDevices(const QString &bookHash, const QString &excludeDeviceId,
                             std::function<void(std::optional<ProgressSyncLog::RemoteEntry>)> callback);

// Replaces the QNetworkAccessManager-backed HTTP layer with a test double.
// Passing nullptr restores the default.
void setHttpClientForTesting(std::unique_ptr<IHttpClient> client);

} // namespace GoogleDriveSync
