#include "MainWindow.h"

#include "app/BookmarkStore.h"
#include "app/RecentFiles.h"
#include "app/SyncFolder.h"
#include "core/Bookmark.h"
#include "core/ReaderView.h"
#include "epub/EpubDocument.h"
#include "ui/BookmarksDock.h"
#include "ui/EpubView.h"
#ifdef MNEMOSYNE_ENABLE_HTML
#include "ui/HtmlView.h"
#endif
#include "ui/LibraryView.h"
#include "ui/PdfView.h"
#include "ui/SearchDock.h"
#include "ui/Theme.h"
#include "ui/TocDock.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>

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
        BookmarkStore::removeBookmark(m_currentFilePath, index);
        refreshBookmarksDock();
    });

    connect(m_searchDock, &SearchDock::searchRequested, this, [this](const QString &query) {
        m_searchDock->setResults(m_currentView ? m_currentView->search(query) : QVector<SearchResult>());
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
    auto *toolbar = new QToolBar(tr("Sidebar"), this);
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    addToolBar(Qt::LeftToolBarArea, toolbar);

    m_sidebarToggleAction = toolbar->addAction(QStringLiteral("‹"));
    m_sidebarToggleAction->setToolTip(tr("Hide Sidebar"));
    connect(m_sidebarToggleAction, &QAction::triggered, this, &MainWindow::toggleSidebar);
}

void MainWindow::toggleSidebar()
{
    const bool currentlyVisible = m_tocDock->isVisible() || m_bookmarksDock->isVisible() || m_searchDock->isVisible();

    if (currentlyVisible) {
        m_tocDock->hide();
        m_bookmarksDock->hide();
        m_searchDock->hide();
        m_sidebarToggleAction->setText(QStringLiteral("›"));
        m_sidebarToggleAction->setToolTip(tr("Show Sidebar"));
    } else {
        m_tocDock->show();
        m_bookmarksDock->show();
        m_searchDock->show();
        m_tocDock->raise();
        m_sidebarToggleAction->setText(QStringLiteral("‹"));
        m_sidebarToggleAction->setToolTip(tr("Hide Sidebar"));
    }
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
           "<p>A PDF, EPUB, and HTML reader.</p>")
            .arg(QStringLiteral(MNEMOSYNE_VERSION)));
}

void MainWindow::openFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open Document"), QString(),
#ifdef MNEMOSYNE_ENABLE_HTML
        tr("Documents (*.pdf *.epub *.html *.htm)"));
#else
        tr("Documents (*.pdf *.epub)"));
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

    BookmarkStore::addBookmark(m_currentFilePath, bookmark);
    refreshBookmarksDock();
}

void MainWindow::focusSearch()
{
    m_searchDock->show();
    m_searchDock->raise();
    m_searchDock->focusSearchField();
}

void MainWindow::setDarkModeEnabled(bool enabled)
{
    qApp->setPalette(enabled ? m_darkPalette : m_lightPalette);
    qApp->setStyleSheet(Theme::styleSheet(enabled));
    QSettings().setValue(QStringLiteral("darkMode"), enabled);

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (auto *epubView = dynamic_cast<EpubView *>(m_tabWidget->widget(i))) {
            epubView->setDarkMode(enabled);
        }
    }
}

void MainWindow::refreshBookmarksDock()
{
    if (m_currentFilePath.isEmpty()) {
        m_bookmarksDock->clear();
        return;
    }
    m_bookmarksDock->setBookmarks(BookmarkStore::bookmarksFor(m_currentFilePath));
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
