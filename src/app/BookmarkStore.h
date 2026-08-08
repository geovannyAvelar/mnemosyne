#pragma once

#include "core/Bookmark.h"

#include <QString>
#include <QVector>

// Persists user-created bookmarks per document (keyed by file path) via
// QSettings. Distinct from TocNode, which comes from the document itself.
namespace BookmarkStore {

// Returns bookmarks for filePath, sorted by target position.
QVector<Bookmark> bookmarksFor(const QString &filePath);

void addBookmark(const QString &filePath, const Bookmark &bookmark);
void removeBookmark(const QString &filePath, int index);

} // namespace BookmarkStore
