#pragma once

#include <QCache>
#include <QFutureWatcher>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPixmap>
#include <QSize>
#include <QString>

// Generates small preview images for LibraryView's recent-documents list --
// the first page (PDF/CBZ) or first chapter (EPUB/MOBI/Markdown/TXT),
// rendered off the UI thread and cached to disk (keyed by path + mtime +
// size, so an edited file gets a fresh thumbnail next time) so reopening the
// library doesn't re-render every entry from scratch.
class ThumbnailProvider : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailProvider(QObject *parent = nullptr);

    // Returns the cached thumbnail if one is already in memory. Otherwise
    // returns a null QPixmap and, unless generation for this file is already
    // underway, kicks off async generation -- thumbnailReady() fires when it
    // completes. A still-null result there means no thumbnail could be
    // produced (e.g. HTML documents, or a load failure) and the caller
    // should keep its fallback icon.
    QPixmap request(const QString &filePath, const QString &format);

    // Fixed size every thumbnail is rendered/cropped to (roughly a book
    // page's aspect ratio), so LibraryView can lay out a uniform grid.
    static QSize thumbnailSize();

signals:
    void thumbnailReady(const QString &filePath, const QPixmap &pixmap);

private:
    QCache<QString, QPixmap> m_memoryCache;
    QHash<QString, QFutureWatcher<QImage> *> m_pending; // keyed by cache key, not filePath
};
