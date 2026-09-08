#pragma once

#include <QString>
#include <QStringList>

// Persists user-defined "shelves" (collections) and which books belong to
// which, via QSettings -- mirrors RecentFiles.h's use of
// FileIdentity::contentHash as a book's stable identity, so a shelf
// assignment survives a book being re-imported to a new path the same way
// its Recent Documents entry does.
//
// A collection's existence (its name) is tracked separately from
// membership: creating an empty shelf and adding books to it later has to
// work, so a collection isn't implicitly deleted just because its last book
// was removed from it -- only deleteCollection() below does that.
namespace CollectionStore {

// Every collection that exists, in creation order.
QStringList allCollections();

// Creates name if it doesn't already exist (case-sensitive; trimmed).
// Returns the trimmed name, or an empty string if name was blank.
// Idempotent -- creating an already-existing collection just returns its
// name without duplicating it.
QString createCollection(const QString &name);

// Renames a collection and updates every book's membership entry that
// referenced the old name. A no-op if oldName doesn't exist or newName is
// blank; if newName already names a different existing collection, oldName
// is merged into it (every book in oldName ends up in newName too, and
// oldName stops existing) rather than silently overwriting.
void renameCollection(const QString &oldName, const QString &newName);

// Deletes a collection and removes it from every book's membership.
void deleteCollection(const QString &name);

// Every collection bookHash currently belongs to, in the order it was
// added to each.
QStringList collectionsForBook(const QString &bookHash);

// Every book (by content hash) currently in collection.
QStringList booksInCollection(const QString &collection);

// Adds bookHash to collection, creating the collection first if it doesn't
// already exist. A no-op if bookHash is already a member.
void addBookToCollection(const QString &bookHash, const QString &collection);

void removeBookFromCollection(const QString &bookHash, const QString &collection);

} // namespace CollectionStore
