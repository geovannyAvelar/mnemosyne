#pragma once

#include <QString>

// Poppler/libzip do their own POSIX file I/O and have no idea what a
// content:// URI is, so a document picked via Android's Storage Access
// Framework (see AndroidStorageAccess) can't be opened directly by
// openDocument()/EpubDocument::load() the way a desktop file path can.
//
// resolveToLocalFile() is the one place that gap gets bridged: it copies a
// content:// URI to a real file in the app's private cache (via QFile,
// which Qt's Android content-resolver file engine *does* understand) and
// returns that local path. Everything downstream of this call — FileIdentity,
// openDocument(), EpubDocument, BookmarkStore, ReadingProgressStore — stays
// exactly the same unmodified desktop code operating on a real filesystem
// path, on both platforms. Desktop paths and any non-content:// input pass
// through unchanged.
namespace ContentUriCache {

// format is "pdf" or "epub" (see LibraryModel::FormatRole), used to give the
// cached copy the right extension since a content:// URI's own path often
// has no reliable suffix (provider-dependent — some content providers only
// hand out opaque numeric document IDs).
//
// Repeated calls for the same URI reuse the previously cached copy rather
// than re-copying every open, keyed by a hash of the URI string itself —
// note this means a change to the underlying document *without* a change of
// URI (unusual for user-picked files, but possible) would serve a stale
// cached copy; not handled yet.
QString resolveToLocalFile(const QString &filePathOrUri, const QString &format, QString *errorMessage);

} // namespace ContentUriCache
