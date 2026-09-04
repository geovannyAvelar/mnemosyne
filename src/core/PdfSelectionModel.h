#pragma once

#include "core/Document.h" // TextWord

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

// Word-snapped drag-selection state machine, shared between desktop's
// mouse click-drag selection (src/ui/PdfPageStackView) and Qt Quick's touch
// long-press-drag selection (src/quick/PdfSelectionController) -- both used
// to keep their own copy of this exact state (anchor point, active page's
// words, selectWordRange() call), just fed mouse vs. touch points.
//
// Deliberately platform-agnostic: it doesn't know how to fetch a page's
// words itself (each platform caches/locks access differently -- Qt Quick's
// PdfDocumentModel::wordsForPage() takes a mutex around Poppler; desktop
// reads from PdfPageStackView's own already-materialized per-page cache),
// so callers hand the word list in at beginSelection() instead.
class PdfSelectionModel
{
public:
    // pagePoint and words.boundingBox are both page-space (points, 1/72in) --
    // callers convert from their own pixel space first.
    void beginSelection(int pageIndex, const QPointF &pagePoint, const QVector<TextWord> &words);
    void updateSelection(const QPointF &pagePoint);
    // Returns true if there was a selection to clear, so callers can skip a
    // redundant change notification.
    bool clearSelection();

    QString selectedText() const { return m_selectedText; }
    QVector<QRectF> selectionRects() const { return m_selectionRects; } // page-space, reading order
    QRectF selectionBoundingRect() const;                               // union of selectionRects()
    int selectionPageIndex() const { return m_activePageIndex; }

private:
    void applySelection(const QPointF &focusPoint);

    int m_activePageIndex = -1;
    QVector<TextWord> m_words; // active page's words, cached for the current gesture
    QPointF m_anchorPoint;
    QString m_selectedText;
    QVector<QRectF> m_selectionRects;
};
