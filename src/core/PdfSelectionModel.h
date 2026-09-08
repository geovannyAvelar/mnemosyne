#pragma once

#include "core/Document.h" // TextWord

#include <QHash>
#include <QMap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

// Word-snapped drag-selection state machine, shared between desktop's
// mouse click-drag selection (src/ui/PdfPageStackView) and Qt Quick's touch
// long-press-drag selection (src/quick/PdfSelectionController) -- both used
// to keep their own copy of this exact state, just fed mouse vs. touch
// points.
//
// Spans any number of pages: the anchor (where the gesture started) and the
// focus (where it currently is) can land on different pages, and every page
// in between the two is selected in full. Deliberately platform-agnostic:
// it doesn't know how to fetch a page's words itself (each platform
// caches/locks access differently -- Qt Quick's PdfDocumentModel::
// wordsForPage() takes a mutex around Poppler; desktop reads from
// PdfPageStackView's own already-materialized per-page cache, falling back
// to an on-demand fetch for a page a drag reaches that isn't), so callers
// hand each page's word list in as the gesture reaches it, via
// begin/updateSelection's `words` parameter -- see hasWordsForPage() for how
// a caller avoids re-fetching a page's words on every move event once
// they're already known here.
class PdfSelectionModel
{
public:
    // pagePoint and words[].boundingBox are both page-space (points,
    // 1/72in) -- callers convert from their own pixel space first. `words`
    // may be empty if the caller already knows this page's words are
    // cached here (see hasWordsForPage()) -- an empty vector never
    // overwrites an already-cached non-empty one.
    void beginSelection(int pageIndex, const QPointF &pagePoint, const QVector<TextWord> &words);
    void updateSelection(int pageIndex, const QPointF &pagePoint, const QVector<TextWord> &words);
    // Returns true if there was a selection to clear, so callers can skip a
    // redundant change notification.
    bool clearSelection();

    bool hasSelection() const { return !m_pageSelections.isEmpty(); }
    // Lets a caller skip re-fetching (and re-passing) a page's words on
    // every move event once the drag has already reached that page once.
    bool hasWordsForPage(int pageIndex) const { return m_pageWords.contains(pageIndex); }

    // Every page's text joined by "\n" between pages, in ascending page
    // order regardless of which end of the drag (anchor or focus) is
    // actually earlier in the document.
    QString selectedText() const { return m_selectedText; }

    // Pages currently spanned by the selection, ascending -- the inclusive
    // range between the anchor's page and the focus's page (never sparse:
    // every page strictly between the two is selected in full).
    QVector<int> selectionPageIndices() const;

    // Page-space rects/text/bounding-box for just one page's portion of the
    // selection -- empty/null if that page isn't currently part of it.
    QVector<QRectF> selectionRectsForPage(int pageIndex) const;
    QRectF selectionBoundingRectForPage(int pageIndex) const;
    QString selectionTextForPage(int pageIndex) const;

private:
    struct PageSelection
    {
        QString text;
        QVector<QRectF> rects;
    };

    void recompute();

    int m_anchorPageIndex = -1;
    QPointF m_anchorPoint;
    int m_focusPageIndex = -1;
    QPointF m_focusPoint;

    QHash<int, QVector<TextWord>> m_pageWords; // every page touched during this gesture
    QMap<int, PageSelection> m_pageSelections; // sorted by page index; only pages within [min,max] with known words
    QString m_selectedText;
};
