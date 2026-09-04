#pragma once

#include "core/Document.h"
#include "core/Highlight.h"
#include "core/ReaderView.h"

#include <QHash>
#include <QWidget>

#include <memory>

class QKeyEvent;
class QLabel;
class QScrollArea;
class QSpinBox;
class QTimer;
class PdfPageStackView;
class SyncPromptBar;
namespace ProgressSyncLog {
struct RemoteEntry;
}

class PdfView : public QWidget, public IReaderView
{
    Q_OBJECT

public:
    explicit PdfView(std::unique_ptr<IDocument> document, QString filePath, QWidget *parent = nullptr);

    QString documentTitle() const override;
    QVector<TocNode> tableOfContents() const override;
    void goToTocNode(const TocNode &node) override;
    int currentPosition() const override;
    QVector<SearchResult> search(const QString &query) const override;
    void setSearchTerm(const QString &term) override;
    void refreshHighlights() override;
    void flushProgress() override;

    // Same search as above, but independent of any PdfView instance: opens
    // its own Poppler document from filePath rather than touching a live
    // view's m_document. Poppler documents aren't safe to use concurrently
    // from multiple threads, so this is what MainWindow calls on a
    // background thread while the view (and its m_document, used by the
    // main thread for rendering) keeps running independently.
    static QVector<SearchResult> searchFile(const QString &filePath, const QString &query);

    QString selectedText() const;
    bool hasPendingSyncPrompt() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

public slots:
    void goToPage(int index); // 0-based
    void nextPage();
    void previousPage();
    void zoomIn();
    void zoomOut();
    void copySelection();
    void addHighlightForSelection();
    void addNoteForSelection();

signals:
    // Emitted whenever this view adds, edits, or removes a highlight/note,
    // so MainWindow can keep the Notes dock in sync without polling.
    void highlightsChanged();

private:
    void setupUi();

    // The topmost page substantially visible in the viewport right now —
    // this is what m_currentPage tracks continuously while scrolling.
    int topmostVisiblePage() const;
    void onScrolled();

    void setZoom(qreal newZoom);
    void updateNavigationState();
    void showCanvasContextMenu(const QPoint &globalPos, int pageIndex, const QPointF &pagePoint);
    int highlightIndexAtPagePoint(const QPointF &pagePoint, int pageIndex) const;
    void restoreProgressAndCheckSync();
    void offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote);
    void scheduleProgressSave();
    void saveProgressNow();

    std::unique_ptr<IDocument> m_document;
    QString m_filePath;
    QString m_bookHash;
    int m_currentPage = 0; // topmost substantially-visible page
    qreal m_zoom = 1.5;
    QVector<Highlight> m_highlights; // all highlights for this document
    QString m_searchTerm; // active search-dock query; empty means no search overlay

    PdfPageStackView *m_pageStackView = nullptr; // the scroll area's content widget
    QScrollArea *m_scrollArea = nullptr;
    QSpinBox *m_pageSpinBox = nullptr;
    QLabel *m_pageCountLabel = nullptr;
    SyncPromptBar *m_syncPromptBar = nullptr;
    QTimer *m_progressSaveTimer = nullptr;
};
