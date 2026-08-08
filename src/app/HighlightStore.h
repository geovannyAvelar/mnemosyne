#pragma once

#include "core/Highlight.h"

#include <QString>
#include <QVector>

// Persists user-created highlights per document (keyed by file path) via
// QSettings, mirroring BookmarkStore.
namespace HighlightStore {

// Returns all highlights for filePath, sorted by target position.
QVector<Highlight> highlightsFor(const QString &filePath);

void addHighlight(const QString &filePath, const Highlight &highlight);
void removeHighlight(const QString &filePath, int index);

} // namespace HighlightStore
