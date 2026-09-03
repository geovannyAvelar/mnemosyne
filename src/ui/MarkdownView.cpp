#include "MarkdownView.h"

#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleDriveSync.h"
#endif
#include "app/HighlightStore.h"
#include "app/HighlightSync.h"
#ifdef MNEMOSYNE_ENABLE_PLUGINS
#include "app/PluginHost.h"
#endif
#include "app/ProgressSyncLog.h"
#include "app/ReadingProgressStore.h"
#include "core/SearchUtil.h"
#include "ui/NoteDialog.h"
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

MarkdownView::MarkdownView(std::unique_ptr<MarkdownDocument> document, QString filePath, QWidget *parent)
    : QWidget(parent)
    , m_document(std::move(document))
    , m_filePath(std::move(filePath))
    , m_highlights(HighlightStore::highlightsFor(m_filePath))
{
    setupUi();
    render();
    restoreProgressAndCheckSync(); // needs the browser's document already populated to resolve heading blocks
}

QString MarkdownView::documentTitle() const
{
    return m_document ? m_document->title() : QString();
}

QVector<TocNode> MarkdownView::tableOfContents() const
{
    return m_document ? m_document->tableOfContents() : QVector<TocNode>();
}

void MarkdownView::goToTocNode(const TocNode &node)
{
    goToHeadingIndex(node.pageNumber);
}

int MarkdownView::currentPosition() const
{
    return nearestHeadingIndexAtScrollTop();
}

bool MarkdownView::hasPendingSyncPrompt() const
{
    return !m_syncPromptBar->isHidden();
}

QVector<SearchResult> MarkdownView::search(const QString &query) const
{
    return searchFile(m_filePath, query);
}

QVector<SearchResult> MarkdownView::searchFile(const QString &filePath, const QString &query)
{
    QVector<SearchResult> results;
    if (query.trimmed().isEmpty()) {
        return results;
    }

    QString error;
    const std::unique_ptr<MarkdownDocument> document = MarkdownDocument::load(filePath, &error);
    if (!document) {
        return results;
    }

    for (const MarkdownDocument::Section &section : document->sections()) {
        // Search the heading title too, not just the body, so a query that
        // only matches a heading's own text still surfaces a result.
        const QString searchable = section.label + QLatin1Char('\n') + section.text;
        if (searchable.contains(query, Qt::CaseInsensitive)) {
            SearchResult result;
            result.targetIndex = section.headingIndex;
            result.label = section.label;
            result.snippet = makeSearchSnippet(searchable, query);
            results.append(result);
        }
    }
    return results;
}

void MarkdownView::setSearchTerm(const QString &term)
{
    m_searchTerm = term.trimmed();
    applyHighlightsToBrowser();
}

void MarkdownView::setDarkMode(bool enabled)
{
    if (m_darkMode == enabled) {
        return;
    }
    m_darkMode = enabled;
    applyPageColors();
}

void MarkdownView::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(6, 4, 6, 4);

    auto *zoomOutButton = new QPushButton(tr("-"), toolbar);
    auto *zoomInButton = new QPushButton(tr("+"), toolbar);
    connect(zoomOutButton, &QPushButton::clicked, this, &MarkdownView::zoomOut);
    connect(zoomInButton, &QPushButton::clicked, this, &MarkdownView::zoomIn);

    toolbarLayout->addStretch();
    toolbarLayout->addWidget(zoomOutButton);
    toolbarLayout->addWidget(zoomInButton);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(false);
    m_browser->setOpenLinks(false);
    m_browser->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_browser, &QTextBrowser::customContextMenuRequested, this, &MarkdownView::showBrowserContextMenu);
    applyPageColors();
    m_browser->viewport()->installEventFilter(this); // Ctrl+wheel zoom

    connect(m_browser->verticalScrollBar(), &QScrollBar::valueChanged, this, &MarkdownView::scheduleProgressSave);

    m_syncPromptBar = new SyncPromptBar(this);

    layout->addWidget(toolbar);
    layout->addWidget(m_syncPromptBar);
    layout->addWidget(m_browser, 1);

    m_progressSaveTimer = new QTimer(this);
    m_progressSaveTimer->setSingleShot(true);
    connect(m_progressSaveTimer, &QTimer::timeout, this, &MarkdownView::saveProgressNow);
}

void MarkdownView::applyPageColors()
{
    if (m_darkMode) {
        m_browser->setStyleSheet(QStringLiteral("QTextBrowser { background-color: #1e1e1e; color: #ddd; }"));
    } else {
        m_browser->setStyleSheet(QStringLiteral("QTextBrowser { background-color: white; color: black; }"));
    }
}

void MarkdownView::render()
{
    if (!m_document) {
        return;
    }
#ifdef MNEMOSYNE_ENABLE_PLUGINS
    // Unlike EPUB/MOBI's HTML <style> injection, Markdown goes through Qt's
    // own Markdown-to-richtext conversion below, which honors a document-
    // level CSS stylesheet -- but only if it's set before setMarkdown() is
    // called, not after.
    m_browser->document()->setDefaultStyleSheet(PluginHost::cssForFormat(QStringLiteral("markdown")));
#endif
    m_browser->document()->setMarkdown(m_document->markdownText());
    applyHighlightsToBrowser();
}

void MarkdownView::goToHeadingIndex(int headingIndex)
{
    if (!m_browser) {
        return;
    }
    if (headingIndex >= 0) {
        int ordinal = -1;
        for (QTextBlock block = m_browser->document()->begin(); block.isValid(); block = block.next()) {
            if (block.blockFormat().headingLevel() > 0) {
                ++ordinal;
                if (ordinal == headingIndex) {
                    m_browser->setTextCursor(QTextCursor(block));
                    m_browser->ensureCursorVisible();
                    return;
                }
            }
        }
        // Heading index out of range (e.g. stale saved progress after the
        // file changed on disk) — fall through to scrolling to the top.
    }
    m_browser->verticalScrollBar()->setValue(0);
}

int MarkdownView::nearestHeadingIndexAtScrollTop() const
{
    if (!m_browser) {
        return -1;
    }
    const int topBlockNumber = m_browser->cursorForPosition(QPoint(1, 1)).blockNumber();

    int headingOrdinal = -1;
    for (QTextBlock block = m_browser->document()->begin();
         block.isValid() && block.blockNumber() <= topBlockNumber; block = block.next()) {
        if (block.blockFormat().headingLevel() > 0) {
            ++headingOrdinal;
        }
    }
    return headingOrdinal;
}

void MarkdownView::zoomIn()
{
    if (m_fontZoomSteps >= 20) {
        return;
    }
    setFontZoomSteps(m_fontZoomSteps + 1);
    scheduleProgressSave();
}

void MarkdownView::zoomOut()
{
    if (m_fontZoomSteps <= -5) {
        return;
    }
    setFontZoomSteps(m_fontZoomSteps - 1);
    scheduleProgressSave();
}

void MarkdownView::setFontZoomSteps(int steps)
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

void MarkdownView::applyHighlightsToBrowser()
{
    QList<QTextEdit::ExtraSelection> selections;

    // Unlike EpubView there's no per-chapter filtering: a Markdown file is
    // one document, so every stored highlight applies to it.
    for (const Highlight &highlight : m_highlights) {
        QTextCharFormat format;
        format.setBackground(highlight.color);
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

void MarkdownView::refreshHighlights()
{
    m_highlights = HighlightStore::highlightsFor(m_filePath);
    applyHighlightsToBrowser();
}

void MarkdownView::addHighlightForSelection()
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
    emit highlightsChanged();
}

void MarkdownView::addNoteForSelection()
{
    const QTextCursor cursor = m_browser->textCursor();
    if (!cursor.hasSelection()) {
        return;
    }

    const std::optional<NoteDialog::Result> result = NoteDialog::show(this, QString(), kDefaultHighlightColor);
    if (!result || result->note.isEmpty()) {
        return;
    }

    Highlight highlight;
    highlight.targetIndex = 0; // unused for filtering (single-document view); kept for schema consistency
    highlight.text = cursor.selectedText();
    highlight.createdAt = QDateTime::currentDateTime();
    highlight.note = result->note;
    highlight.color = result->color;

    HighlightStore::addHighlight(m_filePath, highlight);
    m_highlights = HighlightStore::highlightsFor(m_filePath);
    applyHighlightsToBrowser();
    emit highlightsChanged();
}

void MarkdownView::showBrowserContextMenu(const QPoint &pos)
{
    const QTextCursor selectionCursor = m_browser->textCursor();
    const bool hasSelection = selectionCursor.hasSelection();

    QMenu menu(m_browser);

    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(hasSelection);
    connect(copyAction, &QAction::triggered, m_browser, &QTextBrowser::copy);

    QAction *highlightAction = menu.addAction(tr("Highlight"));
    highlightAction->setEnabled(hasSelection);
    connect(highlightAction, &QAction::triggered, this, &MarkdownView::addHighlightForSelection);

    QAction *addNoteAction = menu.addAction(tr("Add Note..."));
    addNoteAction->setEnabled(hasSelection);
    connect(addNoteAction, &QAction::triggered, this, &MarkdownView::addNoteForSelection);

    if (!hasSelection) {
        const int clickPosition = m_browser->cursorForPosition(pos).position();
        for (int i = 0; i < m_highlights.size(); ++i) {
            const QVector<QTextCursor> occurrences = findAllOccurrences(m_browser->document(), m_highlights[i].text);
            const bool clickedInside = std::any_of(occurrences.begin(), occurrences.end(), [clickPosition](const QTextCursor &c) {
                return clickPosition >= c.selectionStart() && clickPosition < c.selectionEnd();
            });
            if (clickedInside) {
                menu.addSeparator();
                const bool hasNote = !m_highlights[i].note.isEmpty();
                QAction *noteAction = menu.addAction(hasNote ? tr("Edit Note...") : tr("Add Note..."));
                connect(noteAction, &QAction::triggered, this, [this, i] {
                    const std::optional<NoteDialog::Result> result = NoteDialog::show(this, m_highlights[i].note, m_highlights[i].color);
                    if (!result) {
                        return;
                    }
                    HighlightStore::setNote(m_filePath, i, result->note);
                    HighlightStore::setColor(m_filePath, i, result->color);
                    m_highlights = HighlightStore::highlightsFor(m_filePath);
                    applyHighlightsToBrowser();
                    emit highlightsChanged();
                });
                QAction *removeAction = menu.addAction(hasNote ? tr("Remove Note") : tr("Remove Highlight"));
                connect(removeAction, &QAction::triggered, this, [this, i] {
                    HighlightStore::removeHighlight(m_filePath, i);
                    m_highlights = HighlightStore::highlightsFor(m_filePath);
                    applyHighlightsToBrowser();
                    emit highlightsChanged();
                });
                break;
            }
        }
    }

    menu.exec(m_browser->mapToGlobal(pos));
}

void MarkdownView::restoreProgressAndCheckSync()
{
    m_bookHash = FileIdentity::contentHash(m_filePath);
    if (m_bookHash.isEmpty() || !m_document) {
        return;
    }

    if (const auto local = ReadingProgressStore::get(m_bookHash)) {
        setFontZoomSteps(static_cast<int>(std::lround(local->zoom)));
        goToHeadingIndex(local->position);
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

    HighlightSync::pull(m_bookHash, [this](bool changed) {
        if (changed) {
            refreshHighlights();
            emit highlightsChanged();
        }
    });
}

void MarkdownView::offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote)
{
    if (remote.position == currentPosition()) {
        return;
    }
    m_syncPromptBar->showPrompt(
        tr("Synced position available (from %1) — jump?").arg(remote.deviceName));
    const int remotePosition = remote.position;
    const qreal remoteZoom = remote.zoom;
    connect(m_syncPromptBar, &SyncPromptBar::jumpRequested, this, [this, remotePosition, remoteZoom] {
        setFontZoomSteps(static_cast<int>(std::lround(remoteZoom)));
        goToHeadingIndex(remotePosition);
    });
}

void MarkdownView::scheduleProgressSave()
{
    m_progressSaveTimer->start(1500);
}

void MarkdownView::flushProgress()
{
    if (!m_progressSaveTimer->isActive()) {
        return;
    }
    m_progressSaveTimer->stop();
    saveProgressNow();
}

void MarkdownView::saveProgressNow()
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

bool MarkdownView::eventFilter(QObject *watched, QEvent *event)
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
