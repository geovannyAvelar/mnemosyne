#include "ComicView.h"

#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleDriveSync.h"
#endif
#include "app/ProgressSyncLog.h"
#include "app/ReadingProgressStore.h"
#include "ui/PdfPageCanvas.h"
#include "ui/SyncPromptBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
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
} // namespace

ComicView::ComicView(std::unique_ptr<CbzDocument> document, QString filePath, QWidget *parent)
    : QWidget(parent)
    , m_document(std::move(document))
    , m_filePath(std::move(filePath))
{
    setupUi();
    restoreProgressAndCheckSync(); // sets m_currentPage/m_zoom before the first render, so there's no visible jump
    renderCurrentPage();
    updateNavigationState();
}

QString ComicView::documentTitle() const
{
    return m_document ? m_document->title() : QString();
}

void ComicView::goToTocNode(const TocNode &node)
{
    if (node.pageNumber >= 0) {
        goToPage(node.pageNumber);
    }
}

int ComicView::currentPosition() const
{
    return m_currentPage;
}

bool ComicView::hasPendingSyncPrompt() const
{
    return !m_syncPromptBar->isHidden();
}

void ComicView::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(6, 4, 6, 4);

    auto *prevButton = new QPushButton(tr("< Prev"), toolbar);
    auto *nextButton = new QPushButton(tr("Next >"), toolbar);
    connect(prevButton, &QPushButton::clicked, this, &ComicView::previousPage);
    connect(nextButton, &QPushButton::clicked, this, &ComicView::nextPage);

    m_pageSpinBox = new QSpinBox(toolbar);
    m_pageSpinBox->setMinimum(1);
    m_pageSpinBox->setMaximum(std::max(1, m_document ? m_document->pageCount() : 1));
    connect(m_pageSpinBox, &QSpinBox::editingFinished, this, [this] {
        goToPage(m_pageSpinBox->value() - 1);
    });

    m_pageCountLabel = new QLabel(toolbar);

    auto *zoomOutButton = new QPushButton(tr("-"), toolbar);
    auto *zoomInButton = new QPushButton(tr("+"), toolbar);
    connect(zoomOutButton, &QPushButton::clicked, this, &ComicView::zoomOut);
    connect(zoomInButton, &QPushButton::clicked, this, &ComicView::zoomIn);

    toolbarLayout->addWidget(prevButton);
    toolbarLayout->addWidget(m_pageSpinBox);
    toolbarLayout->addWidget(m_pageCountLabel);
    toolbarLayout->addWidget(nextButton);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(zoomOutButton);
    toolbarLayout->addWidget(zoomInButton);

    m_canvas = new PdfPageCanvas(this);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_canvas);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->viewport()->installEventFilter(this); // scrolling past the top/bottom edge turns the page; Ctrl+wheel zooms

    m_syncPromptBar = new SyncPromptBar(this);

    layout->addWidget(toolbar);
    layout->addWidget(m_syncPromptBar);
    layout->addWidget(m_scrollArea, 1);

    m_progressSaveTimer = new QTimer(this);
    m_progressSaveTimer->setSingleShot(true);
    connect(m_progressSaveTimer, &QTimer::timeout, this, &ComicView::saveProgressNow);
}

void ComicView::goToPage(int index)
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

void ComicView::nextPage()
{
    goToPage(m_currentPage + 1);
}

void ComicView::previousPage()
{
    goToPage(m_currentPage - 1);
}

void ComicView::zoomIn()
{
    m_zoom = std::min(kMaxZoom, m_zoom + kZoomStep);
    renderCurrentPage();
    scheduleProgressSave();
}

void ComicView::zoomOut()
{
    m_zoom = std::max(kMinZoom, m_zoom - kZoomStep);
    renderCurrentPage();
    scheduleProgressSave();
}

void ComicView::renderCurrentPage()
{
    if (!m_document || m_document->pageCount() == 0) {
        return;
    }

    std::unique_ptr<IPage> page = m_document->page(m_currentPage);
    if (!page) {
        return;
    }

    const QImage image = page->renderToImage(m_zoom);
    m_canvas->setPage(image, m_zoom);
}

void ComicView::updateNavigationState()
{
    if (!m_document) {
        return;
    }
    const QSignalBlocker blocker(m_pageSpinBox);
    m_pageSpinBox->setMaximum(std::max(1, m_document->pageCount()));
    m_pageSpinBox->setValue(m_currentPage + 1);
    m_pageCountLabel->setText(tr("of %1").arg(m_document->pageCount()));
}

void ComicView::restoreProgressAndCheckSync()
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
    GoogleDriveSync::latestFromOtherDevices(
        bookHash, DeviceIdentity::id(),
        [this, bookHash, localFolderRemote](std::optional<ProgressSyncLog::RemoteEntry> googleRemote) {
            if (!googleRemote || bookHash != m_bookHash) {
                return;
            }
            if (!ProgressSyncLog::isGoogleDriveNewer(*googleRemote, localFolderRemote)) {
                return;
            }
            offerSyncedPosition(*googleRemote);
        });
#endif
}

void ComicView::offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote)
{
    if (remote.position == m_currentPage) {
        return;
    }
    m_syncPromptBar->showPrompt(
        tr("Synced position available: page %1 (from %2) — jump?").arg(remote.position + 1).arg(remote.deviceName));
    const int remotePosition = remote.position;
    const qreal remoteZoom = remote.zoom;
    connect(m_syncPromptBar, &SyncPromptBar::jumpRequested, this, [this, remotePosition, remoteZoom] {
        m_zoom = std::clamp(remoteZoom, kMinZoom, kMaxZoom);
        goToPage(remotePosition);
    });
}

void ComicView::scheduleProgressSave()
{
    m_progressSaveTimer->start(1500);
}

void ComicView::flushProgress()
{
    if (!m_progressSaveTimer->isActive()) {
        return;
    }
    m_progressSaveTimer->stop();
    saveProgressNow();
}

void ComicView::saveProgressNow()
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

bool ComicView::eventFilter(QObject *watched, QEvent *event)
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
    }

    if (watched == m_scrollArea->viewport() && event->type() == QEvent::Wheel && !m_pageTurnCooldown) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        QScrollBar *vbar = m_scrollArea->verticalScrollBar();
        const int deltaY = wheelEvent->angleDelta().y();

        // See PdfView::eventFilter for the sign convention and the
        // min==max==0 (whole page fits in view) reasoning.
        const bool scrollingDownPastBottom = deltaY < 0 && vbar->value() >= vbar->maximum();
        const bool scrollingUpPastTop = deltaY > 0 && vbar->value() <= vbar->minimum();

        if (scrollingDownPastBottom && m_document && m_currentPage < m_document->pageCount() - 1) {
            nextPage();
            m_scrollArea->verticalScrollBar()->setValue(0);
            m_pageTurnCooldown = true;
            QTimer::singleShot(400, this, [this] { m_pageTurnCooldown = false; });
            return true;
        }
        if (scrollingUpPastTop && m_currentPage > 0) {
            previousPage();
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
