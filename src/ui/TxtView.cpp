#include "TxtView.h"

#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleDriveSync.h"
#endif
#include "app/HighlightStore.h"
#include "app/ProgressSyncLog.h"
#include "app/ReadingProgressStore.h"
#include "ui/SyncPromptBar.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
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

TxtView::TxtView(std::unique_ptr<TxtDocument> document, QString filePath, QWidget *parent)
    : QWidget(parent)
    , m_document(std::move(document))
    , m_filePath(std::move(filePath))
    , m_highlights(HighlightStore::highlightsFor(m_filePath))
{
    setupUi();
    m_browser->setPlainText(m_document ? m_document->text() : QString());
    applyHighlightsToBrowser();
    restoreProgressAndCheckSync(); // needs the browser's content already set, to resolve a character offset
}

QString TxtView::documentTitle() const
{
    return m_document ? m_document->title() : QString();
}

void TxtView::goToTocNode(const TocNode &node)
{
    goToCharacterOffset(node.pageNumber);
}

int TxtView::currentPosition() const
{
    if (!m_browser) {
        return 0;
    }
    return m_browser->cursorForPosition(QPoint(1, 1)).position();
}

bool TxtView::hasPendingSyncPrompt() const
{
    return !m_syncPromptBar->isHidden();
}

QVector<SearchResult> TxtView::search(const QString &query) const
{
    return searchFile(m_filePath, query);
}

QVector<SearchResult> TxtView::searchFile(const QString &filePath, const QString &query)
{
    QVector<SearchResult> results;
    if (query.trimmed().isEmpty()) {
        return results;
    }

    QString error;
    const std::unique_ptr<TxtDocument> document = TxtDocument::load(filePath, &error);
    if (!document) {
        return results;
    }

    const QString text = document->text();
    // Groups nearby matches into a single result (like other formats'
    // "one result per chapter/section") instead of one per raw occurrence,
    // which for a common word in a large file could otherwise mean
    // hundreds of near-duplicate results a few characters apart.
    constexpr int kResultSpacingChars = 2000;
    constexpr int kContextChars = 40;

    int searchFrom = 0;
    int nextResultAllowedFrom = 0;
    while (true) {
        const int matchIndex = text.indexOf(query, searchFrom, Qt::CaseInsensitive);
        if (matchIndex < 0) {
            break;
        }
        searchFrom = matchIndex + query.size();
        if (matchIndex < nextResultAllowedFrom) {
            continue;
        }
        nextResultAllowedFrom = matchIndex + kResultSpacingChars;

        SearchResult result;
        result.targetIndex = matchIndex;
        result.label = tr("Line %1").arg(text.left(matchIndex).count(QLatin1Char('\n')) + 1);

        const int start = std::max(0, matchIndex - kContextChars);
        const int end = std::min(text.size(), matchIndex + query.size() + kContextChars);
        QString snippet = text.mid(start, end - start).simplified();
        if (start > 0) {
            snippet.prepend(QStringLiteral("…"));
        }
        if (end < text.size()) {
            snippet.append(QStringLiteral("…"));
        }
        result.snippet = snippet;

        results.append(result);
    }
    return results;
}

void TxtView::setSearchTerm(const QString &term)
{
    m_searchTerm = term.trimmed();
    applyHighlightsToBrowser();
}

void TxtView::setDarkMode(bool enabled)
{
    if (m_darkMode == enabled) {
        return;
    }
    m_darkMode = enabled;
    applyPageColors();
}

void TxtView::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(6, 4, 6, 4);

    auto *zoomOutButton = new QPushButton(tr("-"), toolbar);
    auto *zoomInButton = new QPushButton(tr("+"), toolbar);
    connect(zoomOutButton, &QPushButton::clicked, this, &TxtView::zoomOut);
    connect(zoomInButton, &QPushButton::clicked, this, &TxtView::zoomIn);

    toolbarLayout->addStretch();
    toolbarLayout->addWidget(zoomOutButton);
    toolbarLayout->addWidget(zoomInButton);

    m_browser = new QTextBrowser(this);
    m_browser->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_browser, &QTextBrowser::customContextMenuRequested, this, &TxtView::showBrowserContextMenu);
    applyPageColors();
    m_browser->viewport()->installEventFilter(this); // Ctrl+wheel zoom

    connect(m_browser->verticalScrollBar(), &QScrollBar::valueChanged, this, &TxtView::scheduleProgressSave);

    m_syncPromptBar = new SyncPromptBar(this);

    layout->addWidget(toolbar);
    layout->addWidget(m_syncPromptBar);
    layout->addWidget(m_browser, 1);

    m_progressSaveTimer = new QTimer(this);
    m_progressSaveTimer->setSingleShot(true);
    connect(m_progressSaveTimer, &QTimer::timeout, this, &TxtView::saveProgressNow);
}

void TxtView::applyPageColors()
{
    if (m_darkMode) {
        m_browser->setStyleSheet(QStringLiteral("QTextBrowser { background-color: #1e1e1e; color: #ddd; }"));
    } else {
        m_browser->setStyleSheet(QStringLiteral("QTextBrowser { background-color: white; color: black; }"));
    }
}

void TxtView::goToCharacterOffset(int offset)
{
    if (!m_browser || !m_browser->document()) {
        return;
    }
    offset = std::clamp(offset, 0, m_browser->document()->characterCount() - 1);
    QTextCursor cursor(m_browser->document());
    cursor.setPosition(offset);
    m_browser->setTextCursor(cursor);
    m_browser->ensureCursorVisible();
}

void TxtView::zoomIn()
{
    if (m_fontZoomSteps >= 20) {
        return;
    }
    setFontZoomSteps(m_fontZoomSteps + 1);
    scheduleProgressSave();
}

void TxtView::zoomOut()
{
    if (m_fontZoomSteps <= -5) {
        return;
    }
    setFontZoomSteps(m_fontZoomSteps - 1);
    scheduleProgressSave();
}

void TxtView::setFontZoomSteps(int steps)
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

void TxtView::applyHighlightsToBrowser()
{
    QList<QTextEdit::ExtraSelection> selections;
    QTextCharFormat format;
    format.setBackground(QColor(255, 235, 59, 140));

    for (const Highlight &highlight : m_highlights) {
        for (const QTextCursor &occurrence : findAllOccurrences(m_browser->document(), highlight.text)) {
            QTextEdit::ExtraSelection selection;
            selection.cursor = occurrence;
            selection.format = format;
            selections.append(selection);
        }
    }

    if (!m_searchTerm.isEmpty()) {
        QTextCharFormat searchFormat;
        searchFormat.setBackground(QColor(255, 214, 0, 170));
        for (const QTextCursor &occurrence : findAllOccurrences(m_browser->document(), m_searchTerm)) {
            QTextEdit::ExtraSelection selection;
            selection.cursor = occurrence;
            selection.format = searchFormat;
            selections.append(selection);
        }
    }

    m_browser->setExtraSelections(selections);
}

void TxtView::addHighlightForSelection()
{
    const QTextCursor cursor = m_browser->textCursor();
    if (!cursor.hasSelection()) {
        return;
    }

    Highlight highlight;
    highlight.targetIndex = 0; // unused for filtering (single-document view); kept for schema consistency
    highlight.text = cursor.selectedText();
    highlight.createdAt = QDateTime::currentDateTime();

    HighlightStore::addHighlight(m_filePath, highlight);
    m_highlights = HighlightStore::highlightsFor(m_filePath);
    applyHighlightsToBrowser();
}

void TxtView::showBrowserContextMenu(const QPoint &pos)
{
    const QTextCursor selectionCursor = m_browser->textCursor();
    const bool hasSelection = selectionCursor.hasSelection();

    QMenu menu(m_browser);

    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(hasSelection);
    connect(copyAction, &QAction::triggered, m_browser, &QTextBrowser::copy);

    QAction *highlightAction = menu.addAction(tr("Highlight"));
    highlightAction->setEnabled(hasSelection);
    connect(highlightAction, &QAction::triggered, this, &TxtView::addHighlightForSelection);

    if (!hasSelection) {
        const int clickPosition = m_browser->cursorForPosition(pos).position();
        for (int i = 0; i < m_highlights.size(); ++i) {
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

void TxtView::restoreProgressAndCheckSync()
{
    m_bookHash = FileIdentity::contentHash(m_filePath);
    if (m_bookHash.isEmpty() || !m_document) {
        return;
    }

    if (const auto local = ReadingProgressStore::get(m_bookHash)) {
        setFontZoomSteps(static_cast<int>(std::lround(local->zoom)));
        goToCharacterOffset(local->position);
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
                return;
            }
            if (localFolderTimestamp.isValid() && googleRemote->timestamp <= localFolderTimestamp) {
                return;
            }
            offerSyncedPosition(*googleRemote);
        });
#endif
}

void TxtView::offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote)
{
    if (remote.position == currentPosition()) {
        return;
    }
    m_syncPromptBar->showPrompt(tr("Synced position available (from %1) — jump?").arg(remote.deviceName));
    const int remotePosition = remote.position;
    const qreal remoteZoom = remote.zoom;
    connect(m_syncPromptBar, &SyncPromptBar::jumpRequested, this, [this, remotePosition, remoteZoom] {
        setFontZoomSteps(static_cast<int>(std::lround(remoteZoom)));
        goToCharacterOffset(remotePosition);
    });
}

void TxtView::scheduleProgressSave()
{
    m_progressSaveTimer->start(1500);
}

void TxtView::saveProgressNow()
{
    if (m_bookHash.isEmpty()) {
        return;
    }
    const int position = currentPosition();
    ReadingProgressStore::set(m_bookHash, position, m_fontZoomSteps);
    ProgressSyncLog::appendEntry(m_bookHash, documentTitle(), position, m_fontZoomSteps);
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    GoogleDriveSync::appendEntry(m_bookHash, documentTitle(), position, m_fontZoomSteps);
#endif
}

bool TxtView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_browser->viewport() && event->type() == QEvent::Wheel) {
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
    return QWidget::eventFilter(watched, event);
}
