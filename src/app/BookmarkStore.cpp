#include "BookmarkStore.h"

#include <QCryptographicHash>
#include <QSettings>

#include <algorithm>

namespace {

// bookHash is already a content hash (see FileIdentity::contentHash), safe
// on its own as a QSettings group name — this MD5 pass just keeps the key
// a fixed, short length regardless of what hash algorithm callers use.
QString groupKeyFor(const QString &bookHash)
{
    const QByteArray hash = QCryptographicHash::hash(bookHash.toUtf8(), QCryptographicHash::Md5).toHex();
    return QStringLiteral("Bookmarks/%1").arg(QString::fromLatin1(hash));
}

void writeBookmarks(const QString &bookHash, const QVector<Bookmark> &bookmarks)
{
    QSettings settings;
    const QString group = groupKeyFor(bookHash);
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

QVector<Bookmark> bookmarksFor(const QString &bookHash)
{
    QSettings settings;
    QVector<Bookmark> result;
    const int size = settings.beginReadArray(groupKeyFor(bookHash));
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

void addBookmark(const QString &bookHash, const Bookmark &bookmark)
{
    QVector<Bookmark> bookmarks = bookmarksFor(bookHash);
    bookmarks.erase(std::remove_if(bookmarks.begin(), bookmarks.end(),
                                    [&](const Bookmark &b) { return b.targetIndex == bookmark.targetIndex; }),
                     bookmarks.end());
    bookmarks.append(bookmark);
    std::sort(bookmarks.begin(), bookmarks.end(), [](const Bookmark &a, const Bookmark &b) {
        return a.targetIndex < b.targetIndex;
    });
    writeBookmarks(bookHash, bookmarks);
}

void removeBookmark(const QString &bookHash, int index)
{
    QVector<Bookmark> bookmarks = bookmarksFor(bookHash);
    if (index < 0 || index >= bookmarks.size()) {
        return;
    }
    bookmarks.removeAt(index);
    writeBookmarks(bookHash, bookmarks);
}

} // namespace BookmarkStore
