#pragma once

#include <QString>
#include <QStringList>

// Persists free-form tags per book via QSettings, keyed by
// FileIdentity::contentHash the same way CollectionStore.h and
// RecentFiles.h are. Unlike a collection, a tag has no separate
// creation/deletion step or membership-order to track -- it simply exists
// for as long as at least one book has it, and setTagsForBook() below
// replaces a book's whole tag set in one call (matching how a
// comma-separated "tags" text field naturally edits them) rather than
// offering separate add/remove-one-tag calls.
namespace TagStore {

// bookHash's tags, in no particular guaranteed order (whatever order they
// were stored in).
QStringList tagsForBook(const QString &bookHash);

// Replaces bookHash's entire tag set. Each tag is trimmed; blank and
// duplicate entries are dropped.
void setTagsForBook(const QString &bookHash, const QStringList &tags);

// Every distinct tag currently used by at least one book, sorted
// case-insensitively -- for a filter/autocomplete list.
QStringList allTags();

// Every book (by content hash) currently tagged with tag.
QStringList booksWithTag(const QString &tag);

} // namespace TagStore
