#include "EpubView.h"

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
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
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

// Same as findAllOccurrences, but only returns matches whose block number
// falls within [startBlock, endBlock) -- used to scope a highlight's search
// to its own chapter's content, now that multiple chapters can be loaded in
// the same QTextDocument simultaneously (a phrase repeated verbatim in two
// loaded chapters would otherwise cross-contaminate).
QVector<QTextCursor> findOccurrencesInRange(QTextDocument *document, const QString &text, int startBlock, int endBlock)
{
    QVector<QTextCursor> occurrences;
    if (text.isEmpty()) {
        return occurrences;
    }
    QTextCursor cursor(document->findBlockByNumber(std::max(0, startBlock)));
    while (true) {
        cursor = document->find(text, cursor);
        if (cursor.isNull() || cursor.blockNumber() >= endBlock) {
            break;
        }
        if (cursor.blockNumber() >= startBlock) {
            occurrences.append(cursor);
        }
    }
    return occurrences;
}

constexpr int kEdgeThresholdPx = 300; // how close to the top/bottom edge triggers loading the next/previous chapter

} // namespace

EpubView::EpubView(std::unique_ptr<EpubDocument> document, QString filePath, QWidget *parent)
    : QWidget(parent)
    , m_document(std::move(document))
    , m_filePath(std::move(filePath))
    // m_bookHash isn't set until restoreProgressAndCheckSync() runs in the
    // constructor body below, so the initial load recomputes the hash here
    // directly (cheap — FileIdentity caches by path+size+mtime).
    , m_highlights(HighlightStore::highlightsFor(FileIdentity::contentHash(m_filePath)))
{
    setupUi();
    restoreProgressAndCheckSync(); // sets m_currentChapter/m_fontZoomSteps before the first render, so there's no visible jump
    loadWindowStartingAt(m_currentChapter);
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
    return searchFile(m_filePath, query);
}

QVector<SearchResult> EpubView::searchFile(const QString &filePath, const QString &query)
{
    QVector<SearchResult> results;
    if (query.trimmed().isEmpty()) {
        return results;
    }

    QString error;
    const std::unique_ptr<EpubDocument> document = EpubDocument::load(filePath, &error);
    if (!document) {
        return results;
    }

    for (int i = 0; i < document->spineCount(); ++i) {
        QTextDocument doc;
        doc.setHtml(document->chapterHtml(i));
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

void EpubView::setSearchTerm(const QString &term)
{
    m_searchTerm = term.trimmed();
    applyHighlightsToBrowser();
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
    // The dark-mode color override is injected per-fragment (see
    // chapterHtmlFragment()), so it can't be patched into an
    // already-merged multi-chapter document -- reset back to a single-
    // chapter window, which the reader can re-grow by scrolling. Dark-mode
    // toggling is rare enough that this is an acceptable tradeoff.
    loadWindowStartingAt(m_currentChapter);
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
    connect(m_browser, &QTextBrowser::anchorClicked, this, &EpubView::onVideoLinkActivated);
    applyPageColors();
    m_browser->viewport()->installEventFilter(this); // Ctrl+wheel zoom
    connect(m_browser->verticalScrollBar(), &QScrollBar::valueChanged, this, &EpubView::onScrolled);

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

QString EpubView::chapterHtmlFragment(int spineIndex) const
{
    QString html = m_document->chapterHtml(spineIndex);

    // The book's own CSS (e.g. "body { color: #222 }") would otherwise stay
    // in force and become unreadable against a dark page. Since this has
    // the same selector specificity as that rule, it must come after it in
    // document order to win the cascade — appending to the end of the
    // string isn't enough, as Qt's HTML parser may not honor a <style>
    // block outside <head>. Elements with a *more specific* rule (a styled
    // heading, say) still keep the author's intended color. Plugin CSS (see
    // PluginHost::cssForFormat) comes after dark mode's, in the same block,
    // so a plugin can override it too.
    QString styleOverride = m_darkMode ? QStringLiteral("body,p,div,span{color:#ddd;}") : QString();
#ifdef MNEMOSYNE_ENABLE_PLUGINS
    styleOverride += PluginHost::cssForFormat(QStringLiteral("epub"));
#endif
    if (!styleOverride.isEmpty()) {
        const QString styleBlock = QStringLiteral("<style>") + styleOverride + QStringLiteral("</style>");
        const int headEnd = html.indexOf(QStringLiteral("</head>"), 0, Qt::CaseInsensitive);
        if (headEnd >= 0) {
            html.insert(headEnd, styleBlock);
        } else {
            html.prepend(styleBlock);
        }
    }

    // A boundary marker so loadWindowStartingAt() can scrollToAnchor()
    // straight to this chapter's start.
    const QString anchor = QStringLiteral("<a name=\"mnemosyne-chapter-%1\"></a>").arg(spineIndex);
    const int bodyStart = html.indexOf(QStringLiteral("<body"), 0, Qt::CaseInsensitive);
    const int bodyTagEnd = bodyStart >= 0 ? html.indexOf(QLatin1Char('>'), bodyStart) : -1;
    if (bodyTagEnd >= 0) {
        html.insert(bodyTagEnd + 1, anchor);
    } else {
        html.prepend(anchor);
    }
    return html;
}

void EpubView::loadWindowStartingAt(int spineIndex)
{
    if (!m_document || m_document->spineCount() == 0) {
        return;
    }
    spineIndex = std::clamp(spineIndex, 0, m_document->spineCount() - 1);

    m_chapterStartBlock.clear();
    m_browser->setHtml(chapterHtmlFragment(spineIndex));
    m_chapterStartBlock.insert(spineIndex, 0);
    m_loadedChapterStart = spineIndex;
    m_loadedChapterEnd = spineIndex;
    m_currentChapter = spineIndex;

    applyHighlightsToBrowser();
    updateNavigationState();
    m_browser->scrollToAnchor(QStringLiteral("mnemosyne-chapter-%1").arg(spineIndex));
}

void EpubView::goToChapter(int spineIndex)
{
    if (!m_document) {
        return;
    }
    spineIndex = std::clamp(spineIndex, 0, m_document->spineCount() - 1);
    // Not just "already on this chapter": the loaded window may have grown
    // past it via scrolling even while it's still the dominant one, and an
    // explicit jump here should always reset to just that one chapter.
    if (spineIndex == m_currentChapter && spineIndex == m_loadedChapterStart && spineIndex == m_loadedChapterEnd) {
        updateNavigationState();
        return;
    }
    loadWindowStartingAt(spineIndex);
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

void EpubView::appendNextChapter()
{
    const int newChapterIndex = m_loadedChapterEnd + 1;
    if (!m_document || newChapterIndex >= m_document->spineCount()) {
        return;
    }
    m_loadingAdjacentChapter = true;

    const int startBlock = m_browser->document()->blockCount();

    QTextCursor cursor(m_browser->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertBlock();
    cursor.insertHtml(chapterHtmlFragment(newChapterIndex));

    m_chapterStartBlock.insert(newChapterIndex, startBlock);
    m_loadedChapterEnd = newChapterIndex;

    applyHighlightsToBrowser();
    m_loadingAdjacentChapter = false;
}

void EpubView::prependPreviousChapter()
{
    const int newChapterIndex = m_loadedChapterStart - 1;
    if (newChapterIndex < 0) {
        return;
    }
    m_loadingAdjacentChapter = true;

    QScrollBar *vbar = m_browser->verticalScrollBar();
    const int oldMax = vbar->maximum();
    const int oldValue = vbar->value();
    const int oldBlockCount = m_browser->document()->blockCount();

    QTextCursor cursor(m_browser->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.insertHtml(chapterHtmlFragment(newChapterIndex));
    cursor.insertBlock(); // separate the just-inserted chapter from what follows it

    const int insertedBlocks = m_browser->document()->blockCount() - oldBlockCount;
    QHash<int, int> shifted;
    for (auto it = m_chapterStartBlock.constBegin(); it != m_chapterStartBlock.constEnd(); ++it) {
        shifted.insert(it.key(), it.value() + insertedBlocks);
    }
    m_chapterStartBlock = shifted;
    m_chapterStartBlock.insert(newChapterIndex, 0);
    m_loadedChapterStart = newChapterIndex;

    // Keep the same content pixel-anchored under the viewport: shift the
    // scroll value by exactly however much the document just grew.
    vbar->setValue(oldValue + (vbar->maximum() - oldMax));

    applyHighlightsToBrowser();
    m_loadingAdjacentChapter = false;
}

void EpubView::onScrolled()
{
    if (m_loadingAdjacentChapter || !m_document) {
        return;
    }
    QScrollBar *vbar = m_browser->verticalScrollBar();

    if (vbar->value() >= vbar->maximum() - kEdgeThresholdPx && m_loadedChapterEnd < m_document->spineCount() - 1) {
        appendNextChapter();
    } else if (vbar->value() <= vbar->minimum() + kEdgeThresholdPx && m_loadedChapterStart > 0) {
        prependPreviousChapter();
    }

    const QTextCursor topCursor = m_browser->cursorForPosition(QPoint(0, 0));
    const int chapter = chapterAtBlockNumber(topCursor.blockNumber());
    if (chapter != m_currentChapter) {
        m_currentChapter = chapter;
        updateNavigationState();
        scheduleProgressSave();
    }
}

int EpubView::chapterAtBlockNumber(int blockNumber) const
{
    int best = m_loadedChapterStart;
    int bestBlock = -1;
    for (auto it = m_chapterStartBlock.constBegin(); it != m_chapterStartBlock.constEnd(); ++it) {
        if (it.value() <= blockNumber && it.value() > bestBlock) {
            bestBlock = it.value();
            best = it.key();
        }
    }
    return best;
}

QPair<int, int> EpubView::chapterBlockRange(int spineIndex) const
{
    const int start = m_chapterStartBlock.value(spineIndex, 0);
    const int nextStart = m_chapterStartBlock.value(spineIndex + 1, -1);
    const int end = nextStart >= 0 ? nextStart : m_browser->document()->blockCount();
    return {start, end};
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

    for (const Highlight &highlight : m_highlights) {
        if (highlight.targetIndex < m_loadedChapterStart || highlight.targetIndex > m_loadedChapterEnd) {
            continue;
        }
        const QPair<int, int> range = chapterBlockRange(highlight.targetIndex);
        QTextCharFormat format;
        format.setBackground(highlight.color);
        for (const QTextCursor &occurrence :
             findOccurrencesInRange(m_browser->document(), highlight.text, range.first, range.second)) {
            QTextEdit::ExtraSelection selection;
            selection.cursor = occurrence;
            selection.format = format;
            selections.append(selection);
        }
    }

    // Bolder/more saturated than persisted highlights, and added last so it
    // paints on top — a dauber pass marking every hit of the active search
    // term, distinct from highlights the reader made themselves. Not scoped
    // to one chapter's range: it's meant to cover everything loaded.
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

void EpubView::refreshHighlights()
{
    m_highlights = HighlightStore::highlightsFor(m_bookHash);
    applyHighlightsToBrowser();
}

void EpubView::addHighlightForSelection()
{
    const QTextCursor cursor = m_browser->textCursor();
    if (!cursor.hasSelection()) {
        return;
    }

    Highlight highlight;
    highlight.targetIndex = chapterAtBlockNumber(cursor.blockNumber());
    highlight.text = cursor.selectedText();
    highlight.createdAt = QDateTime::currentDateTime();

    HighlightStore::addHighlight(m_bookHash, highlight);
    m_highlights = HighlightStore::highlightsFor(m_bookHash);
    applyHighlightsToBrowser();
    emit highlightsChanged();
}

void EpubView::addNoteForSelection()
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
    highlight.targetIndex = chapterAtBlockNumber(cursor.blockNumber());
    highlight.text = cursor.selectedText();
    highlight.createdAt = QDateTime::currentDateTime();
    highlight.note = result->note;
    highlight.color = result->color;

    HighlightStore::addHighlight(m_bookHash, highlight);
    m_highlights = HighlightStore::highlightsFor(m_bookHash);
    applyHighlightsToBrowser();
    emit highlightsChanged();
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

    QAction *addNoteAction = menu.addAction(tr("Add Note..."));
    addNoteAction->setEnabled(hasSelection);
    connect(addNoteAction, &QAction::triggered, this, &EpubView::addNoteForSelection);

    if (!hasSelection) {
        const QTextCursor clickCursor = m_browser->cursorForPosition(pos);
        const int clickPosition = clickCursor.position();
        const int clickedChapter = chapterAtBlockNumber(clickCursor.blockNumber());
        const QPair<int, int> range = chapterBlockRange(clickedChapter);
        for (int i = 0; i < m_highlights.size(); ++i) {
            if (m_highlights[i].targetIndex != clickedChapter) {
                continue;
            }
            const QVector<QTextCursor> occurrences =
                findOccurrencesInRange(m_browser->document(), m_highlights[i].text, range.first, range.second);
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
                    HighlightStore::setNote(m_bookHash, i, result->note);
                    HighlightStore::setColor(m_bookHash, i, result->color);
                    m_highlights = HighlightStore::highlightsFor(m_bookHash);
                    applyHighlightsToBrowser();
                    emit highlightsChanged();
                });
                QAction *removeAction = menu.addAction(hasNote ? tr("Remove Note") : tr("Remove Highlight"));
                connect(removeAction, &QAction::triggered, this, [this, i] {
                    HighlightStore::removeHighlight(m_bookHash, i);
                    m_highlights = HighlightStore::highlightsFor(m_bookHash);
                    applyHighlightsToBrowser();
                    emit highlightsChanged();
                });
                break;
            }
        }
    }

    menu.exec(m_browser->mapToGlobal(pos));
}

void EpubView::onVideoLinkActivated(const QUrl &url)
{
    const QString scheme = url.scheme();
    if (scheme != QLatin1String("mnemosyne-video")) {
        return; // an ordinary in-book link; setOpenLinks(false) means Qt won't follow it either way
    }

    bool ok = false;
    const int videoIndex = url.toString().mid(scheme.size() + 1).toInt(&ok);
    if (!ok || !m_document) {
        return;
    }

    const QVector<QString> videoPaths = m_document->chapterVideoPaths(m_currentChapter);
    if (videoIndex < 0 || videoIndex >= videoPaths.size() || videoPaths.at(videoIndex).isEmpty()) {
        return;
    }
    const QString archivePath = videoPaths.at(videoIndex);

    bool readOk = false;
    const QByteArray data = m_document->readResource(archivePath, &readOk);
    if (!readOk) {
        QMessageBox::warning(this, tr("Could Not Play Video"), tr("Could not read the video from this book."));
        return;
    }

    // Cached by book + archive path, not re-extracted on every click: videos
    // can be tens of megabytes, and the same one is often replayed.
    const QString cacheDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/video-playback");
    QDir().mkpath(cacheDir);
    const QString tempPath =
        cacheDir + QLatin1Char('/') + m_bookHash + QLatin1Char('-') + QFileInfo(archivePath).fileName();

    if (!QFileInfo::exists(tempPath)) {
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly) || tempFile.write(data) != data.size()) {
            QMessageBox::warning(this, tr("Could Not Play Video"), tr("Could not prepare the video for playback."));
            return;
        }
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(tempPath))) {
        QMessageBox::warning(this, tr("Could Not Play Video"), tr("No application is available to play this video."));
    }
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

    HighlightSync::pull(m_bookHash, [this](bool changed) {
        if (changed) {
            refreshHighlights();
            emit highlightsChanged();
        }
    });
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

void EpubView::flushProgress()
{
    if (!m_progressSaveTimer->isActive()) {
        return;
    }
    m_progressSaveTimer->stop();
    saveProgressNow();
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

        // Not consumed above -- native scrolling handles it. But if the
        // scrollbar's range is degenerate (e.g. a chapter shorter than the
        // viewport), its value won't actually change, so valueChanged won't
        // fire and onScrolled() would never run to grow the loaded window.
        // Call it directly, deferred past the native scroll this triggers.
        QPointer<EpubView> self = this;
        QTimer::singleShot(0, this, [self] {
            if (self) {
                self->onScrolled();
            }
        });
    }
    return QWidget::eventFilter(watched, event);
}
