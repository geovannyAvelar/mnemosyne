#include "app/BookmarkStore.h"
#include "app/HighlightStore.h"
#include "app/RecentFiles.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTest>

class AppPersistenceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void recentFilesTracksMostRecentFirst();
    void recentFilesReopeningMovesToFront();
    void recentFilesCanBeRemoved();

    void bookmarksRoundTripSortedByPosition();
    void bookmarksAtSamePositionReplaceRatherThanDuplicate();
    void bookmarksCanBeRemovedByIndex();

    void highlightsRoundTripWithRectAndText();
    void multipleHighlightsOnSamePageCoexist();
    void highlightsCanBeRemovedByIndex();
    void highlightsAreIsolatedPerFile();
};

void AppPersistenceTest::initTestCase()
{
    // Isolate from the real app's settings (organizationName "Mnemosyne") so
    // this test can never read or clobber actual user data on the machine
    // it runs on.
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void AppPersistenceTest::init()
{
    QSettings().clear(); // fresh state for each test function
}

void AppPersistenceTest::cleanupTestCase()
{
    QSettings().clear(); // don't leave test settings behind on disk
}

void AppPersistenceTest::recentFilesTracksMostRecentFirst()
{
    RecentFiles::recordOpened("/tmp/a.pdf", "A", "pdf");
    RecentFiles::recordOpened("/tmp/b.epub", "B", "epub");

    const QVector<RecentFiles::Entry> entries = RecentFiles::list();
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries[0].filePath, QStringLiteral("/tmp/b.epub"));
    QCOMPARE(entries[1].filePath, QStringLiteral("/tmp/a.pdf"));
}

void AppPersistenceTest::recentFilesReopeningMovesToFront()
{
    RecentFiles::recordOpened("/tmp/a.pdf", "A", "pdf");
    RecentFiles::recordOpened("/tmp/b.epub", "B", "epub");
    RecentFiles::recordOpened("/tmp/a.pdf", "A", "pdf"); // reopen

    const QVector<RecentFiles::Entry> entries = RecentFiles::list();
    QCOMPARE(entries.size(), 2); // no duplicate entry
    QCOMPARE(entries[0].filePath, QStringLiteral("/tmp/a.pdf"));
}

void AppPersistenceTest::recentFilesCanBeRemoved()
{
    RecentFiles::recordOpened("/tmp/a.pdf", "A", "pdf");
    RecentFiles::recordOpened("/tmp/b.epub", "B", "epub");
    RecentFiles::remove("/tmp/a.pdf");

    const QVector<RecentFiles::Entry> entries = RecentFiles::list();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].filePath, QStringLiteral("/tmp/b.epub"));
}

void AppPersistenceTest::bookmarksRoundTripSortedByPosition()
{
    const QString book = "/tmp/book.epub";
    BookmarkStore::addBookmark(book, Bookmark{5, "Five", QDateTime::currentDateTime()});
    BookmarkStore::addBookmark(book, Bookmark{1, "One", QDateTime::currentDateTime()});
    BookmarkStore::addBookmark(book, Bookmark{3, "Three", QDateTime::currentDateTime()});

    const QVector<Bookmark> bookmarks = BookmarkStore::bookmarksFor(book);
    QCOMPARE(bookmarks.size(), 3);
    QCOMPARE(bookmarks[0].targetIndex, 1);
    QCOMPARE(bookmarks[1].targetIndex, 3);
    QCOMPARE(bookmarks[2].targetIndex, 5);

    // A different book's bookmarks must not bleed into this one.
    QVERIFY(BookmarkStore::bookmarksFor("/tmp/other.pdf").isEmpty());
}

void AppPersistenceTest::bookmarksAtSamePositionReplaceRatherThanDuplicate()
{
    const QString book = "/tmp/book.epub";
    BookmarkStore::addBookmark(book, Bookmark{2, "First Label", QDateTime::currentDateTime()});
    BookmarkStore::addBookmark(book, Bookmark{2, "Replaced Label", QDateTime::currentDateTime()});

    const QVector<Bookmark> bookmarks = BookmarkStore::bookmarksFor(book);
    QCOMPARE(bookmarks.size(), 1);
    QCOMPARE(bookmarks[0].label, QStringLiteral("Replaced Label"));
}

void AppPersistenceTest::bookmarksCanBeRemovedByIndex()
{
    const QString book = "/tmp/book.epub";
    BookmarkStore::addBookmark(book, Bookmark{1, "One", QDateTime::currentDateTime()});
    BookmarkStore::addBookmark(book, Bookmark{2, "Two", QDateTime::currentDateTime()});

    BookmarkStore::removeBookmark(book, 0); // removes "One" (sorted first)

    const QVector<Bookmark> bookmarks = BookmarkStore::bookmarksFor(book);
    QCOMPARE(bookmarks.size(), 1);
    QCOMPARE(bookmarks[0].label, QStringLiteral("Two"));
}

void AppPersistenceTest::highlightsRoundTripWithRectAndText()
{
    const QString book = "/tmp/book.pdf";
    Highlight highlight;
    highlight.targetIndex = 2;
    highlight.pageRect = QRectF(10, 20, 100, 30);
    highlight.text = "highlighted passage";
    highlight.createdAt = QDateTime::currentDateTime();

    HighlightStore::addHighlight(book, highlight);

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(book);
    QCOMPARE(highlights.size(), 1);
    QCOMPARE(highlights[0].targetIndex, 2);
    QCOMPARE(highlights[0].pageRect, QRectF(10, 20, 100, 30));
    QCOMPARE(highlights[0].text, QStringLiteral("highlighted passage"));
}

void AppPersistenceTest::multipleHighlightsOnSamePageCoexist()
{
    const QString book = "/tmp/book.pdf";
    HighlightStore::addHighlight(book, Highlight{3, QRectF(0, 0, 10, 10), "first", QDateTime::currentDateTime()});
    HighlightStore::addHighlight(book, Highlight{3, QRectF(0, 20, 10, 10), "second", QDateTime::currentDateTime()});
    HighlightStore::addHighlight(book, Highlight{1, QRectF(0, 0, 10, 10), "earlier page", QDateTime::currentDateTime()});

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(book);
    QCOMPARE(highlights.size(), 3);
    // Sorted by targetIndex; the page-1 highlight comes first.
    QCOMPARE(highlights[0].text, QStringLiteral("earlier page"));
    QCOMPARE(highlights[1].targetIndex, 3);
    QCOMPARE(highlights[2].targetIndex, 3);
}

void AppPersistenceTest::highlightsCanBeRemovedByIndex()
{
    const QString book = "/tmp/book.pdf";
    HighlightStore::addHighlight(book, Highlight{1, QRectF(0, 0, 10, 10), "first", QDateTime::currentDateTime()});
    HighlightStore::addHighlight(book, Highlight{2, QRectF(0, 0, 10, 10), "second", QDateTime::currentDateTime()});

    HighlightStore::removeHighlight(book, 0); // removes "first" (sorted first by page)

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(book);
    QCOMPARE(highlights.size(), 1);
    QCOMPARE(highlights[0].text, QStringLiteral("second"));
}

void AppPersistenceTest::highlightsAreIsolatedPerFile()
{
    HighlightStore::addHighlight("/tmp/a.pdf", Highlight{0, QRectF(0, 0, 10, 10), "in a", QDateTime::currentDateTime()});
    QVERIFY(HighlightStore::highlightsFor("/tmp/b.pdf").isEmpty());
}

QTEST_MAIN(AppPersistenceTest)
#include "AppPersistenceTest.moc"
