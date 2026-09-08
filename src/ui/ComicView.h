#pragma once

#include "comic/CbzDocument.h"
#include "core/ReaderView.h"

#include <QWidget>

#include <memory>

class QLabel;
class QPushButton;
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
// specifically about text: no selection, no highlights, no search. Adds one
// thing PdfView doesn't need: a double-page spread mode (with a
// right-to-left/manga reading order option), since a comic's two-page
// layout is often meant to be seen as one continuous spread the way a
// prose book's single column never is.
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
    void refreshHighlights() override { } // no text layer, no highlights
    void flushProgress() override;

    bool hasPendingSyncPrompt() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void goToPage(int index); // 0-based -- snapped to its spread's starting index when doublePageMode is on
    void nextPage();
    void previousPage();
    void zoomIn();
    void zoomOut();
    void setDoublePageMode(bool enabled);
    void setRightToLeft(bool enabled);

private:
    void setupUi();
    // Renders m_currentPage alone, or (ComicReadingSettings::doublePageMode())
    // m_currentPage composed side by side with m_currentPage + 1 into one
    // QImage -- see the .cpp for the compose step. Falls back to a single
    // page when m_currentPage is the last page of an odd-paged comic.
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
    bool m_doublePage = false; // mirrors ComicReadingSettings::doublePageMode() at construction; live-toggled below
    bool m_rightToLeft = false; // ditto, ComicReadingSettings::rightToLeft()

    PdfPageCanvas *m_canvas = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QSpinBox *m_pageSpinBox = nullptr;
    QLabel *m_pageCountLabel = nullptr;
    QPushButton *m_doublePageButton = nullptr;
    QPushButton *m_rightToLeftButton = nullptr;
    SyncPromptBar *m_syncPromptBar = nullptr;
    QTimer *m_progressSaveTimer = nullptr;
};
