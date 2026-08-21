#pragma once

#include "core/Document.h"

#include <QString>
#include <QVector>

struct SearchResult
{
    int targetIndex = -1; // PDF page index, EPUB/MOBI spine (or "part") index, or Markdown heading index
    QString label; // e.g. "Page 3", "Chapter 2", "Part 4", or a Markdown heading's title
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

    // TocNode::pageNumber means: PDF/CBZ page index for PdfView/ComicView,
    // spine index for EpubView, part index for MobiView, heading index for
    // MarkdownView. ComicView has no table of contents (yet), so this is
    // moot for it in practice.
    virtual void goToTocNode(const TocNode &node) = 0;

    // Current PDF/CBZ page index, EPUB/MOBI spine (or "part") index, or Markdown heading index; used for bookmarking.
    virtual int currentPosition() const = 0;

    // Case-insensitive full-text search across the whole document.
    virtual QVector<SearchResult> search(const QString &query) const = 0;

    // Marks every occurrence of term on the currently displayed page/chapter
    // in yellow, like a bingo dauber marking hits as you scan the card.
    // Recomputed on navigation so it follows the reader to each search
    // result; pass an empty string to clear it. Distinct from the persisted,
    // user-created Highlight annotations.
    virtual void setSearchTerm(const QString &term) = 0;
};
