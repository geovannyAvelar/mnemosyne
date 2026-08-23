#pragma once

#include "core/Highlight.h"

#include <QString>
#include <QVector>

// Persists user-created highlights per document, keyed by content hash (see
// FileIdentity::contentHash) via QSettings, mirroring BookmarkStore —
// same rationale: stays correct if the file moves/renames, and on Android a
// picked document's path is a cache-local copy resolved from its
// content:// URI (see ContentUriCache), not a stable identity on its own.
namespace HighlightStore {

// Returns all highlights for bookHash, sorted by target position.
QVector<Highlight> highlightsFor(const QString &bookHash);

void addHighlight(const QString &bookHash, const Highlight &highlight);
void removeHighlight(const QString &bookHash, int index);

} // namespace HighlightStore
