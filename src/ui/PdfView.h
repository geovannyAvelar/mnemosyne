#pragma once

#include "core/Document.h"
#include "core/Highlight.h"
#include "core/ReaderView.h"

#include <QWidget>

#include <memory>

class QKeyEvent;
class QLabel;
class QScrollArea;
class QSpinBox;
class QTimer;
class PdfPageCanvas;
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

    // Same search as above, but independent of any PdfView instance: opens
    // its own Poppler document from filePath rather than touching a live
    // view's m_document. Poppler documents aren't safe to use concurrently
    // from multiple threads, so this is what MainWindow calls on a
    // background thread while the view (and its m_document, used by the
    // main thread for rendering) keeps running independently.
    static QVector<SearchResult> searchFile(const QString &filePath, const QString &query);

    QString selectedText() const { return m_selectedText; }
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
    void renderCurrentPage();
    void updateNavigationState();
    void updateSelectionFromDrag();
    void showCanvasContextMenu(const QPoint &pos);
    void refreshHighlightOverlay();
    void refreshSearchOverlay();
    int highlightIndexAtPagePoint(const QPointF &pagePoint) const;
    void restoreProgressAndCheckSync();
    void offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote);
    void scheduleProgressSave();
    void saveProgressNow();
    void turnToNextPageAndResumeAtTop();
    void turnToPreviousPageAndResumeAtBottom();

    std::unique_ptr<IDocument> m_document;
    QString m_filePath;
    QString m_bookHash;
    int m_currentPage = 0;
    bool m_pageTurnCooldown = false; // guards against one fast scroll gesture skipping multiple pages
    qreal m_zoom = 1.5;
    QString m_selectedText;
    // Bounding rect (page points) of the word-snapped selection shown live in
    // updateSelectionFromDrag() -- addHighlightForSelection() persists this,
    // not the raw drag rect, so a saved highlight covers exactly what the
    // live blue overlay showed rather than whatever pixels the mouse
    // actually started/ended on mid-character.
    QRectF m_selectedBoundingPageRect;
    QVector<TextWord> m_currentPageWords; // words on m_currentPage, in reading order
    QVector<Highlight> m_highlights; // all highlights for this document
    QString m_searchTerm; // active search-dock query; empty means no search overlay

    PdfPageCanvas *m_canvas = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QSpinBox *m_pageSpinBox = nullptr;
    QLabel *m_pageCountLabel = nullptr;
    SyncPromptBar *m_syncPromptBar = nullptr;
    QTimer *m_progressSaveTimer = nullptr;
};
