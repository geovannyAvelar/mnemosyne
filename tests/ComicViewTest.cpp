#include "comic/CbzDocument.h"
#include "ui/ComicView.h"
#include "ui/PdfPageCanvas.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTest>

#include <algorithm>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}
} // namespace

// test.cbz has exactly 3 real pages (page1.png, page2.png, page10.png --
// see CbzDocumentTest's own doc comment on why not 5), so page index 2 (the
// 3rd, last page) is the odd-one-out double-page mode's pairing has to fall
// back to showing alone.
class ComicViewTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void singlePageModeByDefault();
    void doublePageModeComposesTwoPagesSideBySide();
    void doublePageModeFallsBackToSingleOnTrailingOddPage();
    void goToOddPageSnapsDownToItsSpreadStart();
    void nextPageAdvancesByTwoInDoublePageMode();
    void togglingDoublePageModePersists();

private:
    std::unique_ptr<ComicView> makeComicView();
    // The single-page width/height CbzDocument itself would render page 0
    // at -- independently derived (not hardcoded) so this stays valid if
    // the fixture image changes.
    QSize singlePageSize(int index);
};

void ComicViewTest::initTestCase()
{
    // Isolate from the real app's settings (organizationName "Mnemosyne") --
    // see AppPersistenceTest for the same pattern. ComicReadingSettings is
    // global, so without this a test run could read/clobber a real user's
    // actual double-page-mode preference.
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void ComicViewTest::init()
{
    QSettings().clear(); // fresh state (single-page, left-to-right) for each test
}

void ComicViewTest::cleanupTestCase()
{
    QSettings().clear();
}

std::unique_ptr<ComicView> ComicViewTest::makeComicView()
{
    QString error;
    auto doc = CbzDocument::load(fixturePath("test.cbz"), &error);
    Q_ASSERT_X(doc, "makeComicView", qPrintable(error));
    auto view = std::make_unique<ComicView>(std::move(doc), fixturePath("test.cbz"));
    view->resize(900, 700);
    view->show();
    static_cast<void>(QTest::qWaitForWindowExposed(view.get()));
    return view;
}

QSize ComicViewTest::singlePageSize(int index)
{
    QString error;
    auto doc = CbzDocument::load(fixturePath("test.cbz"), &error);
    Q_ASSERT_X(doc, "singlePageSize", qPrintable(error));
    std::unique_ptr<IPage> page = doc->page(index);
    Q_ASSERT(page);
    return page->renderToImage(1.0).size(); // 1.0 == ComicView's own default m_zoom
}

void ComicViewTest::singlePageModeByDefault()
{
    auto view = makeComicView();
    auto *canvas = view->findChild<PdfPageCanvas *>();
    QVERIFY(canvas);
    QCOMPARE(canvas->size(), singlePageSize(0));
}

void ComicViewTest::doublePageModeComposesTwoPagesSideBySide()
{
    auto view = makeComicView();
    auto *canvas = view->findChild<PdfPageCanvas *>();
    QVERIFY(canvas);

    view->setDoublePageMode(true);

    const QSize page0 = singlePageSize(0);
    const QSize page1 = singlePageSize(1);
    // Matches ComicView::renderCurrentPage()'s own compose step: side by
    // side with a 4px gap, canvas height the taller of the two.
    QCOMPARE(canvas->width(), page0.width() + 4 + page1.width());
    QCOMPARE(canvas->height(), std::max(page0.height(), page1.height()));
}

void ComicViewTest::doublePageModeFallsBackToSingleOnTrailingOddPage()
{
    auto view = makeComicView();
    auto *canvas = view->findChild<PdfPageCanvas *>();
    QVERIFY(canvas);

    view->setDoublePageMode(true);
    view->goToPage(2); // the 3rd (last) page -- no partner to pair with

    QCOMPARE(view->currentPosition(), 2);
    QCOMPARE(canvas->size(), singlePageSize(2));
}

void ComicViewTest::goToOddPageSnapsDownToItsSpreadStart()
{
    auto view = makeComicView();
    view->setDoublePageMode(true);

    view->goToPage(1); // page 2 of the 0-1 spread -- not a spread start itself
    QCOMPARE(view->currentPosition(), 0);
}

void ComicViewTest::nextPageAdvancesByTwoInDoublePageMode()
{
    auto view = makeComicView();
    view->setDoublePageMode(true);
    QCOMPARE(view->currentPosition(), 0);

    view->nextPage();
    QCOMPARE(view->currentPosition(), 2); // the 2-page spread starting at 0 covers indices 0-1, so next starts at 2
}

void ComicViewTest::togglingDoublePageModePersists()
{
    {
        auto view = makeComicView();
        view->setDoublePageMode(true);
        view->setRightToLeft(true);
    }
    // A fresh view (as if the tab were closed and the file reopened) should
    // pick up the same preference -- it's global, not per-book (see
    // ComicReadingSettings's own doc comment for why).
    auto view = makeComicView();
    auto *canvas = view->findChild<PdfPageCanvas *>();
    QVERIFY(canvas);
    const QSize page0 = singlePageSize(0);
    const QSize page1 = singlePageSize(1);
    QCOMPARE(canvas->width(), page0.width() + 4 + page1.width()); // still composing a spread, not a single page
}

QTEST_MAIN(ComicViewTest)
#include "ComicViewTest.moc"
