#include "BookmarkStore.h"

#include <QCryptographicHash>
#include <QSettings>

#include <algorithm>

namespace {

// QSettings groups can't safely contain '/' from a raw file path, and
// qHash() is seeded per-process (unstable across runs), so a persistent key
// needs a proper content hash instead.
QString groupKeyFor(const QString &filePath)
{
    const QByteArray hash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Md5).toHex();
    return QStringLiteral("Bookmarks/%1").arg(QString::fromLatin1(hash));
}

void writeBookmarks(const QString &filePath, const QVector<Bookmark> &bookmarks)
{
    QSettings settings;
    const QString group = groupKeyFor(filePath);
    settings.remove(group);
    settings.beginWriteArray(group);
    for (int i = 0; i < bookmarks.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("targetIndex", bookmarks[i].targetIndex);
        settings.setValue("label", bookmarks[i].label);
        settings.setValue("createdAt", bookmarks[i].createdAt);
    }
    settings.endArray();
}

} // namespace

namespace BookmarkStore {

QVector<Bookmark> bookmarksFor(const QString &filePath)
{
    QSettings settings;
    QVector<Bookmark> result;
    const int size = settings.beginReadArray(groupKeyFor(filePath));
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        Bookmark b;
        b.targetIndex = settings.value("targetIndex", -1).toInt();
        b.label = settings.value("label").toString();
        b.createdAt = settings.value("createdAt").toDateTime();
        result.append(b);
    }
    settings.endArray();

    std::sort(result.begin(), result.end(), [](const Bookmark &a, const Bookmark &b) {
        return a.targetIndex < b.targetIndex;
    });
    return result;
}

void addBookmark(const QString &filePath, const Bookmark &bookmark)
{
    QVector<Bookmark> bookmarks = bookmarksFor(filePath);
    bookmarks.erase(std::remove_if(bookmarks.begin(), bookmarks.end(),
                                    [&](const Bookmark &b) { return b.targetIndex == bookmark.targetIndex; }),
                     bookmarks.end());
    bookmarks.append(bookmark);
    std::sort(bookmarks.begin(), bookmarks.end(), [](const Bookmark &a, const Bookmark &b) {
        return a.targetIndex < b.targetIndex;
    });
    writeBookmarks(filePath, bookmarks);
}

void removeBookmark(const QString &filePath, int index)
{
    QVector<Bookmark> bookmarks = bookmarksFor(filePath);
    if (index < 0 || index >= bookmarks.size()) {
        return;
    }
    bookmarks.removeAt(index);
    writeBookmarks(filePath, bookmarks);
}

} // namespace BookmarkStore
