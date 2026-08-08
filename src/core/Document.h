#pragma once

#include <QImage>
#include <QMetaType>
#include <QSizeF>
#include <QString>
#include <QVector>

#include <memory>

struct TocNode
{
    QString title;
    int pageNumber = -1; // 0-based target page; -1 if unknown
    QVector<TocNode> children;
};
Q_DECLARE_METATYPE(TocNode)

class IPage
{
public:
    virtual ~IPage() = default;

    // Page size in points (1/72 inch), independent of render scale.
    virtual QSizeF sizePoints() const = 0;

    // scale 1.0 corresponds to 72 DPI; callers multiply by desired zoom.
    virtual QImage renderToImage(qreal scale) const = 0;

    // Plain text content of the page. If rect is null, returns all text on
    // the page (used for search); otherwise returns text within that
    // page-space rectangle, in points (used for click-drag selection).
    virtual QString text(const QRectF &rect = QRectF()) const = 0;
};

class IDocument
{
public:
    virtual ~IDocument() = default;

    virtual int pageCount() const = 0;
    virtual std::unique_ptr<IPage> page(int index) const = 0;
    virtual QVector<TocNode> tableOfContents() const = 0;
    virtual QString title() const = 0;
};

// Opens a PDF via the Poppler backend. Returns nullptr and fills errorMessage
// on failure. EPUB uses a separate loading path (see epub/EpubDocument.h)
// since reflowable HTML chapters don't fit the raster-page IDocument contract.
std::unique_ptr<IDocument> openDocument(const QString &filePath, QString *errorMessage);
