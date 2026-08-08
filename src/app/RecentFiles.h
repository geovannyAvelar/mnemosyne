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
};

// Returns entries sorted newest-first.
QVector<Entry> list();

// Adds filePath to the list (moving it to the front if already present),
// evicting the oldest entry once the list exceeds its cap.
void recordOpened(const QString &filePath, const QString &title, const QString &format);

void remove(const QString &filePath);

} // namespace RecentFiles
