#include "PdfSelectionModel.h"

#include "core/TextSelectionUtil.h"

void PdfSelectionModel::beginSelection(int pageIndex, const QPointF &pagePoint, const QVector<TextWord> &words)
{
    m_activePageIndex = pageIndex;
    m_words = words;
    m_anchorPoint = pagePoint;
    applySelection(m_anchorPoint);
}

void PdfSelectionModel::updateSelection(const QPointF &pagePoint)
{
    if (m_words.isEmpty()) {
        return;
    }
    applySelection(pagePoint);
}

bool PdfSelectionModel::clearSelection()
{
    m_words.clear();
    m_activePageIndex = -1;
    if (m_selectedText.isEmpty() && m_selectionRects.isEmpty()) {
        return false;
    }
    m_selectedText.clear();
    m_selectionRects.clear();
    return true;
}

QRectF PdfSelectionModel::selectionBoundingRect() const
{
    QRectF bounding;
    for (const QRectF &rect : m_selectionRects) {
        bounding = bounding.isNull() ? rect : bounding.united(rect);
    }
    return bounding;
}

void PdfSelectionModel::applySelection(const QPointF &focusPoint)
{
    const TextSelectionResult selection = selectWordRange(m_words, m_anchorPoint, focusPoint);
    m_selectedText = selection.text;
    m_selectionRects = selection.wordRects;
}
