#include "epub/EpubDocument.h"

#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}
} // namespace

class EpubDocumentTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsTitleAndSpine();
    void parsesNestedToc();
    void inlinesRelativeStylesheetAcrossDirectories();
    void embedsImagesAsDataUris();
    void failsGracefullyOnMissingFile();
};

void EpubDocumentTest::loadsTitleAndSpine()
{
    QString error;
    auto doc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY2(doc, qPrintable(error));

    // dc:title contained an XML entity (&amp;) that must be decoded, not left literal.
    QCOMPARE(doc->title(), QStringLiteral("Test Book & Friends"));
    QCOMPARE(doc->spineCount(), 2);
}

void EpubDocumentTest::parsesNestedToc()
{
    QString error;
    auto doc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY2(doc, qPrintable(error));

    const QVector<TocNode> toc = doc->tableOfContents();
    QCOMPARE(toc.size(), 2);

    QCOMPARE(toc[0].title, QStringLiteral("Chapter One"));
    QCOMPARE(toc[0].pageNumber, 0);
    QVERIFY(toc[0].children.isEmpty());

    QCOMPARE(toc[1].title, QStringLiteral("Chapter Two"));
    QCOMPARE(toc[1].pageNumber, 1);
    QCOMPARE(toc[1].children.size(), 1);

    // A fragment-only difference (#section1) still resolves to the same chapter.
    QCOMPARE(toc[1].children[0].title, QStringLiteral("Section 2.1"));
    QCOMPARE(toc[1].children[0].pageNumber, 1);
}

void EpubDocumentTest::inlinesRelativeStylesheetAcrossDirectories()
{
    QString error;
    auto doc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY2(doc, qPrintable(error));

    // Chapter 1 (OEBPS/chap1.xhtml) references "style.css" directly.
    const QString chapter1Html = doc->chapterHtml(0);
    QVERIFY(chapter1Html.contains(QStringLiteral("<style>")));
    QVERIFY(chapter1Html.contains(QStringLiteral("color: #900")));
    QVERIFY(!chapter1Html.contains(QStringLiteral("<link")));

    // Chapter 2 (OEBPS/text/chap2.xhtml) references "../style.css" — this
    // exercises the ".." segment-popping logic in the path resolver.
    const QString chapter2Html = doc->chapterHtml(1);
    QVERIFY(chapter2Html.contains(QStringLiteral("<style>")));
    QVERIFY(chapter2Html.contains(QStringLiteral("color: #900")));
    QVERIFY(!chapter2Html.contains(QStringLiteral("<link")));
}

void EpubDocumentTest::embedsImagesAsDataUris()
{
    QString error;
    auto doc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY2(doc, qPrintable(error));

    const QString chapter1Html = doc->chapterHtml(0);
    QVERIFY(chapter1Html.contains(QStringLiteral("data:image/png;base64,")));
    QVERIFY(!chapter1Html.contains(QStringLiteral("src=\"images/cover.png\"")));
}

void EpubDocumentTest::failsGracefullyOnMissingFile()
{
    QString error;
    auto doc = EpubDocument::load(fixturePath("does-not-exist.epub"), &error);
    QVERIFY(!doc);
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(EpubDocumentTest)
#include "EpubDocumentTest.moc"
