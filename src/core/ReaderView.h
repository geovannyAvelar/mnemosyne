#pragma once

#include "core/Document.h"

#include <QString>
#include <QVector>

struct SearchResult
{
    int targetIndex = -1; // PDF page index, EPUB/MOBI spine (or "part") index, Markdown heading index, or TXT character offset
    QString label; // e.g. "Page 3", "Chapter 2", "Part 4", "Line 87", or a Markdown heading's title
    QString snippet;
};

// Common surface MainWindow (and the TOC/search/notes docks) use to drive
// whatever format-specific view is currently open, without caring whether
// it's paginated raster (PDF) or reflowable HTML (EPUB).
class IReaderView
{
public:
    virtual ~IReaderView() = default;

    virtual QString documentTitle() const = 0;
    virtual QVector<TocNode> tableOfContents() const = 0;

    // TocNode::pageNumber means: PDF/CBZ page index for PdfView/ComicView,
    // spine index for EpubView, part index for MobiView, heading index for
    // MarkdownView, character offset for TxtView. ComicView/TxtView have no
    // table of contents (TXT has no structure to derive one from; CBZ just
    // doesn't parse ComicInfo.xml yet), so this is moot for them in practice
    // — but TxtView still relies on the same TocNode::pageNumber field for
    // search-result/note navigation.
    virtual void goToTocNode(const TocNode &node) = 0;

    // Current PDF/CBZ page index, EPUB/MOBI spine (or "part") index, Markdown heading index, or TXT character offset; used for reading-progress sync and TOC/search-result navigation.
    virtual int currentPosition() const = 0;

    // Case-insensitive full-text search across the whole document.
    virtual QVector<SearchResult> search(const QString &query) const = 0;

    // Marks every occurrence of term on the currently displayed page/chapter
    // in yellow, like a bingo dauber marking hits as you scan the card.
    // Recomputed on navigation so it follows the reader to each search
    // result; pass an empty string to clear it. Distinct from the persisted,
    // user-created Highlight annotations.
    virtual void setSearchTerm(const QString &term) = 0;

    // Reloads this document's highlights/notes from HighlightStore and
    // repaints them — called after the Notes dock edits or removes one out
    // from under the currently open view. A no-op for formats with no
    // highlight support (ComicView, HtmlView).
    virtual void refreshHighlights() = 0;
};
