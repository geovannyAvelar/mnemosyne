#include "HtmlView.h"

#include "app/FileIdentity.h"
#include "app/ProgressSyncLog.h"
#include "app/ReadingProgressStore.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineView>

#include <algorithm>

namespace {
constexpr qreal kMinZoom = 0.25;
constexpr qreal kMaxZoom = 5.0; // QWebEngineView's own documented upper bound
constexpr qreal kZoomStep = 0.25;
}

HtmlView::HtmlView(const QString &filePath, QWidget *parent)
    : QWidget(parent)
    , m_filePath(filePath)
    , m_fallbackTitle(QFileInfo(filePath).completeBaseName())
    , m_bookHash(FileIdentity::contentHash(filePath))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(6, 4, 6, 4);

    m_titleLabel = new QLabel(m_fallbackTitle, toolbar);

    auto *zoomOutButton = new QPushButton(tr("-"), toolbar);
    auto *zoomInButton = new QPushButton(tr("+"), toolbar);
    connect(zoomOutButton, &QPushButton::clicked, this, &HtmlView::zoomOut);
    connect(zoomInButton, &QPushButton::clicked, this, &HtmlView::zoomIn);

    toolbarLayout->addWidget(m_titleLabel);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(zoomOutButton);
    toolbarLayout->addWidget(zoomInButton);

    m_webView = new QWebEngineView(this);
    connect(m_webView, &QWebEngineView::titleChanged, m_titleLabel, [this](const QString &title) {
        m_titleLabel->setText(title.isEmpty() ? m_fallbackTitle : title);
    });

    layout->addWidget(toolbar);
    layout->addWidget(m_webView, 1);

    m_progressSaveTimer = new QTimer(this);
    m_progressSaveTimer->setSingleShot(true);
    connect(m_progressSaveTimer, &QTimer::timeout, this, &HtmlView::saveProgressNow);

    restoreZoom(); // local resume only; there's no "position" for a single page to sync/jump between devices
    m_webView->load(QUrl::fromLocalFile(filePath));
}

QString HtmlView::documentTitle() const
{
    const QString liveTitle = m_webView->title();
    return liveTitle.isEmpty() ? m_fallbackTitle : liveTitle;
}

QVector<TocNode> HtmlView::tableOfContents() const
{
    return {};
}

void HtmlView::goToTocNode(const TocNode &)
{
}

int HtmlView::currentPosition() const
{
    return 0;
}

QVector<SearchResult> HtmlView::search(const QString &) const
{
    return {};
}

qreal HtmlView::currentZoomFactor() const
{
    return m_webView->zoomFactor();
}

void HtmlView::zoomIn()
{
    m_webView->setZoomFactor(std::min(kMaxZoom, m_webView->zoomFactor() + kZoomStep));
    scheduleProgressSave();
}

void HtmlView::zoomOut()
{
    m_webView->setZoomFactor(std::max(kMinZoom, m_webView->zoomFactor() - kZoomStep));
    scheduleProgressSave();
}

void HtmlView::restoreZoom()
{
    if (m_bookHash.isEmpty()) {
        return;
    }
    if (const auto local = ReadingProgressStore::get(m_bookHash)) {
        m_webView->setZoomFactor(std::clamp(local->zoom, kMinZoom, kMaxZoom));
    }
}

void HtmlView::scheduleProgressSave()
{
    m_progressSaveTimer->start(1500);
}

void HtmlView::saveProgressNow()
{
    if (m_bookHash.isEmpty()) {
        return;
    }
    const qreal zoom = m_webView->zoomFactor();
    ReadingProgressStore::set(m_bookHash, 0, zoom);
    ProgressSyncLog::appendEntry(m_bookHash, documentTitle(), 0, zoom);
}
