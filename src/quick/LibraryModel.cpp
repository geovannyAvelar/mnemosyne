#include "LibraryModel.h"

#include "app/CollectionStore.h"
#include "app/RecentFiles.h"
#include "app/TagStore.h"

#include <algorithm>

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const Row &row = m_rows.at(index.row());
    switch (role) {
    case FilePathRole:
        return row.filePath;
    case TitleRole:
        return row.title.isEmpty() ? row.filePath : row.title;
    case FormatRole:
        return row.format;
    case LastOpenedRole:
        return row.lastOpened;
    case ContentHashRole:
        return row.contentHash;
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {
        {FilePathRole, "filePath"},
        {TitleRole, "title"},
        {FormatRole, "format"},
        {LastOpenedRole, "lastOpened"},
        {ContentHashRole, "contentHash"},
    };
}

void LibraryModel::setCollectionFilter(const QString &collection)
{
    if (m_collectionFilter == collection) {
        return;
    }
    m_collectionFilter = collection;
    emit collectionFilterChanged();
    refresh();
}

void LibraryModel::setTagFilter(const QString &tag)
{
    if (m_tagFilter == tag) {
        return;
    }
    m_tagFilter = tag;
    emit tagFilterChanged();
    refresh();
}

void LibraryModel::refresh()
{
    beginResetModel();
    m_rows.clear();

    QVector<RecentFiles::Entry> entries = RecentFiles::list();

    // A pre-hash Recent entry (empty contentHash -- see RecentFiles::Entry's
    // own doc comment) can't be matched against either store, so it drops
    // out of any filtered view rather than showing up under every shelf/tag;
    // re-opening the book re-records it with a hash and fixes this.
    if (!m_collectionFilter.isEmpty()) {
        const QStringList members = CollectionStore::booksInCollection(m_collectionFilter);
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                      [&](const RecentFiles::Entry &e) {
                                          return e.contentHash.isEmpty() || !members.contains(e.contentHash);
                                      }),
                      entries.end());
    }
    if (!m_tagFilter.isEmpty()) {
        const QStringList tagged = TagStore::booksWithTag(m_tagFilter);
        entries.erase(std::remove_if(entries.begin(), entries.end(),
                                      [&](const RecentFiles::Entry &e) {
                                          return e.contentHash.isEmpty() || !tagged.contains(e.contentHash);
                                      }),
                      entries.end());
    }

    m_rows.reserve(entries.size());
    for (const RecentFiles::Entry &entry : entries) {
        m_rows.append({entry.filePath, entry.title, entry.format, entry.lastOpened, entry.contentHash});
    }
    endResetModel();
}

void LibraryModel::recordOpened(const QString &filePath, const QString &title, const QString &format)
{
    RecentFiles::recordOpened(filePath, title, format);
    refresh();
}

void LibraryModel::removeEntry(const QString &filePath)
{
    RecentFiles::remove(filePath);
    refresh();
}

QStringList LibraryModel::allCollections() const
{
    return CollectionStore::allCollections();
}

QStringList LibraryModel::collectionsForBook(const QString &bookHash) const
{
    return CollectionStore::collectionsForBook(bookHash);
}

QString LibraryModel::createCollection(const QString &name)
{
    return CollectionStore::createCollection(name);
}

void LibraryModel::addBookToCollection(const QString &bookHash, const QString &collection)
{
    CollectionStore::addBookToCollection(bookHash, collection);
    refresh(); // may newly match collectionFilter
}

void LibraryModel::removeBookFromCollection(const QString &bookHash, const QString &collection)
{
    CollectionStore::removeBookFromCollection(bookHash, collection);
    refresh(); // may no longer match collectionFilter
}

void LibraryModel::deleteCollection(const QString &collection)
{
    CollectionStore::deleteCollection(collection);
    if (m_collectionFilter == collection) {
        setCollectionFilter(QString()); // the shelf just being viewed no longer exists
    }
}

QStringList LibraryModel::allTags() const
{
    return TagStore::allTags();
}

QStringList LibraryModel::tagsForBook(const QString &bookHash) const
{
    return TagStore::tagsForBook(bookHash);
}

void LibraryModel::setTagsForBook(const QString &bookHash, const QStringList &tags)
{
    TagStore::setTagsForBook(bookHash, tags);
    refresh(); // may newly match/no longer match tagFilter
}
