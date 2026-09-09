#include "pdf/PopplerPdfDocument.h"
#include "ui/PdfPageStackView.h"

#include <QPoint>
#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}
}

// PdfPageStackView's two-page mode pairs pages (0,1), (2,3), ... side by
// side within a row, rows still stacked vertically -- these tests exercise
// its offset/hit-testing math directly (no rendering/painting needed, since
// every helper here is plain analytic geometry over page sizes) using
// tests/fixtures/test_multipage.pdf's 3 pages, which also covers the
// trailing-odd-page-with-no-partner case.
class PdfTwoPageModeTest : public QObject
{
    Q_OBJECT

private slots:
    void singlePageModeIsUnaffectedByDefault();
    void twoPageModeLaysPairedPagesSideBySide();
    void twoPageModeStacksRowsVertically();
    void trailingOddPageWithNoPartnerIsCenteredAlone();
    void pageIndexAtOffsetYReturnsRowsLeftIndex();
    void pageIndexAtResolvesLeftOrRightColumnFromX();
    void togglingBackToSinglePageModeRestoresOriginalLayout();

private:
    std::unique_ptr<PdfPageStackView> makeStackView(qreal zoom = 1.0);
};

std::unique_ptr<PdfPageStackView> PdfTwoPageModeTest::makeStackView(qreal zoom)
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test_multipage.pdf"), &error);
    Q_ASSERT_X(doc, "makeStackView", qPrintable(error));

    auto view = std::make_unique<PdfPageStackView>();
    // Ownership of `doc` stays with the caller's stack frame in the real
    // PdfView (m_document there); leaking it here is fine -- QVERIFY/QCOMPARE
    // failures abort the test binary anyway, and every other PdfPageStackView
    // test in this suite (PdfTextSelectionTest etc.) goes through PdfView,
    // which does own its document, so this is this file's own local
    // simplification, not a pattern copied elsewhere.
    view->setDocument(doc.release());
    view->setZoom(zoom);
    return view;
}

void PdfTwoPageModeTest::singlePageModeIsUnaffectedByDefault()
{
    auto view = makeStackView();
    QVERIFY(!view->twoPageMode());
    QCOMPARE(view->pageCount(), 3);

    // Every page in its own row, centered individually -- unchanged from
    // before two-page mode existed.
    QVERIFY(view->pageOffsetY(1) > view->pageOffsetY(0));
    QVERIFY(view->pageOffsetY(2) > view->pageOffsetY(1));
}

void PdfTwoPageModeTest::twoPageModeLaysPairedPagesSideBySide()
{
    auto view = makeStackView();
    view->setTwoPageMode(true);
    QVERIFY(view->twoPageMode());

    // Pages 0 and 1 share a row: same top, page 1 sits to the right of
    // page 0's right edge.
    QCOMPARE(view->pageOffsetY(0), view->pageOffsetY(1));
    QVERIFY(view->pageXOffset(1) >= view->pageXOffset(0) + view->pageWidthPx(0));
}

void PdfTwoPageModeTest::twoPageModeStacksRowsVertically()
{
    auto view = makeStackView();
    view->setTwoPageMode(true);

    // Page 2 (no partner -- only 3 pages) starts a new row, below row 0.
    QVERIFY(view->pageOffsetY(2) > view->pageOffsetY(0));
}

void PdfTwoPageModeTest::trailingOddPageWithNoPartnerIsCenteredAlone()
{
    auto view = makeStackView();
    view->setTwoPageMode(true);

    // Row 0 (pages 0+1 side by side) is wider than row 1 (page 2 alone), so
    // page 2 should be centered within that wider row, not flush left.
    QVERIFY(view->pageXOffset(2) > 0.0);
}

void PdfTwoPageModeTest::pageIndexAtOffsetYReturnsRowsLeftIndex()
{
    auto view = makeStackView();
    view->setTwoPageMode(true);

    // A Y within row 0's band resolves to page 0 (the row's left/even
    // index), regardless of whether it's nearer page 0's or page 1's own
    // (possibly different) height.
    QCOMPARE(view->pageIndexAtOffsetY(view->pageOffsetY(0) + 1), 0);
    QCOMPARE(view->pageIndexAtOffsetY(view->pageOffsetY(2) + 1), 2);
}

void PdfTwoPageModeTest::pageIndexAtResolvesLeftOrRightColumnFromX()
{
    auto view = makeStackView();
    view->setTwoPageMode(true);

    const int rowY = int(view->pageOffsetY(0)) + 1;
    QCOMPARE(view->pageIndexAt(QPoint(int(view->pageXOffset(0)), rowY)), 0);
    QCOMPARE(view->pageIndexAt(QPoint(int(view->pageXOffset(1)) + 1, rowY)), 1);
}

void PdfTwoPageModeTest::togglingBackToSinglePageModeRestoresOriginalLayout()
{
    auto view = makeStackView();
    const qreal originalPage1OffsetY = view->pageOffsetY(1);
    const qreal originalPage1OffsetX = view->pageXOffset(1);

    view->setTwoPageMode(true);
    QVERIFY(!qFuzzyCompare(view->pageOffsetY(1), originalPage1OffsetY));

    view->setTwoPageMode(false);
    QCOMPARE(view->pageOffsetY(1), originalPage1OffsetY);
    QCOMPARE(view->pageXOffset(1), originalPage1OffsetX);
}

QTEST_MAIN(PdfTwoPageModeTest)
#include "PdfTwoPageModeTest.moc"
