#pragma once

#include "core/Document.h"

#include <QString>
#include <QVector>

struct SearchResult
{
    int targetIndex = -1; // PDF page index or EPUB spine index
    QString label; // e.g. "Page 3" or "Chapter 2"
    QString snippet;
};

// Common surface MainWindow (and later the TOC/search/bookmarks docks) use to
// drive whatever format-specific view is currently open, without caring
// whether it's paginated raster (PDF) or reflowable HTML (EPUB).
class IReaderView
{
public:
    virtual ~IReaderView() = default;

    virtual QString documentTitle() const = 0;
    virtual QVector<TocNode> tableOfContents() const = 0;

    // TocNode::pageNumber means: PDF page index for PdfView, spine index for EpubView.
    virtual void goToTocNode(const TocNode &node) = 0;

    // Current PDF page index or EPUB spine index; used for bookmarking.
    virtual int currentPosition() const = 0;

    // Case-insensitive full-text search across the whole document.
    virtual QVector<SearchResult> search(const QString &query) const = 0;
};
