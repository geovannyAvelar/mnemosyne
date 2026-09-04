#include "PdfPageStackView.h"

#include "core/CoordinateUtil.h"
#include "core/TextSelectionUtil.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kMaterializeRadius = 2; // pages rendered on either side of the current-page hint
constexpr int kPageSpacing = 8; // px between stacked pages, and above the first/below the last
constexpr int kMinSelectionPixels = 3; // ignore accidental clicks/tiny drags

// Mirrors selectWordRange()'s (see core/TextSelectionUtil.h) text reconstruction rules
// (newline on a line break, space when hasSpaceAfter) so search matches
// found in the concatenated text map back to the right word rects, including
// matches that span a word boundary (e.g. a two-word query).
QString concatenatePageWords(const QVector<TextWord> &words, QVector<QPair<int, int>> &wordSpans)
{
    QString text;
    wordSpans.clear();
    wordSpans.reserve(words.size());

    for (int i = 0; i < words.size(); ++i) {
        const TextWord &word = words[i];
        const int start = text.size();
        text += word.text;
        wordSpans.append({start, text.size()});

        if (i + 1 < words.size()) {
            const TextWord &next = words[i + 1];
            const qreal verticalGap = std::abs(next.boundingBox.center().y() - word.boundingBox.center().y());
            if (verticalGap > word.boundingBox.height() / 2.0) {
                text += QLatin1Char('\n');
            } else if (word.hasSpaceAfter) {
                text += QLatin1Char(' ');
            }
        }
    }
    return text;
}
}

PdfPageStackView::PdfPageStackView(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::ClickFocus);
}

void PdfPageStackView::setDocument(IDocument *document)
{
    m_document = document;
    m_pageSizePoints.clear();
    if (!m_document) {
        return;
    }
    const int count = m_document->pageCount();
    m_pageSizePoints.reserve(count);
    for (int i = 0; i < count; ++i) {
        const std::unique_ptr<IPage> page = m_document->page(i);
        m_pageSizePoints.append(page ? page->sizePoints() : QSizeF(612, 792)); // US Letter fallback
    }
}

void PdfPageStackView::recomputeOffsets()
{
    m_pageOffsetY.resize(m_pageSizePoints.size());
    m_maxPageWidthPx = 0.0;
    qreal y = kPageSpacing;
    for (int i = 0; i < m_pageSizePoints.size(); ++i) {
        m_pageOffsetY[i] = y;
        m_maxPageWidthPx = std::max(m_maxPageWidthPx, pageWidthPx(i));
        y += pageHeightPx(i) + kPageSpacing;
    }
    resize(std::max(1, int(std::ceil(m_maxPageWidthPx))), std::max(1, int(std::ceil(y))));
}

void PdfPageStackView::setZoom(qreal zoom)
{
    m_zoom = zoom;
    recomputeOffsets();

    const QVector<int> materializedIndices = m_pageWords.keys();
    for (int i : materializedIndices) {
        materializePage(i, /*forceRerender=*/true);
    }
    update();
}

qreal PdfPageStackView::pageOffsetY(int index) const
{
    if (index < 0 || index >= m_pageOffsetY.size()) {
        return 0.0;
    }
    return m_pageOffsetY[index];
}

qreal PdfPageStackView::pageHeightPx(int index) const
{
    if (index < 0 || index >= m_pageSizePoints.size()) {
        return 0.0;
    }
    return m_pageSizePoints[index].height() * m_zoom;
}

qreal PdfPageStackView::pageWidthPx(int index) const
{
    if (index < 0 || index >= m_pageSizePoints.size()) {
        return 0.0;
    }
    return m_pageSizePoints[index].width() * m_zoom;
}

qreal PdfPageStackView::pageXOffset(int index) const
{
    return std::max(0.0, (m_maxPageWidthPx - pageWidthPx(index)) / 2.0);
}

int PdfPageStackView::pageIndexAtOffsetY(qreal absoluteY) const
{
    if (m_pageOffsetY.isEmpty()) {
        return -1;
    }
    // Finds the smallest index whose bottom edge exceeds absoluteY -- same
    // semantics as the old linear scan ("first canvas whose y()+height()
    // exceeds the query point"), including attributing a point that falls
    // exactly within the kPageSpacing gap between two pages to the
    // following page, and clamping anything past the last page's bottom to
    // the last page.
    int lo = 0;
    int hi = static_cast<int>(m_pageOffsetY.size()) - 1;
    while (lo < hi) {
        const int mid = lo + (hi - lo) / 2;
        if (m_pageOffsetY[mid] + pageHeightPx(mid) > absoluteY) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return lo;
}

void PdfPageStackView::setCurrentPageHint(int index)
{
    if (m_pageSizePoints.isEmpty()) {
        return;
    }
    const int windowStart = std::max(0, index - kMaterializeRadius);
    const int windowEnd = std::min(static_cast<int>(m_pageSizePoints.size()) - 1, index + kMaterializeRadius);

    for (int i = windowStart; i <= windowEnd; ++i) {
        materializePage(i);
    }

    const QVector<int> materializedIndices = m_pageWords.keys();
    for (int i : materializedIndices) {
        if (i < windowStart || i > windowEnd) {
            evictPage(i);
        }
    }
    update();
}

void PdfPageStackView::materializePage(int index, bool forceRerender)
{
    if (!m_document || index < 0 || index >= m_pageSizePoints.size()) {
        return;
    }
    const bool alreadyMaterialized = m_pageWords.contains(index);
    if (alreadyMaterialized && !forceRerender) {
        return;
    }

    std::unique_ptr<IPage> page = m_document->page(index);
    if (!page) {
        return;
    }
    if (!alreadyMaterialized) {
        m_pageWords.insert(index, page->words());
    }

    m_pageImages.insert(index, page->renderToImage(m_zoom));
    applyOverlaysToPage(index);
}

void PdfPageStackView::evictPage(int index)
{
    if (index < 0 || !m_pageWords.contains(index)) {
        return;
    }
    if (index == m_selectedPageIndex) {
        m_selectedText.clear();
        m_selectedBoundingPageRect = QRectF();
        m_selectedPageIndex = -1;
    }
    if (index == m_dragPageIndex) {
        // The page holding an in-progress drag just scrolled far enough
        // away to be evicted -- drop the drag rather than let a later
        // move/release operate against a stale/missing word list.
        m_dragging = false;
        m_dragPageIndex = -1;
        m_liveSelectionRects.clear();
    }
    m_pageWords.remove(index);
    m_pageImages.remove(index);
    m_pageHighlightRects.remove(index);
    m_pageSearchRects.remove(index);
}

void PdfPageStackView::setHighlights(const QVector<Highlight> &highlights)
{
    m_highlights = highlights;
    const QVector<int> materializedIndices = m_pageWords.keys();
    for (int i : materializedIndices) {
        applyOverlaysToPage(i);
    }
}

void PdfPageStackView::setSearchTerm(const QString &term)
{
    m_searchTerm = term;
    const QVector<int> materializedIndices = m_pageWords.keys();
    for (int i : materializedIndices) {
        applyOverlaysToPage(i);
    }
}

void PdfPageStackView::applyOverlaysToPage(int index)
{
    if (index < 0 || !m_pageWords.contains(index)) {
        return;
    }
    const int offsetX = int(pageXOffset(index));
    const int offsetY = int(pageOffsetY(index));

    QVector<HighlightMark> marks;
    for (const Highlight &highlight : m_highlights) {
        if (highlight.targetIndex == index) {
            QRect pixelRect = pageRectToPixelRect(highlight.pageRect, m_zoom);
            pixelRect.translate(offsetX, offsetY);
            marks.append({pixelRect, highlight.color});
        }
    }
    m_pageHighlightRects.insert(index, marks);

    QVector<QRect> searchRects;
    if (!m_searchTerm.isEmpty()) {
        const QVector<TextWord> &words = m_pageWords[index];
        if (!words.isEmpty()) {
            QVector<QPair<int, int>> wordSpans;
            const QString text = concatenatePageWords(words, wordSpans);

            int searchFrom = 0;
            while (true) {
                const int matchStart = text.indexOf(m_searchTerm, searchFrom, Qt::CaseInsensitive);
                if (matchStart < 0) {
                    break;
                }
                const int matchEnd = matchStart + m_searchTerm.size();

                for (int i = 0; i < wordSpans.size(); ++i) {
                    if (wordSpans[i].first < matchEnd && wordSpans[i].second > matchStart) {
                        QRect pixelRect = pageRectToPixelRect(words[i].boundingBox, m_zoom);
                        pixelRect.translate(offsetX, offsetY);
                        searchRects.append(pixelRect);
                    }
                }
                searchFrom = matchStart + 1; // allow overlapping matches (e.g. query "aa" in "aaa")
            }
        }
    }
    m_pageSearchRects.insert(index, searchRects);
    update();
}

void PdfPageStackView::clearSelection()
{
    m_dragging = false;
    m_hasSelection = false;
    m_dragPageIndex = -1;
    m_liveSelectionRects.clear();
    m_selectedText.clear();
    m_selectedBoundingPageRect = QRectF();
    m_selectedPageIndex = -1;
    update();
}

QPointF PdfPageStackView::toPagePoint(const QPoint &viewportPos, int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= m_pageSizePoints.size() || m_zoom <= 0) {
        return QPointF();
    }
    return QPointF((viewportPos.x() - pageXOffset(pageIndex)) / m_zoom,
                    (viewportPos.y() - pageOffsetY(pageIndex)) / m_zoom);
}

void PdfPageStackView::updateLiveSelection()
{
    // Reset "committed" state first -- repopulated below only if this
    // drag/click actually resolves to a real selection, mirroring the old
    // PdfView::updateSelectionFromDrag()'s unconditional clear-then-maybe-
    // repopulate structure.
    m_selectedText.clear();
    m_selectedBoundingPageRect = QRectF();
    m_selectedPageIndex = -1;

    // Not dragging and no committed selection either -- e.g. a released
    // click too small to count. Same OR-not-AND reasoning as the old
    // PdfPageCanvas::mouseReleaseEvent: a purely horizontal or vertical
    // drag has ~0 extent on the other axis.
    const bool nothingToShow = m_dragPageIndex < 0 || (!m_dragging && !m_hasSelection);
    if (nothingToShow) {
        m_liveSelectionRects.clear();
        emit selectionChanged();
        update();
        return;
    }

    const QVector<TextWord> words = m_pageWords.value(m_dragPageIndex);
    if (words.isEmpty()) {
        m_liveSelectionRects.clear();
        emit selectionChanged();
        update();
        return;
    }

    const QPointF anchorPoint = toPagePoint(m_dragAnchorPixel, m_dragPageIndex);
    const QPointF focusPoint = toPagePoint(m_dragFocusPixel, m_dragPageIndex);
    const TextSelectionResult selection = selectWordRange(words, anchorPoint, focusPoint);
    if (selection.text.isEmpty()) {
        m_liveSelectionRects.clear();
        emit selectionChanged();
        update();
        return;
    }

    const int offsetX = int(pageXOffset(m_dragPageIndex));
    const int offsetY = int(pageOffsetY(m_dragPageIndex));
    QVector<QRect> pixelRects;
    pixelRects.reserve(selection.wordRects.size());
    QRectF boundingPageRect;
    for (const QRectF &pageRect : selection.wordRects) {
        QRect pixelRect = pageRectToPixelRect(pageRect, m_zoom);
        pixelRect.translate(offsetX, offsetY);
        pixelRects.append(pixelRect);
        boundingPageRect = boundingPageRect.isNull() ? pageRect : boundingPageRect.united(pageRect);
    }

    m_liveSelectionRects = pixelRects;
    m_selectedText = selection.text;
    m_selectedBoundingPageRect = boundingPageRect;
    m_selectedPageIndex = m_dragPageIndex;

    emit selectionChanged();
    update();
}

void PdfPageStackView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), palette().color(QPalette::Base));

    if (m_pageSizePoints.isEmpty()) {
        return;
    }

    const int firstIndex = pageIndexAtOffsetY(event->rect().top());
    const int lastIndex = pageIndexAtOffsetY(event->rect().bottom());

    for (int i = firstIndex; i <= lastIndex; ++i) {
        const auto imageIt = m_pageImages.constFind(i);
        if (imageIt != m_pageImages.constEnd()) {
            painter.drawImage(QPointF(pageXOffset(i), pageOffsetY(i)), imageIt.value());
        }
        // else: not yet materialized -- the placeholder fill above already
        // covers this page's footprint.

        for (const HighlightMark &mark : m_pageHighlightRects.value(i)) {
            painter.fillRect(mark.rect, mark.color);
        }
        // Bolder/more saturated than persisted highlights so a dozen search
        // hits read as a distinct "dauber" pass rather than blending into
        // any highlights the reader made themselves.
        for (const QRect &rect : m_pageSearchRects.value(i)) {
            painter.fillRect(rect, QColor(255, 214, 0, 170));
        }
        // No outline, unlike a boxed-off region: a plain fill per word
        // reads as normal text selection, as in any text field.
        if (i == m_dragPageIndex) {
            for (const QRect &rect : m_liveSelectionRects) {
                painter.fillRect(rect, QColor(60, 130, 230, 90));
            }
        }
    }
}

void PdfPageStackView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_pageSizePoints.isEmpty()) {
        return;
    }
    setFocus();
    m_dragPageIndex = pageIndexAtOffsetY(event->pos().y());
    m_dragging = true;
    m_hasSelection = false;
    m_dragAnchorPixel = event->pos();
    m_dragFocusPixel = event->pos();
    updateLiveSelection();
}

void PdfPageStackView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging) {
        return;
    }
    m_dragFocusPixel = event->pos();
    updateLiveSelection();
}

void PdfPageStackView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_dragging || event->button() != Qt::LeftButton) {
        return;
    }
    m_dragging = false;
    m_dragFocusPixel = event->pos();

    const QRect rect = QRect(m_dragAnchorPixel, m_dragFocusPixel).normalized();
    m_hasSelection = rect.width() >= kMinSelectionPixels || rect.height() >= kMinSelectionPixels;
    updateLiveSelection();
}

void PdfPageStackView::contextMenuEvent(QContextMenuEvent *event)
{
    const int pageIndex = pageIndexAtOffsetY(event->pos().y());
    if (pageIndex < 0) {
        return;
    }
    emit contextMenuRequested(event->globalPos(), pageIndex, toPagePoint(event->pos(), pageIndex));
}
