#include "quick/LibraryModel.h"

#include "app/FileIdentity.h"

#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

// LibraryModel is a thin QAbstractListModel wrapper over app/RecentFiles.h,
// the same store AppPersistenceTest already exercises directly -- this
// covers the model-specific part instead: role data and refresh()/
// recordOpened()/removeEntry() keeping the model in sync with the store.
class LibraryModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void recordOpenedAddsARowWithCorrectRoleData();
    void titleFallsBackToFilePathWhenEmpty();
    void removeEntryDropsTheRow();

    void collectionBridgeMethodsRoundTripThroughCollectionStore();
    void tagBridgeMethodsRoundTripThroughTagStore();
    void collectionFilterShowsOnlyMatchingBooks();
    void tagFilterShowsOnlyMatchingBooks();
    void combinedCollectionAndTagFiltersAreAnded();
    void deletingActiveCollectionFilterClearsIt();

private:
    // A row's ContentHashRole only ever has something to filter on when the
    // underlying file actually exists on disk (see FileIdentity::contentHash);
    // a fake /tmp path like the other tests above use resolves to an empty
    // hash. Writes a small distinct-content temp file (so each call gets its
    // own hash) and returns its path -- callers then recordOpened() it
    // themselves and can compute its hash via FileIdentity::contentHash().
    QString createRealFile(const QString &name, const QString &content);

    std::unique_ptr<QTemporaryDir> m_dir;
};

void LibraryModelTest::initTestCase()
{
    // Isolate from the real app's settings, same as AppPersistenceTest.
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void LibraryModelTest::init()
{
    QSettings().clear();
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

QString LibraryModelTest::createRealFile(const QString &name, const QString &content)
{
    const QString path = m_dir->filePath(name);
    QFile file(path);
    const bool opened = file.open(QIODevice::WriteOnly);
    Q_ASSERT(opened);
    file.write(content.toUtf8());
    file.close();
    return path;
}

void LibraryModelTest::cleanupTestCase()
{
    QSettings().clear();
}

void LibraryModelTest::recordOpenedAddsARowWithCorrectRoleData()
{
    LibraryModel model;
    model.recordOpened(QStringLiteral("/tmp/a.pdf"), QStringLiteral("Book A"), QStringLiteral("pdf"));

    QCOMPARE(model.rowCount(), 1);
    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, LibraryModel::FilePathRole).toString(), QStringLiteral("/tmp/a.pdf"));
    QCOMPARE(model.data(index, LibraryModel::TitleRole).toString(), QStringLiteral("Book A"));
    QCOMPARE(model.data(index, LibraryModel::FormatRole).toString(), QStringLiteral("pdf"));
}

void LibraryModelTest::titleFallsBackToFilePathWhenEmpty()
{
    LibraryModel model;
    model.recordOpened(QStringLiteral("/tmp/untitled.epub"), QString(), QStringLiteral("epub"));

    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, LibraryModel::TitleRole).toString(), QStringLiteral("/tmp/untitled.epub"));
}

void LibraryModelTest::removeEntryDropsTheRow()
{
    LibraryModel model;
    model.recordOpened(QStringLiteral("/tmp/a.pdf"), QStringLiteral("A"), QStringLiteral("pdf"));
    model.recordOpened(QStringLiteral("/tmp/b.epub"), QStringLiteral("B"), QStringLiteral("epub"));
    QCOMPARE(model.rowCount(), 2);

    model.removeEntry(QStringLiteral("/tmp/b.epub"));

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::FilePathRole).toString(), QStringLiteral("/tmp/a.pdf"));
}

void LibraryModelTest::collectionBridgeMethodsRoundTripThroughCollectionStore()
{
    LibraryModel model;
    QVERIFY(model.allCollections().isEmpty());

    QCOMPARE(model.createCollection(QStringLiteral("  Sci-Fi  ")), QStringLiteral("Sci-Fi"));
    QCOMPARE(model.allCollections(), QStringList{QStringLiteral("Sci-Fi")});

    model.addBookToCollection(QStringLiteral("book-1"), QStringLiteral("Sci-Fi"));
    QCOMPARE(model.collectionsForBook(QStringLiteral("book-1")), QStringList{QStringLiteral("Sci-Fi")});

    model.removeBookFromCollection(QStringLiteral("book-1"), QStringLiteral("Sci-Fi"));
    QVERIFY(model.collectionsForBook(QStringLiteral("book-1")).isEmpty());
    QCOMPARE(model.allCollections(), QStringList{QStringLiteral("Sci-Fi")}); // still exists, just empty

    model.deleteCollection(QStringLiteral("Sci-Fi"));
    QVERIFY(model.allCollections().isEmpty());
}

void LibraryModelTest::tagBridgeMethodsRoundTripThroughTagStore()
{
    LibraryModel model;
    QVERIFY(model.allTags().isEmpty());

    model.setTagsForBook(QStringLiteral("book-1"), {QStringLiteral("fiction"), QStringLiteral("favorite")});
    QCOMPARE(model.tagsForBook(QStringLiteral("book-1")).size(), 2);
    QCOMPARE(model.allTags().size(), 2);
}

void LibraryModelTest::collectionFilterShowsOnlyMatchingBooks()
{
    LibraryModel model;
    const QString pathA = createRealFile(QStringLiteral("a.pdf"), QStringLiteral("book A content"));
    const QString pathB = createRealFile(QStringLiteral("b.pdf"), QStringLiteral("book B content"));
    model.recordOpened(pathA, QStringLiteral("A"), QStringLiteral("pdf"));
    model.recordOpened(pathB, QStringLiteral("B"), QStringLiteral("pdf"));
    QCOMPARE(model.rowCount(), 2);

    const QString hashA = FileIdentity::contentHash(pathA);
    QVERIFY(!hashA.isEmpty());
    model.addBookToCollection(hashA, QStringLiteral("Favorites"));

    model.setCollectionFilter(QStringLiteral("Favorites"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::TitleRole).toString(), QStringLiteral("A"));

    model.setCollectionFilter(QString());
    QCOMPARE(model.rowCount(), 2);
}

void LibraryModelTest::tagFilterShowsOnlyMatchingBooks()
{
    LibraryModel model;
    const QString pathA = createRealFile(QStringLiteral("a.pdf"), QStringLiteral("book A content"));
    const QString pathB = createRealFile(QStringLiteral("b.pdf"), QStringLiteral("book B content"));
    model.recordOpened(pathA, QStringLiteral("A"), QStringLiteral("pdf"));
    model.recordOpened(pathB, QStringLiteral("B"), QStringLiteral("pdf"));

    const QString hashB = FileIdentity::contentHash(pathB);
    model.setTagsForBook(hashB, {QStringLiteral("reference")});

    model.setTagFilter(QStringLiteral("reference"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::TitleRole).toString(), QStringLiteral("B"));
}

void LibraryModelTest::combinedCollectionAndTagFiltersAreAnded()
{
    LibraryModel model;
    const QString pathA = createRealFile(QStringLiteral("a.pdf"), QStringLiteral("book A content"));
    const QString pathB = createRealFile(QStringLiteral("b.pdf"), QStringLiteral("book B content"));
    model.recordOpened(pathA, QStringLiteral("A"), QStringLiteral("pdf"));
    model.recordOpened(pathB, QStringLiteral("B"), QStringLiteral("pdf"));

    const QString hashA = FileIdentity::contentHash(pathA);
    const QString hashB = FileIdentity::contentHash(pathB);
    // Both books share the shelf, but only B carries the tag -- the AND of
    // the two filters should leave just B.
    model.addBookToCollection(hashA, QStringLiteral("Favorites"));
    model.addBookToCollection(hashB, QStringLiteral("Favorites"));
    model.setTagsForBook(hashB, {QStringLiteral("reference")});

    model.setCollectionFilter(QStringLiteral("Favorites"));
    model.setTagFilter(QStringLiteral("reference"));

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::TitleRole).toString(), QStringLiteral("B"));
}

void LibraryModelTest::deletingActiveCollectionFilterClearsIt()
{
    LibraryModel model;
    const QString pathA = createRealFile(QStringLiteral("a.pdf"), QStringLiteral("book A content"));
    model.recordOpened(pathA, QStringLiteral("A"), QStringLiteral("pdf"));
    const QString hashA = FileIdentity::contentHash(pathA);
    model.addBookToCollection(hashA, QStringLiteral("Favorites"));

    model.setCollectionFilter(QStringLiteral("Favorites"));
    QCOMPARE(model.rowCount(), 1);

    model.deleteCollection(QStringLiteral("Favorites"));

    QCOMPARE(model.collectionFilter(), QString());
    QCOMPARE(model.rowCount(), 1); // back to unfiltered -- still shows A
}

QTEST_MAIN(LibraryModelTest)
#include "LibraryModelTest.moc"
