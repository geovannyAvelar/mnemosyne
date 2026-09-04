#include "PdfSelectionController.h"

#include "PdfDocumentModel.h"

PdfSelectionController::PdfSelectionController(PdfDocumentModel *documentModel, QObject *parent)
    : QObject(parent)
    , m_documentModel(documentModel)
{
}

QVariantList PdfSelectionController::selectionRects() const
{
    QVariantList rects;
    const QVector<QRectF> modelRects = m_model.selectionRects();
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

void PdfSelectionController::updateSelection(qreal pageX, qreal pageY)
{
    m_model.updateSelection(QPointF(pageX, pageY));
    emit selectionChanged();
}

void PdfSelectionController::clearSelection()
{
    if (m_model.clearSelection()) {
        emit selectionChanged();
    }
}
