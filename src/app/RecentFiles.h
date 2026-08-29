#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

// Persists the list of recently opened documents via QSettings.
namespace RecentFiles {

struct Entry
{
    QString filePath;
    QString title;
    QString format; // "pdf" or "epub"
    QDateTime lastOpened;
    // See FileIdentity::contentHash -- lets recordOpened() recognize "the
    // same book" even when it was re-imported to a new path (iOS's
    // document picker copies the picked file into this app's own sandbox
    // under a fresh, randomly-named path on every single pick, so the
    // same source PDF picked twice would otherwise never match by path
    // alone and would show up twice in Recents).
    QString contentHash;
};

// Returns entries sorted newest-first.
QVector<Entry> list();

// Adds filePath to the list (moving it to the front if already present),
// evicting the oldest entry once the list exceeds its cap.
void recordOpened(const QString &filePath, const QString &title, const QString &format);

void remove(const QString &filePath);

} // namespace RecentFiles
