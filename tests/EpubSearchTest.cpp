#include "epub/EpubSearch.h"

#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}
} // namespace

// searchEpubFile() is the shared core-level logic behind both desktop's
// MainWindow.cpp (m_epubSearchWatcher) and mobile's SearchResultsModel
// (quick/SearchResultsModel.cpp) -- see ViewSearchTest.cpp's own
// epubSearch*() cases, which exercise the older, non-streaming
// EpubView::searchFile against the same fixture.
class EpubSearchTest : public QObject
{
    Q_OBJECT

private slots:
    void findsMatchesAcrossChapters();
    void isCaseInsensitive();
    void returnsEmptyForNoMatch();
    void returnsEmptyForBlankQuery();
    void returnsEmptyForMissingFile();
    void cancelledTokenStopsBeforeFirstChapter();
    void makeSearchCancelTokenReturnsFreshUncancelledToken();
};

void EpubSearchTest::findsMatchesAcrossChapters()
{
    // Both "Chapter One" and "Chapter Two" headings contain the word.
    QVector<SearchResult> results;
    searchEpubFile(
        fixturePath("test.epub"), QStringLiteral("chapter"),
        [&results](const SearchResult &r) { results.append(r); }, nullptr);

    QCOMPARE(results.size(), 2);
    QCOMPARE(results[0].targetIndex, 0);
    QCOMPARE(results[0].label, QStringLiteral("Chapter 1"));
    QCOMPARE(results[1].targetIndex, 1);
    QCOMPARE(results[1].label, QStringLiteral("Chapter 2"));
}

void EpubSearchTest::isCaseInsensitive()
{
    // Chapter 1's body text contains "special <characters>" (only chapter 1).
    QVector<SearchResult> results;
    searchEpubFile(
        fixturePath("test.epub"), QStringLiteral("SPECIAL"),
        [&results](const SearchResult &r) { results.append(r); }, nullptr);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].targetIndex, 0);
}

void EpubSearchTest::returnsEmptyForNoMatch()
{
    QVector<SearchResult> results;
    searchEpubFile(
        fixturePath("test.epub"), QStringLiteral("nonexistent-xyz"),
        [&results](const SearchResult &r) { results.append(r); }, nullptr);
    QVERIFY(results.isEmpty());
}

void EpubSearchTest::returnsEmptyForBlankQuery()
{
    QVector<SearchResult> results;
    searchEpubFile(
        fixturePath("test.epub"), QStringLiteral("   "),
        [&results](const SearchResult &r) { results.append(r); }, nullptr);
    QVERIFY(results.isEmpty());
}

void EpubSearchTest::returnsEmptyForMissingFile()
{
    QVector<SearchResult> results;
    searchEpubFile(
        fixturePath("does-not-exist.epub"), QStringLiteral("chapter"),
        [&results](const SearchResult &r) { results.append(r); }, nullptr);
    QVERIFY(results.isEmpty());
}

void EpubSearchTest::cancelledTokenStopsBeforeFirstChapter()
{
    EpubSearchCancelToken cancelToken = makeSearchCancelToken();
    cancelToken->store(true); // pre-cancelled: the per-chapter check should fire immediately

    QVector<SearchResult> results;
    searchEpubFile(
        fixturePath("test.epub"), QStringLiteral("chapter"),
        [&results](const SearchResult &r) { results.append(r); }, cancelToken);

    QVERIFY(results.isEmpty());
}

void EpubSearchTest::makeSearchCancelTokenReturnsFreshUncancelledToken()
{
    const EpubSearchCancelToken token = makeSearchCancelToken();
    QVERIFY(token);
    QVERIFY(!token->load());
}

QTEST_MAIN(EpubSearchTest)
#include "EpubSearchTest.moc"
