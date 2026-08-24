#include "quick/BookmarksModel.h"

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

// BookmarksModel wraps app/BookmarkStore.h (already covered directly by
// AppPersistenceTest) scoped to a settable bookHash -- this covers the
// model-specific behavior: role data, isBookmarked()'s plain-method (not
// NOTIFY-backed) query used by the reader screens' star toggle, and
// modelReset firing on every mutation. HighlightsModel is a near-identical
// wrapper over HighlightStore with the same shape; not duplicated here.
class BookmarksModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void togglingAddsThenRemovesABookmark();
    void isBookmarkedReflectsCurrentState();
    void settingBookHashResetsTheModel();
    void emptyBookHashIgnoresToggle();
};

void BookmarksModelTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void BookmarksModelTest::init()
{
    QSettings().clear();
}

void BookmarksModelTest::cleanupTestCase()
{
    QSettings().clear();
}

void BookmarksModelTest::togglingAddsThenRemovesABookmark()
{
    BookmarksModel model;
    model.setBookHash(QStringLiteral("book-1"));

    model.toggleBookmark(3, QStringLiteral("Page 4"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), BookmarksModel::TargetIndexRole).toInt(), 3);
    QCOMPARE(model.data(model.index(0, 0), BookmarksModel::LabelRole).toString(), QStringLiteral("Page 4"));

    model.toggleBookmark(3, QStringLiteral("Page 4")); // same target index toggles it back off
    QCOMPARE(model.rowCount(), 0);
}

void BookmarksModelTest::isBookmarkedReflectsCurrentState()
{
    BookmarksModel model;
    model.setBookHash(QStringLiteral("book-1"));

    QVERIFY(!model.isBookmarked(5));
    model.toggleBookmark(5, QStringLiteral("Five"));
    QVERIFY(model.isBookmarked(5));
    QVERIFY(!model.isBookmarked(6));
}

void BookmarksModelTest::settingBookHashResetsTheModel()
{
    BookmarksModel model;
    model.setBookHash(QStringLiteral("book-1"));
    model.toggleBookmark(1, QStringLiteral("One"));
    QCOMPARE(model.rowCount(), 1);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.setBookHash(QStringLiteral("book-2")); // a different, empty-so-far book
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(model.rowCount(), 0); // book-2's bookmarks are isolated from book-1's
}

void BookmarksModelTest::emptyBookHashIgnoresToggle()
{
    BookmarksModel model; // bookHash never set
    model.toggleBookmark(0, QStringLiteral("Ignored"));
    QCOMPARE(model.rowCount(), 0);
}

QTEST_MAIN(BookmarksModelTest)
#include "BookmarksModelTest.moc"
