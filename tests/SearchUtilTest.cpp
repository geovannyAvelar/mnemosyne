#include "core/SearchUtil.h"

#include <QTest>

class SearchUtilTest : public QObject
{
    Q_OBJECT

private slots:
    void returnsEmptyWhenNoMatch();
    void matchIsCaseInsensitive();
    void addsEllipsisOnBothSidesWhenMatchIsInMiddle();
    void omitsLeadingEllipsisWhenMatchIsNearStart();
    void omitsTrailingEllipsisWhenMatchIsNearEnd();
    void omitsBothEllipsesWhenWholeTextFitsInContext();
};

void SearchUtilTest::returnsEmptyWhenNoMatch()
{
    QCOMPARE(makeSearchSnippet("the quick brown fox", "elephant"), QString());
}

void SearchUtilTest::matchIsCaseInsensitive()
{
    const QString snippet = makeSearchSnippet("The Quick Brown Fox", "quick");
    QVERIFY(snippet.contains("Quick"));
}

void SearchUtilTest::addsEllipsisOnBothSidesWhenMatchIsInMiddle()
{
    const QString haystack = QString("word ").repeated(30) + "NEEDLE" + QString(" word").repeated(30);
    const QString snippet = makeSearchSnippet(haystack, "NEEDLE", 10);

    QVERIFY(snippet.startsWith(QStringLiteral("…")));
    QVERIFY(snippet.endsWith(QStringLiteral("…")));
    QVERIFY(snippet.contains("NEEDLE"));
}

void SearchUtilTest::omitsLeadingEllipsisWhenMatchIsNearStart()
{
    const QString haystack = "NEEDLE " + QString("word ").repeated(30);
    const QString snippet = makeSearchSnippet(haystack, "NEEDLE", 10);

    QVERIFY(!snippet.startsWith(QStringLiteral("…")));
    QVERIFY(snippet.endsWith(QStringLiteral("…")));
}

void SearchUtilTest::omitsTrailingEllipsisWhenMatchIsNearEnd()
{
    const QString haystack = QString("word ").repeated(30) + "NEEDLE";
    const QString snippet = makeSearchSnippet(haystack, "NEEDLE", 10);

    QVERIFY(snippet.startsWith(QStringLiteral("…")));
    QVERIFY(!snippet.endsWith(QStringLiteral("…")));
}

void SearchUtilTest::omitsBothEllipsesWhenWholeTextFitsInContext()
{
    const QString snippet = makeSearchSnippet("short text with NEEDLE inside", "NEEDLE", 100);

    QVERIFY(!snippet.startsWith(QStringLiteral("…")));
    QVERIFY(!snippet.endsWith(QStringLiteral("…")));
    QCOMPARE(snippet, QStringLiteral("short text with NEEDLE inside"));
}

QTEST_MAIN(SearchUtilTest)
#include "SearchUtilTest.moc"
