#include "PdfSelectionController.h"

#include "PdfDocumentModel.h"

PdfSelectionController::PdfSelectionController(PdfDocumentModel *documentModel, QObject *parent)
    : QObject(parent)
    , m_documentModel(documentModel)
{
}

QVariantList PdfSelectionController::selectionPageIndices() const
{
    QVariantList indices;
    for (int pageIndex : m_model.selectionPageIndices()) {
        indices.append(pageIndex);
    }
    return indices;
}

QVariantList PdfSelectionController::selectionRectsForPage(int pageIndex) const
{
    QVariantList rects;
    const QVector<QRectF> modelRects = m_model.selectionRectsForPage(pageIndex);
    rects.reserve(modelRects.size());
    for (const QRectF &rect : modelRects) {
        rects.append(rect);
    }
    return rects;
}

void PdfSelectionController::beginSelection(int pageIndex, qreal pageX, qreal pageY)
{
    m_model.beginSelection(pageIndex, QPointF(pageX, pageY), m_documentModel->wordsForPage(pageIndex));
    emit selectionChanged();
}

void PdfSelectionController::updateSelection(int pageIndex, qreal pageX, qreal pageY)
{
    // wordsForPage() re-extracts via Poppler on every call (see there) --
    // skip it once this page is already known to the model, so dragging
    // back and forth within one page doesn't re-extract its words on every
    // single touch-move tick, only the first time the drag reaches it.
    const QVector<TextWord> words =
        m_model.hasWordsForPage(pageIndex) ? QVector<TextWord>() : m_documentModel->wordsForPage(pageIndex);
    m_model.updateSelection(pageIndex, QPointF(pageX, pageY), words);
    emit selectionChanged();
}

void PdfSelectionController::clearSelection()
{
    if (m_model.clearSelection()) {
        emit selectionChanged();
    }
}
