#pragma once

#include "core/Bookmark.h"

#include <QString>
#include <QVector>

// Persists user-created bookmarks per document, keyed by content hash (see
// FileIdentity::contentHash) rather than file path — same rationale as
// ReadingProgressStore: it stays correct if the file moves/renames, and on
// Android a picked document's path is a cache-local copy resolved from its
// content:// URI (see ContentUriCache), not a stable identity on its own.
// Distinct from TocNode, which comes from the document itself.
namespace BookmarkStore {

// Returns bookmarks for bookHash, sorted by target position.
QVector<Bookmark> bookmarksFor(const QString &bookHash);

void addBookmark(const QString &bookHash, const Bookmark &bookmark);
void removeBookmark(const QString &bookHash, int index);

} // namespace BookmarkStore
