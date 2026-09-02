#pragma once

#include "core/Highlight.h"

#include <QString>

#include <functional>

// The single entry point for cross-device highlight/note sync. Wraps the two
// backends (HighlightSyncLog for a locally-synced folder, and — when built
// with MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC — GoogleDriveHighlightSync) behind
// one API, so callers never need to know which backends exist or care about
// that build flag.
//
// pushUpsert/pushDelete are called only from HighlightStore, right after it
// writes a local change. pull() is called once per view when a book opens,
// mirroring where each view already checks for a newer synced reading
// position (see e.g. PdfView::restoreProgressAndCheckSync).
namespace HighlightSync {

// No-ops when no sync backend is configured/signed in.
void pushUpsert(const QString &bookHash, const Highlight &highlight);
void pushDelete(const QString &bookHash, const QString &highlightId);

// Pulls highlight changes from other devices for bookHash, merges them into
// local storage (see HighlightStore::replaceMerged) using last-write-wins
// per highlight id, and invokes callback(true) once per backend that
// actually changed something (so it may fire 0, 1, or 2 times — once for
// the local-folder backend, once for Drive, applied independently and
// convergently as each becomes available). Invokes callback(false) if a
// backend was checked and found nothing new.
void pull(const QString &bookHash, std::function<void(bool changed)> callback);

} // namespace HighlightSync
