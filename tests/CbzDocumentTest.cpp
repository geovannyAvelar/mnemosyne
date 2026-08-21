#include "comic/CbzDocument.h"

#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}
} // namespace

class CbzDocumentTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsTitleAndFiltersNonImageEntries();
    void sortsPagesNaturally();
    void hasNoTextLayer();
    void failsGracefullyOnMissingFile();
};

void CbzDocumentTest::loadsTitleAndFiltersNonImageEntries()
{
    QString error;
    auto doc = CbzDocument::load(fixturePath("test.cbz"), &error);
    QVERIFY2(doc, qPrintable(error));

    QCOMPARE(doc->title(), QStringLiteral("test"));
    // The fixture has 3 real page images plus ComicInfo.xml and a
    // __MACOSX/._page1.png junk entry, which must not be counted as pages.
    QCOMPARE(doc->pageCount(), 3);
}

void CbzDocumentTest::sortsPagesNaturally()
{
    QString error;
    auto doc = CbzDocument::load(fixturePath("test.cbz"), &error);
    QVERIFY2(doc, qPrintable(error));

    // The fixture's entries are page1.png (red), page10.png (green), and
    // page2.png (blue), added to the zip in that (non-numeric) order —
    // reading order must still come out 1, 2, 10, not the zip's own order
    // or a plain lexicographic sort (which would put page10 before page2).
    const std::unique_ptr<IPage> page0 = doc->page(0);
    const std::unique_ptr<IPage> page1 = doc->page(1);
    const std::unique_ptr<IPage> page2 = doc->page(2);
    QVERIFY(page0 && page1 && page2);

    QCOMPARE(page0->renderToImage(1.0).pixelColor(0, 0), QColor(255, 0, 0));
    QCOMPARE(page1->renderToImage(1.0).pixelColor(0, 0), QColor(0, 0, 255));
    QCOMPARE(page2->renderToImage(1.0).pixelColor(0, 0), QColor(0, 255, 0));
}

void CbzDocumentTest::hasNoTextLayer()
{
    QString error;
    auto doc = CbzDocument::load(fixturePath("test.cbz"), &error);
    QVERIFY2(doc, qPrintable(error));

    const std::unique_ptr<IPage> page = doc->page(0);
    QVERIFY(page);
    QVERIFY(page->text().isEmpty());
    QVERIFY(page->words().isEmpty());
    QCOMPARE(page->sizePoints(), QSizeF(10, 10));
}

void CbzDocumentTest::failsGracefullyOnMissingFile()
{
    QString error;
    auto doc = CbzDocument::load(fixturePath("does-not-exist.cbz"), &error);
    QVERIFY(!doc);
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(CbzDocumentTest)
#include "CbzDocumentTest.moc"
