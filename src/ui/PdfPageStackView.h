#pragma once

#include "core/Document.h"
#include "core/Highlight.h"
#include "core/InkStroke.h"
#include "core/PdfSelectionModel.h"

#include <QColor>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QPoint>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <QSharedPointer>
#include <QVector>
#include <QWidget>

#include <memory>

class QContextMenuEvent;
class QMouseEvent;

// Shared with background page-render tasks (see PdfPageStackView.cpp) so
// they can safely reach cached pages without racing their destruction --
// mirrors PdfPageImageProvider::documentMutex() on the Qt Quick side. Held
// via QSharedPointer rather than accessed through a raw PdfPageStackView*
// from those tasks, so a render already queued or running on QThreadPool
// stays safe even if this widget is destroyed first (e.g. a tab closing
// mid-render): the task keeps its own reference to this struct.
struct PdfPageRenderContext
{
    QMutex mutex;

    // Keyed by page index; populated once per page's first materialization
    // (see PdfPageStackView::materializePage()) instead of re-fetching via
    // IDocument::page() on every render -- Poppler reparses a page's
    // content stream each time page() is called, and before this cache
    // existed a page paid that cost once for its words and again on every
    // single render, including every re-render from a zoom change. Reused
    // across all of those; cleared per index by evictPage(), or entirely by
    // setDocument(nullptr), in lockstep with m_pageWords, whose keys remain
    // the single source of truth for "is this page materialized". Guarded
    // by `mutex` since a background render task reads it from a worker
    // thread -- and a cached IPage is only ever touched by a task while
    // still holding that lock, never copied out for later unlocked use, so
    // clearing this map under the lock (setDocument(nullptr), called from
    // PdfView's destructor before the real IDocument is freed) is enough to
    // guarantee no cached page outlives its document, matching Poppler-Qt's
    // own contract that a Page must not outlive its Document.
    QHash<int, std::shared_ptr<IPage>> pageCache;
};

// Paints the whole PDF page stack as a single continuous, virtualized
// viewport widget -- no per-page child widgets. Replaces the old model of
// one PdfPageCanvas QWidget per document page (still used, differently, by
// ComicView -- a single reused canvas re-imaged per page turn, untouched by
// this class): for a long document that meant thousands of always-alive
// QWidgets, plus a QShortcut and connections each, even though only a
// handful of pages are ever rendered. This widget instead computes every
// page's on-screen rectangle analytically from its point size and the
// current zoom, and paintEvent() only ever touches pages whose rectangle
// intersects the exposed region.
//
// Kept as the sole child of PdfView's QScrollArea (setWidgetResizable(false)),
// so wheel scrolling, the scrollbar, and the scroll area's own
// Qt::AlignHCenter (which centers this widget as a whole when it's
// narrower than the viewport) keep working unchanged -- this widget only
// reports its own total size and does the *per-page* centering within it
// (see pageXOffset()), mirroring the per-canvas Qt::AlignHCenter the old
// QVBoxLayout used to apply to each canvas individually.
class PdfPageStackView : public QWidget
{
    Q_OBJECT

public:
    struct HighlightMark
    {
        QRect rect;
        QColor color;
    };

    explicit PdfPageStackView(QWidget *parent = nullptr);

    // One-time setup: reads every page's size (points -- cheap, no
    // rasterization) from document, which must outlive this widget (PdfView
    // owns the real IDocument). Call setZoom() afterward to establish the
    // initial layout.
    //
    // Also called with nullptr from PdfView's destructor before m_document
    // is freed: this takes the render-context mutex to clear every cached
    // IPage background render tasks might reach (see PdfPageRenderContext),
    // blocking until any task currently mid-render (i.e. already holding
    // that same lock) finishes, so no cached page can still be touched --
    // or exist at all -- once this call returns.
    void setDocument(IDocument *document);

    void setZoom(qreal zoom);
    qreal zoom() const { return m_zoom; }

    // Dark-mode reading: inverts every rendered page's colors at paint time
    // (see paintEvent()) rather than re-rendering or caching a second
    // inverted QImage per page -- cheap enough to redo every frame, and
    // means toggling it needs no re-render or cache invalidation, just a
    // repaint. Highlights/search-match overlays are painted after the
    // invert and so keep their normal (non-inverted) colors.
    void setInvertColors(bool enabled);
    bool invertColors() const { return m_invertColors; }

    int pageCount() const { return m_pageSizePoints.size(); }

    // Page geometry in this widget's own local pixel coordinates at the
    // current zoom -- callers (PdfView's goToPage/setZoom, and tests) use
    // these instead of the old canvas->y()/height().
    qreal pageOffsetY(int index) const;
    qreal pageHeightPx(int index) const;
    qreal pageWidthPx(int index) const;
    qreal pageXOffset(int index) const; // horizontal centering within the widest page

    // The page whose [top, bottom) interval contains absoluteY (this
    // widget's local Y, same space as pageOffsetY()) -- a binary search
    // replacing the old O(n) scan over every canvas's y()/height().
    int pageIndexAtOffsetY(qreal absoluteY) const;

    // Recomputes which pages (index +/- kMaterializeRadius) should have a
    // rendered image + cached words, materializing newly-in-range pages and
    // evicting newly-out-of-range ones. Call whenever the current page
    // changes (scroll, goToPage) or after setZoom().
    void setCurrentPageHint(int index);

    // Recomputes overlay rects (highlights / search matches) for every
    // currently-materialized page and repaints. Call whenever the
    // corresponding PdfView-level state changes.
    void setHighlights(const QVector<Highlight> &highlights);
    void setSearchTerm(const QString &term);

    // Freehand pen strokes (see core/InkStroke.h) -- an app-side overlay,
    // like highlights, never written into the PDF itself. setInkStrokes()
    // mirrors setHighlights() (recomputes every materialized page's pixel
    // polygons and repaints); drawMode(), while on, makes a mouse drag draw
    // a new stroke instead of a text selection -- committed on release via
    // inkStrokeDrawn(), which PdfView persists (InkStore) and then feeds
    // back in through setInkStrokes(), the same round-trip
    // addHighlightForSelection()/setHighlights() already do.
    void setInkStrokes(const QVector<InkStroke> &strokes);
    void setDrawMode(bool enabled);
    bool drawMode() const { return m_drawMode; }

    // Live text selection, resolved from mouse drags via the shared
    // core/PdfSelectionModel -- the same state machine Qt Quick's
    // PdfSelectionController uses for touch long-press-drag selection. A
    // drag isn't confined to the page it started on: dragging past a
    // page's top/bottom edge continues the selection onto the next page,
    // so these are all page-plural -- see PdfSelectionModel's own doc
    // comment for how a multi-page range is resolved.
    QString selectedText() const { return m_selectionModel.selectedText(); }
    QVector<int> selectionPageIndices() const { return m_selectionModel.selectionPageIndices(); }
    QRectF selectedBoundingPageRectForPage(int pageIndex) const
    {
        return m_selectionModel.selectionBoundingRectForPage(pageIndex);
    }
    QString selectedTextForPage(int pageIndex) const { return m_selectionModel.selectionTextForPage(pageIndex); }
    // Whether the *last completed* drag was big enough to count as a
    // deliberate selection (kMinSelectionPixels) -- distinct from
    // selectedText() being non-empty, which is also true mid-drag before
    // release decides whether it commits. Used only to gate the
    // Highlight/Note actions.
    bool hasSelection() const { return m_committedSelection; }
    void clearSelection();

    // Test-visibility accessor, mirroring why PdfPageCanvas::searchRects()
    // existed -- lets ViewSearchTest verify the overlay without duplicating
    // the search-matching logic itself.
    QVector<QRect> searchRectsForPage(int index) const { return m_pageSearchRects.value(index); }

    // Test-visibility accessor confirming a materialized page's image has
    // actually arrived from its background render task -- lets a test poll
    // for that without duplicating the render-dispatch logic itself.
    bool hasRenderedImage(int index) const { return m_pageImages.contains(index); }

signals:
    // Fired on press, every move while dragging, and release -- mirrors
    // PdfPageCanvas::selectionChanged(). Nothing currently needs to connect
    // to this (PdfView queries the getters above directly instead of
    // keeping its own copy), kept for parity/future use.
    void selectionChanged();

    // Fired from contextMenuEvent() once pageIndex/pagePoint are resolved --
    // PdfView builds and execs the actual QMenu (it needs m_highlights/
    // HighlightStore, which this widget doesn't know about).
    void contextMenuRequested(const QPoint &globalPos, int pageIndex, const QPointF &pagePoint);

    // Fired from mouseReleaseEvent() for a plain click -- a press/release
    // with no meaningful drag between them (see kMinSelectionPixels) -- so
    // PdfView can hit-test it against m_highlightController (which, like
    // contextMenuRequested's HighlightStore access above, this widget has
    // no knowledge of) and show a note popup if it landed on a highlight
    // that has one. Not fired for a drag that commits a text selection.
    void clicked(int pageIndex, const QPointF &pagePoint, const QPoint &globalPos);

    // Fired from mouseReleaseEvent() when drawMode() is on and the just-
    // finished drag has at least 2 points -- PdfView persists it (InkStore)
    // and hands the updated stroke list back via setInkStrokes().
    void inkStrokeDrawn(int pageIndex, const QVector<QPointF> &pagePoints);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void materializePage(int index, bool forceRerender = false);
    void evictPage(int index);
    void applyOverlaysToPage(int index);
    void recomputeOffsets();
    // Dispatches a background render of page `index` at `zoom` unless one
    // for that exact (index, zoom) pair is already in flight. The image
    // itself is the only part of materialization moved off the UI thread --
    // words/overlays stay synchronous since they're cheap and everything
    // else (selection, search, highlights) only ever needs those, not the
    // rendered image.
    void requestPageImage(int index, qreal zoom);
    // Slot for PdfPageRenderTask::finished, delivered via a queued
    // connection from whichever QThreadPool worker thread rendered it.
    // Discards the result if the page has since been evicted or `zoom` no
    // longer matches m_zoom (a page can be re-requested at a new zoom while
    // an older-zoom render for it is still in flight).
    void onPageRendered(int index, qreal zoom, const QImage &image);
    // Re-derives m_liveSelectionRectsByPage (pixel space, per page) from
    // m_selectionModel's current page-space rects, emits selectionChanged(),
    // and repaints. Called after every mouse-handler touch of the model.
    void refreshLiveSelectionRects();
    // Converts a viewport-local pixel point to a page-space point (points,
    // zoom-independent) within the given page.
    QPointF toPagePoint(const QPoint &viewportPos, int pageIndex) const;
    // Words for `index`, from the materialized cache (m_pageWords) if
    // present, else fetched on demand and cached in m_onDemandWordCache --
    // a drag can reach a page well outside the materialize-for-render
    // radius (see kMaterializeRadius), and extracting a page's words is
    // cheap (no rasterization) compared to what materializePage() actually
    // guards against re-doing on every call.
    QVector<TextWord> wordsForPage(int index);
    // Translates+scales a stroke's page-space points into this widget's
    // current-zoom pixel space, for painting/caching (see m_pageInkMarks).
    QPolygonF strokeToPixelPolygon(const InkStroke &stroke) const;

    IDocument *m_document = nullptr; // non-owning; PdfView owns the real thing
    QVector<QSizeF> m_pageSizePoints;
    QVector<qreal> m_pageOffsetY; // cumulative top offset per page, at current zoom
    qreal m_zoom = 1.0;
    qreal m_maxPageWidthPx = 0.0;
    bool m_invertColors = false;

    QSharedPointer<PdfPageRenderContext> m_renderContext = QSharedPointer<PdfPageRenderContext>::create();
    QHash<int, qreal> m_pendingRenderZoom; // pageIndex -> zoom of its current in-flight render task, if any

    QHash<int, QImage> m_pageImages;
    QHash<int, QVector<TextWord>> m_pageWords; // also doubles as "is this page materialized"
    QHash<int, QVector<HighlightMark>> m_pageHighlightRects;
    QHash<int, QVector<QRect>> m_pageSearchRects;

    struct InkMark
    {
        QPolygonF polygon; // pixel space, current zoom
        QColor color;
        qreal widthPx = 2.0;
    };
    QVector<InkStroke> m_inkStrokes; // every stroke for the whole document, all pages
    QHash<int, QVector<InkMark>> m_pageInkMarks; // pixel-space, recomputed per materialized page in applyOverlaysToPage()
    bool m_drawMode = false;
    bool m_isDrawingStroke = false;
    int m_drawingPageIndex = -1;
    QVector<QPointF> m_currentStrokePoints; // page-space, the not-yet-committed in-progress stroke
    QColor m_drawColor = Qt::black; // pen used for the in-progress stroke's live preview
    qreal m_drawWidth = 2.0;
    // Words fetched on demand for a page a selection drag reached that
    // wasn't (and may never be) materialized for rendering -- see
    // wordsForPage(). Grows for the document's lifetime rather than being
    // evicted alongside render state: word lists are small (no image data),
    // and re-parsing the same page's content stream again on a later drag
    // that revisits it isn't worth the bookkeeping to avoid. Cleared only
    // by setDocument() (a new document invalidates every cached word list).
    QHash<int, QVector<TextWord>> m_onDemandWordCache;

    QVector<Highlight> m_highlights;
    QString m_searchTerm;

    // Live selection drag state. Unlike the old per-canvas model, a drag is
    // NOT locked to whichever page it started on: mouseMoveEvent()/
    // mouseReleaseEvent() re-resolve the page under the cursor on every
    // event (Qt keeps delivering these to this widget for the whole
    // gesture once it's pressed, even past this widget's own visible
    // viewport, the same mechanism that makes edge-of-viewport
    // auto-scroll-while-selecting possible in other Qt widgets), so the
    // selection can grow onto adjacent pages as the drag crosses their
    // boundary.
    QPoint m_dragAnchorPixel;
    QPoint m_dragFocusPixel;
    bool m_dragging = false;
    bool m_committedSelection = false;
    QHash<int, QVector<QRect>> m_liveSelectionRectsByPage; // pixel space, keyed by page index

    PdfSelectionModel m_selectionModel;
};
