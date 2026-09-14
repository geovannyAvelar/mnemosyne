#include "quick/SearchResultsModel.h"

#include <QSignalSpy>
#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}
} // namespace

// SearchResultsModel is the QML-facing wrapper (see quick/SearchResultsModel.h)
// around epub/EpubSearch.h's searchEpubFile(), already covered directly by
// EpubSearchTest.cpp -- this checks the QAbstractListModel plumbing, the
// async isSearching/stale-result handling around QtConcurrent::run(), and
// cancel().
class SearchResultsModelTest : public QObject
{
    Q_OBJECT

private slots:
    void searchPopulatesModelWithMatchingChapters();
    void searchWithNoMatchesLeavesModelEmpty();
    void clearResetsModelAndSearchingState();
    void searchWithBlankQueryClearsModel();
    void cancelWithNoActiveSearchIsNoOp();
    void cancelDuringSearchEventuallyStopsSearching();
    void searchAfterCancelStartsFresh();
};

void SearchResultsModelTest::searchPopulatesModelWithMatchingChapters()
{
    SearchResultsModel model;
    QVERIFY(!model.isSearching());

    QSignalSpy searchingSpy(&model, &SearchResultsModel::isSearchingChanged);
    model.search(fixturePath("test.epub"), QStringLiteral("chapter"));
    QVERIFY(model.isSearching());

    QTRY_VERIFY(!model.isSearching());
    QVERIFY(searchingSpy.count() >= 2); // true then false

    QCOMPARE(model.rowCount(), 2);
    const QModelIndex first = model.index(0, 0);
    QCOMPARE(model.data(first, SearchResultsModel::TargetIndexRole).toInt(), 0);
    QCOMPARE(model.data(first, SearchResultsModel::LabelRole).toString(), QStringLiteral("Chapter 1"));
    QVERIFY(!model.data(first, SearchResultsModel::SnippetRole).toString().isEmpty());
}

void SearchResultsModelTest::searchWithNoMatchesLeavesModelEmpty()
{
    SearchResultsModel model;
    model.search(fixturePath("test.epub"), QStringLiteral("nonexistent-xyz"));
    QTRY_VERIFY(!model.isSearching());
    QCOMPARE(model.rowCount(), 0);
}

void SearchResultsModelTest::clearResetsModelAndSearchingState()
{
    SearchResultsModel model;
    model.search(fixturePath("test.epub"), QStringLiteral("chapter"));
    QTRY_VERIFY(!model.isSearching());
    QCOMPARE(model.rowCount(), 2);

    model.clear();
    QVERIFY(!model.isSearching());
    QCOMPARE(model.rowCount(), 0);
}

void SearchResultsModelTest::searchWithBlankQueryClearsModel()
{
    SearchResultsModel model;
    model.search(fixturePath("test.epub"), QStringLiteral("chapter"));
    QTRY_VERIFY(!model.isSearching());
    QCOMPARE(model.rowCount(), 2);

    model.search(fixturePath("test.epub"), QStringLiteral("   "));
    QVERIFY(!model.isSearching());
    QCOMPARE(model.rowCount(), 0);
}

void SearchResultsModelTest::cancelWithNoActiveSearchIsNoOp()
{
    SearchResultsModel model;
    model.cancel(); // must not crash with no search ever started
    QVERIFY(!model.isSearching());
}

void SearchResultsModelTest::cancelDuringSearchEventuallyStopsSearching()
{
    SearchResultsModel model;
    model.search(fixturePath("test.epub"), QStringLiteral("chapter"));
    model.cancel();

    // The worker notices the cancel token on its next per-chapter check
    // (see epub/EpubSearch.h), so this may take a moment -- unlike clear(),
    // cancel() doesn't force isSearching false immediately.
    QTRY_VERIFY(!model.isSearching());
}

void SearchResultsModelTest::searchAfterCancelStartsFresh()
{
    SearchResultsModel model;
    model.search(fixturePath("test.epub"), QStringLiteral("chapter"));
    model.cancel();
    QTRY_VERIFY(!model.isSearching());

    // A cancelled search must not leave stale state (generation, watcher)
    // behind that breaks the next real one.
    model.search(fixturePath("test.epub"), QStringLiteral("chapter"));
    QTRY_VERIFY(!model.isSearching());
    QCOMPARE(model.rowCount(), 2);
}

QTEST_MAIN(SearchResultsModelTest)
#include "SearchResultsModelTest.moc"
