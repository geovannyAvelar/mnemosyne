#include "ThumbnailProvider.h"

#include "comic/CbzDocument.h"
#include "core/Document.h"
#include "epub/EpubDocument.h"
#include "markdown/MarkdownDocument.h"
#include "mobi/MobiDocument.h"
#include "txt/TxtDocument.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QStandardPaths>
#include <QTextDocument>
#include <QtConcurrentRun>

#include <algorithm>

namespace {

// Identifies a file's content (not just its path) so an edited document
// picks up a fresh thumbnail instead of showing a stale disk-cached one.
QString cacheKey(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QString raw = filePath + QLatin1Char('|')
        + QString::number(info.lastModified().toSecsSinceEpoch()) + QLatin1Char('|')
        + QString::number(info.size());
    return QString::fromLatin1(QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Md5).toHex());
}

QString diskCachePath(const QString &key)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/thumbnails");
    return dir + QLatin1Char('/') + key + QStringLiteral(".png");
}

// Scales up to fill targetSize on both axes, then crops the centered
// targetSize region -- so a page fills its thumbnail box edge-to-edge
// instead of floating inside letterboxing when its aspect ratio doesn't
// match (comic pages in particular vary widely).
QImage coverFit(const QImage &source, const QSize &targetSize)
{
    if (source.isNull()) {
        return {};
    }
    const QImage scaled = source.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int x = (scaled.width() - targetSize.width()) / 2;
    const int y = (scaled.height() - targetSize.height()) / 2;
    return scaled.copy(x, y, targetSize.width(), targetSize.height());
}

QImage renderRasterThumbnail(IDocument &document, const QSize &targetSize)
{
    if (document.pageCount() <= 0) {
        return {};
    }
    const std::unique_ptr<IPage> page = document.page(0);
    if (!page) {
        return {};
    }
    const QSizeF pagePoints = page->sizePoints();
    if (pagePoints.width() <= 0 || pagePoints.height() <= 0) {
        return {};
    }
    const qreal scale = std::max(targetSize.width() / pagePoints.width(),
                                  targetSize.height() / pagePoints.height());
    return coverFit(page->renderToImage(scale), targetSize);
}

// Shared by every HTML/text-based format: lay the content out at 2x the
// thumbnail size (for antialiased downscaled text) and rasterize whatever
// falls within the first page-sized region, cropping the rest -- this is a
// preview of the start of the document, not a full-page paginated render.
QImage renderTextDocumentThumbnail(QTextDocument &doc, const QSize &targetSize)
{
    const qreal superSample = 2.0;
    const QSize renderSize = targetSize * superSample;

    doc.setTextWidth(renderSize.width());
    doc.setDocumentMargin(14.0 * superSample);

    QImage image(renderSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    doc.drawContents(&painter, QRectF(QPointF(0, 0), QSizeF(renderSize)));
    painter.end();

    return image.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage renderHtmlThumbnail(const QString &html, const QSize &targetSize)
{
    if (html.isEmpty()) {
        return {};
    }
    QTextDocument doc;
    doc.setHtml(html);
    return renderTextDocumentThumbnail(doc, targetSize);
}

QImage renderMarkdownThumbnail(const QString &markdown, const QSize &targetSize)
{
    if (markdown.isEmpty()) {
        return {};
    }
    QTextDocument doc;
    doc.setMarkdown(markdown);
    return renderTextDocumentThumbnail(doc, targetSize);
}

QImage renderPlainTextThumbnail(const QString &text, const QSize &targetSize)
{
    if (text.isEmpty()) {
        return {};
    }
    QTextDocument doc;
    doc.setPlainText(text);
    return renderTextDocumentThumbnail(doc, targetSize);
}

// Runs entirely off the UI thread (see ThumbnailProvider::request()): each
// branch loads just enough of the document to grab its first page/chapter,
// which is fine to pay for once since the result gets disk-cached.
QImage renderThumbnail(const QString &filePath, const QString &format, const QSize &targetSize)
{
    QString errorMessage;

    if (format == QLatin1String("pdf")) {
        const std::unique_ptr<IDocument> document = openDocument(filePath, &errorMessage);
        return document ? renderRasterThumbnail(*document, targetSize) : QImage();
    }
    if (format == QLatin1String("cbz")) {
        const std::unique_ptr<CbzDocument> document = CbzDocument::load(filePath, &errorMessage);
        return document ? renderRasterThumbnail(*document, targetSize) : QImage();
    }
    if (format == QLatin1String("epub")) {
        const std::unique_ptr<EpubDocument> document = EpubDocument::load(filePath, &errorMessage);
        return (document && document->spineCount() > 0) ? renderHtmlThumbnail(document->chapterHtml(0), targetSize)
                                                          : QImage();
    }
    if (format == QLatin1String("mobi") || format == QLatin1String("azw") || format == QLatin1String("azw3")) {
        const std::unique_ptr<MobiDocument> document = MobiDocument::load(filePath, &errorMessage);
        return (document && document->partCount() > 0) ? renderHtmlThumbnail(document->partHtml(0), targetSize)
                                                         : QImage();
    }
    if (format == QLatin1String("md") || format == QLatin1String("markdown")) {
        const std::unique_ptr<MarkdownDocument> document = MarkdownDocument::load(filePath, &errorMessage);
        return document ? renderMarkdownThumbnail(document->markdownText(), targetSize) : QImage();
    }
    if (format == QLatin1String("txt")) {
        const std::unique_ptr<TxtDocument> document = TxtDocument::load(filePath, &errorMessage);
        return document ? renderPlainTextThumbnail(document->text(), targetSize) : QImage();
    }

    // html/htm (and anything unrecognized): no cheap first-page render
    // available here, so LibraryView keeps showing its generic placeholder.
    return {};
}

} // namespace

ThumbnailProvider::ThumbnailProvider(QObject *parent)
    : QObject(parent)
{
    m_memoryCache.setMaxCost(200);
}

QSize ThumbnailProvider::thumbnailSize()
{
    return QSize(120, 160);
}

QPixmap ThumbnailProvider::request(const QString &filePath, const QString &format)
{
    const QString key = cacheKey(filePath);

    if (QPixmap *cached = m_memoryCache.object(key)) {
        return *cached;
    }
    if (m_pending.contains(key)) {
        return {};
    }

    const QString diskPath = diskCachePath(key);
    const QSize targetSize = thumbnailSize();

    auto *watcher = new QFutureWatcher<QImage>(this);
    m_pending.insert(key, watcher);

    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, key, filePath, watcher] {
        const QImage image = watcher->result();
        m_pending.remove(key);
        watcher->deleteLater();

        if (image.isNull()) {
            return;
        }
        const QPixmap pixmap = QPixmap::fromImage(image);
        m_memoryCache.insert(key, new QPixmap(pixmap));
        emit thumbnailReady(filePath, pixmap);
    });

    watcher->setFuture(QtConcurrent::run([filePath, format, diskPath, targetSize]() -> QImage {
        QImage image;
        if (QFileInfo::exists(diskPath)) {
            image.load(diskPath);
        }
        if (image.isNull()) {
            image = renderThumbnail(filePath, format, targetSize);
            if (!image.isNull()) {
                QDir().mkpath(QFileInfo(diskPath).absolutePath());
                image.save(diskPath, "PNG");
            }
        }
        return image;
    }));

    return {};
}
