#include "PdfView.h"

#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleDriveSync.h"
#endif
#include "app/HighlightStore.h"
#include "app/HighlightSync.h"
#include "app/ProgressSyncLog.h"
#include "app/ReadingProgressStore.h"
#include "core/CoordinateUtil.h"
#include "core/SearchUtil.h"
#include "core/TextSelectionUtil.h"
#include "pdf/PopplerPdfDocument.h"
#include "ui/NoteDialog.h"
#include "ui/PdfPageCanvas.h"
#include "ui/SyncPromptBar.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPair>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QKeyEvent>

#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kMinZoom = 0.25;
constexpr qreal kMaxZoom = 4.0;
constexpr qreal kZoomStep = 0.25;
constexpr int kMaterializeRadius = 2; // pages rendered on either side of m_currentPage
constexpr int kPageSpacing = 8; // px between stacked pages
constexpr int kArrowKeyScrollStep = 60; // px per arrow key press

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

PdfView::PdfView(std::unique_ptr<IDocument> document, QString filePath, QWidget *parent)
    : QWidget(parent)
    , m_document(std::move(document))
    , m_filePath(std::move(filePath))
    // m_bookHash isn't set until restoreProgressAndCheckSync() runs in the
    // constructor body below, so the initial load recomputes the hash here
    // directly (cheap — FileIdentity caches by path+size+mtime).
    , m_highlights(HighlightStore::highlightsFor(FileIdentity::contentHash(m_filePath)))
{
    setupUi();
    restoreProgressAndCheckSync(); // sets m_currentPage/m_zoom before the first render, so there's no visible jump
    buildPageStack();
    updateNavigationState();
    updateMaterializationWindow();

    // The page stack's layout (and thus each canvas's y()) isn't realized
    // until the widget is actually shown/processed by the event loop;
    // defer the initial scroll-to-saved-page by one tick.
    const int targetPage = m_currentPage;
    QPointer<PdfView> self = this;
    QTimer::singleShot(0, this, [self, targetPage] {
        if (!self || targetPage < 0 || targetPage >= self->m_pageCanvases.size()) {
            return;
        }
        self->m_scrollArea->verticalScrollBar()->setValue(self->m_pageCanvases[targetPage]->y());
    });
}

QString PdfView::documentTitle() const
{
    return m_document ? m_document->title() : QString();
}

QVector<TocNode> PdfView::tableOfContents() const
{
    return m_document ? m_document->tableOfContents() : QVector<TocNode>();
}

void PdfView::goToTocNode(const TocNode &node)
{
    if (node.pageNumber >= 0) {
        goToPage(node.pageNumber);
    }
}

int PdfView::currentPosition() const
{
    return m_currentPage;
}

bool PdfView::hasPendingSyncPrompt() const
{
    // See EpubView::hasPendingSyncPrompt() for why isHidden(), not isVisible().
    return !m_syncPromptBar->isHidden();
}

QVector<SearchResult> PdfView::search(const QString &query) const
{
    return searchFile(m_filePath, query);
}

QVector<SearchResult> PdfView::searchFile(const QString &filePath, const QString &query)
{
    QVector<SearchResult> results;
    if (query.trimmed().isEmpty()) {
        return results;
    }

    QString error;
    const std::unique_ptr<PopplerPdfDocument> document = PopplerPdfDocument::load(filePath, &error);
    if (!document) {
        return results;
    }

    for (int i = 0; i < document->pageCount(); ++i) {
        std::unique_ptr<IPage> page = document->page(i);
        if (!page) {
            continue;
        }
        const QString text = page->text();
        if (text.contains(query, Qt::CaseInsensitive)) {
            SearchResult result;
            result.targetIndex = i;
            result.label = tr("Page %1").arg(i + 1);
            result.snippet = makeSearchSnippet(text, query);
            results.append(result);
        }
    }
    return results;
}

void PdfView::setSearchTerm(const QString &term)
{
    m_searchTerm = term.trimmed();
    refreshSearchOverlay();
}

void PdfView::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(6, 4, 6, 4);

    auto *prevButton = new QPushButton(tr("< Prev"), toolbar);
    auto *nextButton = new QPushButton(tr("Next >"), toolbar);
    connect(prevButton, &QPushButton::clicked, this, &PdfView::previousPage);
    connect(nextButton, &QPushButton::clicked, this, &PdfView::nextPage);

    m_pageSpinBox = new QSpinBox(toolbar);
    m_pageSpinBox->setMinimum(1);
    m_pageSpinBox->setMaximum(std::max(1, m_document ? m_document->pageCount() : 1));
    connect(m_pageSpinBox, &QSpinBox::editingFinished, this, [this] {
        goToPage(m_pageSpinBox->value() - 1);
    });

    m_pageCountLabel = new QLabel(toolbar);

    auto *zoomOutButton = new QPushButton(tr("-"), toolbar);
    auto *zoomInButton = new QPushButton(tr("+"), toolbar);
    connect(zoomOutButton, &QPushButton::clicked, this, &PdfView::zoomOut);
    connect(zoomInButton, &QPushButton::clicked, this, &PdfView::zoomIn);

    toolbarLayout->addWidget(prevButton);
    toolbarLayout->addWidget(m_pageSpinBox);
    toolbarLayout->addWidget(m_pageCountLabel);
    toolbarLayout->addWidget(nextButton);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(zoomOutButton);
    toolbarLayout->addWidget(zoomInButton);

    m_pageStack = new QWidget(this);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_pageStack);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignHCenter);
    m_scrollArea->viewport()->installEventFilter(this); // Ctrl+wheel zoom
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, &PdfView::onScrolled);

    m_syncPromptBar = new SyncPromptBar(this);

    layout->addWidget(toolbar);
    layout->addWidget(m_syncPromptBar);
    layout->addWidget(m_scrollArea, 1);

    m_progressSaveTimer = new QTimer(this);
    m_progressSaveTimer->setSingleShot(true);
    connect(m_progressSaveTimer, &QTimer::timeout, this, &PdfView::saveProgressNow);
}

void PdfView::buildPageStack()
{
    if (!m_document) {
        return;
    }
    const int count = m_document->pageCount();

    auto *stackLayout = new QVBoxLayout(m_pageStack);
    stackLayout->setContentsMargins(0, kPageSpacing, 0, kPageSpacing);
    stackLayout->setSpacing(kPageSpacing);

    m_pageSizePoints.reserve(count);
    m_pageCanvases.reserve(count);

    for (int i = 0; i < count; ++i) {
        const std::unique_ptr<IPage> page = m_document->page(i);
        const QSizeF points = page ? page->sizePoints() : QSizeF(612, 792); // US Letter fallback
        m_pageSizePoints.append(points);

        auto *canvas = new PdfPageCanvas(m_pageStack);
        canvas->setFixedSize((points * m_zoom).toSize());
        canvas->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(canvas, &PdfPageCanvas::selectionChanged, this, &PdfView::updateSelectionFromDrag);
        connect(canvas, &PdfPageCanvas::customContextMenuRequested, this, &PdfView::showCanvasContextMenu);

        auto *copyShortcut = new QShortcut(QKeySequence::Copy, canvas);
        connect(copyShortcut, &QShortcut::activated, this, &PdfView::copySelection);

        stackLayout->addWidget(canvas, 0, Qt::AlignHCenter);
        m_pageCanvases.append(canvas);
    }
    stackLayout->addStretch();

    // QScrollArea (widgetResizable == false) uses the content widget's own
    // size() to compute the scroll range -- unlike PdfPageCanvas's old
    // setFixedSize() self-sizing, a QWidget with a layout doesn't resize
    // itself to its layout's size hint on its own until asked.
    m_pageStack->adjustSize();
}

void PdfView::goToPage(int index)
{
    if (!m_document || m_pageCanvases.isEmpty()) {
        return;
    }
    index = std::clamp(index, 0, static_cast<int>(m_pageCanvases.size()) - 1);
    m_currentPage = index;
    updateMaterializationWindow();
    updateNavigationState();
    scheduleProgressSave();

    // Deferred: a resize/re-layout (e.g. from a just-applied zoom change)
    // may not have settled into accurate canvas y() positions yet.
    QPointer<PdfView> self = this;
    QTimer::singleShot(0, this, [self, index] {
        if (!self || index >= self->m_pageCanvases.size()) {
            return;
        }
        self->m_scrollArea->verticalScrollBar()->setValue(self->m_pageCanvases[index]->y());
    });
}

void PdfView::nextPage()
{
    goToPage(m_currentPage + 1);
}

void PdfView::previousPage()
{
    goToPage(m_currentPage - 1);
}

void PdfView::zoomIn()
{
    setZoom(std::min(kMaxZoom, m_zoom + kZoomStep));
}

void PdfView::zoomOut()
{
    setZoom(std::max(kMinZoom, m_zoom - kZoomStep));
}

void PdfView::setZoom(qreal newZoom)
{
    if (m_pageCanvases.isEmpty() || qFuzzyCompare(newZoom, m_zoom)) {
        return;
    }

    // Preserve the reader's visual position: capture where the viewport top
    // sits within m_currentPage's canvas, as a fraction of its (old) height,
    // so the same spot stays under the viewport top once every page resizes.
    PdfPageCanvas *currentCanvas = m_pageCanvases[m_currentPage];
    const int oldTop = currentCanvas->y();
    const int oldHeight = currentCanvas->height();
    const qreal offsetFraction =
        oldHeight > 0 ? qreal(m_scrollArea->verticalScrollBar()->value() - oldTop) / oldHeight : 0.0;

    m_zoom = newZoom;

    const QVector<int> materializedIndices = m_pageWords.keys();
    for (int i = 0; i < m_pageCanvases.size(); ++i) {
        m_pageCanvases[i]->setFixedSize((m_pageSizePoints[i] * m_zoom).toSize());
    }
    m_pageStack->adjustSize(); // resize the content widget to match the new total page-stack height
    for (int i : materializedIndices) {
        materializePage(i, /*forceRerender=*/true);
    }
    updateMaterializationWindow();

    const int page = m_currentPage;
    const qreal fraction = offsetFraction;
    QPointer<PdfView> self = this;
    QTimer::singleShot(0, this, [self, page, fraction] {
        if (!self || page >= self->m_pageCanvases.size()) {
            return;
        }
        PdfPageCanvas *canvas = self->m_pageCanvases[page];
        self->m_scrollArea->verticalScrollBar()->setValue(canvas->y() + int(fraction * canvas->height()));
    });

    scheduleProgressSave();
}

void PdfView::copySelection()
{
    if (!m_selectedText.isEmpty()) {
        QApplication::clipboard()->setText(m_selectedText);
    }
}

void PdfView::materializePage(int index, bool forceRerender)
{
    if (!m_document || index < 0 || index >= m_pageCanvases.size()) {
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

    const QImage image = page->renderToImage(m_zoom);
    m_pageCanvases[index]->setPage(image, m_zoom);
    applyOverlaysToPage(index);
}

void PdfView::evictPage(int index)
{
    if (index < 0 || index >= m_pageCanvases.size() || !m_pageWords.contains(index)) {
        return;
    }
    if (index == m_selectedPageIndex) {
        m_selectedText.clear();
        m_selectedBoundingPageRect = QRectF();
        m_selectedPageIndex = -1;
    }
    m_pageWords.remove(index);
    m_pageCanvases[index]->clearPage();
}

void PdfView::updateMaterializationWindow()
{
    if (m_pageCanvases.isEmpty()) {
        return;
    }
    const int windowStart = std::max(0, m_currentPage - kMaterializeRadius);
    const int windowEnd = std::min(static_cast<int>(m_pageCanvases.size()) - 1, m_currentPage + kMaterializeRadius);

    for (int i = windowStart; i <= windowEnd; ++i) {
        materializePage(i);
    }

    const QVector<int> materializedIndices = m_pageWords.keys();
    for (int i : materializedIndices) {
        if (i < windowStart || i > windowEnd) {
            evictPage(i);
        }
    }
}

int PdfView::topmostVisiblePage() const
{
    if (m_pageCanvases.isEmpty()) {
        return 0;
    }
    const int viewportTop = m_scrollArea->verticalScrollBar()->value();
    const int viewportHeight = m_scrollArea->viewport()->height();
    const int viewportMid = viewportTop + viewportHeight / 2;

    for (int i = 0; i < m_pageCanvases.size(); ++i) {
        PdfPageCanvas *canvas = m_pageCanvases[i];
        if (canvas->y() + canvas->height() > viewportMid) {
            return i;
        }
    }
    return static_cast<int>(m_pageCanvases.size()) - 1;
}

void PdfView::onScrolled()
{
    const int page = topmostVisiblePage();
    if (page != m_currentPage) {
        m_currentPage = page;
        updateNavigationState();
        scheduleProgressSave();
    }
    updateMaterializationWindow();
}

void PdfView::updateNavigationState()
{
    if (!m_document) {
        return;
    }
    const QSignalBlocker blocker(m_pageSpinBox);
    m_pageSpinBox->setMaximum(std::max(1, m_document->pageCount()));
    m_pageSpinBox->setValue(m_currentPage + 1);
    m_pageCountLabel->setText(tr("of %1").arg(m_document->pageCount()));
}

void PdfView::updateSelectionFromDrag()
{
    auto *canvas = qobject_cast<PdfPageCanvas *>(sender());
    if (!canvas) {
        return;
    }
    const int pageIndex = m_pageCanvases.indexOf(canvas);
    if (pageIndex < 0) {
        return;
    }

    // A drag starting on a different page than whatever was last selected
    // supersedes it — clear that other canvas's stale overlay.
    if (m_selectedPageIndex >= 0 && m_selectedPageIndex != pageIndex && m_selectedPageIndex < m_pageCanvases.size()) {
        m_pageCanvases[m_selectedPageIndex]->setSelectionRects({});
    }

    m_selectedText.clear();
    m_selectedBoundingPageRect = QRectF();
    m_selectedPageIndex = -1;

    if (!canvas->isDragging() && !canvas->hasSelection()) {
        canvas->setSelectionRects({});
        return;
    }

    const QVector<TextWord> words = m_pageWords.value(pageIndex);
    if (words.isEmpty()) {
        canvas->setSelectionRects({});
        return;
    }

    const qreal scale = canvas->scale();
    const QPointF anchorPoint = QPointF(canvas->dragAnchorPixel()) / scale;
    const QPointF focusPoint = QPointF(canvas->dragFocusPixel()) / scale;

    const TextSelectionResult selection = selectWordRange(words, anchorPoint, focusPoint);
    if (selection.text.isEmpty()) {
        canvas->setSelectionRects({});
        return;
    }

    QVector<QRect> pixelRects;
    pixelRects.reserve(selection.wordRects.size());
    QRectF boundingPageRect;
    for (const QRectF &pageRect : selection.wordRects) {
        pixelRects.append(pageRectToPixelRect(pageRect, m_zoom));
        boundingPageRect = boundingPageRect.isNull() ? pageRect : boundingPageRect.united(pageRect);
    }

    canvas->setSelectionRects(pixelRects);
    m_selectedText = selection.text;
    m_selectedBoundingPageRect = boundingPageRect;
    m_selectedPageIndex = pageIndex;
}

void PdfView::applyOverlaysToPage(int index)
{
    if (index < 0 || index >= m_pageCanvases.size() || !m_pageWords.contains(index)) {
        return;
    }
    PdfPageCanvas *canvas = m_pageCanvases[index];

    QVector<PdfPageCanvas::HighlightMark> marks;
    for (const Highlight &highlight : m_highlights) {
        if (highlight.targetIndex == index) {
            marks.append({pageRectToPixelRect(highlight.pageRect, m_zoom), highlight.color});
        }
    }
    canvas->setHighlightRects(marks);

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
                        searchRects.append(pageRectToPixelRect(words[i].boundingBox, m_zoom));
                    }
                }
                searchFrom = matchStart + 1; // allow overlapping matches (e.g. query "aa" in "aaa")
            }
        }
    }
    canvas->setSearchRects(searchRects);
}

void PdfView::refreshHighlightOverlay()
{
    const QVector<int> materializedIndices = m_pageWords.keys();
    for (int i : materializedIndices) {
        applyOverlaysToPage(i);
    }
}

void PdfView::refreshSearchOverlay()
{
    const QVector<int> materializedIndices = m_pageWords.keys();
    for (int i : materializedIndices) {
        applyOverlaysToPage(i);
    }
}

int PdfView::highlightIndexAtPagePoint(const QPointF &pagePoint, int pageIndex) const
{
    for (int i = 0; i < m_highlights.size(); ++i) {
        if (m_highlights[i].targetIndex == pageIndex && m_highlights[i].pageRect.contains(pagePoint)) {
            return i;
        }
    }
    return -1;
}

void PdfView::refreshHighlights()
{
    m_highlights = HighlightStore::highlightsFor(m_bookHash);
    refreshHighlightOverlay();
}

void PdfView::addHighlightForSelection()
{
    if (m_selectedPageIndex < 0 || m_selectedText.isEmpty()) {
        return;
    }
    PdfPageCanvas *canvas = m_pageCanvases[m_selectedPageIndex];
    if (!canvas->hasSelection()) {
        return;
    }

    Highlight highlight;
    highlight.targetIndex = m_selectedPageIndex;
    highlight.pageRect = m_selectedBoundingPageRect;
    highlight.text = m_selectedText;
    highlight.createdAt = QDateTime::currentDateTime();

    HighlightStore::addHighlight(m_bookHash, highlight);
    m_highlights = HighlightStore::highlightsFor(m_bookHash);

    const int changedPage = m_selectedPageIndex;
    canvas->clearSelection();
    m_selectedText.clear();
    m_selectedPageIndex = -1;
    applyOverlaysToPage(changedPage);
    emit highlightsChanged();
}

void PdfView::addNoteForSelection()
{
    if (m_selectedPageIndex < 0 || m_selectedText.isEmpty()) {
        return;
    }
    PdfPageCanvas *canvas = m_pageCanvases[m_selectedPageIndex];
    if (!canvas->hasSelection()) {
        return;
    }

    const std::optional<NoteDialog::Result> result = NoteDialog::show(this, QString(), kDefaultHighlightColor);
    if (!result || result->note.isEmpty()) {
        return;
    }

    Highlight highlight;
    highlight.targetIndex = m_selectedPageIndex;
    highlight.pageRect = m_selectedBoundingPageRect;
    highlight.text = m_selectedText;
    highlight.createdAt = QDateTime::currentDateTime();
    highlight.note = result->note;
    highlight.color = result->color;

    HighlightStore::addHighlight(m_bookHash, highlight);
    m_highlights = HighlightStore::highlightsFor(m_bookHash);

    const int changedPage = m_selectedPageIndex;
    canvas->clearSelection();
    m_selectedText.clear();
    m_selectedPageIndex = -1;
    applyOverlaysToPage(changedPage);
    emit highlightsChanged();
}

void PdfView::showCanvasContextMenu(const QPoint &pos)
{
    auto *canvas = qobject_cast<PdfPageCanvas *>(sender());
    if (!canvas) {
        return;
    }
    const int pageIndex = m_pageCanvases.indexOf(canvas);
    if (pageIndex < 0) {
        return;
    }

    const bool hasSelectionHere = pageIndex == m_selectedPageIndex && !m_selectedText.isEmpty();

    QMenu menu(canvas);

    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(hasSelectionHere);
    connect(copyAction, &QAction::triggered, this, &PdfView::copySelection);

    QAction *highlightAction = menu.addAction(tr("Highlight"));
    highlightAction->setEnabled(hasSelectionHere);
    connect(highlightAction, &QAction::triggered, this, &PdfView::addHighlightForSelection);

    QAction *addNoteAction = menu.addAction(tr("Add Note..."));
    addNoteAction->setEnabled(hasSelectionHere);
    connect(addNoteAction, &QAction::triggered, this, &PdfView::addNoteForSelection);

    const QPointF pagePoint(pos.x() / canvas->scale(), pos.y() / canvas->scale());
    const int existingHighlightIndex = highlightIndexAtPagePoint(pagePoint, pageIndex);
    if (existingHighlightIndex >= 0) {
        menu.addSeparator();
        const bool hasNote = !m_highlights[existingHighlightIndex].note.isEmpty();
        QAction *noteAction = menu.addAction(hasNote ? tr("Edit Note...") : tr("Add Note..."));
        connect(noteAction, &QAction::triggered, this, [this, existingHighlightIndex, pageIndex] {
            const std::optional<NoteDialog::Result> result = NoteDialog::show(
                this, m_highlights[existingHighlightIndex].note, m_highlights[existingHighlightIndex].color);
            if (!result) {
                return;
            }
            HighlightStore::setNote(m_bookHash, existingHighlightIndex, result->note);
            HighlightStore::setColor(m_bookHash, existingHighlightIndex, result->color);
            m_highlights = HighlightStore::highlightsFor(m_bookHash);
            applyOverlaysToPage(pageIndex);
            emit highlightsChanged();
        });
        QAction *removeAction = menu.addAction(hasNote ? tr("Remove Note") : tr("Remove Highlight"));
        connect(removeAction, &QAction::triggered, this, [this, existingHighlightIndex, pageIndex] {
            HighlightStore::removeHighlight(m_bookHash, existingHighlightIndex);
            m_highlights = HighlightStore::highlightsFor(m_bookHash);
            applyOverlaysToPage(pageIndex);
            emit highlightsChanged();
        });
    }

    menu.exec(canvas->mapToGlobal(pos));
}

void PdfView::restoreProgressAndCheckSync()
{
    m_bookHash = FileIdentity::contentHash(m_filePath);
    if (m_bookHash.isEmpty() || !m_document) {
        return;
    }

    if (const auto local = ReadingProgressStore::get(m_bookHash)) {
        m_currentPage = std::clamp(local->position, 0, std::max(0, m_document->pageCount() - 1));
        m_zoom = std::clamp(local->zoom, kMinZoom, kMaxZoom);
    }

    std::optional<ProgressSyncLog::RemoteEntry> localFolderRemote =
        ProgressSyncLog::latestFromOtherDevices(m_bookHash, DeviceIdentity::id());
    if (localFolderRemote) {
        offerSyncedPosition(*localFolderRemote);
    }

#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    const QString bookHash = m_bookHash;
    const QDateTime localFolderTimestamp = localFolderRemote ? localFolderRemote->timestamp : QDateTime();
    GoogleDriveSync::latestFromOtherDevices(
        bookHash, DeviceIdentity::id(),
        [this, bookHash, localFolderTimestamp](std::optional<ProgressSyncLog::RemoteEntry> googleRemote) {
            if (!googleRemote || bookHash != m_bookHash) {
                return; // no result, or this view has since moved on to a different book
            }
            if (localFolderTimestamp.isValid() && googleRemote->timestamp <= localFolderTimestamp) {
                return; // the local-folder sync already offered something at least as new
            }
            offerSyncedPosition(*googleRemote);
        });
#endif

    HighlightSync::pull(m_bookHash, [this](bool changed) {
        if (changed) {
            refreshHighlights();
            emit highlightsChanged();
        }
    });
}

void PdfView::offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote)
{
    if (remote.position == m_currentPage) {
        return;
    }
    m_syncPromptBar->showPrompt(
        tr("Synced position available: page %1 (from %2) — jump?").arg(remote.position + 1).arg(remote.deviceName));
    const int remotePosition = remote.position;
    const qreal remoteZoom = remote.zoom;
    connect(m_syncPromptBar, &SyncPromptBar::jumpRequested, this, [this, remotePosition, remoteZoom] {
        setZoom(std::clamp(remoteZoom, kMinZoom, kMaxZoom));
        goToPage(remotePosition);
    });
}

void PdfView::scheduleProgressSave()
{
    m_progressSaveTimer->start(1500);
}

void PdfView::saveProgressNow()
{
    if (m_bookHash.isEmpty()) {
        return;
    }
    ReadingProgressStore::set(m_bookHash, m_currentPage, m_zoom);
    ProgressSyncLog::appendEntry(m_bookHash, documentTitle(), m_currentPage, m_zoom);
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    GoogleDriveSync::appendEntry(m_bookHash, documentTitle(), m_currentPage, m_zoom);
#endif
}

bool PdfView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_scrollArea->viewport() && event->type() == QEvent::Wheel) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);

        if (wheelEvent->modifiers().testFlag(Qt::ControlModifier)) {
            if (wheelEvent->angleDelta().y() > 0) {
                zoomIn();
            } else if (wheelEvent->angleDelta().y() < 0) {
                zoomOut();
            }
            return true;
        }

        // Not consumed above -- native scrolling handles it. But if the
        // scrollbar's range is degenerate (e.g. a page shorter than the
        // viewport), its value won't actually change, so valueChanged won't
        // fire and onScrolled() would never run. Call it directly, deferred
        // past the native scroll this event triggers.
        QPointer<PdfView> self = this;
        QTimer::singleShot(0, this, [self] {
            if (self) {
                self->onScrolled();
            }
        });
    }
    return QWidget::eventFilter(watched, event);
}

void PdfView::keyPressEvent(QKeyEvent *event)
{
    // PdfPageCanvas doesn't handle key events, so Qt bubbles them up here
    // regardless of whether the canvas or the scroll area has focus. Just a
    // plain scroll nudge now — no page-turn/boundary logic, since scrolling
    // past a page's edge naturally continues into the next page's content.
    QScrollBar *vbar = m_scrollArea->verticalScrollBar();
    if (event->key() == Qt::Key_Down) {
        vbar->setValue(vbar->value() + kArrowKeyScrollStep);
        onScrolled(); // see eventFilter()'s wheel handling for why this can't just rely on valueChanged
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Up) {
        vbar->setValue(vbar->value() - kArrowKeyScrollStep);
        onScrolled();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
