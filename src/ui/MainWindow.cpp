#include "MainWindow.h"

#include "app/BookmarkStore.h"
#include "app/FileIdentity.h"
#include "app/RecentFiles.h"
#include "app/SyncFolder.h"
#ifdef MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC
#include "app/GoogleAuth.h"
#endif
#include "comic/CbzDocument.h"
#include "core/Bookmark.h"
#include "core/ReaderView.h"
#include "epub/EpubDocument.h"
#include "markdown/MarkdownDocument.h"
#include "mobi/MobiDocument.h"
#include "txt/TxtDocument.h"
#include "ui/BookmarksDock.h"
#include "ui/ComicView.h"
#include "ui/EpubView.h"
#ifdef MNEMOSYNE_ENABLE_HTML
#include "ui/HtmlView.h"
#endif
#include "ui/LibraryView.h"
#include "ui/MarkdownView.h"
#include "ui/MobiView.h"
#include "ui/PdfView.h"
#include "ui/SearchDock.h"
#include "ui/Theme.h"
#include "ui/TocDock.h"
#include "ui/TopBar.h"
#include "ui/TrafficLightButton.h"
#include "ui/TxtView.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QtConcurrent/QtConcurrentRun>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_lightPalette(Theme::lightPalette())
    , m_darkPalette(Theme::darkPalette())
{
    setWindowTitle(tr("Mnemosyne"));

    setupDocks();
    setupTabs();
    setupSidebarToggle();
    setupMenus();

    const bool savedDarkMode = QSettings().value(QStringLiteral("darkMode"), false).toBool();
    m_darkModeAction->setChecked(savedDarkMode);
    setDarkModeEnabled(savedDarkMode); // setChecked() only emits toggled() on a change, so apply explicitly

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

    m_bookmarksDock = new BookmarksDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_bookmarksDock);
    tabifyDockWidget(m_tocDock, m_bookmarksDock);

    m_searchDock = new SearchDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_searchDock);
    tabifyDockWidget(m_tocDock, m_searchDock);

    m_searchWatcher = new QFutureWatcher<QVector<SearchResult>>(this);

    // The tab strip above (from setTabPosition) already labels each dock, so
    // each dock's own title bar would just be a redundant duplicate label.
    m_tocDock->setTitleBarWidget(new QWidget(m_tocDock));
    m_bookmarksDock->setTitleBarWidget(new QWidget(m_bookmarksDock));
    m_searchDock->setTitleBarWidget(new QWidget(m_searchDock));

    m_tocDock->raise(); // Contents is the more useful default tab on opening a book

    connect(m_tocDock, &TocDock::nodeActivated, this, [this](const TocNode &node) {
        if (m_currentView) {
            m_currentView->goToTocNode(node);
        }
    });

    connect(m_bookmarksDock, &BookmarksDock::bookmarkActivated, this, [this](int targetIndex) {
        if (m_currentView) {
            TocNode node;
            node.pageNumber = targetIndex;
            m_currentView->goToTocNode(node);
        }
    });

    connect(m_bookmarksDock, &BookmarksDock::addBookmarkRequested, this, &MainWindow::addBookmarkForCurrentPosition);

    connect(m_bookmarksDock, &BookmarksDock::removeBookmarkRequested, this, [this](int index) {
        if (m_currentFilePath.isEmpty()) {
            return;
        }
        BookmarkStore::removeBookmark(FileIdentity::contentHash(m_currentFilePath), index);
        refreshBookmarksDock();
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
    // On macOS the native title bar is hidden entirely (see MacWindowChrome)
    // and this bar draws its own close/minimize/fullscreen buttons instead,
    // so the window chrome is genuinely part of the app's own GUI.
    auto *topBar = new TopBar(tr("Window"), this);
    topBar->setObjectName(QStringLiteral("windowTopBar"));
    topBar->setMovable(false);
    topBar->setFloatable(false);
    topBar->setIconSize(QSize(18, 18));
    topBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    addToolBar(Qt::TopToolBarArea, topBar);
#ifdef Q_OS_MACOS
    topBar->setFixedHeight(52);

    auto *trafficLights = new QWidget(topBar);
    auto *trafficLightsLayout = new QHBoxLayout(trafficLights);
    trafficLightsLayout->setContentsMargins(0, 0, 0, 0);
    trafficLightsLayout->setSpacing(8);

    auto *closeButton = new TrafficLightButton(QColor(0xFF, 0x5F, 0x57), TrafficLightButton::Glyph::Close, trafficLights);
    auto *minimizeButton = new TrafficLightButton(QColor(0xFE, 0xBC, 0x2E), TrafficLightButton::Glyph::Minimize, trafficLights);
    auto *zoomButton = new TrafficLightButton(QColor(0x28, 0xC8, 0x40), TrafficLightButton::Glyph::Zoom, trafficLights);
    closeButton->setToolTip(tr("Close"));
    minimizeButton->setToolTip(tr("Minimize"));
    zoomButton->setToolTip(tr("Full Screen"));

    connect(closeButton, &QAbstractButton::clicked, this, &QWidget::close);
    connect(minimizeButton, &QAbstractButton::clicked, this, &QWidget::showMinimized);
    connect(zoomButton, &QAbstractButton::clicked, this, [this] {
        setWindowState(windowState() ^ Qt::WindowFullScreen);
    });

    trafficLightsLayout->addWidget(closeButton);
    trafficLightsLayout->addWidget(minimizeButton);
    trafficLightsLayout->addWidget(zoomButton);
    topBar->addWidget(trafficLights);

    auto *trafficLightSpacer = new QWidget(topBar);
    trafficLightSpacer->setFixedWidth(16);
    topBar->addWidget(trafficLightSpacer);
#endif

    m_sidebarToggleAction = topBar->addAction(Theme::sidebarToggleIcon(), QString());
    m_sidebarToggleAction->setToolTip(tr("Hide Sidebar"));
    connect(m_sidebarToggleAction, &QAction::triggered, this, &MainWindow::toggleSidebar);

    QAction *searchAction = topBar->addAction(Theme::searchIcon(), QString());
    searchAction->setToolTip(tr("Search"));
    connect(searchAction, &QAction::triggered, this, &MainWindow::focusSearch);

    auto *spacer = new QWidget(topBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
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
    const bool currentlyVisible = !m_tocDock->isHidden() || !m_bookmarksDock->isHidden() || !m_searchDock->isHidden();

    if (currentlyVisible) {
        setFocus();
        m_tocDock->hide();
        m_bookmarksDock->hide();
        m_searchDock->hide();
        QSettings().setValue(QStringLiteral("sidebarVisible"), false);
        m_sidebarToggleAction->setToolTip(tr("Show Sidebar"));
    } else {
        showSidebar();
    }
}

// Shared by toggleSidebar()'s show branch and focusSearch(): anywhere the
// sidebar gets shown must keep the persisted "sidebarVisible" state and the
// toggle button's tooltip in sync, or a restart re-hides a sidebar the user
// last saw open (see focusSearch(), which used to show the docks directly
// without going through here — the bug that prompted this refactor).
void MainWindow::showSidebar()
{
    m_tocDock->show();
    m_bookmarksDock->show();
    m_searchDock->show();
    m_tocDock->raise();
    // If the dock group was last hidden while a dock other than m_tocDock
    // was the raised/active tab (e.g. via focusSearch()), Qt can fail to
    // restore this area's width on show() and leave it collapsed to 0 —
    // the docks are then technically "visible" but occupy no space. Force a
    // sane width explicitly rather than relying on Qt to infer one.
    resizeDocks({m_tocDock}, {260}, Qt::Horizontal);
    QSettings().setValue(QStringLiteral("sidebarVisible"), true);
    m_sidebarToggleAction->setToolTip(tr("Hide Sidebar"));
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

    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction *findAction = editMenu->addAction(tr("&Find..."));
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &MainWindow::focusSearch);

    QMenu *bookmarksMenu = menuBar()->addMenu(tr("&Bookmarks"));
    m_addBookmarkAction = bookmarksMenu->addAction(tr("&Add Bookmark..."));
    m_addBookmarkAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    m_addBookmarkAction->setEnabled(false);
    connect(m_addBookmarkAction, &QAction::triggered, this, &MainWindow::addBookmarkForCurrentPosition);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_tocDock->toggleViewAction());
    viewMenu->addAction(m_bookmarksDock->toggleViewAction());
    viewMenu->addAction(m_searchDock->toggleViewAction());
    viewMenu->addSeparator();

    m_darkModeAction = viewMenu->addAction(tr("&Dark Mode"));
    m_darkModeAction->setCheckable(true);
    connect(m_darkModeAction, &QAction::toggled, this, &MainWindow::setDarkModeEnabled);

    m_syncMenu = menuBar()->addMenu(tr("&Sync"));
    connect(m_syncMenu, &QMenu::aboutToShow, this, &MainWindow::populateSyncMenu);

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
           "<p>A PDF, EPUB, HTML, Markdown, MOBI/AZW, CBZ comic, and plain text reader.</p>"
           "<p><small>App icon derived from the \"books\" emoji (U+1F4DA) in the "
           "<a href=\"https://github.com/googlefonts/noto-emoji\">Noto Emoji</a> project by Google, "
           "used under the <a href=\"https://github.com/googlefonts/noto-emoji/blob/main/svg/LICENSE\">"
           "Apache License, Version 2.0</a>.</small></p>")
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

    if (suffix == QLatin1String("pdf")) {
        std::unique_ptr<IDocument> document = openDocument(filePath, &errorMessage);
        if (document) {
            auto *pdfView = new PdfView(std::move(document), filePath, m_tabWidget);
            widget = pdfView;
            view = pdfView;
        }
    } else if (suffix == QLatin1String("epub")) {
        std::unique_ptr<EpubDocument> document = EpubDocument::load(filePath, &errorMessage);
        if (document) {
            auto *epubView = new EpubView(std::move(document), filePath, m_tabWidget);
            epubView->setDarkMode(m_darkModeAction->isChecked());
            widget = epubView;
            view = epubView;
        }
    } else if (suffix == QLatin1String("md") || suffix == QLatin1String("markdown")) {
        std::unique_ptr<MarkdownDocument> document = MarkdownDocument::load(filePath, &errorMessage);
        if (document) {
            auto *markdownView = new MarkdownView(std::move(document), filePath, m_tabWidget);
            markdownView->setDarkMode(m_darkModeAction->isChecked());
            widget = markdownView;
            view = markdownView;
        }
    } else if (suffix == QLatin1String("txt")) {
        std::unique_ptr<TxtDocument> document = TxtDocument::load(filePath, &errorMessage);
        if (document) {
            auto *txtView = new TxtView(std::move(document), filePath, m_tabWidget);
            txtView->setDarkMode(m_darkModeAction->isChecked());
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
            auto *mobiView = new MobiView(std::move(document), filePath, m_tabWidget);
            mobiView->setDarkMode(m_darkModeAction->isChecked());
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

    m_tabFilePaths.insert(widget, filePath);
    const int index = m_tabWidget->addTab(widget, view->documentTitle());
    m_tabWidget->setCurrentIndex(index); // triggers onTabChanged, which populates docks/title
}

void MainWindow::onTabChanged(int index)
{
    QWidget *widget = m_tabWidget->widget(index);

    if (!widget || widget == m_libraryView) {
        m_currentView = nullptr;
        m_currentFilePath.clear();
        m_tocDock->clear();
        m_bookmarksDock->clear();
        m_searchDock->clear();
        m_addBookmarkAction->setEnabled(false);
        setWindowTitle(tr("Mnemosyne"));
        if (widget == m_libraryView) {
            m_libraryView->refresh();
        }
        return;
    }

    m_currentView = dynamic_cast<IReaderView *>(widget);
    m_currentFilePath = m_tabFilePaths.value(widget);

    if (m_currentView) {
        m_tocDock->setTableOfContents(m_currentView->tableOfContents());
        refreshBookmarksDock();
        m_searchDock->clear();
        m_addBookmarkAction->setEnabled(true);
        setWindowTitle(tr("%1 — Mnemosyne").arg(m_currentView->documentTitle()));
    }
}

void MainWindow::onTabCloseRequested(int index)
{
    QWidget *widget = m_tabWidget->widget(index);
    if (!widget || widget == m_libraryView) {
        return; // Library tab has no close button, but guard anyway
    }
    m_tabWidget->removeTab(index);
    m_tabFilePaths.remove(widget);
    widget->deleteLater();
}

void MainWindow::addBookmarkForCurrentPosition()
{
    if (!m_currentView || m_currentFilePath.isEmpty()) {
        return;
    }

    bool ok = false;
    const QString label = QInputDialog::getText(this, tr("Add Bookmark"), tr("Label (optional):"),
                                                  QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }

    Bookmark bookmark;
    bookmark.targetIndex = m_currentView->currentPosition();
    bookmark.label = label;
    bookmark.createdAt = QDateTime::currentDateTime();

    BookmarkStore::addBookmark(FileIdentity::contentHash(m_currentFilePath), bookmark);
    refreshBookmarksDock();
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

void MainWindow::refreshBookmarksDock()
{
    if (m_currentFilePath.isEmpty()) {
        m_bookmarksDock->clear();
        return;
    }
    m_bookmarksDock->setBookmarks(BookmarkStore::bookmarksFor(FileIdentity::contentHash(m_currentFilePath)));
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
