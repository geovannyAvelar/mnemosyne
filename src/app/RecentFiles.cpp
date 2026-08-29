#include "RecentFiles.h"

#include "FileIdentity.h"

#include <QSettings>

#include <algorithm>

namespace {
constexpr int kMaxEntries = 30;
constexpr const char *kGroup = "RecentFiles";

void writeEntries(const QVector<RecentFiles::Entry> &entries)
{
    QSettings settings;
    settings.remove(kGroup);
    settings.beginWriteArray(kGroup);
    for (int i = 0; i < entries.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("filePath", entries[i].filePath);
        settings.setValue("title", entries[i].title);
        settings.setValue("format", entries[i].format);
        settings.setValue("lastOpened", entries[i].lastOpened);
        settings.setValue("contentHash", entries[i].contentHash);
    }
    settings.endArray();
}
} // namespace

namespace RecentFiles {

QVector<Entry> list()
{
    QSettings settings;
    QVector<Entry> entries;
    const int size = settings.beginReadArray(kGroup);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        Entry e;
        e.filePath = settings.value("filePath").toString();
        e.title = settings.value("title").toString();
        e.format = settings.value("format").toString();
        e.lastOpened = settings.value("lastOpened").toDateTime();
        e.contentHash = settings.value("contentHash").toString();
        if (!e.filePath.isEmpty()) {
            entries.append(e);
        }
    }
    settings.endArray();

    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.lastOpened > b.lastOpened;
    });
    return entries;
}

void recordOpened(const QString &filePath, const QString &title, const QString &format)
{
    const QString hash = FileIdentity::contentHash(filePath);

    QVector<Entry> entries = list();
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                  [&](const Entry &e) {
                                      // Prefer matching by content hash so
                                      // re-picking the same book through a
                                      // path that changes on every import
                                      // (see the Entry::contentHash doc
                                      // comment) replaces the old entry
                                      // instead of duplicating it. Entries
                                      // written before this hash existed
                                      // fall back to the old path-equality
                                      // check.
                                      if (!hash.isEmpty() && !e.contentHash.isEmpty()) {
                                          return e.contentHash == hash;
                                      }
                                      return e.filePath == filePath;
                                  }),
                  entries.end());

    Entry e;
    e.filePath = filePath;
    e.title = title;
    e.format = format;
    e.lastOpened = QDateTime::currentDateTime();
    e.contentHash = hash;
    entries.prepend(e);

    if (entries.size() > kMaxEntries) {
        entries.resize(kMaxEntries);
    }

    writeEntries(entries);
}

void remove(const QString &filePath)
{
    QVector<Entry> entries = list();
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                  [&](const Entry &e) { return e.filePath == filePath; }),
                  entries.end());
    writeEntries(entries);
}

} // namespace RecentFiles
