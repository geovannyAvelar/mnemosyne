#include "TagStore.h"

#include <QSettings>

#include <algorithm>

namespace {

constexpr const char *kGroup = "BookTags";

struct TagEntry
{
    QString bookHash;
    QString tag;
};

QVector<TagEntry> readAll()
{
    QSettings settings;
    QVector<TagEntry> result;
    const int size = settings.beginReadArray(kGroup);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        TagEntry e;
        e.bookHash = settings.value("bookHash").toString();
        e.tag = settings.value("tag").toString();
        if (!e.bookHash.isEmpty() && !e.tag.isEmpty()) {
            result.append(e);
        }
    }
    settings.endArray();
    return result;
}

void writeAll(const QVector<TagEntry> &entries)
{
    QSettings settings;
    settings.remove(kGroup);
    settings.beginWriteArray(kGroup);
    for (int i = 0; i < entries.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("bookHash", entries[i].bookHash);
        settings.setValue("tag", entries[i].tag);
    }
    settings.endArray();
}

} // namespace

namespace TagStore {

QStringList tagsForBook(const QString &bookHash)
{
    QStringList result;
    for (const TagEntry &e : readAll()) {
        if (e.bookHash == bookHash) {
            result.append(e.tag);
        }
    }
    return result;
}

void setTagsForBook(const QString &bookHash, const QStringList &tags)
{
    if (bookHash.isEmpty()) {
        return;
    }

    QVector<TagEntry> entries = readAll();
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                  [&](const TagEntry &e) { return e.bookHash == bookHash; }),
                  entries.end());

    QStringList cleaned;
    for (const QString &tag : tags) {
        const QString trimmed = tag.trimmed();
        if (!trimmed.isEmpty() && !cleaned.contains(trimmed)) {
            cleaned.append(trimmed);
        }
    }
    for (const QString &tag : cleaned) {
        entries.append({bookHash, tag});
    }

    writeAll(entries);
}

QStringList allTags()
{
    QStringList result;
    for (const TagEntry &e : readAll()) {
        if (!result.contains(e.tag)) {
            result.append(e.tag);
        }
    }
    std::sort(result.begin(), result.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return result;
}

QStringList booksWithTag(const QString &tag)
{
    QStringList result;
    for (const TagEntry &e : readAll()) {
        if (e.tag == tag) {
            result.append(e.bookHash);
        }
    }
    return result;
}

} // namespace TagStore
