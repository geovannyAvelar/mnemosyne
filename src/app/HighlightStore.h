#pragma once

#include "core/Highlight.h"

#include <QColor>
#include <QString>
#include <QVector>

// Persists user-created highlights per document, keyed by content hash (see
// FileIdentity::contentHash) via QSettings, mirroring ReadingProgressStore —
// same rationale: stays correct if the file moves/renames, and on Android a
// picked document's path is a cache-local copy resolved from its
// content:// URI (see ContentUriCache), not a stable identity on its own.
namespace HighlightStore {

// Returns all highlights for bookHash, sorted by target position.
QVector<Highlight> highlightsFor(const QString &bookHash);

void addHighlight(const QString &bookHash, const Highlight &highlight);
void removeHighlight(const QString &bookHash, int index);

// Sets (or clears, with an empty string) the note on the highlight at index,
// as returned by the most recent highlightsFor() call for bookHash.
void setNote(const QString &bookHash, int index, const QString &note);

// Sets the marker color on the highlight at index, as returned by the most
// recent highlightsFor() call for bookHash.
void setColor(const QString &bookHash, int index, const QColor &color);

// Overwrites the full highlight set for bookHash with a merged result (see
// HighlightSync::pull). Unlike the mutators above, this does not push
// anything to sync itself — the caller already derived merged from sync
// data, so pushing it back would just echo it.
void replaceMerged(const QString &bookHash, const QVector<Highlight> &merged);

} // namespace HighlightStore
