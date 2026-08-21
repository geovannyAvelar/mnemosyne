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
    // 0-based target: PDF page index, EPUB/MOBI spine (or "part") index, or
    // Markdown heading index (see MarkdownDocument::tableOfContents());
    // -1 if unknown.
    int pageNumber = -1;
    QVector<TocNode> children;
};
Q_DECLARE_METATYPE(TocNode)

// A single word's bounding box, in page-space points, in reading order.
// Used to render click-drag selection shaped to the actual text (a run of
// per-word highlight rects) instead of one rectangle spanning whatever it
// happens to overlap.
struct TextWord
{
    QString text;
    QRectF boundingBox;
    bool hasSpaceAfter = false;
};

class IPage
{
public:
    virtual ~IPage() = default;

    // Page size in points (1/72 inch), independent of render scale.
    virtual QSizeF sizePoints() const = 0;

    // scale 1.0 corresponds to 72 DPI; callers multiply by desired zoom.
    virtual QImage renderToImage(qreal scale) const = 0;

    // Plain text content of the whole page (used for search).
    virtual QString text() const = 0;

    // Word boxes for the whole page, in reading order (used to resolve
    // click-drag selection into the actual words it spans).
    virtual QVector<TextWord> words() const = 0;
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
