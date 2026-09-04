#pragma once

#include "core/Document.h"
#include "core/Highlight.h"

#include <QColor>
#include <QHash>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QVector>
#include <QWidget>

class QContextMenuEvent;
class QMouseEvent;

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
    void setDocument(IDocument *document);

    void setZoom(qreal zoom);
    qreal zoom() const { return m_zoom; }

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

    // Live text selection, resolved from mouse drags via
    // core/TextSelectionUtil.h's selectWordRange() -- the same algorithm the
    // old per-canvas PdfView::updateSelectionFromDrag() used.
    QString selectedText() const { return m_selectedText; }
    int selectedPageIndex() const { return m_selectedPageIndex; }
    QRectF selectedBoundingPageRect() const { return m_selectedBoundingPageRect; }
    bool hasSelection() const { return m_hasSelection; }
    void clearSelection();

    // Test-visibility accessor, mirroring why PdfPageCanvas::searchRects()
    // existed -- lets ViewSearchTest verify the overlay without duplicating
    // the search-matching logic itself.
    QVector<QRect> searchRectsForPage(int index) const { return m_pageSearchRects.value(index); }

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
    void updateLiveSelection();
    // Converts a viewport-local pixel point to a page-space point (points,
    // zoom-independent) within the given page.
    QPointF toPagePoint(const QPoint &viewportPos, int pageIndex) const;

    IDocument *m_document = nullptr; // non-owning; PdfView owns the real thing
    QVector<QSizeF> m_pageSizePoints;
    QVector<qreal> m_pageOffsetY; // cumulative top offset per page, at current zoom
    qreal m_zoom = 1.0;
    qreal m_maxPageWidthPx = 0.0;

    QHash<int, QImage> m_pageImages;
    QHash<int, QVector<TextWord>> m_pageWords; // also doubles as "is this page materialized"
    QHash<int, QVector<HighlightMark>> m_pageHighlightRects;
    QHash<int, QVector<QRect>> m_pageSearchRects;

    QVector<Highlight> m_highlights;
    QString m_searchTerm;

    // Live selection drag state -- a drag is locked to whichever page it
    // started on for its whole gesture (see mousePressEvent), reproducing
    // the old per-canvas model's implicit behavior: Qt auto-grabs the mouse
    // to the pressed widget, so a drag that visually crosses into an
    // adjacent canvas's area still delivered move/release events to the
    // original canvas.
    int m_dragPageIndex = -1;
    QPoint m_dragAnchorPixel;
    QPoint m_dragFocusPixel;
    bool m_dragging = false;
    bool m_hasSelection = false;
    QVector<QRect> m_liveSelectionRects; // pixel space, for m_dragPageIndex only

    QString m_selectedText;
    QRectF m_selectedBoundingPageRect;
    int m_selectedPageIndex = -1;
};
