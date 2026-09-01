#pragma once

#include "core/Highlight.h"
#include "core/ReaderView.h"
#include "epub/EpubDocument.h"

#include <QHash>
#include <QPair>
#include <QWidget>

#include <memory>

class QLabel;
class QTextBrowser;
class QTextCursor;
class QTimer;
class QUrl;
class SyncPromptBar;
namespace ProgressSyncLog {
struct RemoteEntry;
}

class EpubView : public QWidget, public IReaderView
{
    Q_OBJECT

public:
    explicit EpubView(std::unique_ptr<EpubDocument> document, QString filePath, QWidget *parent = nullptr);

    QString documentTitle() const override;
    QVector<TocNode> tableOfContents() const override;
    void goToTocNode(const TocNode &node) override;
    int currentPosition() const override;
    QVector<SearchResult> search(const QString &query) const override;
    void setSearchTerm(const QString &term) override;
    void refreshHighlights() override;

    // Same search as above, but independent of any EpubView instance: opens
    // its own EpubDocument (and zip archive handle) from filePath rather
    // than touching a live view's m_document. libzip reads aren't safe to
    // share across threads, so this is what MainWindow calls on a
    // background thread while the view (and its own m_document, used by the
    // main thread for rendering) keeps running independently.
    static QVector<SearchResult> searchFile(const QString &filePath, const QString &query);

    void setDarkMode(bool enabled);

    // The document's current base font size, reflecting any applied zoom.
    qreal currentFontPointSize() const;

    bool hasPendingSyncPrompt() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void goToChapter(int spineIndex);
    void nextChapter();
    void previousChapter();
    void zoomIn();
    void zoomOut();

signals:
    // Emitted whenever this view adds, edits, or removes a highlight/note,
    // so MainWindow can keep the Notes dock in sync without polling.
    void highlightsChanged();

private:
    void setupUi();

    // Discards whatever chapter window is currently loaded and starts fresh
    // at spineIndex: setHtml() with just that chapter, scrolled to its top.
    // Used for TOC/search jumps, initial open, and dark-mode toggling.
    void loadWindowStartingAt(int spineIndex);

    // Grows the loaded window by one chapter, called from onScrolled() as
    // the reader nears the bottom/top edge of what's currently loaded.
    void appendNextChapter();
    void prependPreviousChapter();
    void onScrolled();

    // spineIndex's chapter HTML (see EpubDocument::chapterHtml), with the
    // dark-mode color override and a "mnemosyne-chapter-N" boundary anchor
    // injected -- the fragment appendNextChapter()/prependPreviousChapter()/
    // loadWindowStartingAt() all insert into m_browser's document.
    QString chapterHtmlFragment(int spineIndex) const;

    // Which loaded chapter a given document block number falls within, and
    // that chapter's [startBlock, endBlock) range -- the block-number
    // bookkeeping that replaces "only one chapter is ever loaded" as the way
    // highlights/search/selection resolve which chapter they belong to.
    int chapterAtBlockNumber(int blockNumber) const;
    QPair<int, int> chapterBlockRange(int spineIndex) const;

    void updateNavigationState();
    void applyPageColors();
    void applyHighlightsToBrowser();
    void addHighlightForSelection();
    void addNoteForSelection();
    void showBrowserContextMenu(const QPoint &pos);
    void onVideoLinkActivated(const QUrl &url);
    void setFontZoomSteps(int steps);
    void restoreProgressAndCheckSync();
    void offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote);
    void scheduleProgressSave();
    void saveProgressNow();

    std::unique_ptr<EpubDocument> m_document;
    QString m_filePath;
    QString m_bookHash;
    int m_currentChapter = 0; // dominantly-visible chapter, tracked continuously while scrolling
    int m_loadedChapterStart = 0; // spine index range currently present in m_browser's document
    int m_loadedChapterEnd = 0;
    QHash<int, int> m_chapterStartBlock; // spine index -> QTextCursor::blockNumber() where it begins
    bool m_loadingAdjacentChapter = false; // re-entrancy guard: prepending adjusts the scrollbar itself
    bool m_darkMode = false;
    int m_fontZoomSteps = 0; // clamps how far zoomIn()/zoomOut() can go; Qt's QTextDocument keeps the actual zoomed font size across setHtml() calls on its own
    QVector<Highlight> m_highlights; // all highlights for this document
    QString m_searchTerm; // active search-dock query; empty means no search overlay

    QTextBrowser *m_browser = nullptr;
    QLabel *m_chapterLabel = nullptr;
    SyncPromptBar *m_syncPromptBar = nullptr;
    QTimer *m_progressSaveTimer = nullptr;
};
