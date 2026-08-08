#pragma once

#include "core/ReadingProgress.h"

#include <QString>

#include <optional>

// Local "resume where I left off" state, keyed by content hash (not local
// file path) so it stays correct even if the file gets moved/renamed, and
// so it shares a key space with ProgressSyncLog for cross-device merging.
namespace ReadingProgressStore {

std::optional<ReadingProgress> get(const QString &bookHash);
void set(const QString &bookHash, int position, qreal zoom);

} // namespace ReadingProgressStore
