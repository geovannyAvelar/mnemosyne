#pragma once

#include "core/Highlight.h"
#include "core/ReaderView.h"
#include "markdown/MarkdownDocument.h"

#include <QWidget>

#include <memory>

class QTextBrowser;
class QTimer;
class SyncPromptBar;
namespace ProgressSyncLog {
struct RemoteEntry;
}

// Markdown files are a single flat document, unlike EPUB's chapter spine —
// this mirrors EpubView (same QTextBrowser-based rendering, highlight/sync
// machinery) but with no chapter navigation: goToTocNode()/currentPosition()
// work in terms of heading index instead of chapter index.
class MarkdownView : public QWidget, public IReaderView
{
    Q_OBJECT

public:
    explicit MarkdownView(std::unique_ptr<MarkdownDocument> document, QString filePath, QWidget *parent = nullptr);

    QString documentTitle() const override;
    QVector<TocNode> tableOfContents() const override;
    void goToTocNode(const TocNode &node) override;
    int currentPosition() const override;
    QVector<SearchResult> search(const QString &query) const override;
    void setSearchTerm(const QString &term) override;
    void refreshHighlights() override;

    // Independent of any MarkdownView instance — see EpubView::searchFile()
    // for why this reopens the file rather than touching a live instance.
    static QVector<SearchResult> searchFile(const QString &filePath, const QString &query);

    void setDarkMode(bool enabled);

    bool hasPendingSyncPrompt() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void goToHeadingIndex(int headingIndex); // -1 scrolls to the very top
    void zoomIn();
    void zoomOut();

signals:
    // Emitted whenever this view adds, edits, or removes a highlight/note,
    // so MainWindow can keep the Notes dock in sync without polling.
    void highlightsChanged();

private:
    void setupUi();
    void render();
    void applyPageColors();
    void applyHighlightsToBrowser();
    void addHighlightForSelection();
    void addNoteForSelection();
    void showBrowserContextMenu(const QPoint &pos);
    void setFontZoomSteps(int steps);
    int nearestHeadingIndexAtScrollTop() const;
    void restoreProgressAndCheckSync();
    void offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote);
    void scheduleProgressSave();
    void saveProgressNow();

    std::unique_ptr<MarkdownDocument> m_document;
    QString m_filePath;
    QString m_bookHash;
    bool m_darkMode = false;
    int m_fontZoomSteps = 0;
    QVector<Highlight> m_highlights;
    QString m_searchTerm;

    QTextBrowser *m_browser = nullptr;
    SyncPromptBar *m_syncPromptBar = nullptr;
    QTimer *m_progressSaveTimer = nullptr;
};
