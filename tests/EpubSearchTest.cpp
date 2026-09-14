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
// EpubView::searchFile (ui/EpubView.cpp) and mobile's SearchResultsModel
// (quick/SearchResultsModel.cpp) -- see ViewSearchTest.cpp's
// epubSearch*() cases, which exercise the same fixture through EpubView.
class EpubSearchTest : public QObject
{
    Q_OBJECT

private slots:
    void findsMatchesAcrossChapters();
    void isCaseInsensitive();
    void returnsEmptyForNoMatch();
    void returnsEmptyForBlankQuery();
    void returnsEmptyForMissingFile();
};

void EpubSearchTest::findsMatchesAcrossChapters()
{
    // Both "Chapter One" and "Chapter Two" headings contain the word.
    const QVector<SearchResult> results = searchEpubFile(fixturePath("test.epub"), QStringLiteral("chapter"));

    QCOMPARE(results.size(), 2);
    QCOMPARE(results[0].targetIndex, 0);
    QCOMPARE(results[0].label, QStringLiteral("Chapter 1"));
    QCOMPARE(results[1].targetIndex, 1);
    QCOMPARE(results[1].label, QStringLiteral("Chapter 2"));
}

void EpubSearchTest::isCaseInsensitive()
{
    // Chapter 1's body text contains "special <characters>" (only chapter 1).
    const QVector<SearchResult> results = searchEpubFile(fixturePath("test.epub"), QStringLiteral("SPECIAL"));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].targetIndex, 0);
}

void EpubSearchTest::returnsEmptyForNoMatch()
{
    QVERIFY(searchEpubFile(fixturePath("test.epub"), QStringLiteral("nonexistent-xyz")).isEmpty());
}

void EpubSearchTest::returnsEmptyForBlankQuery()
{
    QVERIFY(searchEpubFile(fixturePath("test.epub"), QStringLiteral("   ")).isEmpty());
}

void EpubSearchTest::returnsEmptyForMissingFile()
{
    QVERIFY(searchEpubFile(fixturePath("does-not-exist.epub"), QStringLiteral("chapter")).isEmpty());
}

QTEST_MAIN(EpubSearchTest)
#include "EpubSearchTest.moc"
