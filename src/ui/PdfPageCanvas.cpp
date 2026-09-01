#include "PdfPageCanvas.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr int kMinSelectionPixels = 3; // ignore accidental clicks/tiny drags
}

PdfPageCanvas::PdfPageCanvas(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::ClickFocus);
}

void PdfPageCanvas::setPage(const QImage &image, qreal scale)
{
    m_image = image;
    m_scale = scale;
    clearSelection();
    setFixedSize(m_image.size());
    update();
}

void PdfPageCanvas::clearSelection()
{
    m_selecting = false;
    m_hasSelection = false;
    m_selectionRects.clear();
    update();
}

void PdfPageCanvas::clearPage()
{
    m_image = QImage();
    m_highlightRects.clear();
    m_searchRects.clear();
    clearSelection(); // also calls update()
}

void PdfPageCanvas::setSelectionRects(const QVector<QRect> &rects)
{
    m_selectionRects = rects;
    update();
}

void PdfPageCanvas::setHighlightRects(const QVector<HighlightMark> &marks)
{
    m_highlightRects = marks;
    update();
}

void PdfPageCanvas::setSearchRects(const QVector<QRect> &rects)
{
    m_searchRects = rects;
    update();
}

void PdfPageCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    if (m_image.isNull()) {
        // Not yet materialized (outside PdfView's render window) — a flat
        // placeholder keeps the page's footprint in the scroll region
        // without paying for a render that isn't visible yet.
        painter.fillRect(rect(), palette().color(QPalette::Base));
        return;
    }

    painter.drawImage(0, 0, m_image);

    for (const HighlightMark &mark : m_highlightRects) {
        painter.fillRect(mark.rect, mark.color);
    }

    // Bolder/more saturated than persisted highlights so a dozen search hits
    // read as a distinct "dauber" pass over the page rather than blending
    // into any highlights the reader made themselves.
    for (const QRect &rect : m_searchRects) {
        painter.fillRect(rect, QColor(255, 214, 0, 170));
    }

    // No outline, unlike the old single-rectangle overlay: a plain fill per
    // word reads as normal text selection (as in any text field) instead of
    // a boxed-off region.
    for (const QRect &rect : m_selectionRects) {
        painter.fillRect(rect, QColor(60, 130, 230, 90));
    }
}

void PdfPageCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    setFocus();
    m_selecting = true;
    m_hasSelection = false;
    m_selectionStart = event->pos();
    m_selectionEnd = event->pos();
    update();
    emit selectionChanged();
}

void PdfPageCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_selecting) {
        return;
    }
    m_selectionEnd = event->pos();
    update();
    emit selectionChanged();
}

void PdfPageCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_selecting || event->button() != Qt::LeftButton) {
        return;
    }
    m_selecting = false;
    m_selectionEnd = event->pos();

    // OR, not AND: a plain drag across one line of text has ~0 height (every
    // word on a line shares nearly the same vertical extent), so requiring
    // both dimensions to clear the threshold meant a purely horizontal
    // selection could fail to commit at all.
    const QRect rect = QRect(m_selectionStart, m_selectionEnd).normalized();
    m_hasSelection = rect.width() >= kMinSelectionPixels || rect.height() >= kMinSelectionPixels;
    update();
    emit selectionChanged();
}
