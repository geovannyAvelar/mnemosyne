#pragma once

#include "app/BookMetadataClient.h" // BookMetadata
#include "app/HighlightExporter.h" // HighlightExporter::ExportEntry
#ifdef MNEMOSYNE_ENABLE_PLUGINS
#include "app/PluginHost.h" // PluginHost::PluginExporter
#endif
#include "core/Document.h"
#include "core/ReaderView.h" // SearchResult

#include <QFutureWatcher>
#include <QHash>
#include <QMainWindow>
#include <QPalette>
#include <QVector>

class BookInfoDock;
class IReaderView;
class LibraryView;
class NotesDock;
class SearchDock;
class TocDock;
class QAction;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QMenu;
class QMouseEvent;
class QTabWidget;
class QToolBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void openPath(const QString &filePath);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openFile();
    void showLibrary();
    void focusSearch();
    void closeCurrentTab();
    void setDarkModeEnabled(bool enabled);
    void chooseSyncFolder();
    void disableSync();
    void exportNotesAsMarkdown();
    void exportNotesAsAnki();
    void exportLibraryAsMarkdown();
    void exportLibraryAsAnki();
#ifdef MNEMOSYNE_ENABLE_PLUGINS
    void showPluginsDialog();
#endif
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    void signInWithGoogle();
    void signOutOfGoogle();
#endif
    void toggleSidebar();
    void toggleFullScreen();
    void showAbout();

private:
    void setupMenus();
    void setupDocks();
    void setupTabs();
    void setupSidebarToggle();
    void showSidebar();
    void populateOpenRecentMenu();
    void populateSyncMenu();
    void refreshNotesDock();
    void refreshBookInfoDock();
    QVector<HighlightExporter::ExportEntry> buildExportEntries() const;
    // Every RecentFiles entry with at least one highlight, most-recently-
    // opened first (same order RecentFiles::list() already returns) --
    // "library" here means the recents list, same as the Library tab's own
    // grid, not a filesystem scan for every book ever opened.
    QVector<HighlightExporter::BookExport> buildLibraryExportBooks() const;
#ifdef MNEMOSYNE_ENABLE_PLUGINS
    void populatePluginExportActions();
    void runPluginExporter(const PluginHost::PluginExporter &exporter);
    void populatePluginCommandActions();
    void runPluginCommand(const PluginHost::PluginCommand &command);
#endif
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    int findTabForFilePath(const QString &filePath) const;

    TocDock *m_tocDock = nullptr;
    NotesDock *m_notesDock = nullptr;
    SearchDock *m_searchDock = nullptr;
    BookInfoDock *m_bookInfoDock = nullptr;
    BookMetadataClient *m_bookMetadataClient = nullptr;
    QMenu *m_openRecentMenu = nullptr;
    QMenu *m_syncMenu = nullptr;
    QMenu *m_exportNotesMenu = nullptr;
#ifdef MNEMOSYNE_ENABLE_PLUGINS
    QMenu *m_pluginsMenu = nullptr;
    // Actions populatePluginExportActions()/populatePluginCommandActions()
    // add to m_exportNotesMenu/m_pluginsMenu (a leading separator plus one
    // per registered exporter/command) -- tracked so they can be removed
    // and rebuilt each time the menu opens, without touching the fixed
    // built-in actions also in those menus.
    QVector<QAction *> m_pluginExportActions;
    QVector<QAction *> m_pluginCommandActions;
#endif
    QAction *m_darkModeAction = nullptr;
    QAction *m_sidebarToggleAction = nullptr;
    QAction *m_fullScreenAction = nullptr;
    QAction *m_bookInfoLookupAction = nullptr;

    QTabWidget *m_tabWidget = nullptr;
    LibraryView *m_libraryView = nullptr; // always tab 0, not closable
    QHash<QWidget *, QString> m_tabFilePaths;
    QHash<QWidget *, QString> m_tabIsbn; // empty when the tab has no (or an unrecognized) ISBN
    QString m_currentIsbn; // active tab's ISBN; guards a stale async reply after switching tabs
    // EPUB/MOBI's own OPF/EXTH metadata, captured at open time -- shown in
    // BookInfoDock immediately, with no network access; default-constructed
    // (all fields empty) for every other format's tab.
    QHash<QWidget *, BookMetadata> m_tabLocalInfo;
    BookMetadata m_currentLocalInfo;

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

    // Set while minimized, to whether the window was maximized just before
    // that; see changeEvent()'s WindowStateChange handling.
    bool m_wasMaximizedBeforeMinimize = false;
    // Same idea, for fullscreen: set on entering fullscreen, to whether the
    // window was maximized just before that.
    bool m_wasMaximizedBeforeFullScreen = false;
};
