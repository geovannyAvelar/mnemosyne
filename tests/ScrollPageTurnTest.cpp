#include "epub/EpubDocument.h"
#include "pdf/PopplerPdfDocument.h"
#include "ui/EpubView.h"
#include "ui/PdfView.h"

#include <QCoreApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QTest>
#include <QTextBrowser>
#include <QWheelEvent>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {

QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}

// Qt convention: negative angleDelta().y() == scrolled down; positive == up.
void sendWheelScroll(QWidget *target, int angleDeltaY)
{
    QWheelEvent event(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, angleDeltaY),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(target, &event);
}

} // namespace

// Exercises the "scroll past the page edge turns the page" behavior added to
// PdfView/EpubView's event filters. The scroll area/browser are located via
// findChild() rather than adding test-only public accessors, since they're
// already discoverable through the QObject parent-child tree Qt itself uses.
class ScrollPageTurnTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void pdfScrollingPastBottomAdvancesAndLandsAtTop();
    void pdfScrollingPastTopGoesBackAndLandsAtBottom();
    void pdfDoesNotAdvancePastLastPage();
    void pdfDoesNotGoBeforeFirstPage();
    void pdfCooldownPreventsDoublePageTurn();

    void epubScrollingPastBottomAdvancesChapter();
    void epubScrollingPastTopGoesToPreviousChapter();

private:
    std::unique_ptr<PdfView> makePdfView();
    std::unique_ptr<EpubView> makeEpubView();
};

void ScrollPageTurnTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

std::unique_ptr<PdfView> ScrollPageTurnTest::makePdfView()
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test_multipage.pdf"), &error);
    Q_ASSERT_X(doc, "makePdfView", qPrintable(error));
    auto view = std::make_unique<PdfView>(std::move(doc), fixturePath("test_multipage.pdf"));
    view->resize(400, 300); // smaller than a rendered page, so there's real scroll range to test
    // Layout (and thus the scroll area's viewport size / scrollbar range)
    // isn't fully realized until the widget is actually shown.
    view->show();
    static_cast<void>(QTest::qWaitForWindowExposed(view.get()));
    return view;
}

std::unique_ptr<EpubView> ScrollPageTurnTest::makeEpubView()
{
    QString error;
    auto doc = EpubDocument::load(fixturePath("test.epub"), &error);
    Q_ASSERT_X(doc, "makeEpubView", qPrintable(error));
    auto view = std::make_unique<EpubView>(std::move(doc), fixturePath("test.epub"));
    view->resize(400, 300);
    view->show();
    static_cast<void>(QTest::qWaitForWindowExposed(view.get()));
    return view;
}

void ScrollPageTurnTest::pdfScrollingPastBottomAdvancesAndLandsAtTop()
{
    auto view = makePdfView();
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);

    QVERIFY(scrollArea->verticalScrollBar()->maximum() > 0); // page taller than the 300px viewport
    scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());

    sendWheelScroll(scrollArea->viewport(), -120);

    QCOMPARE(view->currentPosition(), 1);
    QCOMPARE(scrollArea->verticalScrollBar()->value(), 0);
}

void ScrollPageTurnTest::pdfScrollingPastTopGoesBackAndLandsAtBottom()
{
    auto view = makePdfView();
    view->goToPage(1);
    QCoreApplication::processEvents();
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);

    scrollArea->verticalScrollBar()->setValue(0);
    sendWheelScroll(scrollArea->viewport(), 120);

    QCOMPARE(view->currentPosition(), 0);

    // Landing at the bottom is deferred a tick (the new page's scroll range
    // isn't known until layout catches up), so let the event loop run once.
    QTRY_COMPARE_WITH_TIMEOUT(scrollArea->verticalScrollBar()->value(), scrollArea->verticalScrollBar()->maximum(), 2000);
    QVERIFY(scrollArea->verticalScrollBar()->value() > 0); // genuinely at the bottom, not just stuck at 0
}

void ScrollPageTurnTest::pdfDoesNotAdvancePastLastPage()
{
    auto view = makePdfView();
    view->goToPage(2); // the fixture's last page (0-based index 2 of 3)
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);

    scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());
    sendWheelScroll(scrollArea->viewport(), -120);

    QCOMPARE(view->currentPosition(), 2); // stayed put, no crash
}

void ScrollPageTurnTest::pdfDoesNotGoBeforeFirstPage()
{
    auto view = makePdfView();
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);

    scrollArea->verticalScrollBar()->setValue(0);
    sendWheelScroll(scrollArea->viewport(), 120);

    QCOMPARE(view->currentPosition(), 0);
}

void ScrollPageTurnTest::pdfCooldownPreventsDoublePageTurn()
{
    auto view = makePdfView();
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);

    scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());
    sendWheelScroll(scrollArea->viewport(), -120); // triggers page 0 -> 1
    QCOMPARE(view->currentPosition(), 1);

    // Immediately simulate "already at the bottom of the new page too" and
    // scroll again, well within the cooldown window.
    scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());
    sendWheelScroll(scrollArea->viewport(), -120);

    QCOMPARE(view->currentPosition(), 1); // did NOT skip ahead to page 2
}

void ScrollPageTurnTest::epubScrollingPastBottomAdvancesChapter()
{
    auto view = makeEpubView();
    auto *browser = view->findChild<QTextBrowser *>();
    QVERIFY(browser);

    // This fixture's chapters are short enough that min==max==0, so any
    // scroll-down should turn the page immediately (see the shared
    // reasoning in PdfView::eventFilter's comment).
    sendWheelScroll(browser->viewport(), -120);

    QCOMPARE(view->currentPosition(), 1);
}

void ScrollPageTurnTest::epubScrollingPastTopGoesToPreviousChapter()
{
    auto view = makeEpubView();
    view->goToChapter(1);
    auto *browser = view->findChild<QTextBrowser *>();
    QVERIFY(browser);

    sendWheelScroll(browser->viewport(), 120);

    QCOMPARE(view->currentPosition(), 0);
}

QTEST_MAIN(ScrollPageTurnTest)
#include "ScrollPageTurnTest.moc"
