#include "PdfHighlightController.h"

#include "app/HighlightStore.h"

#include <QDateTime>

void PdfHighlightController::reload()
{
    m_highlights = HighlightStore::highlightsFor(m_bookHash);
}

int PdfHighlightController::indexAtPagePoint(const QPointF &pagePoint, int pageIndex) const
{
    for (int i = 0; i < m_highlights.size(); ++i) {
        if (m_highlights[i].targetIndex == pageIndex && m_highlights[i].pageRect.contains(pagePoint)) {
            return i;
        }
    }
    return -1;
}

void PdfHighlightController::addHighlight(int pageIndex, const QRectF &pageRect, const QString &text)
{
    Highlight highlight;
    highlight.targetIndex = pageIndex;
    highlight.pageRect = pageRect;
    highlight.text = text;
    highlight.createdAt = QDateTime::currentDateTime();

    HighlightStore::addHighlight(m_bookHash, highlight);
    reload();
}

void PdfHighlightController::addNote(int pageIndex, const QRectF &pageRect, const QString &text,
                                      const QString &note, const QColor &color)
{
    Highlight highlight;
    highlight.targetIndex = pageIndex;
    highlight.pageRect = pageRect;
    highlight.text = text;
    highlight.createdAt = QDateTime::currentDateTime();
    highlight.note = note;
    highlight.color = color;

    HighlightStore::addHighlight(m_bookHash, highlight);
    reload();
}

void PdfHighlightController::setNote(int index, const QString &note, const QColor &color)
{
    HighlightStore::setNote(m_bookHash, index, note);
    HighlightStore::setColor(m_bookHash, index, color);
    reload();
}

void PdfHighlightController::removeHighlight(int index)
{
    HighlightStore::removeHighlight(m_bookHash, index);
    reload();
}
