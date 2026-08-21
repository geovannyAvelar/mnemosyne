#pragma once

#include "core/Highlight.h"
#include "core/ReaderView.h"
#include "mobi/MobiDocument.h"

#include <QWidget>

#include <memory>

class QLabel;
class QTextBrowser;
class QTimer;
class SyncPromptBar;
namespace ProgressSyncLog {
struct RemoteEntry;
}

// MOBI/AZW/AZW3 reading — structurally the same shape as EpubView (a
// document reconstructs into chapter-like "parts" with an NCX-derived table
// of contents targeting them by index), so this mirrors EpubView almost
// exactly, with "part" standing in for "chapter"/"spine index".
class MobiView : public QWidget, public IReaderView
{
    Q_OBJECT

public:
    explicit MobiView(std::unique_ptr<MobiDocument> document, QString filePath, QWidget *parent = nullptr);

    QString documentTitle() const override;
    QVector<TocNode> tableOfContents() const override;
    void goToTocNode(const TocNode &node) override;
    int currentPosition() const override;
    QVector<SearchResult> search(const QString &query) const override;
    void setSearchTerm(const QString &term) override;

    // Independent of any MobiView instance — see EpubView::searchFile() for
    // why this reopens the file rather than touching a live instance.
    static QVector<SearchResult> searchFile(const QString &filePath, const QString &query);

    void setDarkMode(bool enabled);

    bool hasPendingSyncPrompt() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void goToPart(int partIndex);
    void nextPart();
    void previousPart();
    void zoomIn();
    void zoomOut();

private:
    void setupUi();
    void renderCurrentPart();
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

    std::unique_ptr<MobiDocument> m_document;
    QString m_filePath;
    QString m_bookHash;
    int m_currentPart = 0;
    bool m_darkMode = false;
    bool m_pageTurnCooldown = false;
    int m_fontZoomSteps = 0;
    QVector<Highlight> m_highlights;
    QString m_searchTerm;

    QTextBrowser *m_browser = nullptr;
    QLabel *m_partLabel = nullptr;
    SyncPromptBar *m_syncPromptBar = nullptr;
    QTimer *m_progressSaveTimer = nullptr;
};
