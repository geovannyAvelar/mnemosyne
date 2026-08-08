#pragma once

#include <QImage>
#include <QPoint>
#include <QVector>
#include <QWidget>

// Paints a rendered PDF page and lets the user click-drag to select a
// region, shown as a translucent overlay. Coordinate conversion between
// widget pixels and page points (needed to ask Poppler for the text under a
// selection) is the caller's job — this widget only tracks pixel rects.
class PdfPageCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit PdfPageCanvas(QWidget *parent = nullptr);

    // scale is the render scale used to produce image (1.0 == 72 DPI),
    // stored so callers can convert the pixel selection back to page points.
    void setPage(const QImage &image, qreal scale);

    qreal scale() const { return m_scale; }
    bool hasSelection() const { return m_hasSelection; }
    QRect selectionPixelRect() const;
    void clearSelection();

    // Persisted highlight rects (image pixels) for the currently shown page,
    // drawn under the live selection overlay.
    void setHighlightRects(const QVector<QRect> &rects);

signals:
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

    QVector<QRect> m_highlightRects;
};
