#include "PdfView.h"

#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#include "app/HighlightStore.h"
#include "app/ProgressSyncLog.h"
#include "app/ReadingProgressStore.h"
#include "core/CoordinateUtil.h"
#include "core/SearchUtil.h"
#include "ui/PdfPageCanvas.h"
#include "ui/SyncPromptBar.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
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

#include <algorithm>

namespace {
constexpr qreal kMinZoom = 0.25;
constexpr qreal kMaxZoom = 4.0;
constexpr qreal kZoomStep = 0.25;
}

PdfView::PdfView(std::unique_ptr<IDocument> document, QString filePath, QWidget *parent)
    : QWidget(parent)
    , m_document(std::move(document))
    , m_filePath(std::move(filePath))
    , m_highlights(HighlightStore::highlightsFor(m_filePath))
{
    setupUi();
    restoreProgressAndCheckSync(); // sets m_currentPage/m_zoom before the first render, so there's no visible jump
    updateNavigationState();
    renderCurrentPage();
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
    QVector<SearchResult> results;
    if (!m_document || query.trimmed().isEmpty()) {
        return results;
    }

    for (int i = 0; i < m_document->pageCount(); ++i) {
        std::unique_ptr<IPage> page = m_document->page(i);
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

    m_canvas = new PdfPageCanvas(this);
    connect(m_canvas, &PdfPageCanvas::selectionChanged, this, &PdfView::updateSelectedText);
    m_canvas->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_canvas, &PdfPageCanvas::customContextMenuRequested, this, &PdfView::showCanvasContextMenu);

    auto *copyShortcut = new QShortcut(QKeySequence::Copy, m_canvas);
    connect(copyShortcut, &QShortcut::activated, this, &PdfView::copySelection);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_canvas);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->viewport()->installEventFilter(this); // scrolling past the top/bottom edge turns the page

    m_syncPromptBar = new SyncPromptBar(this);

    layout->addWidget(toolbar);
    layout->addWidget(m_syncPromptBar);
    layout->addWidget(m_scrollArea, 1);

    m_progressSaveTimer = new QTimer(this);
    m_progressSaveTimer->setSingleShot(true);
    connect(m_progressSaveTimer, &QTimer::timeout, this, &PdfView::saveProgressNow);
}

void PdfView::goToPage(int index)
{
    if (!m_document) {
        return;
    }
    index = std::clamp(index, 0, m_document->pageCount() - 1);
    if (index == m_currentPage) {
        updateNavigationState();
        return;
    }
    m_currentPage = index;
    renderCurrentPage();
    updateNavigationState();
    scheduleProgressSave();
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
    m_zoom = std::min(kMaxZoom, m_zoom + kZoomStep);
    renderCurrentPage();
    scheduleProgressSave();
}

void PdfView::zoomOut()
{
    m_zoom = std::max(kMinZoom, m_zoom - kZoomStep);
    renderCurrentPage();
    scheduleProgressSave();
}

void PdfView::copySelection()
{
    if (!m_selectedText.isEmpty()) {
        QApplication::clipboard()->setText(m_selectedText);
    }
}

void PdfView::renderCurrentPage()
{
    if (!m_document || m_document->pageCount() == 0) {
        return;
    }

    std::unique_ptr<IPage> page = m_document->page(m_currentPage);
    if (!page) {
        return;
    }

    const QImage image = page->renderToImage(m_zoom);
    m_canvas->setPage(image, m_zoom); // also clears any stale selection from the previous page
    m_selectedText.clear();
    refreshHighlightOverlay();
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

void PdfView::updateSelectedText()
{
    m_selectedText.clear();

    if (!m_document || !m_canvas->hasSelection()) {
        return;
    }

    std::unique_ptr<IPage> page = m_document->page(m_currentPage);
    if (!page) {
        return;
    }

    const QRectF pageRect = pixelRectToPageRect(m_canvas->selectionPixelRect(), m_canvas->scale());
    m_selectedText = page->text(pageRect);
}

void PdfView::refreshHighlightOverlay()
{
    QVector<QRect> pixelRects;
    for (const Highlight &highlight : m_highlights) {
        if (highlight.targetIndex == m_currentPage) {
            pixelRects.append(pageRectToPixelRect(highlight.pageRect, m_zoom));
        }
    }
    m_canvas->setHighlightRects(pixelRects);
}

int PdfView::highlightIndexAtPagePoint(const QPointF &pagePoint) const
{
    for (int i = 0; i < m_highlights.size(); ++i) {
        if (m_highlights[i].targetIndex == m_currentPage && m_highlights[i].pageRect.contains(pagePoint)) {
            return i;
        }
    }
    return -1;
}

void PdfView::addHighlightForSelection()
{
    if (!m_canvas->hasSelection() || m_selectedText.isEmpty()) {
        return;
    }

    Highlight highlight;
    highlight.targetIndex = m_currentPage;
    highlight.pageRect = pixelRectToPageRect(m_canvas->selectionPixelRect(), m_canvas->scale());
    highlight.text = m_selectedText;
    highlight.createdAt = QDateTime::currentDateTime();

    HighlightStore::addHighlight(m_filePath, highlight);
    m_highlights = HighlightStore::highlightsFor(m_filePath);

    m_canvas->clearSelection();
    m_selectedText.clear();
    refreshHighlightOverlay();
}

void PdfView::showCanvasContextMenu(const QPoint &pos)
{
    QMenu menu(m_canvas);

    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(!m_selectedText.isEmpty());
    connect(copyAction, &QAction::triggered, this, &PdfView::copySelection);

    QAction *highlightAction = menu.addAction(tr("Highlight"));
    highlightAction->setEnabled(!m_selectedText.isEmpty());
    connect(highlightAction, &QAction::triggered, this, &PdfView::addHighlightForSelection);

    const QPointF pagePoint(pos.x() / m_canvas->scale(), pos.y() / m_canvas->scale());
    const int existingHighlightIndex = highlightIndexAtPagePoint(pagePoint);
    if (existingHighlightIndex >= 0) {
        menu.addSeparator();
        QAction *removeAction = menu.addAction(tr("Remove Highlight"));
        connect(removeAction, &QAction::triggered, this, [this, existingHighlightIndex] {
            HighlightStore::removeHighlight(m_filePath, existingHighlightIndex);
            m_highlights = HighlightStore::highlightsFor(m_filePath);
            refreshHighlightOverlay();
        });
    }

    menu.exec(m_canvas->mapToGlobal(pos));
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

    if (const auto remote = ProgressSyncLog::latestFromOtherDevices(m_bookHash, DeviceIdentity::id())) {
        if (remote->position != m_currentPage) {
            m_syncPromptBar->showPrompt(tr("Synced position available: page %1 (from %2) — jump?")
                                             .arg(remote->position + 1)
                                             .arg(remote->deviceName));
            const int remotePosition = remote->position;
            const qreal remoteZoom = remote->zoom;
            connect(m_syncPromptBar, &SyncPromptBar::jumpRequested, this, [this, remotePosition, remoteZoom] {
                m_zoom = std::clamp(remoteZoom, kMinZoom, kMaxZoom);
                goToPage(remotePosition);
            });
        }
    }
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
}

bool PdfView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_scrollArea->viewport() && event->type() == QEvent::Wheel && !m_pageTurnCooldown) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        QScrollBar *vbar = m_scrollArea->verticalScrollBar();
        const int deltaY = wheelEvent->angleDelta().y();

        // Qt convention: positive angleDelta().y() == scrolling up (toward
        // smaller scrollbar values); negative == scrolling down. When the
        // whole page already fits in the viewport, min == max == 0, so
        // either direction's boundary check is satisfied immediately —
        // exactly the desired "any scroll turns the page" behavior there.
        const bool scrollingDownPastBottom = deltaY < 0 && vbar->value() >= vbar->maximum();
        const bool scrollingUpPastTop = deltaY > 0 && vbar->value() <= vbar->minimum();

        if (scrollingDownPastBottom && m_document && m_currentPage < m_document->pageCount() - 1) {
            nextPage();
            m_scrollArea->verticalScrollBar()->setValue(0); // resume at the top of the new page
            m_pageTurnCooldown = true;
            QTimer::singleShot(400, this, [this] { m_pageTurnCooldown = false; });
            return true;
        }
        if (scrollingUpPastTop && m_currentPage > 0) {
            previousPage();
            // The new page's scroll range isn't known until layout catches
            // up with the resize triggered by previousPage(); defer.
            QPointer<QScrollArea> scrollArea = m_scrollArea;
            QTimer::singleShot(0, this, [scrollArea] {
                if (scrollArea) {
                    scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());
                }
            });
            m_pageTurnCooldown = true;
            QTimer::singleShot(400, this, [this] { m_pageTurnCooldown = false; });
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
