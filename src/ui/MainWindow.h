#pragma once

#include "core/Document.h"
#include "core/ReaderView.h" // SearchResult

#include <QFutureWatcher>
#include <QHash>
#include <QMainWindow>
#include <QPalette>
#include <QVector>

class BookmarksDock;
class IReaderView;
class LibraryView;
class SearchDock;
class TocDock;
class QAction;
class QMenu;
class QTabWidget;
class QToolBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void openPath(const QString &filePath);

private slots:
    void openFile();
    void showLibrary();
    void addBookmarkForCurrentPosition();
    void focusSearch();
    void closeCurrentTab();
    void setDarkModeEnabled(bool enabled);
    void chooseSyncFolder();
    void disableSync();
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    void signInWithGoogle();
    void signOutOfGoogle();
#endif
    void toggleSidebar();
    void showAbout();

private:
    void setupMenus();
    void setupDocks();
    void setupTabs();
    void setupSidebarToggle();
    void populateOpenRecentMenu();
    void populateSyncMenu();
    void refreshBookmarksDock();
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    int findTabForFilePath(const QString &filePath) const;

    TocDock *m_tocDock = nullptr;
    BookmarksDock *m_bookmarksDock = nullptr;
    SearchDock *m_searchDock = nullptr;
    QMenu *m_openRecentMenu = nullptr;
    QMenu *m_syncMenu = nullptr;
    QAction *m_addBookmarkAction = nullptr;
    QAction *m_darkModeAction = nullptr;
    QAction *m_sidebarToggleAction = nullptr;

    QTabWidget *m_tabWidget = nullptr;
    LibraryView *m_libraryView = nullptr; // always tab 0, not closable
    QHash<QWidget *, QString> m_tabFilePaths;

    IReaderView *m_currentView = nullptr; // active tab's view; nullptr when Library is active
    QString m_currentFilePath; // active tab's file path; empty when Library is active

    // Runs IReaderView::search() on the global QThreadPool so a slow scan
    // over a large document doesn't freeze the UI; see onSearchFinished()
    // for how a stale result (search finished after the user moved to a
    // different tab) is detected and discarded.
    QFutureWatcher<QVector<SearchResult>> *m_searchWatcher = nullptr;
    QString m_pendingSearchFilePath;
    QString m_pendingSearchQuery;

    QPalette m_lightPalette;
    QPalette m_darkPalette;
};
