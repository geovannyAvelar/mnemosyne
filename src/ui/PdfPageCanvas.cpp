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

QRect PdfPageCanvas::selectionPixelRect() const
{
    if (!m_hasSelection) {
        return {};
    }
    return QRect(m_selectionStart, m_selectionEnd).normalized();
}

void PdfPageCanvas::clearSelection()
{
    m_selecting = false;
    m_hasSelection = false;
    update();
}

void PdfPageCanvas::setHighlightRects(const QVector<QRect> &rects)
{
    m_highlightRects = rects;
    update();
}

void PdfPageCanvas::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawImage(0, 0, m_image);

    for (const QRect &rect : m_highlightRects) {
        painter.fillRect(rect, QColor(255, 235, 59, 110));
    }

    if (m_selecting || m_hasSelection) {
        const QRect rect = QRect(m_selectionStart, m_selectionEnd).normalized();
        painter.fillRect(rect, QColor(60, 130, 230, 90));
        painter.setPen(QColor(40, 100, 200));
        painter.drawRect(rect.adjusted(0, 0, -1, -1));
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
}

void PdfPageCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_selecting) {
        return;
    }
    m_selectionEnd = event->pos();
    update();
}

void PdfPageCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_selecting || event->button() != Qt::LeftButton) {
        return;
    }
    m_selecting = false;
    m_selectionEnd = event->pos();

    const QRect rect = QRect(m_selectionStart, m_selectionEnd).normalized();
    m_hasSelection = rect.width() >= kMinSelectionPixels && rect.height() >= kMinSelectionPixels;
    update();
    emit selectionChanged();
}
