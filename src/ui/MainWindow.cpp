#include "MainWindow.h"

#include "app/BookMetadataClient.h"
#include "app/FileIdentity.h"
#include "app/HighlightExporter.h"
#include "app/HighlightStore.h"
#include "app/RecentFiles.h"
#include "app/SyncFolder.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleAuth.h"
#endif
#include "comic/CbzDocument.h"
#include "core/Highlight.h"
#include "core/ReaderView.h"
#include "epub/EpubDocument.h"
#include "markdown/MarkdownDocument.h"
#include "mobi/MobiDocument.h"
#include "txt/TxtDocument.h"
#include "ui/BookInfoDock.h"
#include "ui/ComicView.h"
#include "ui/EpubView.h"
#ifdef MNEMOSYNE_ENABLE_HTML
#include "ui/HtmlView.h"
#endif
#include "ui/LibraryView.h"
#include "ui/MarkdownView.h"
#include "ui/MobiView.h"
#include "ui/NoteDialog.h"
#include "ui/NotesDock.h"
#ifdef MNEMOSYNE_ENABLE_PLUGINS
#include "ui/PluginFormDialog.h"
#include "ui/PluginsDialog.h"
#endif
#include "ui/PdfView.h"
#ifdef Q_OS_MACOS
#include "platform/MacTouchBar.h"
#endif
#include "ui/SearchDock.h"
#include "ui/Theme.h"
#include "ui/TocDock.h"
#include "ui/TopBar.h"
#include "ui/TxtView.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonValue>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QRegularExpression>
#include <QSettings>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QWindow>
#include <QWindowStateChangeEvent>
#include <QtConcurrent/QtConcurrentRun>

#include <optional>

namespace {
// Title alone doesn't count: it's virtually always present (falls back to
// the filename) and already duplicates the tab/window title, so it's not
// worth a dedicated sidebar tab on its own -- this only reports true when
// there's something the tab title doesn't already show.
bool bookMetadataHasContent(const BookMetadata &info)
{
    return !info.authors.isEmpty() || !info.publisher.isEmpty() || !info.publishDate.isEmpty()
        || !info.description.isEmpty() || !info.cover.isNull();
}

// A suggested export filename derived from the book's title -- strips
// characters that are invalid (or awkward) in a filename on any of the
// three desktop platforms.
QString sanitizedForFilename(const QString &title)
{
    QString result = title;
    result.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    return result.trimmed().isEmpty() ? QStringLiteral("Untitled") : result.trimmed();
}

// Same wording as buildExportEntries()'s per-view dynamic_cast, keyed by
// RecentFiles::Entry::format (a raw lowercased file suffix, see
// MainWindow::openPath) instead of a live view instance -- library export
// covers books that aren't necessarily open in a tab.
QString positionLabelForFormat(const QString &format, int targetIndex)
{
    if (format == QLatin1String("pdf")) {
        return MainWindow::tr("Page %1").arg(targetIndex + 1);
    }
    if (format == QLatin1String("epub")) {
        return MainWindow::tr("Chapter %1").arg(targetIndex + 1);
    }
    if (format == QLatin1String("mobi") || format == QLatin1String("azw") || format == QLatin1String("azw3")) {
        return MainWindow::tr("Part %1").arg(targetIndex + 1);
    }
    return QString(); // markdown/txt (no discrete unit), cbz/html (no highlights at all)
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_lightPalette(Theme::lightPalette())
    , m_darkPalette(Theme::darkPalette())
{
    setWindowTitle(tr("Mnemosyne"));
    setAcceptDrops(true);

#ifdef MNEMOSYNE_ENABLE_PLUGINS
    // All before any document can be opened, so hooks/showMessage()/
    // showForm() are live for a plugin from the very first thing it can do.
    PluginHost::setMessageHandler(
        [this](const QString &text) { QMessageBox::information(this, tr("Plugin"), text); });
    PluginHost::setFormHandler(
        [this](const QJsonValue &schema) { return PluginFormDialog::show(this, schema); });
    PluginHost::reload();
#endif

    setupDocks();
    setupTabs();
    setupSidebarToggle();
    setupMenus();

    const bool savedDarkMode = QSettings().value(QStringLiteral("darkMode"), false).toBool();
    m_darkModeAction->setChecked(savedDarkMode);
    setDarkModeEnabled(savedDarkMode); // setChecked() only emits toggled() on a change, so apply explicitly

    // Off by default: this toggle gates the app's only network call
    // unrelated to Drive sync (see BookMetadataClient), so it should never
    // fire without the user having explicitly turned it on.
    const bool savedBookInfoLookup = QSettings().value(QStringLiteral("bookInfoLookupEnabled"), false).toBool();
    m_bookInfoLookupAction->setChecked(savedBookInfoLookup);

    // The docks setupDocks() just created start out visible, so only act
    // when the saved state actually disagrees with that default. Deferred
    // to the next event-loop iteration (rather than called here directly):
    // QMainWindowLayout re-asserts its own default dock visibility as part
    // of the main window's first show() (main.cpp's showMaximized(), which
    // hasn't happened yet at this point in the constructor), silently
    // undoing a hide() called before then. Running after that first show
    // cycle has completed avoids the race.
    QTimer::singleShot(0, this, [this] {
        const bool savedSidebarVisible = QSettings().value(QStringLiteral("sidebarVisible"), true).toBool();
        if (!savedSidebarVisible) {
            toggleSidebar();
        }
    });
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    if (event->type() != QEvent::WindowStateChange) {
        return;
    }

    // Keeps the View menu's checkmark correct regardless of how fullscreen
    // was entered/exited -- this action, F11, the macOS traffic-light zoom
    // button, or the OS itself (e.g. a native fullscreen gesture).
    m_fullScreenAction->setChecked(windowState() & Qt::WindowFullScreen);

    const Qt::WindowStates oldState = static_cast<QWindowStateChangeEvent *>(event)->oldState();

    if (windowState() & Qt::WindowMinimized) {
        m_wasMaximizedBeforeMinimize = oldState & Qt::WindowMaximized;
        return;
    }

    // Some Linux window managers drop the maximized flag on restore from
    // minimized instead of preserving it; when that happens, QMainWindow
    // falls back to its last "normal" geometry, which for this window was
    // never meaningfully set (see main.cpp's resize(1024, 768) fallback
    // right before showMaximized() at launch) -- so the window snaps back to
    // that small, centered size instead of staying maximized. Put it back.
    if (m_wasMaximizedBeforeMinimize && !(windowState() & Qt::WindowMaximized)) {
        m_wasMaximizedBeforeMinimize = false;
        showMaximized();
    }

    // Same window manager quirk, different trigger: entering fullscreen
    // un-maximizes the real window too (confirmed live on GNOME/Mutter --
    // toggleFullScreen()'s XOR alone doesn't survive the round trip), and
    // the maximized flag isn't restored on the way back out either. Track
    // it across the transition and put it back here, same as above.
    if (!(oldState & Qt::WindowFullScreen) && (windowState() & Qt::WindowFullScreen)) {
        m_wasMaximizedBeforeFullScreen = oldState & Qt::WindowMaximized;
    } else if (m_wasMaximizedBeforeFullScreen && (oldState & Qt::WindowFullScreen)
               && !(windowState() & (Qt::WindowFullScreen | Qt::WindowMaximized))) {
        m_wasMaximizedBeforeFullScreen = false;
        showMaximized();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    // openPath() already shows its own warning for a URL that isn't a file
    // this app recognizes (or exist at all), same as picking one via
    // File > Open, so nothing here filters by extension first.
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            openPath(url.toLocalFile());
        }
    }
    event->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Each view debounces its own progress saves (see e.g. PdfView::
    // scheduleProgressSave), so a page/scroll change made just before quit
    // can still be sitting in that debounce window when the app exits.
    // Flush every open tab now so "resume where I left off" always reflects
    // the true last state, not whatever was saved 1.5s before it.
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (auto *view = dynamic_cast<IReaderView *>(m_tabWidget->widget(i))) {
            view->flushProgress();
        }
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::setupTabs()
{
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    setCentralWidget(m_tabWidget);

    m_libraryView = new LibraryView(m_tabWidget);
    connect(m_libraryView, &LibraryView::fileActivated, this, &MainWindow::openPath);
    connect(m_libraryView, &LibraryView::openRequested, this, &MainWindow::openFile);
    const int libraryIndex = m_tabWidget->addTab(m_libraryView, tr("Library"));
    // The Library tab can't be closed; the close button's side is platform-style-dependent.
    if (QWidget *closeButton = m_tabWidget->tabBar()->tabButton(libraryIndex, QTabBar::LeftSide)) {
        closeButton->hide();
    }
    if (QWidget *closeButton = m_tabWidget->tabBar()->tabButton(libraryIndex, QTabBar::RightSide)) {
        closeButton->hide();
    }

    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
}

void MainWindow::setupDocks()
{
    setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);

    m_tocDock = new TocDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_tocDock);

    m_notesDock = new NotesDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_notesDock);
    tabifyDockWidget(m_tocDock, m_notesDock);

    m_searchDock = new SearchDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_searchDock);
    tabifyDockWidget(m_tocDock, m_searchDock);

    m_bookInfoDock = new BookInfoDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_bookInfoDock);
    tabifyDockWidget(m_tocDock, m_bookInfoDock);

    m_searchWatcher = new QFutureWatcher<QVector<SearchResult>>(this);

    m_bookMetadataClient = new BookMetadataClient(this);
    connect(m_bookMetadataClient, &BookMetadataClient::metadataReady, this,
            [this](const QString &isbn, const BookMetadata &apiInfo) {
                if (isbn != m_currentIsbn) {
                    return; // stale: user moved on before this arrived
                }
                if (m_tocDock->isHidden()) {
                    return; // user hid the sidebar while this was in flight; don't pop it back open
                }
                // The API enhances the local metadata rather than replacing
                // it outright: a field it didn't return (e.g. a description
                // when there were no "excerpts") keeps its local value.
                BookMetadata merged = m_currentLocalInfo;
                if (!apiInfo.title.isEmpty()) {
                    merged.title = apiInfo.title;
                }
                if (!apiInfo.authors.isEmpty()) {
                    merged.authors = apiInfo.authors;
                }
                if (!apiInfo.publisher.isEmpty()) {
                    merged.publisher = apiInfo.publisher;
                }
                if (!apiInfo.publishDate.isEmpty()) {
                    merged.publishDate = apiInfo.publishDate;
                }
                if (!apiInfo.description.isEmpty()) {
                    merged.description = apiInfo.description;
                }
                if (!apiInfo.cover.isNull()) {
                    merged.cover = apiInfo.cover;
                }
                m_bookInfoDock->setMetadata(merged);
            });
    connect(m_bookMetadataClient, &BookMetadataClient::lookupFailed, this,
            [this](const QString &isbn, const QString &reason) {
                if (isbn != m_currentIsbn) {
                    return;
                }
                if (m_tocDock->isHidden()) {
                    return; // user hid the sidebar while this was in flight
                }
                // Only surface the failure if there was nothing local to
                // fall back on -- otherwise leave the local info in place;
                // the API was just an enhancement attempt that came up short.
                if (!bookMetadataHasContent(m_currentLocalInfo)) {
                    m_bookInfoDock->setUnavailable(reason);
                }
            });

    // The tab strip above (from setTabPosition) already labels each dock, so
    // each dock's own title bar would just be a redundant duplicate label.
    m_tocDock->setTitleBarWidget(new QWidget(m_tocDock));
    m_notesDock->setTitleBarWidget(new QWidget(m_notesDock));
    m_searchDock->setTitleBarWidget(new QWidget(m_searchDock));
    m_bookInfoDock->setTitleBarWidget(new QWidget(m_bookInfoDock));

    m_tocDock->raise(); // Contents is the more useful default tab on opening a book

    connect(m_tocDock, &TocDock::nodeActivated, this, [this](const TocNode &node) {
        if (m_currentView) {
            m_currentView->goToTocNode(node);
        }
    });

    connect(m_notesDock, &NotesDock::noteActivated, this, [this](int targetIndex) {
        if (m_currentView) {
            TocNode node;
            node.pageNumber = targetIndex;
            m_currentView->goToTocNode(node);
        }
    });

    connect(m_notesDock, &NotesDock::editNoteRequested, this, [this](int index) {
        if (m_currentFilePath.isEmpty()) {
            return;
        }
        const QString bookHash = FileIdentity::contentHash(m_currentFilePath);
        const QVector<Highlight> highlights = HighlightStore::highlightsFor(bookHash);
        if (index < 0 || index >= highlights.size()) {
            return;
        }
        const std::optional<NoteDialog::Result> result = NoteDialog::show(this, highlights[index].note, highlights[index].color);
        if (!result) {
            return;
        }
        HighlightStore::setNote(bookHash, index, result->note);
        HighlightStore::setColor(bookHash, index, result->color);
        refreshNotesDock();
        if (m_currentView) {
            m_currentView->refreshHighlights();
        }
    });

    connect(m_notesDock, &NotesDock::removeNoteRequested, this, [this](int index) {
        if (m_currentFilePath.isEmpty()) {
            return;
        }
        HighlightStore::removeHighlight(FileIdentity::contentHash(m_currentFilePath), index);
        refreshNotesDock();
        if (m_currentView) {
            m_currentView->refreshHighlights();
        }
    });

    connect(m_searchDock, &SearchDock::searchRequested, this, [this](const QString &query) {
        if (!m_currentView) {
            m_searchDock->setResults({});
            return;
        }

        // The actual scan runs on the global QThreadPool via the format's
        // searchFile() (see PdfView/EpubView), which opens its own document
        // handle from disk instead of touching m_currentView's — Poppler and
        // libzip aren't safe to use concurrently with the main thread's
        // rendering of the live document. Captured by value so the task has
        // no dependency on this (or any widget) still being alive when it runs.
        const QString filePath = m_currentFilePath;
        const QString suffix = QFileInfo(filePath).suffix().toLower();

        m_pendingSearchFilePath = filePath;
        m_pendingSearchQuery = query;
        m_searchDock->setSearching(true);

        m_searchWatcher->setFuture(QtConcurrent::run([filePath, suffix, query]() -> QVector<SearchResult> {
            if (suffix == QLatin1String("pdf")) {
                return PdfView::searchFile(filePath, query);
            }
            if (suffix == QLatin1String("epub")) {
                return EpubView::searchFile(filePath, query);
            }
            if (suffix == QLatin1String("md") || suffix == QLatin1String("markdown")) {
                return MarkdownView::searchFile(filePath, query);
            }
            if (suffix == QLatin1String("mobi") || suffix == QLatin1String("azw") || suffix == QLatin1String("azw3")) {
                return MobiView::searchFile(filePath, query);
            }
            if (suffix == QLatin1String("txt")) {
                return TxtView::searchFile(filePath, query);
            }
            return {}; // HTML/CBZ: no text layer, so find-in-page search isn't supported
        }));
    });

    connect(m_searchWatcher, &QFutureWatcher<QVector<SearchResult>>::finished, this, [this] {
        m_searchDock->setSearching(false);
        const QVector<SearchResult> results = m_searchWatcher->result();

        // Discard a result that's no longer relevant: the user switched
        // documents (or closed the tab) while the background scan was running.
        if (m_pendingSearchFilePath != m_currentFilePath || !m_currentView) {
            return;
        }

        m_searchDock->setResults(results);
        m_currentView->setSearchTerm(m_pendingSearchQuery);

        if (!results.isEmpty()) {
            TocNode node;
            node.pageNumber = results.first().targetIndex;
            m_currentView->goToTocNode(node);
        }
    });

    connect(m_searchDock, &SearchDock::resultActivated, this, [this](int targetIndex) {
        if (m_currentView) {
            TocNode node;
            node.pageNumber = targetIndex;
            m_currentView->goToTocNode(node);
        }
    });
}

void MainWindow::setupSidebarToggle()
{
    auto *topBar = new TopBar(tr("Window"), this);
    topBar->setObjectName(QStringLiteral("windowTopBar"));
    topBar->setMovable(false);
    topBar->setFloatable(false);
    topBar->setIconSize(QSize(18, 18));
    topBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    addToolBar(Qt::TopToolBarArea, topBar);

    m_sidebarToggleAction = topBar->addAction(Theme::sidebarToggleIcon(), QString());
    m_sidebarToggleAction->setToolTip(tr("Hide Sidebar"));
    connect(m_sidebarToggleAction, &QAction::triggered, this, &MainWindow::toggleSidebar);

    QAction *searchAction = topBar->addAction(Theme::searchIcon(), QString());
    searchAction->setToolTip(tr("Search"));
    connect(searchAction, &QAction::triggered, this, &MainWindow::focusSearch);

    auto *spacer = new QWidget(topBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // Without this, this widget -- not nullptr -- is what childAt() finds
    // under the cursor across most of the bar's width, so
    // TopBar::mousePressEvent() never sees "empty background" and the
    // window-drag path never fires.
    spacer->setAttribute(Qt::WA_TransparentForMouseEvents);
    topBar->addWidget(spacer);
}

void MainWindow::toggleSidebar()
{
    // Not isVisible(): that also depends on the whole ancestor chain (this
    // dock, MainWindow itself) actually being shown on screen. The
    // constructor calls this to restore a saved "hidden" state before
    // MainWindow has been shown at all, at which point isVisible() is
    // always false regardless of the dock's own state — making this always
    // take the "currently hidden" branch and show the sidebar instead of
    // hiding it. isHidden() reflects just the dock's own explicit
    // show()/hide() state, independent of the window (see EpubView::
    // hasPendingSyncPrompt() for the same distinction documented before).
    const bool currentlyVisible = !m_tocDock->isHidden() || !m_notesDock->isHidden() || !m_searchDock->isHidden();

    if (currentlyVisible) {
        setFocus();
        m_tocDock->hide();
        m_notesDock->hide();
        m_searchDock->hide();
        m_bookInfoDock->hide(); // regardless of content -- refreshBookInfoDock() reconciles on show
        QSettings().setValue(QStringLiteral("sidebarVisible"), false);
        m_sidebarToggleAction->setToolTip(tr("Show Sidebar"));
    } else {
        showSidebar();
    }
}

void MainWindow::toggleFullScreen()
{
    // XOR, not showFullScreen()/showNormal(): those two reset every other
    // state bit, so leaving fullscreen would drop back to a plain (not
    // maximized) window even if that's what it was before. XOR flips just
    // the fullscreen bit and leaves Maximized (or any other bit) alone.
    setWindowState(windowState() ^ Qt::WindowFullScreen);
}

// Shared by toggleSidebar()'s show branch and focusSearch(): anywhere the
// sidebar gets shown must keep the persisted "sidebarVisible" state and the
// toggle button's tooltip in sync, or a restart re-hides a sidebar the user
// last saw open (see focusSearch(), which used to show the docks directly
// without going through here — the bug that prompted this refactor).
void MainWindow::showSidebar()
{
    m_tocDock->show();
    m_notesDock->show();
    m_searchDock->show();
    m_tocDock->raise();
    // If the dock group was last hidden while a dock other than m_tocDock
    // was the raised/active tab (e.g. via focusSearch()), Qt can fail to
    // restore this area's width on show() and leave it collapsed to 0 —
    // the docks are then technically "visible" but occupy no space. Force a
    // sane width explicitly rather than relying on Qt to infer one. 300,
    // not 260: with Book Info now a fourth tab in this group, 260 elides
    // the tab bar's labels down to "B…"/"S…"/"B…" and scrolls "Contents"
    // out of view entirely.
    resizeDocks({m_tocDock}, {300}, Qt::Horizontal);
    QSettings().setValue(QStringLiteral("sidebarVisible"), true);
    m_sidebarToggleAction->setToolTip(tr("Hide Sidebar"));

    // Book Info was left hidden by toggleSidebar() regardless of whether it
    // had content (see there); recompute for the current tab now that the
    // group is visible again, rather than force it open unconditionally.
    refreshBookInfoDock();
}

void MainWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *openAction = fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    m_openRecentMenu = fileMenu->addMenu(tr("Open &Recent"));
    connect(m_openRecentMenu, &QMenu::aboutToShow, this, &MainWindow::populateOpenRecentMenu);

    fileMenu->addSeparator();
    QAction *libraryAction = fileMenu->addAction(tr("&Library"));
    libraryAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(libraryAction, &QAction::triggered, this, &MainWindow::showLibrary);

    QAction *closeTabAction = fileMenu->addAction(tr("&Close Tab"));
    closeTabAction->setShortcut(QKeySequence::Close);
    connect(closeTabAction, &QAction::triggered, this, &MainWindow::closeCurrentTab);

    m_exportNotesMenu = fileMenu->addMenu(tr("Export &Notes"));
    QAction *exportMarkdownAction = m_exportNotesMenu->addAction(tr("as &Markdown..."));
    connect(exportMarkdownAction, &QAction::triggered, this, &MainWindow::exportNotesAsMarkdown);
    QAction *exportAnkiAction = m_exportNotesMenu->addAction(tr("as &Anki Cards (TSV)..."));
    connect(exportAnkiAction, &QAction::triggered, this, &MainWindow::exportNotesAsAnki);
    m_exportNotesMenu->addSeparator();
    QAction *exportLibraryMarkdownAction = m_exportNotesMenu->addAction(tr("Export &Library as Markdown..."));
    connect(exportLibraryMarkdownAction, &QAction::triggered, this, &MainWindow::exportLibraryAsMarkdown);
    QAction *exportLibraryAnkiAction = m_exportNotesMenu->addAction(tr("Export Library as Anki Cards (&TSV)..."));
    connect(exportLibraryAnkiAction, &QAction::triggered, this, &MainWindow::exportLibraryAsAnki);
#ifdef MNEMOSYNE_ENABLE_PLUGINS
    connect(m_exportNotesMenu, &QMenu::aboutToShow, this, &MainWindow::populatePluginExportActions);
#endif

    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction *findAction = editMenu->addAction(tr("&Find..."));
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &MainWindow::focusSearch);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_tocDock->toggleViewAction());
    viewMenu->addAction(m_notesDock->toggleViewAction());
    viewMenu->addAction(m_searchDock->toggleViewAction());
    viewMenu->addAction(m_bookInfoDock->toggleViewAction());
    viewMenu->addSeparator();

    m_darkModeAction = viewMenu->addAction(tr("&Dark Mode"));
    m_darkModeAction->setCheckable(true);
    connect(m_darkModeAction, &QAction::toggled, this, &MainWindow::setDarkModeEnabled);

    // Off by default -- see BookMetadataClient's doc comment for why this
    // needs to be an explicit opt-in rather than automatic.
    m_bookInfoLookupAction = viewMenu->addAction(tr("Look Up Book &Info Online"));
    m_bookInfoLookupAction->setCheckable(true);
    connect(m_bookInfoLookupAction, &QAction::toggled, this, [this](bool enabled) {
        QSettings().setValue(QStringLiteral("bookInfoLookupEnabled"), enabled);
        refreshBookInfoDock();
    });

    viewMenu->addSeparator();
    m_fullScreenAction = viewMenu->addAction(tr("&Full Screen"));
    m_fullScreenAction->setCheckable(true);
    m_fullScreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    // Not tied to toggled(): changeEvent() is the single source of truth for
    // this checkbox, so it stays correct no matter how fullscreen was
    // entered/exited (this action, F11, the macOS traffic-light zoom button,
    // or the OS itself) instead of just the paths that go through here.
    connect(m_fullScreenAction, &QAction::triggered, this, &MainWindow::toggleFullScreen);

    m_syncMenu = menuBar()->addMenu(tr("&Sync"));
    connect(m_syncMenu, &QMenu::aboutToShow, this, &MainWindow::populateSyncMenu);
    // Populate synchronously too: macOS's native menu bar hides a top-level
    // menu that has zero actions, so leaving this empty until aboutToShow
    // fires would make it un-clickable there and hide Google Drive sync
    // entirely. aboutToShow still re-runs this on every open to keep the
    // status text (folder path, signed-in account) fresh.
    populateSyncMenu();

#ifdef MNEMOSYNE_ENABLE_PLUGINS
    m_pluginsMenu = menuBar()->addMenu(tr("&Plugins"));
    QAction *managePluginsAction = m_pluginsMenu->addAction(tr("&Manage Plugins..."));
    connect(managePluginsAction, &QAction::triggered, this, &MainWindow::showPluginsDialog);
    connect(m_pluginsMenu, &QMenu::aboutToShow, this, &MainWindow::populatePluginCommandActions);
#endif

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(tr("&About Mnemosyne"));
    aboutAction->setMenuRole(QAction::AboutRole);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("About Mnemosyne"),
        tr("<h3>Mnemosyne</h3>"
           "<p>Version %1</p>"
           "<p>A PDF, EPUB, HTML, Markdown, MOBI/AZW, CBZ comic, and plain text reader.</p>")
            .arg(QStringLiteral(MNEMOSYNE_VERSION)));
}

void MainWindow::openFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open Document"), QString(),
#ifdef MNEMOSYNE_ENABLE_HTML
        tr("Documents (*.pdf *.epub *.html *.htm *.md *.markdown *.mobi *.azw *.azw3 *.cbz *.txt)"));
#else
        tr("Documents (*.pdf *.epub *.md *.markdown *.mobi *.azw *.azw3 *.cbz *.txt)"));
#endif

    if (filePath.isEmpty()) {
        return;
    }
    openPath(filePath);
}

void MainWindow::showLibrary()
{
    m_tabWidget->setCurrentWidget(m_libraryView);
}

void MainWindow::closeCurrentTab()
{
    onTabCloseRequested(m_tabWidget->currentIndex());
}

int MainWindow::findTabForFilePath(const QString &filePath) const
{
    for (auto it = m_tabFilePaths.constBegin(); it != m_tabFilePaths.constEnd(); ++it) {
        if (it.value() == filePath) {
            return m_tabWidget->indexOf(it.key());
        }
    }
    return -1;
}

void MainWindow::openPath(const QString &filePath)
{
    const int existingIndex = findTabForFilePath(filePath);
    if (existingIndex >= 0) {
        m_tabWidget->setCurrentIndex(existingIndex);
        return;
    }

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    QString errorMessage;
    QWidget *widget = nullptr;
    IReaderView *view = nullptr;
    QString isbn; // only EPUB/MOBI carry one; see BookMetadataClient
    BookMetadata localInfo; // ditto: EPUB/MOBI's own OPF/EXTH metadata

    if (suffix == QLatin1String("pdf")) {
        std::unique_ptr<IDocument> document = openDocument(filePath, &errorMessage);
        if (document) {
            auto *pdfView = new PdfView(std::move(document), filePath, m_tabWidget);
            connect(pdfView, &PdfView::highlightsChanged, this, &MainWindow::refreshNotesDock);
            widget = pdfView;
            view = pdfView;
        }
    } else if (suffix == QLatin1String("epub")) {
        std::unique_ptr<EpubDocument> document = EpubDocument::load(filePath, &errorMessage);
        if (document) {
            isbn = document->isbn();
            localInfo.title = document->title();
            localInfo.authors = document->authors();
            localInfo.publisher = document->publisher();
            localInfo.description = document->description();
            localInfo.cover = document->cover();
            auto *epubView = new EpubView(std::move(document), filePath, m_tabWidget);
            epubView->setDarkMode(m_darkModeAction->isChecked());
            connect(epubView, &EpubView::highlightsChanged, this, &MainWindow::refreshNotesDock);
            widget = epubView;
            view = epubView;
        }
    } else if (suffix == QLatin1String("md") || suffix == QLatin1String("markdown")) {
        std::unique_ptr<MarkdownDocument> document = MarkdownDocument::load(filePath, &errorMessage);
        if (document) {
            auto *markdownView = new MarkdownView(std::move(document), filePath, m_tabWidget);
            markdownView->setDarkMode(m_darkModeAction->isChecked());
            connect(markdownView, &MarkdownView::highlightsChanged, this, &MainWindow::refreshNotesDock);
            widget = markdownView;
            view = markdownView;
        }
    } else if (suffix == QLatin1String("txt")) {
        std::unique_ptr<TxtDocument> document = TxtDocument::load(filePath, &errorMessage);
        if (document) {
            auto *txtView = new TxtView(std::move(document), filePath, m_tabWidget);
            txtView->setDarkMode(m_darkModeAction->isChecked());
            connect(txtView, &TxtView::highlightsChanged, this, &MainWindow::refreshNotesDock);
            widget = txtView;
            view = txtView;
        }
    } else if (suffix == QLatin1String("cbz")) {
        std::unique_ptr<CbzDocument> document = CbzDocument::load(filePath, &errorMessage);
        if (document) {
            auto *comicView = new ComicView(std::move(document), filePath, m_tabWidget);
            widget = comicView;
            view = comicView;
        }
    } else if (suffix == QLatin1String("mobi") || suffix == QLatin1String("azw") || suffix == QLatin1String("azw3")) {
        std::unique_ptr<MobiDocument> document = MobiDocument::load(filePath, &errorMessage);
        if (document) {
            isbn = document->isbn();
            localInfo.title = document->title();
            localInfo.authors = document->authors();
            localInfo.publisher = document->publisher();
            localInfo.description = document->description();
            localInfo.cover = document->cover();
            auto *mobiView = new MobiView(std::move(document), filePath, m_tabWidget);
            mobiView->setDarkMode(m_darkModeAction->isChecked());
            connect(mobiView, &MobiView::highlightsChanged, this, &MainWindow::refreshNotesDock);
            widget = mobiView;
            view = mobiView;
        }
    } else if (suffix == QLatin1String("html") || suffix == QLatin1String("htm")) {
#ifdef MNEMOSYNE_ENABLE_HTML
        if (!QFileInfo::exists(filePath)) {
            errorMessage = tr("File does not exist: %1").arg(filePath);
        } else {
            auto *htmlView = new HtmlView(filePath, m_tabWidget);
            widget = htmlView;
            view = htmlView;
        }
#else
        errorMessage = tr("HTML support is not available in this build.");
#endif
    } else {
        errorMessage = tr("Unsupported file type: .%1").arg(suffix);
    }

    if (!widget || !view) {
        QMessageBox::warning(this, tr("Could Not Open File"), errorMessage);
        return;
    }

    RecentFiles::recordOpened(filePath, view->documentTitle(), suffix);
#ifdef MNEMOSYNE_ENABLE_PLUGINS
    QJsonObject documentOpenedPayload;
    documentOpenedPayload["bookHash"] = FileIdentity::contentHash(filePath);
    documentOpenedPayload["title"] = view->documentTitle();
    documentOpenedPayload["format"] = suffix;
    PluginHost::emitEvent(QStringLiteral("documentOpened"), documentOpenedPayload);
#endif

    m_tabFilePaths.insert(widget, filePath);
    m_tabIsbn.insert(widget, isbn);
    m_tabLocalInfo.insert(widget, localInfo);
    const int index = m_tabWidget->addTab(widget, view->documentTitle());
    m_tabWidget->setCurrentIndex(index); // triggers onTabChanged, which populates docks/title
}

void MainWindow::onTabChanged(int index)
{
    QWidget *widget = m_tabWidget->widget(index);

    if (!widget || widget == m_libraryView) {
        m_currentView = nullptr;
        m_currentFilePath.clear();
        m_currentIsbn.clear();
        m_currentLocalInfo = BookMetadata();
        m_tocDock->clear();
        m_notesDock->clear();
        m_searchDock->clear();
        m_bookInfoDock->clear();
        setWindowTitle(tr("Mnemosyne"));
        if (widget == m_libraryView) {
            m_libraryView->refresh();
        }
#ifdef Q_OS_MACOS
        updateTouchBar(widget);
#endif
        return;
    }

    m_currentView = dynamic_cast<IReaderView *>(widget);
    m_currentFilePath = m_tabFilePaths.value(widget);
    m_currentIsbn = m_tabIsbn.value(widget);
    m_currentLocalInfo = m_tabLocalInfo.value(widget);

    if (m_currentView) {
        m_tocDock->setTableOfContents(m_currentView->tableOfContents());
        refreshNotesDock();
        m_searchDock->clear();
        refreshBookInfoDock();
        setWindowTitle(tr("%1 — Mnemosyne").arg(m_currentView->documentTitle()));
    }
#ifdef Q_OS_MACOS
    updateTouchBar(widget);
#endif
}

#ifdef Q_OS_MACOS
void MainWindow::updateTouchBar(QWidget *activeWidget)
{
    auto *pdfView = dynamic_cast<PdfView *>(activeWidget);
    if (!pdfView) {
        MacTouchBar::clearControls(windowHandle());
        return;
    }
    MacTouchBar::installPdfControls(
        windowHandle(), [pdfView] { pdfView->previousPage(); }, [pdfView] { pdfView->nextPage(); },
        [pdfView] { pdfView->zoomOut(); }, [pdfView] { pdfView->zoomIn(); });
}
#endif

void MainWindow::refreshBookInfoDock()
{
    // The sidebar group is a deliberate user choice (the top-bar hide/show
    // toggle) that this dock is now part of -- see toggleSidebar()/
    // showSidebar() -- so don't pop it back open on top of that just
    // because the current tab has info to show. showSidebar() calls back
    // into this once the group is visible again.
    if (m_tocDock->isHidden()) {
        return;
    }

    const bool hasLocal = bookMetadataHasContent(m_currentLocalInfo);
    const bool shouldLookUp = !m_currentIsbn.isEmpty() && m_bookInfoLookupAction->isChecked();

    if (!hasLocal && !shouldLookUp) {
        m_bookInfoDock->clear();
        return;
    }

    // Local metadata (if any) shows immediately -- no network round trip
    // needed for it -- and the API lookup below then enhances it in place
    // once it lands (see metadataReady's handler in setupDocks()).
    if (hasLocal) {
        m_bookInfoDock->setMetadata(m_currentLocalInfo);
    } else {
        m_bookInfoDock->setLoading();
    }

    if (shouldLookUp) {
        m_bookMetadataClient->lookup(m_currentIsbn);
    }
}

void MainWindow::onTabCloseRequested(int index)
{
    QWidget *widget = m_tabWidget->widget(index);
    if (!widget || widget == m_libraryView) {
        return; // Library tab has no close button, but guard anyway
    }
    if (auto *view = dynamic_cast<IReaderView *>(widget)) {
        view->flushProgress(); // see closeEvent() for why this can't wait for the debounce timer
    }
    m_tabWidget->removeTab(index);
    m_tabFilePaths.remove(widget);
    m_tabIsbn.remove(widget);
    m_tabLocalInfo.remove(widget);
    widget->deleteLater();
}

void MainWindow::focusSearch()
{
    showSidebar(); // the three sidebar docks are tabified together and always shown/hidden as one group
    m_searchDock->raise();
    m_searchDock->focusSearchField();
}

void MainWindow::setDarkModeEnabled(bool enabled)
{
    qApp->setPalette(enabled ? m_darkPalette : m_lightPalette);
    qApp->setStyleSheet(Theme::styleSheet(enabled));
    QSettings().setValue(QStringLiteral("darkMode"), enabled);

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QWidget *tab = m_tabWidget->widget(i);
        if (auto *epubView = dynamic_cast<EpubView *>(tab)) {
            epubView->setDarkMode(enabled);
        } else if (auto *markdownView = dynamic_cast<MarkdownView *>(tab)) {
            markdownView->setDarkMode(enabled);
        } else if (auto *mobiView = dynamic_cast<MobiView *>(tab)) {
            mobiView->setDarkMode(enabled);
        } else if (auto *txtView = dynamic_cast<TxtView *>(tab)) {
            txtView->setDarkMode(enabled);
        }
    }
}

QVector<HighlightExporter::ExportEntry> MainWindow::buildExportEntries() const
{
    QVector<HighlightExporter::ExportEntry> entries;
    if (m_currentFilePath.isEmpty()) {
        return entries;
    }

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(FileIdentity::contentHash(m_currentFilePath));

    // Position labels mirror the wording each view already uses in its own
    // sync-prompt text (see e.g. PdfView::offerSyncedPosition) -- Markdown/
    // TXT show no position number anywhere else either, so they get none
    // here.
    for (const Highlight &highlight : highlights) {
        QString label;
        if (dynamic_cast<PdfView *>(m_currentView)) {
            label = tr("Page %1").arg(highlight.targetIndex + 1);
        } else if (dynamic_cast<EpubView *>(m_currentView)) {
            label = tr("Chapter %1").arg(highlight.targetIndex + 1);
        } else if (dynamic_cast<MobiView *>(m_currentView)) {
            label = tr("Part %1").arg(highlight.targetIndex + 1);
        }
        entries.append({highlight, label});
    }
    return entries;
}

void MainWindow::exportNotesAsMarkdown()
{
    const QVector<HighlightExporter::ExportEntry> entries = buildExportEntries();
    if (entries.isEmpty()) {
        QMessageBox::information(this, tr("Export Notes"), tr("No highlights to export yet."));
        return;
    }

    const QString title = m_currentView->documentTitle();
    const QString filePath = QFileDialog::getSaveFileName(this, tr("Export Notes as Markdown"),
                                                            sanitizedForFilename(title) + QStringLiteral(".md"),
                                                            tr("Markdown Files (*.md)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
        || file.write(HighlightExporter::toMarkdown(title, entries).toUtf8()) < 0) {
        QMessageBox::warning(this, tr("Could Not Export Notes"), file.errorString());
    }
}

void MainWindow::exportNotesAsAnki()
{
    const QVector<HighlightExporter::ExportEntry> entries = buildExportEntries();
    const QString tsv = HighlightExporter::toAnkiTsv(entries);
    if (tsv.isEmpty()) {
        QMessageBox::information(this, tr("Export Notes"),
                                  tr("No notes to export yet -- only highlights with a note attached become Anki cards."));
        return;
    }

    const QString title = m_currentView->documentTitle();
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export Notes as Anki Cards"), sanitizedForFilename(title) + QStringLiteral(".tsv"),
        tr("Tab-Separated Values (*.tsv)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) || file.write(tsv.toUtf8()) < 0) {
        QMessageBox::warning(this, tr("Could Not Export Notes"), file.errorString());
    }
}

QVector<HighlightExporter::BookExport> MainWindow::buildLibraryExportBooks() const
{
    QVector<HighlightExporter::BookExport> books;
    for (const RecentFiles::Entry &recent : RecentFiles::list()) {
        // Prefer the hash RecentFiles already recorded (see
        // RecentFiles::recordOpened) over recomputing it, so a book whose
        // file has since moved or been deleted doesn't just drop out of the
        // export -- only entries written before contentHash existed need
        // the fallback.
        const QString bookHash =
            !recent.contentHash.isEmpty() ? recent.contentHash : FileIdentity::contentHash(recent.filePath);
        const QVector<Highlight> highlights = HighlightStore::highlightsFor(bookHash);
        if (highlights.isEmpty()) {
            continue;
        }

        QVector<HighlightExporter::ExportEntry> entries;
        entries.reserve(highlights.size());
        for (const Highlight &highlight : highlights) {
            entries.append({highlight, positionLabelForFormat(recent.format, highlight.targetIndex)});
        }
        books.append({recent.title, entries});
    }
    return books;
}

void MainWindow::exportLibraryAsMarkdown()
{
    const QVector<HighlightExporter::BookExport> books = buildLibraryExportBooks();
    if (books.isEmpty()) {
        QMessageBox::information(this, tr("Export Notes"), tr("No highlights to export yet across your library."));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(this, tr("Export Library Notes as Markdown"),
                                                            tr("Mnemosyne Library.md"), tr("Markdown Files (*.md)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
        || file.write(HighlightExporter::toMarkdownForLibrary(books).toUtf8()) < 0) {
        QMessageBox::warning(this, tr("Could Not Export Notes"), file.errorString());
    }
}

void MainWindow::exportLibraryAsAnki()
{
    const QVector<HighlightExporter::BookExport> books = buildLibraryExportBooks();
    const QString tsv = HighlightExporter::toAnkiTsvForLibrary(books);
    if (tsv.isEmpty()) {
        QMessageBox::information(
            this, tr("Export Notes"),
            tr("No notes to export yet -- only highlights with a note attached become Anki cards."));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(this, tr("Export Library Notes as Anki Cards"),
                                                            tr("Mnemosyne Library.tsv"),
                                                            tr("Tab-Separated Values (*.tsv)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) || file.write(tsv.toUtf8()) < 0) {
        QMessageBox::warning(this, tr("Could Not Export Notes"), file.errorString());
    }
}

#ifdef MNEMOSYNE_ENABLE_PLUGINS
void MainWindow::populatePluginExportActions()
{
    for (QAction *action : std::as_const(m_pluginExportActions)) {
        m_exportNotesMenu->removeAction(action);
        action->deleteLater();
    }
    m_pluginExportActions.clear();

    const QVector<PluginHost::PluginExporter> exporters = PluginHost::registeredExporters();
    if (exporters.isEmpty()) {
        return;
    }

    m_pluginExportActions.append(m_exportNotesMenu->addSeparator());
    for (const PluginHost::PluginExporter &exporter : exporters) {
        QAction *action = m_exportNotesMenu->addAction(exporter.label.isEmpty() ? exporter.id : exporter.label);
        connect(action, &QAction::triggered, this, [this, exporter] { runPluginExporter(exporter); });
        m_pluginExportActions.append(action);
    }
}

void MainWindow::runPluginExporter(const PluginHost::PluginExporter &exporter)
{
    const QVector<HighlightExporter::ExportEntry> entries = buildExportEntries();
    if (entries.isEmpty()) {
        QMessageBox::information(this, tr("Export Notes"), tr("No highlights to export yet."));
        return;
    }

    const QString title = m_currentView->documentTitle();
    QString errorMessage;
    const QString output = PluginHost::runExporter(exporter.id, title, entries, &errorMessage);
    if (output.isNull()) {
        QMessageBox::warning(this, tr("Could Not Export Notes"), errorMessage);
        return;
    }

    const QString extension = exporter.defaultExtension.isEmpty() ? QStringLiteral("txt") : exporter.defaultExtension;
    const QString filter = exporter.fileFilter.isEmpty() ? tr("All Files (*)") : exporter.fileFilter;
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export Notes"), sanitizedForFilename(title) + QLatin1Char('.') + extension, filter);
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text) || file.write(output.toUtf8()) < 0) {
        QMessageBox::warning(this, tr("Could Not Export Notes"), file.errorString());
    }
}

void MainWindow::showPluginsDialog()
{
    PluginsDialog::show(this);
}

void MainWindow::populatePluginCommandActions()
{
    for (QAction *action : std::as_const(m_pluginCommandActions)) {
        m_pluginsMenu->removeAction(action);
        action->deleteLater();
    }
    m_pluginCommandActions.clear();

    const QVector<PluginHost::PluginCommand> commands = PluginHost::registeredCommands();
    if (commands.isEmpty()) {
        return;
    }

    m_pluginCommandActions.append(m_pluginsMenu->addSeparator());
    for (const PluginHost::PluginCommand &command : commands) {
        QAction *action = m_pluginsMenu->addAction(command.label.isEmpty() ? command.id : command.label);
        connect(action, &QAction::triggered, this, [this, command] { runPluginCommand(command); });
        m_pluginCommandActions.append(action);
    }
}

void MainWindow::runPluginCommand(const PluginHost::PluginCommand &command)
{
    std::optional<PluginHost::CommandContext> context;
    if (m_currentView && !m_currentFilePath.isEmpty()) {
        PluginHost::CommandContext ctx;
        ctx.bookHash = FileIdentity::contentHash(m_currentFilePath);
        ctx.title = m_currentView->documentTitle();
        ctx.highlights = buildExportEntries();
        context = ctx;
    }
    PluginHost::runCommand(command.id, context);
}
#endif

void MainWindow::refreshNotesDock()
{
    if (m_currentFilePath.isEmpty()) {
        m_notesDock->clear();
        return;
    }
    m_notesDock->setHighlights(HighlightStore::highlightsFor(FileIdentity::contentHash(m_currentFilePath)));
}

void MainWindow::populateOpenRecentMenu()
{
    m_openRecentMenu->clear();

    const QVector<RecentFiles::Entry> entries = RecentFiles::list();
    if (entries.isEmpty()) {
        QAction *empty = m_openRecentMenu->addAction(tr("(No Recent Files)"));
        empty->setEnabled(false);
        return;
    }

    for (const RecentFiles::Entry &entry : entries) {
        QAction *action = m_openRecentMenu->addAction(entry.title.isEmpty() ? entry.filePath : entry.title);
        const QString path = entry.filePath;
        connect(action, &QAction::triggered, this, [this, path] { openPath(path); });
    }
}

void MainWindow::populateSyncMenu()
{
    m_syncMenu->clear();

    const QString currentPath = SyncFolder::path();
    QAction *statusAction = m_syncMenu->addAction(
        currentPath.isEmpty() ? tr("Sync: Off") : tr("Sync folder: %1").arg(currentPath));
    statusAction->setEnabled(false);

    m_syncMenu->addSeparator();

    QAction *chooseAction = m_syncMenu->addAction(tr("Choose Sync Folder..."));
    connect(chooseAction, &QAction::triggered, this, &MainWindow::chooseSyncFolder);

    QAction *disableAction = m_syncMenu->addAction(tr("Disable Sync"));
    disableAction->setEnabled(!currentPath.isEmpty());
    connect(disableAction, &QAction::triggered, this, &MainWindow::disableSync);

#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
    m_syncMenu->addSeparator();

    const bool googleSignedIn = GoogleAuth::isSignedIn();
    QAction *googleStatusAction = m_syncMenu->addAction(
        googleSignedIn ? tr("Google Drive: signed in as %1").arg(GoogleAuth::accountEmail())
                        : tr("Google Drive: Off"));
    googleStatusAction->setEnabled(false);

    QAction *googleSignInAction = m_syncMenu->addAction(tr("Sign in with Google Drive..."));
    googleSignInAction->setEnabled(!googleSignedIn && GoogleAuth::hasClientCredentials());
    if (!GoogleAuth::hasClientCredentials()) {
        googleSignInAction->setToolTip(tr("This build wasn't compiled with Google sign-in support."));
    }
    connect(googleSignInAction, &QAction::triggered, this, &MainWindow::signInWithGoogle);

    QAction *googleSignOutAction = m_syncMenu->addAction(tr("Sign out of Google Drive"));
    googleSignOutAction->setEnabled(googleSignedIn);
    connect(googleSignOutAction, &QAction::triggered, this, &MainWindow::signOutOfGoogle);
#endif
}

void MainWindow::chooseSyncFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(
        this, tr("Choose a Cloud-Synced Folder"), SyncFolder::path());
    if (folder.isEmpty()) {
        return;
    }

    SyncFolder::setPath(folder);
    QMessageBox::information(this, tr("Sync Folder Set"),
                              tr("Reading progress will now be written to and read from:\n%1\n\n"
                                 "This app doesn't talk to any cloud service directly — it just treats "
                                 "this folder as a plain local directory. Point it at a folder your OS "
                                 "or a service like iCloud Drive or Google Drive already syncs across "
                                 "your devices, and that sync client does the rest.")
                                  .arg(SyncFolder::dataDirectory()));
}

void MainWindow::disableSync()
{
    SyncFolder::setPath(QString());
}

#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
void MainWindow::signInWithGoogle()
{
    GoogleAuth::startSignIn([this](bool ok, const QString &error) {
        if (ok) {
            QMessageBox::information(this, tr("Signed In"),
                                      tr("Reading progress will now also sync through Google Drive."));
        } else {
            QMessageBox::warning(this, tr("Google Sign-In Failed"), error);
        }
    });
}

void MainWindow::signOutOfGoogle()
{
    GoogleAuth::signOut();
}
#endif
