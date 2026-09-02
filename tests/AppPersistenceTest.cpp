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

    void highlightsRoundTripWithRectAndText();
    void multipleHighlightsOnSamePageCoexist();
    void highlightsCanBeRemovedByIndex();
    void highlightsAreIsolatedPerFile();

    void highlightNoteDefaultsToEmpty();
    void highlightNoteCanBeSetAndCleared();
    void settingNoteOnInvalidIndexIsNoOp();

    void highlightColorDefaultsToYellow();
    void highlightColorRoundTrips();

    void addHighlightAssignsStableUniqueId();
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

void AppPersistenceTest::highlightNoteDefaultsToEmpty()
{
    const QString book = "/tmp/book.pdf";
    HighlightStore::addHighlight(book, Highlight{1, QRectF(0, 0, 10, 10), "no note yet", QDateTime::currentDateTime()});

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(book);
    QCOMPARE(highlights.size(), 1);
    QVERIFY(highlights[0].note.isEmpty());
}

void AppPersistenceTest::highlightNoteCanBeSetAndCleared()
{
    const QString book = "/tmp/book.pdf";
    HighlightStore::addHighlight(book, Highlight{1, QRectF(0, 0, 10, 10), "annotated", QDateTime::currentDateTime()});

    HighlightStore::setNote(book, 0, "a comment on this passage");
    QCOMPARE(HighlightStore::highlightsFor(book)[0].note, QStringLiteral("a comment on this passage"));

    HighlightStore::setNote(book, 0, QString());
    QVERIFY(HighlightStore::highlightsFor(book)[0].note.isEmpty());
}

void AppPersistenceTest::settingNoteOnInvalidIndexIsNoOp()
{
    const QString book = "/tmp/book.pdf";
    HighlightStore::addHighlight(book, Highlight{1, QRectF(0, 0, 10, 10), "only one", QDateTime::currentDateTime()});

    HighlightStore::setNote(book, -1, "nope");
    HighlightStore::setNote(book, 5, "nope");

    QVERIFY(HighlightStore::highlightsFor(book)[0].note.isEmpty());
}

void AppPersistenceTest::highlightColorDefaultsToYellow()
{
    const QString book = "/tmp/book.pdf";
    HighlightStore::addHighlight(book, Highlight{1, QRectF(0, 0, 10, 10), "a real highlight", QDateTime::currentDateTime()});

    QCOMPARE(HighlightStore::highlightsFor(book)[0].color, kDefaultHighlightColor);
}

void AppPersistenceTest::highlightColorRoundTrips()
{
    const QString book = "/tmp/book.pdf";
    Highlight annotated;
    annotated.targetIndex = 1;
    annotated.pageRect = QRectF(0, 0, 10, 10);
    annotated.text = "a passage with a note in a custom color";
    annotated.createdAt = QDateTime::currentDateTime();
    annotated.note = "why this passage matters";
    annotated.color = QColor(33, 150, 243, 140); // blue, picked from the palette

    HighlightStore::addHighlight(book, annotated);

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(book);
    const Highlight &roundTripped = highlights[0];
    QCOMPARE(roundTripped.color, QColor(33, 150, 243, 140));
    QCOMPARE(roundTripped.note, QStringLiteral("why this passage matters"));
}

void AppPersistenceTest::addHighlightAssignsStableUniqueId()
{
    // Highlight sync (see HighlightSync.h) identifies a highlight by this id
    // across devices, so it must be non-empty, distinct per highlight, and
    // unaffected by no sync backend being configured (the default here).
    const QString book = "/tmp/book.pdf";
    HighlightStore::addHighlight(book, Highlight{1, QRectF(0, 0, 10, 10), "first", QDateTime::currentDateTime()});
    HighlightStore::addHighlight(book, Highlight{2, QRectF(0, 0, 10, 10), "second", QDateTime::currentDateTime()});

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(book);
    QCOMPARE(highlights.size(), 2);
    QVERIFY(!highlights[0].id.isEmpty());
    QVERIFY(!highlights[1].id.isEmpty());
    QVERIFY(highlights[0].id != highlights[1].id);

    // Re-reading must return the same ids, not mint fresh ones each time.
    const QVector<Highlight> again = HighlightStore::highlightsFor(book);
    QCOMPARE(again[0].id, highlights[0].id);
    QCOMPARE(again[1].id, highlights[1].id);
}

QTEST_MAIN(AppPersistenceTest)
#include "AppPersistenceTest.moc"
