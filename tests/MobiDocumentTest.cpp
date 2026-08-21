#include "mobi/MobiDocument.h"

#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}
} // namespace

class MobiDocumentTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsTitleAndParts();
    void parsesNestedToc();
    void refusesDrmProtectedFileWithoutDecrypting();
    void failsGracefullyOnMissingFile();
};

void MobiDocumentTest::loadsTitleAndParts()
{
    QString error;
    auto doc = MobiDocument::load(fixturePath("test.mobi"), &error);
    QVERIFY2(doc, qPrintable(error));

    QCOMPARE(doc->title(), QStringLiteral("libmobi ncx test"));
    QCOMPARE(doc->partCount(), 3);
    QVERIFY(doc->partHtml(0).contains(QStringLiteral("<html"), Qt::CaseInsensitive));
}

void MobiDocumentTest::parsesNestedToc()
{
    QString error;
    auto doc = MobiDocument::load(fixturePath("test.mobi"), &error);
    QVERIFY2(doc, qPrintable(error));

    const QVector<TocNode> toc = doc->tableOfContents();
    QCOMPARE(toc.size(), 2);

    QCOMPARE(toc[0].title, QStringLiteral("Test chapter 1"));
    QCOMPARE(toc[0].pageNumber, 0);
    QVERIFY(toc[0].children.isEmpty());

    QCOMPARE(toc[1].title, QStringLiteral("Test chapter 2"));
    QCOMPARE(toc[1].pageNumber, 1);
    QCOMPARE(toc[1].children.size(), 2);

    QCOMPARE(toc[1].children[0].title, QStringLiteral("Test subchapter 2-1"));
    QCOMPARE(toc[1].children[0].pageNumber, 2);
    QCOMPARE(toc[1].children[1].title, QStringLiteral("Test subchapter 2-2"));
    QCOMPARE(toc[1].children[1].pageNumber, 2);
}

void MobiDocumentTest::refusesDrmProtectedFileWithoutDecrypting()
{
    QString error;
    auto doc = MobiDocument::load(fixturePath("test-drm.mobi"), &error);
    QVERIFY(!doc);
    QVERIFY2(error.contains(QStringLiteral("DRM"), Qt::CaseInsensitive), qPrintable(error));
}

void MobiDocumentTest::failsGracefullyOnMissingFile()
{
    QString error;
    auto doc = MobiDocument::load(fixturePath("does-not-exist.mobi"), &error);
    QVERIFY(!doc);
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(MobiDocumentTest)
#include "MobiDocumentTest.moc"
