#pragma once

#include "core/InkStroke.h"

#include <QString>
#include <QVector>

// Persists freehand ink strokes per document, keyed by content hash (see
// FileIdentity::contentHash) via QSettings -- mirrors HighlightStore, minus
// its highlight-sync/plugin-event integration (not asked for here, and easy
// to add later against the same shape if it is).
namespace InkStore {

// Returns all strokes for bookHash, in the order they were drawn.
QVector<InkStroke> strokesFor(const QString &bookHash);

void addStroke(const QString &bookHash, const InkStroke &stroke);

// Removes every stroke on pageIndex -- the "Clear Page Drawings" toolbar
// action's undo mechanism, simpler than per-stroke selection/deletion.
void clearPage(const QString &bookHash, int pageIndex);

} // namespace InkStore
