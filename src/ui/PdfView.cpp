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
#include "core/SearchUtil.h"
#include "pdf/PopplerPdfDocument.h"
#include "ui/NoteDialog.h"
#include "ui/PdfPageStackView.h"
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
#include <QKeyEvent>

#include <algorithm>

namespace {
constexpr qreal kMinZoom = 0.25;
constexpr qreal kMaxZoom = 4.0;
constexpr qreal kZoomStep = 0.25;
constexpr int kArrowKeyScrollStep = 60; // px per arrow key press
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

    m_pageStackView->setDocument(m_document.get());
    m_pageStackView->setZoom(m_zoom);
    m_pageStackView->setHighlights(m_highlights);
    updateNavigationState();
    m_pageStackView->setCurrentPageHint(m_currentPage);

    // Unlike the old per-canvas QVBoxLayout (whose layout, and thus each
    // canvas's y(), wasn't realized until the widget was actually shown),
    // PdfPageStackView's offsets are plain analytic math -- valid the
    // instant setZoom() above returns, so the initial scroll-to-saved-page
    // can happen synchronously here instead of deferred a tick.
    m_scrollArea->verticalScrollBar()->setValue(int(m_pageStackView->pageOffsetY(m_currentPage)));
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

QString PdfView::selectedText() const
{
    return m_pageStackView->selectedText();
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
    m_pageStackView->setSearchTerm(m_searchTerm);
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

    m_pageStackView = new PdfPageStackView(this);
    connect(m_pageStackView, &PdfPageStackView::contextMenuRequested, this, &PdfView::showCanvasContextMenu);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_pageStackView);
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

    // One shortcut for the whole view. PdfPageStackView has no per-page
    // widgets to duplicate this across; the old per-canvas QShortcut
    // pattern registered up to one identical Ctrl+C per document page,
    // which Qt's shortcut map treats as ambiguous past a couple of pages
    // (a canvas scrolled out of the viewport was still isVisible() == true
    // under the default Qt::WindowShortcut context).
    auto *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, &PdfView::copySelection);
}

void PdfView::goToPage(int index)
{
    if (!m_document || m_pageStackView->pageCount() == 0) {
        return;
    }
    index = std::clamp(index, 0, m_pageStackView->pageCount() - 1);
    m_currentPage = index;
    m_pageStackView->setCurrentPageHint(m_currentPage);
    updateNavigationState();
    scheduleProgressSave();

    m_scrollArea->verticalScrollBar()->setValue(int(m_pageStackView->pageOffsetY(index)));
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
    if (m_pageStackView->pageCount() == 0 || qFuzzyCompare(newZoom, m_zoom)) {
        return;
    }

    // Preserve the reader's visual position: capture where the viewport top
    // sits within m_currentPage, as a fraction of its (old) height, so the
    // same spot stays under the viewport top once every page resizes.
    const qreal oldTop = m_pageStackView->pageOffsetY(m_currentPage);
    const qreal oldHeight = m_pageStackView->pageHeightPx(m_currentPage);
    const qreal offsetFraction =
        oldHeight > 0 ? (m_scrollArea->verticalScrollBar()->value() - oldTop) / oldHeight : 0.0;

    m_zoom = newZoom;
    m_pageStackView->setZoom(m_zoom);
    m_pageStackView->setCurrentPageHint(m_currentPage);

    // No QTimer::singleShot needed here (unlike the old per-canvas layout,
    // which needed a tick to settle) -- pageOffsetY/pageHeightPx already
    // reflect the new zoom synchronously.
    const qreal newTop = m_pageStackView->pageOffsetY(m_currentPage);
    const qreal newHeight = m_pageStackView->pageHeightPx(m_currentPage);
    m_scrollArea->verticalScrollBar()->setValue(int(newTop + offsetFraction * newHeight));

    scheduleProgressSave();
}

void PdfView::copySelection()
{
    const QString text = m_pageStackView->selectedText();
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
    }
}

int PdfView::topmostVisiblePage() const
{
    if (m_pageStackView->pageCount() == 0) {
        return 0;
    }
    const int viewportTop = m_scrollArea->verticalScrollBar()->value();
    const int viewportHeight = m_scrollArea->viewport()->height();
    return m_pageStackView->pageIndexAtOffsetY(viewportTop + viewportHeight / 2);
}

void PdfView::onScrolled()
{
    const int page = topmostVisiblePage();
    if (page != m_currentPage) {
        m_currentPage = page;
        updateNavigationState();
        scheduleProgressSave();
    }
    m_pageStackView->setCurrentPageHint(m_currentPage);
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
    m_pageStackView->setHighlights(m_highlights);
}

void PdfView::addHighlightForSelection()
{
    const int pageIndex = m_pageStackView->selectedPageIndex();
    const QString text = m_pageStackView->selectedText();
    if (pageIndex < 0 || text.isEmpty() || !m_pageStackView->hasSelection()) {
        return;
    }

    Highlight highlight;
    highlight.targetIndex = pageIndex;
    highlight.pageRect = m_pageStackView->selectedBoundingPageRect();
    highlight.text = text;
    highlight.createdAt = QDateTime::currentDateTime();

    HighlightStore::addHighlight(m_bookHash, highlight);
    m_highlights = HighlightStore::highlightsFor(m_bookHash);

    m_pageStackView->clearSelection();
    m_pageStackView->setHighlights(m_highlights);
    emit highlightsChanged();
}

void PdfView::addNoteForSelection()
{
    const int pageIndex = m_pageStackView->selectedPageIndex();
    const QString text = m_pageStackView->selectedText();
    if (pageIndex < 0 || text.isEmpty() || !m_pageStackView->hasSelection()) {
        return;
    }

    const std::optional<NoteDialog::Result> result = NoteDialog::show(this, QString(), kDefaultHighlightColor);
    if (!result || result->note.isEmpty()) {
        return;
    }

    Highlight highlight;
    highlight.targetIndex = pageIndex;
    highlight.pageRect = m_pageStackView->selectedBoundingPageRect();
    highlight.text = text;
    highlight.createdAt = QDateTime::currentDateTime();
    highlight.note = result->note;
    highlight.color = result->color;

    HighlightStore::addHighlight(m_bookHash, highlight);
    m_highlights = HighlightStore::highlightsFor(m_bookHash);

    m_pageStackView->clearSelection();
    m_pageStackView->setHighlights(m_highlights);
    emit highlightsChanged();
}

void PdfView::showCanvasContextMenu(const QPoint &globalPos, int pageIndex, const QPointF &pagePoint)
{
    const bool hasSelectionHere =
        pageIndex == m_pageStackView->selectedPageIndex() && m_pageStackView->hasSelection();

    QMenu menu(this);

    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(hasSelectionHere);
    connect(copyAction, &QAction::triggered, this, &PdfView::copySelection);

    QAction *highlightAction = menu.addAction(tr("Highlight"));
    highlightAction->setEnabled(hasSelectionHere);
    connect(highlightAction, &QAction::triggered, this, &PdfView::addHighlightForSelection);

    QAction *addNoteAction = menu.addAction(tr("Add Note..."));
    addNoteAction->setEnabled(hasSelectionHere);
    connect(addNoteAction, &QAction::triggered, this, &PdfView::addNoteForSelection);

    const int existingHighlightIndex = highlightIndexAtPagePoint(pagePoint, pageIndex);
    if (existingHighlightIndex >= 0) {
        menu.addSeparator();
        const bool hasNote = !m_highlights[existingHighlightIndex].note.isEmpty();
        QAction *noteAction = menu.addAction(hasNote ? tr("Edit Note...") : tr("Add Note..."));
        connect(noteAction, &QAction::triggered, this, [this, existingHighlightIndex] {
            const std::optional<NoteDialog::Result> result = NoteDialog::show(
                this, m_highlights[existingHighlightIndex].note, m_highlights[existingHighlightIndex].color);
            if (!result) {
                return;
            }
            HighlightStore::setNote(m_bookHash, existingHighlightIndex, result->note);
            HighlightStore::setColor(m_bookHash, existingHighlightIndex, result->color);
            m_highlights = HighlightStore::highlightsFor(m_bookHash);
            m_pageStackView->setHighlights(m_highlights);
            emit highlightsChanged();
        });
        QAction *removeAction = menu.addAction(hasNote ? tr("Remove Note") : tr("Remove Highlight"));
        connect(removeAction, &QAction::triggered, this, [this, existingHighlightIndex] {
            HighlightStore::removeHighlight(m_bookHash, existingHighlightIndex);
            m_highlights = HighlightStore::highlightsFor(m_bookHash);
            m_pageStackView->setHighlights(m_highlights);
            emit highlightsChanged();
        });
    }

    menu.exec(globalPos);
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

void PdfView::flushProgress()
{
    if (!m_progressSaveTimer->isActive()) {
        return;
    }
    m_progressSaveTimer->stop();
    saveProgressNow();
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
    // PdfPageStackView doesn't handle key events, so Qt bubbles them up here
    // regardless of whether it or the scroll area has focus. Just a plain
    // scroll nudge now — no page-turn/boundary logic, since scrolling past a
    // page's edge naturally continues into the next page's content.
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
