#pragma once

#include "core/Highlight.h"
#include "core/ReaderView.h"
#include "epub/EpubDocument.h"

#include <QWidget>

#include <memory>

class QLabel;
class QTextBrowser;
class QTimer;
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

private:
    void setupUi();
    void renderCurrentChapter();
    void updateNavigationState();
    void applyPageColors();
    void applyHighlightsToBrowser();
    void addHighlightForSelection();
    void showBrowserContextMenu(const QPoint &pos);
    void setFontZoomSteps(int steps);
    void restoreProgressAndCheckSync();
    void offerSyncedPosition(const ProgressSyncLog::RemoteEntry &remote);
    void scheduleProgressSave();
    void saveProgressNow();

    std::unique_ptr<EpubDocument> m_document;
    QString m_filePath;
    QString m_bookHash;
    int m_currentChapter = 0;
    bool m_darkMode = false;
    bool m_pageTurnCooldown = false; // guards against one fast scroll gesture skipping multiple chapters
    int m_fontZoomSteps = 0; // clamps how far zoomIn()/zoomOut() can go; Qt's QTextDocument keeps the actual zoomed font size across setHtml() calls on its own
    QVector<Highlight> m_highlights; // all highlights for this document
    QString m_searchTerm; // active search-dock query; empty means no search overlay

    QTextBrowser *m_browser = nullptr;
    QLabel *m_chapterLabel = nullptr;
    SyncPromptBar *m_syncPromptBar = nullptr;
    QTimer *m_progressSaveTimer = nullptr;
};
