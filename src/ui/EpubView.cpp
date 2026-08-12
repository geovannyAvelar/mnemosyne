#include "EpubView.h"

#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleDriveSync.h"
#endif
#include "app/HighlightStore.h"
#include "app/ProgressSyncLog.h"
#include "app/ReadingProgressStore.h"
#include "core/SearchUtil.h"
#include "ui/SyncPromptBar.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

QVector<QTextCursor> findAllOccurrences(QTextDocument *document, const QString &text)
{
    QVector<QTextCursor> occurrences;
    if (text.isEmpty()) {
        return occurrences;
    }
    QTextCursor cursor(document);
    while (true) {
        cursor = document->find(text, cursor);
        if (cursor.isNull()) {
            break;
        }
        occurrences.append(cursor);
    }
    return occurrences;
}

} // namespace

EpubView::EpubView(std::unique_ptr<EpubDocument> document, QString filePath, QWidget *parent)
    : QWidget(parent)
    , m_document(std::move(document))
    , m_filePath(std::move(filePath))
    , m_highlights(HighlightStore::highlightsFor(m_filePath))
{
    setupUi();
    restoreProgressAndCheckSync(); // sets m_currentChapter/m_fontZoomSteps before the first render, so there's no visible jump
    renderCurrentChapter();
    updateNavigationState();
}

QString EpubView::documentTitle() const
{
    return m_document ? m_document->title() : QString();
}

QVector<TocNode> EpubView::tableOfContents() const
{
    return m_document ? m_document->tableOfContents() : QVector<TocNode>();
}

void EpubView::goToTocNode(const TocNode &node)
{
    if (node.pageNumber >= 0) {
        goToChapter(node.pageNumber);
    }
}

int EpubView::currentPosition() const
{
    return m_currentChapter;
}

bool EpubView::hasPendingSyncPrompt() const
{
    // Not isVisible(): that also depends on the whole ancestor chain (this
    // view, its window) actually being shown on screen, which is true in
    // the real app but not e.g. in a test that constructs the view standalone.
    // isHidden() reflects just this widget's own explicit show()/hide() state.
    return !m_syncPromptBar->isHidden();
}

QVector<SearchResult> EpubView::search(const QString &query) const
{
    QVector<SearchResult> results;
    if (!m_document || query.trimmed().isEmpty()) {
        return results;
    }

    for (int i = 0; i < m_document->spineCount(); ++i) {
        QTextDocument doc;
        doc.setHtml(m_document->chapterHtml(i));
        const QString text = doc.toPlainText();
        if (text.contains(query, Qt::CaseInsensitive)) {
            SearchResult result;
            result.targetIndex = i;
            result.label = tr("Chapter %1").arg(i + 1);
            result.snippet = makeSearchSnippet(text, query);
            results.append(result);
        }
    }
    return results;
}

qreal EpubView::currentFontPointSize() const
{
    return m_browser->document()->defaultFont().pointSizeF();
}

void EpubView::setDarkMode(bool enabled)
{
    if (m_darkMode == enabled) {
        return;
    }
    m_darkMode = enabled;
    applyPageColors();
    renderCurrentChapter(); // re-inject/remove the dark-mode text-color override
}

void EpubView::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(6, 4, 6, 4);

    auto *prevButton = new QPushButton(tr("< Prev"), toolbar);
    auto *nextButton = new QPushButton(tr("Next >"), toolbar);
    connect(prevButton, &QPushButton::clicked, this, &EpubView::previousChapter);
    connect(nextButton, &QPushButton::clicked, this, &EpubView::nextChapter);

    m_chapterLabel = new QLabel(toolbar);

    auto *zoomOutButton = new QPushButton(tr("-"), toolbar);
    auto *zoomInButton = new QPushButton(tr("+"), toolbar);
    connect(zoomOutButton, &QPushButton::clicked, this, &EpubView::zoomOut);
    connect(zoomInButton, &QPushButton::clicked, this, &EpubView::zoomIn);

    toolbarLayout->addWidget(prevButton);
    toolbarLayout->addWidget(m_chapterLabel);
    toolbarLayout->addWidget(nextButton);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(zoomOutButton);
    toolbarLayout->addWidget(zoomInButton);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(false);
    m_browser->setOpenLinks(false);
    m_browser->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_browser, &QTextBrowser::customContextMenuRequested, this, &EpubView::showBrowserContextMenu);
    applyPageColors();
    m_browser->viewport()->installEventFilter(this); // scrolling past the top/bottom edge turns the chapter

    m_syncPromptBar = new SyncPromptBar(this);

    layout->addWidget(toolbar);
    layout->addWidget(m_syncPromptBar);
    layout->addWidget(m_browser, 1);

    m_progressSaveTimer = new QTimer(this);
    m_progressSaveTimer->setSingleShot(true);
    connect(m_progressSaveTimer, &QTimer::timeout, this, &EpubView::saveProgressNow);
}

void EpubView::applyPageColors()
{
    // Chapters render on a fixed "page" background regardless of the app's
    // system theme (matching PdfView's rendered page), except we do offer an
    // explicit dark-mode toggle for the page itself, applied here.
    if (m_darkMode) {
        m_browser->setStyleSheet(QStringLiteral("QTextBrowser { background-color: #1e1e1e; color: #ddd; }"));
    } else {
        m_browser->setStyleSheet(QStringLiteral("QTextBrowser { background-color: white; color: black; }"));
    }
}

void EpubView::goToChapter(int spineIndex)
{
    if (!m_document) {
        return;
    }
    spineIndex = std::clamp(spineIndex, 0, m_document->spineCount() - 1);
    if (spineIndex == m_currentChapter) {
        updateNavigationState();
        return;
    }
    m_currentChapter = spineIndex;
    renderCurrentChapter();
    updateNavigationState();
    scheduleProgressSave();
}

void EpubView::nextChapter()
{
    goToChapter(m_currentChapter + 1);
}

void EpubView::previousChapter()
{
    goToChapter(m_currentChapter - 1);
}

void EpubView::renderCurrentChapter()
{
    if (!m_document || m_document->spineCount() == 0) {
        return;
    }

    QString html = m_document->chapterHtml(m_currentChapter);
    if (m_darkMode) {
        // The book's own CSS (e.g. "body { color: #222 }") would otherwise
        // stay in force and become unreadable against a dark page. Since this
        // has the same selector specificity as that rule, it must come after
        // it in document order to win the cascade — appending to the end of
        // the string isn't enough, as Qt's HTML parser may not honor a
        // <style> block outside <head>. Elements with a *more specific* rule
        // (a styled heading, say) still keep the author's intended color.
        const QString override = QStringLiteral("<style>body,p,div,span{color:#ddd;}</style>");
        const int headEnd = html.indexOf(QStringLiteral("</head>"), 0, Qt::CaseInsensitive);
        if (headEnd >= 0) {
            html.insert(headEnd, override);
        } else {
            html.prepend(override);
        }
    }
    m_browser->setHtml(html);
    applyHighlightsToBrowser();
}

void EpubView::zoomIn()
{
    if (m_fontZoomSteps >= 20) {
        return;
    }
    setFontZoomSteps(m_fontZoomSteps + 1);
    scheduleProgressSave();
}

void EpubView::zoomOut()
{
    if (m_fontZoomSteps <= -5) {
        return;
    }
    setFontZoomSteps(m_fontZoomSteps - 1);
    scheduleProgressSave();
}

void EpubView::setFontZoomSteps(int steps)
{
    steps = std::clamp(steps, -5, 20);
    const int delta = steps - m_fontZoomSteps;
    if (delta > 0) {
        m_browser->zoomIn(delta);
    } else if (delta < 0) {
        m_browser->zoomOut(-delta);
    }
    m_fontZoomSteps = steps;
}

void EpubView::updateNavigationState()
{
    if (!m_document) {
        return;
    }
    m_chapterLabel->setText(tr("Chapter %1 of %2").arg(m_currentChapter + 1).arg(m_document->spineCount()));
}

void EpubView::applyHighlightsToBrowser()
{
    QList<QTextEdit::ExtraSelection> selections;
    QTextCharFormat format;
    format.setBackground(QColor(255, 235, 59, 140));

    for (const Highlight &highlight : m_highlights) {
        if (highlight.targetIndex != m_currentChapter) {
            continue;
        }
        for (const QTextCursor &occurrence : findAllOccurrences(m_browser->document(), highlight.text)) {
            QTextEdit::ExtraSelection selection;
            selection.cursor = occurrence;
            selection.format = format;
            selections.append(selection);
        }
    }

    m_browser->setExtraSelections(selections);
}

void EpubView::addHighlightForSelection()
{
    const QTextCursor cursor = m_browser->textCursor();
    if (!cursor.hasSelection()) {
        return;
    }

    Highlight highlight;
    highlight.targetIndex = m_currentChapter;
    highlight.text = cursor.selectedText();
    highlight.createdAt = QDateTime::currentDateTime();

    HighlightStore::addHighlight(m_filePath, highlight);
    m_highlights = HighlightStore::highlightsFor(m_filePath);
    applyHighlightsToBrowser();
}

void EpubView::showBrowserContextMenu(const QPoint &pos)
{
    const QTextCursor selectionCursor = m_browser->textCursor();
    const bool hasSelection = selectionCursor.hasSelection();

    QMenu menu(m_browser);

    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(hasSelection);
    connect(copyAction, &QAction::triggered, m_browser, &QTextBrowser::copy);

    QAction *highlightAction = menu.addAction(tr("Highlight"));
    highlightAction->setEnabled(hasSelection);
    connect(highlightAction, &QAction::triggered, this, &EpubView::addHighlightForSelection);

    if (!hasSelection) {
        const int clickPosition = m_browser->cursorForPosition(pos).position();
        for (int i = 0; i < m_highlights.size(); ++i) {
            if (m_highlights[i].targetIndex != m_currentChapter) {
                continue;
            }
            const QVector<QTextCursor> occurrences = findAllOccurrences(m_browser->document(), m_highlights[i].text);
            const bool clickedInside = std::any_of(occurrences.begin(), occurrences.end(), [clickPosition](const QTextCursor &c) {
                return clickPosition >= c.selectionStart() && clickPosition < c.selectionEnd();
            });
            if (clickedInside) {
                menu.addSeparator();
                QAction *removeAction = menu.addAction(tr("Remove Highlight"));
                connect(removeAction, &QAction::triggered, this, [this, i] {
                    HighlightStore::removeHighlight(m_filePath, i);
                    m_highlights = HighlightStore::highlightsFor(m_filePath);
                    applyHighlightsToBrowser();
                });
                break;
            }
        }
    }

    menu.exec(m_browser->mapToGlobal(pos));
}

void EpubView::restoreProgressAndCheckSync()
{
    m_bookHash = FileIdentity::contentHash(m_filePath);
    if (m_bookHash.isEmpty() || !m_document) {
        return;
    }

    if (const auto local = ReadingProgressStore::get(m_bookHash)) {
        m_currentChapter = std::clamp(local->position, 0, std::max(0, m_document->spineCount() - 1));
        setFontZoomSteps(static_cast<int>(std::lround(local->zoom)));
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
}

void EpubView::offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote)
{
    if (remote.position == m_currentChapter) {
        return;
    }
    m_syncPromptBar->showPrompt(tr("Synced position available: chapter %1 (from %2) — jump?")
                                     .arg(remote.position + 1)
                                     .arg(remote.deviceName));
    const int remotePosition = remote.position;
    const qreal remoteZoom = remote.zoom;
    connect(m_syncPromptBar, &SyncPromptBar::jumpRequested, this, [this, remotePosition, remoteZoom] {
        setFontZoomSteps(static_cast<int>(std::lround(remoteZoom)));
        goToChapter(remotePosition);
    });
}

void EpubView::scheduleProgressSave()
{
    m_progressSaveTimer->start(1500);
}

void EpubView::saveProgressNow()
{
    if (m_bookHash.isEmpty()) {
        return;
    }
    ReadingProgressStore::set(m_bookHash, m_currentChapter, m_fontZoomSteps);
    ProgressSyncLog::appendEntry(m_bookHash, documentTitle(), m_currentChapter, m_fontZoomSteps);
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    GoogleDriveSync::appendEntry(m_bookHash, documentTitle(), m_currentChapter, m_fontZoomSteps);
#endif
}

bool EpubView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_browser->viewport() && event->type() == QEvent::Wheel && !m_pageTurnCooldown) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        QScrollBar *vbar = m_browser->verticalScrollBar();
        const int deltaY = wheelEvent->angleDelta().y();

        // See PdfView::eventFilter for the sign convention and the
        // min==max==0 (whole chapter fits in view) reasoning.
        const bool scrollingDownPastBottom = deltaY < 0 && vbar->value() >= vbar->maximum();
        const bool scrollingUpPastTop = deltaY > 0 && vbar->value() <= vbar->minimum();

        if (scrollingDownPastBottom && m_document && m_currentChapter < m_document->spineCount() - 1) {
            nextChapter();
            m_browser->verticalScrollBar()->setValue(0); // resume at the top of the new chapter
            m_pageTurnCooldown = true;
            QTimer::singleShot(400, this, [this] { m_pageTurnCooldown = false; });
            return true;
        }
        if (scrollingUpPastTop && m_currentChapter > 0) {
            previousChapter();
            QPointer<QTextBrowser> browser = m_browser;
            QTimer::singleShot(0, this, [browser] {
                if (browser) {
                    browser->verticalScrollBar()->setValue(browser->verticalScrollBar()->maximum());
                }
            });
            m_pageTurnCooldown = true;
            QTimer::singleShot(400, this, [this] { m_pageTurnCooldown = false; });
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
