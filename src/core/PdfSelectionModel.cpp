#include "PdfSelectionModel.h"

#include "core/TextSelectionUtil.h"

#include <algorithm>

void PdfSelectionModel::beginSelection(int pageIndex, const QPointF &pagePoint, const QVector<TextWord> &words)
{
    m_pageWords.clear(); // fresh gesture -- drop anything cached from a previous one
    if (!words.isEmpty()) {
        m_pageWords.insert(pageIndex, words);
    }
    m_anchorPageIndex = pageIndex;
    m_anchorPoint = pagePoint;
    m_focusPageIndex = pageIndex;
    m_focusPoint = pagePoint;
    recompute();
}

void PdfSelectionModel::updateSelection(int pageIndex, const QPointF &pagePoint, const QVector<TextWord> &words)
{
    if (m_anchorPageIndex < 0) {
        return; // no active gesture
    }
    if (!words.isEmpty()) {
        m_pageWords.insert(pageIndex, words);
    }
    m_focusPageIndex = pageIndex;
    m_focusPoint = pagePoint;
    recompute();
}

bool PdfSelectionModel::clearSelection()
{
    const bool hadSelection = !m_pageSelections.isEmpty();
    m_pageWords.clear();
    m_pageSelections.clear();
    m_selectedText.clear();
    m_anchorPageIndex = -1;
    m_focusPageIndex = -1;
    if (!hadSelection) {
        return false;
    }
    return true;
}

QVector<int> PdfSelectionModel::selectionPageIndices() const
{
    const QList<int> keys = m_pageSelections.keys(); // QMap keys() is ascending
    return QVector<int>(keys.begin(), keys.end());
}

QVector<QRectF> PdfSelectionModel::selectionRectsForPage(int pageIndex) const
{
    const auto it = m_pageSelections.constFind(pageIndex);
    return it != m_pageSelections.constEnd() ? it.value().rects : QVector<QRectF>();
}

QRectF PdfSelectionModel::selectionBoundingRectForPage(int pageIndex) const
{
    QRectF bounding;
    for (const QRectF &rect : selectionRectsForPage(pageIndex)) {
        bounding = bounding.isNull() ? rect : bounding.united(rect);
    }
    return bounding;
}

QString PdfSelectionModel::selectionTextForPage(int pageIndex) const
{
    const auto it = m_pageSelections.constFind(pageIndex);
    return it != m_pageSelections.constEnd() ? it.value().text : QString();
}

void PdfSelectionModel::recompute()
{
    m_pageSelections.clear();
    m_selectedText.clear();
    if (m_anchorPageIndex < 0 || m_focusPageIndex < 0) {
        return;
    }

    // Whichever endpoint (anchor or focus) sits on the lower-numbered page
    // is where the selection starts reading from -- true regardless of
    // which one the user actually dragged from/to, or whether they're on
    // the same page (the single-page case: lowPage == highPage, and both
    // "low" and "high" below resolve to the same point pair a plain
    // selectWordRange() would have used).
    const bool anchorIsLower = m_anchorPageIndex <= m_focusPageIndex;
    const int lowPage = anchorIsLower ? m_anchorPageIndex : m_focusPageIndex;
    const int highPage = anchorIsLower ? m_focusPageIndex : m_anchorPageIndex;
    const QPointF &lowPoint = anchorIsLower ? m_anchorPoint : m_focusPoint;
    const QPointF &highPoint = anchorIsLower ? m_focusPoint : m_anchorPoint;

    QStringList pageTexts;
    for (int page = lowPage; page <= highPage; ++page) {
        const auto wordsIt = m_pageWords.constFind(page);
        if (wordsIt == m_pageWords.constEnd() || wordsIt.value().isEmpty()) {
            // Words for this page haven't reached the model yet (the drag
            // jumped straight past it without the caller supplying them --
            // e.g. a fast flick), or it's a genuinely blank page. Either
            // way, skip it rather than mis-render: the caller will fill
            // this in on the next move event that does pass its words, or
            // it stays legitimately absent from the selection.
            continue;
        }
        const QVector<TextWord> &words = wordsIt.value();

        int startIndex = 0;
        int endIndex = words.size() - 1;
        if (page == lowPage) {
            startIndex = nearestWordIndex(words, lowPoint);
        }
        if (page == highPage) {
            endIndex = nearestWordIndex(words, highPoint);
        }
        if (startIndex < 0 || endIndex < 0) {
            continue;
        }
        if (startIndex > endIndex) {
            // Only possible when lowPage == highPage (both snaps landed on
            // the same page but in reverse reading order).
            std::swap(startIndex, endIndex);
        }

        const TextSelectionResult pageResult = wordRangeSelection(words, startIndex, endIndex);
        if (pageResult.wordRects.isEmpty()) {
            continue;
        }
        m_pageSelections.insert(page, {pageResult.text, pageResult.wordRects});
        pageTexts.append(pageResult.text);
    }
    m_selectedText = pageTexts.join(QLatin1Char('\n'));
}
