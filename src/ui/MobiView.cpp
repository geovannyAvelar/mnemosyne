#include "MobiView.h"

#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleDriveSync.h"
#endif
#include "app/HighlightStore.h"
#include "app/ProgressSyncLog.h"
#include "app/ReadingProgressStore.h"
#include "core/SearchUtil.h"
#include "ui/NoteDialog.h"
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

MobiView::MobiView(std::unique_ptr<MobiDocument> document, QString filePath, QWidget *parent)
    : QWidget(parent)
    , m_document(std::move(document))
    , m_filePath(std::move(filePath))
    , m_highlights(HighlightStore::highlightsFor(m_filePath))
{
    setupUi();
    restoreProgressAndCheckSync(); // sets m_currentPart/m_fontZoomSteps before the first render, so there's no visible jump
    renderCurrentPart();
    updateNavigationState();
}

QString MobiView::documentTitle() const
{
    return m_document ? m_document->title() : QString();
}

QVector<TocNode> MobiView::tableOfContents() const
{
    return m_document ? m_document->tableOfContents() : QVector<TocNode>();
}

void MobiView::goToTocNode(const TocNode &node)
{
    if (node.pageNumber >= 0) {
        goToPart(node.pageNumber);
    }
}

int MobiView::currentPosition() const
{
    return m_currentPart;
}

bool MobiView::hasPendingSyncPrompt() const
{
    return !m_syncPromptBar->isHidden();
}

QVector<SearchResult> MobiView::search(const QString &query) const
{
    return searchFile(m_filePath, query);
}

QVector<SearchResult> MobiView::searchFile(const QString &filePath, const QString &query)
{
    QVector<SearchResult> results;
    if (query.trimmed().isEmpty()) {
        return results;
    }

    QString error;
    const std::unique_ptr<MobiDocument> document = MobiDocument::load(filePath, &error);
    if (!document) {
        return results;
    }

    for (int i = 0; i < document->partCount(); ++i) {
        QTextDocument doc;
        doc.setHtml(document->partHtml(i));
        const QString text = doc.toPlainText();
        if (text.contains(query, Qt::CaseInsensitive)) {
            SearchResult result;
            result.targetIndex = i;
            result.label = tr("Part %1").arg(i + 1);
            result.snippet = makeSearchSnippet(text, query);
            results.append(result);
        }
    }
    return results;
}

void MobiView::setSearchTerm(const QString &term)
{
    m_searchTerm = term.trimmed();
    applyHighlightsToBrowser();
}

void MobiView::setDarkMode(bool enabled)
{
    if (m_darkMode == enabled) {
        return;
    }
    m_darkMode = enabled;
    applyPageColors();
    renderCurrentPart();
}

void MobiView::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(6, 4, 6, 4);

    auto *prevButton = new QPushButton(tr("< Prev"), toolbar);
    auto *nextButton = new QPushButton(tr("Next >"), toolbar);
    connect(prevButton, &QPushButton::clicked, this, &MobiView::previousPart);
    connect(nextButton, &QPushButton::clicked, this, &MobiView::nextPart);

    m_partLabel = new QLabel(toolbar);

    auto *zoomOutButton = new QPushButton(tr("-"), toolbar);
    auto *zoomInButton = new QPushButton(tr("+"), toolbar);
    connect(zoomOutButton, &QPushButton::clicked, this, &MobiView::zoomOut);
    connect(zoomInButton, &QPushButton::clicked, this, &MobiView::zoomIn);

    toolbarLayout->addWidget(prevButton);
    toolbarLayout->addWidget(m_partLabel);
    toolbarLayout->addWidget(nextButton);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(zoomOutButton);
    toolbarLayout->addWidget(zoomInButton);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(false);
    m_browser->setOpenLinks(false);
    m_browser->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_browser, &QTextBrowser::customContextMenuRequested, this, &MobiView::showBrowserContextMenu);
    applyPageColors();
    m_browser->viewport()->installEventFilter(this); // scrolling past the top/bottom edge turns the part; Ctrl+wheel zooms

    m_syncPromptBar = new SyncPromptBar(this);

    layout->addWidget(toolbar);
    layout->addWidget(m_syncPromptBar);
    layout->addWidget(m_browser, 1);

    m_progressSaveTimer = new QTimer(this);
    m_progressSaveTimer->setSingleShot(true);
    connect(m_progressSaveTimer, &QTimer::timeout, this, &MobiView::saveProgressNow);
}

void MobiView::applyPageColors()
{
    if (m_darkMode) {
        m_browser->setStyleSheet(QStringLiteral("QTextBrowser { background-color: #1e1e1e; color: #ddd; }"));
    } else {
        m_browser->setStyleSheet(QStringLiteral("QTextBrowser { background-color: white; color: black; }"));
    }
}

void MobiView::goToPart(int partIndex)
{
    if (!m_document) {
        return;
    }
    partIndex = std::clamp(partIndex, 0, m_document->partCount() - 1);
    if (partIndex == m_currentPart) {
        updateNavigationState();
        return;
    }
    m_currentPart = partIndex;
    renderCurrentPart();
    updateNavigationState();
    scheduleProgressSave();
}

void MobiView::nextPart()
{
    goToPart(m_currentPart + 1);
}

void MobiView::previousPart()
{
    goToPart(m_currentPart - 1);
}

void MobiView::renderCurrentPart()
{
    if (!m_document || m_document->partCount() == 0) {
        return;
    }

    QString html = m_document->partHtml(m_currentPart);
    if (m_darkMode) {
        // See EpubView::renderCurrentChapter() for why this has to come
        // after the document's own CSS in document order, rather than just
        // being appended to the string.
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

void MobiView::zoomIn()
{
    if (m_fontZoomSteps >= 20) {
        return;
    }
    setFontZoomSteps(m_fontZoomSteps + 1);
    scheduleProgressSave();
}

void MobiView::zoomOut()
{
    if (m_fontZoomSteps <= -5) {
        return;
    }
    setFontZoomSteps(m_fontZoomSteps - 1);
    scheduleProgressSave();
}

void MobiView::setFontZoomSteps(int steps)
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

void MobiView::updateNavigationState()
{
    if (!m_document) {
        return;
    }
    m_partLabel->setText(tr("Part %1 of %2").arg(m_currentPart + 1).arg(m_document->partCount()));
}

void MobiView::applyHighlightsToBrowser()
{
    QList<QTextEdit::ExtraSelection> selections;

    for (const Highlight &highlight : m_highlights) {
        if (highlight.targetIndex != m_currentPart) {
            continue;
        }
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

void MobiView::refreshHighlights()
{
    m_highlights = HighlightStore::highlightsFor(m_filePath);
    applyHighlightsToBrowser();
}

void MobiView::addHighlightForSelection()
{
    const QTextCursor cursor = m_browser->textCursor();
    if (!cursor.hasSelection()) {
        return;
    }

    Highlight highlight;
    highlight.targetIndex = m_currentPart;
    highlight.text = cursor.selectedText();
    highlight.createdAt = QDateTime::currentDateTime();

    HighlightStore::addHighlight(m_filePath, highlight);
    m_highlights = HighlightStore::highlightsFor(m_filePath);
    applyHighlightsToBrowser();
    emit highlightsChanged();
}

void MobiView::addNoteForSelection()
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
    highlight.targetIndex = m_currentPart;
    highlight.text = cursor.selectedText();
    highlight.createdAt = QDateTime::currentDateTime();
    highlight.note = result->note;
    highlight.color = result->color;

    HighlightStore::addHighlight(m_filePath, highlight);
    m_highlights = HighlightStore::highlightsFor(m_filePath);
    applyHighlightsToBrowser();
    emit highlightsChanged();
}

void MobiView::showBrowserContextMenu(const QPoint &pos)
{
    const QTextCursor selectionCursor = m_browser->textCursor();
    const bool hasSelection = selectionCursor.hasSelection();

    QMenu menu(m_browser);

    QAction *copyAction = menu.addAction(tr("Copy"));
    copyAction->setEnabled(hasSelection);
    connect(copyAction, &QAction::triggered, m_browser, &QTextBrowser::copy);

    QAction *highlightAction = menu.addAction(tr("Highlight"));
    highlightAction->setEnabled(hasSelection);
    connect(highlightAction, &QAction::triggered, this, &MobiView::addHighlightForSelection);

    QAction *addNoteAction = menu.addAction(tr("Add Note..."));
    addNoteAction->setEnabled(hasSelection);
    connect(addNoteAction, &QAction::triggered, this, &MobiView::addNoteForSelection);

    if (!hasSelection) {
        const int clickPosition = m_browser->cursorForPosition(pos).position();
        for (int i = 0; i < m_highlights.size(); ++i) {
            if (m_highlights[i].targetIndex != m_currentPart) {
                continue;
            }
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

void MobiView::restoreProgressAndCheckSync()
{
    m_bookHash = FileIdentity::contentHash(m_filePath);
    if (m_bookHash.isEmpty() || !m_document) {
        return;
    }

    if (const auto local = ReadingProgressStore::get(m_bookHash)) {
        m_currentPart = std::clamp(local->position, 0, std::max(0, m_document->partCount() - 1));
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
                return;
            }
            if (localFolderTimestamp.isValid() && googleRemote->timestamp <= localFolderTimestamp) {
                return;
            }
            offerSyncedPosition(*googleRemote);
        });
#endif
}

void MobiView::offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote)
{
    if (remote.position == m_currentPart) {
        return;
    }
    m_syncPromptBar->showPrompt(tr("Synced position available: part %1 (from %2) — jump?")
                                     .arg(remote.position + 1)
                                     .arg(remote.deviceName));
    const int remotePosition = remote.position;
    const qreal remoteZoom = remote.zoom;
    connect(m_syncPromptBar, &SyncPromptBar::jumpRequested, this, [this, remotePosition, remoteZoom] {
        setFontZoomSteps(static_cast<int>(std::lround(remoteZoom)));
        goToPart(remotePosition);
    });
}

void MobiView::scheduleProgressSave()
{
    m_progressSaveTimer->start(1500);
}

void MobiView::saveProgressNow()
{
    if (m_bookHash.isEmpty()) {
        return;
    }
    ReadingProgressStore::set(m_bookHash, m_currentPart, m_fontZoomSteps);
    ProgressSyncLog::appendEntry(m_bookHash, documentTitle(), m_currentPart, m_fontZoomSteps);
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    GoogleDriveSync::appendEntry(m_bookHash, documentTitle(), m_currentPart, m_fontZoomSteps);
#endif
}

bool MobiView::eventFilter(QObject *watched, QEvent *event)
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

    if (watched == m_browser->viewport() && event->type() == QEvent::Wheel && !m_pageTurnCooldown) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        QScrollBar *vbar = m_browser->verticalScrollBar();
        const int deltaY = wheelEvent->angleDelta().y();

        // See PdfView::eventFilter for the sign convention and the
        // min==max==0 (whole part fits in view) reasoning.
        const bool scrollingDownPastBottom = deltaY < 0 && vbar->value() >= vbar->maximum();
        const bool scrollingUpPastTop = deltaY > 0 && vbar->value() <= vbar->minimum();

        if (scrollingDownPastBottom && m_document && m_currentPart < m_document->partCount() - 1) {
            nextPart();
            m_browser->verticalScrollBar()->setValue(0);
            m_pageTurnCooldown = true;
            QTimer::singleShot(400, this, [this] { m_pageTurnCooldown = false; });
            return true;
        }
        if (scrollingUpPastTop && m_currentPart > 0) {
            previousPart();
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
