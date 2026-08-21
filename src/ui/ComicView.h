#pragma once

#include "comic/CbzDocument.h"
#include "core/ReaderView.h"

#include <QWidget>

#include <memory>

class QLabel;
class QScrollArea;
class QSpinBox;
class QTimer;
class PdfPageCanvas;
class SyncPromptBar;
namespace ProgressSyncLog {
struct RemoteEntry;
}

// CBZ comics are page images in a zip, with no text layer and (for now) no
// table of contents — structurally the paginated-raster half of PdfView's
// model (reuses PdfPageCanvas directly) minus everything PdfView has that's
// specifically about text: no selection, no highlights, no search.
class ComicView : public QWidget, public IReaderView
{
    Q_OBJECT

public:
    explicit ComicView(std::unique_ptr<CbzDocument> document, QString filePath, QWidget *parent = nullptr);

    QString documentTitle() const override;
    QVector<TocNode> tableOfContents() const override { return {}; }
    void goToTocNode(const TocNode &node) override;
    int currentPosition() const override;
    QVector<SearchResult> search(const QString &query) const override { Q_UNUSED(query); return {}; } // no text layer
    void setSearchTerm(const QString &term) override { Q_UNUSED(term); }

    bool hasPendingSyncPrompt() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void goToPage(int index); // 0-based
    void nextPage();
    void previousPage();
    void zoomIn();
    void zoomOut();

private:
    void setupUi();
    void renderCurrentPage();
    void updateNavigationState();
    void restoreProgressAndCheckSync();
    void offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote);
    void scheduleProgressSave();
    void saveProgressNow();

    std::unique_ptr<CbzDocument> m_document;
    QString m_filePath;
    QString m_bookHash;
    int m_currentPage = 0;
    bool m_pageTurnCooldown = false;
    qreal m_zoom = 1.0; // 1.0 == native page resolution, see CbzPage::renderToImage()

    PdfPageCanvas *m_canvas = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QSpinBox *m_pageSpinBox = nullptr;
    QLabel *m_pageCountLabel = nullptr;
    SyncPromptBar *m_syncPromptBar = nullptr;
    QTimer *m_progressSaveTimer = nullptr;
};
