#pragma once

#include <QImage>
#include <QPoint>
#include <QVector>
#include <QWidget>

// Paints a rendered PDF page and lets the user click-drag to select text.
// This widget only tracks the raw drag as pixel points; resolving that drag
// into the actual words it spans (via IPage::words()) and computing the
// per-word highlight rects to show is the caller's job (see PdfView) — the
// same widget/document split as selectionPixelRect() always had, just at
// word granularity now instead of one rectangle.
class PdfPageCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit PdfPageCanvas(QWidget *parent = nullptr);

    // scale is the render scale used to produce image (1.0 == 72 DPI),
    // stored so callers can convert pixel points back to page points.
    void setPage(const QImage &image, qreal scale);

    qreal scale() const { return m_scale; }
    bool isDragging() const { return m_selecting; }
    bool hasSelection() const { return m_hasSelection; }

    // Raw endpoints of the current/last drag, in widget pixels. Order is
    // press-to-release, not normalized — the caller resolves each into a
    // word index and sorts from there, so direction doesn't matter to it.
    QPoint dragAnchorPixel() const { return m_selectionStart; }
    QPoint dragFocusPixel() const { return m_selectionEnd; }

    // Normalized bounding rect of the drag (image pixels) — still used for
    // persisted highlights, which store a single page-space rect rather than
    // a per-word list.
    QRect selectionPixelRect() const;

    void clearSelection();

    // Word-shaped selection highlight (image pixels), recomputed by the
    // caller on every selectionChanged() while dragging.
    void setSelectionRects(const QVector<QRect> &rects);

    // Persisted highlight rects (image pixels) for the currently shown page,
    // drawn under the live selection overlay.
    void setHighlightRects(const QVector<QRect> &rects);

    // Search-match rects (image pixels) for the currently shown page, drawn
    // in a bolder yellow than persisted highlights — a bingo-dauber mark for
    // every hit of the active search term.
    void setSearchRects(const QVector<QRect> &rects);
    QVector<QRect> searchRects() const { return m_searchRects; }

signals:
    // Emitted on press, on every move while dragging, and on release, so the
    // caller can keep the word-shaped highlight live as the drag happens —
    // not just once it's finished.
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QImage m_image;
    qreal m_scale = 1.0;

    QPoint m_selectionStart;
    QPoint m_selectionEnd;
    bool m_selecting = false;
    bool m_hasSelection = false;

    QVector<QRect> m_selectionRects;
    QVector<QRect> m_highlightRects;
    QVector<QRect> m_searchRects;
};
