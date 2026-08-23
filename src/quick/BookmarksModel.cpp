#include "BookmarksModel.h"

#include "app/BookmarkStore.h"

#include <QDateTime>

int BookmarksModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_bookmarks.size();
}

QVariant BookmarksModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_bookmarks.size()) {
        return {};
    }

    const Bookmark &bookmark = m_bookmarks.at(index.row());
    switch (role) {
    case TargetIndexRole:
        return bookmark.targetIndex;
    case LabelRole:
        return bookmark.label;
    case CreatedAtRole:
        return bookmark.createdAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> BookmarksModel::roleNames() const
{
    return {
        {TargetIndexRole, "targetIndex"},
        {LabelRole, "label"},
        {CreatedAtRole, "createdAt"},
    };
}

void BookmarksModel::setBookHash(const QString &bookHash)
{
    if (m_bookHash == bookHash) {
        return;
    }
    m_bookHash = bookHash;
    emit bookHashChanged();
    refresh();
}

bool BookmarksModel::isBookmarked(int targetIndex) const
{
    for (const Bookmark &bookmark : m_bookmarks) {
        if (bookmark.targetIndex == targetIndex) {
            return true;
        }
    }
    return false;
}

void BookmarksModel::toggleBookmark(int targetIndex, const QString &label)
{
    if (m_bookHash.isEmpty()) {
        return;
    }

    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks.at(i).targetIndex == targetIndex) {
            BookmarkStore::removeBookmark(m_bookHash, i);
            refresh();
            return;
        }
    }

    Bookmark bookmark;
    bookmark.targetIndex = targetIndex;
    bookmark.label = label;
    bookmark.createdAt = QDateTime::currentDateTime();
    BookmarkStore::addBookmark(m_bookHash, bookmark);
    refresh();
}

void BookmarksModel::removeBookmarkAt(int row)
{
    if (m_bookHash.isEmpty() || row < 0 || row >= m_bookmarks.size()) {
        return;
    }
    BookmarkStore::removeBookmark(m_bookHash, row);
    refresh();
}

void BookmarksModel::refresh()
{
    beginResetModel();
    m_bookmarks = m_bookHash.isEmpty() ? QVector<Bookmark>() : BookmarkStore::bookmarksFor(m_bookHash);
    endResetModel();
}
