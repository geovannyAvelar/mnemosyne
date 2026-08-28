#pragma once

#include "core/Highlight.h"
#include "core/ReaderView.h"
#include "txt/TxtDocument.h"

#include <QWidget>

#include <memory>

class QTextBrowser;
class QTimer;
class SyncPromptBar;
namespace ProgressSyncLog {
struct RemoteEntry;
}

// Plain text: mirrors MarkdownView (same QTextBrowser-based rendering and
// highlight/sync machinery, one flowing document, no chapters) but with no
// structure to derive a table of contents or per-section search results
// from — navigation and search both address the document by raw character
// offset instead of a heading index.
class TxtView : public QWidget, public IReaderView
{
    Q_OBJECT

public:
    explicit TxtView(std::unique_ptr<TxtDocument> document, QString filePath, QWidget *parent = nullptr);

    QString documentTitle() const override;
    QVector<TocNode> tableOfContents() const override { return {}; }
    void goToTocNode(const TocNode &node) override;
    int currentPosition() const override;
    QVector<SearchResult> search(const QString &query) const override;
    void setSearchTerm(const QString &term) override;
    void refreshHighlights() override;

    // Independent of any TxtView instance — see EpubView::searchFile() for
    // why this reopens the file rather than touching a live instance.
    static QVector<SearchResult> searchFile(const QString &filePath, const QString &query);

    void setDarkMode(bool enabled);

    bool hasPendingSyncPrompt() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void goToCharacterOffset(int offset);
    void zoomIn();
    void zoomOut();

signals:
    // Emitted whenever this view adds, edits, or removes a highlight/note,
    // so MainWindow can keep the Notes dock in sync without polling.
    void highlightsChanged();

private:
    void setupUi();
    void applyPageColors();
    void applyHighlightsToBrowser();
    void addHighlightForSelection();
    void addNoteForSelection();
    void showBrowserContextMenu(const QPoint &pos);
    void setFontZoomSteps(int steps);
    void restoreProgressAndCheckSync();
    void offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote);
    void scheduleProgressSave();
    void saveProgressNow();

    std::unique_ptr<TxtDocument> m_document;
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
