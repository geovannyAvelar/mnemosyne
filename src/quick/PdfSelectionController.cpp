#include "PdfSelectionController.h"

#include "PdfDocumentModel.h"

#include "core/TextSelectionUtil.h"

PdfSelectionController::PdfSelectionController(PdfDocumentModel *documentModel, QObject *parent)
    : QObject(parent)
    , m_documentModel(documentModel)
{
}

QVariantList PdfSelectionController::selectionRects() const
{
    QVariantList rects;
    rects.reserve(m_selectionRects.size());
    for (const QRectF &rect : m_selectionRects) {
        rects.append(rect);
    }
    return rects;
}

void PdfSelectionController::beginSelection(qreal pageX, qreal pageY)
{
    m_words = m_documentModel->wordsForPage(m_documentModel->currentPage());
    m_anchorPoint = QPointF(pageX, pageY);
    applySelection(m_anchorPoint);
}

void PdfSelectionController::updateSelection(qreal pageX, qreal pageY)
{
    if (m_words.isEmpty()) {
        return;
    }
    applySelection(QPointF(pageX, pageY));
}

void PdfSelectionController::clearSelection()
{
    m_words.clear();
    if (m_selectedText.isEmpty() && m_selectionRects.isEmpty()) {
        return;
    }
    m_selectedText.clear();
    m_selectionRects.clear();
    emit selectionChanged();
}

void PdfSelectionController::applySelection(const QPointF &focusPoint)
{
    const TextSelectionResult selection = selectWordRange(m_words, m_anchorPoint, focusPoint);
    m_selectedText = selection.text;
    m_selectionRects = selection.wordRects;
    emit selectionChanged();
}
