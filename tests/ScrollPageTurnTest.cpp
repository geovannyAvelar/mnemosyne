#include "app/FileIdentity.h"
#include "app/HighlightStore.h"
#include "core/Highlight.h"
#include "epub/EpubDocument.h"
#include "pdf/PopplerPdfDocument.h"
#include "ui/EpubView.h"
#include "ui/PdfPageStackView.h"
#include "ui/PdfView.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QKeyEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTest>
#include <QTextBrowser>
#include <QTextDocument>
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

void sendKeyPress(QWidget *target, int key)
{
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(target, &event);
}

} // namespace

// Exercises continuous-scroll behavior in PdfView/EpubView: scrolling past a
// page/chapter boundary continues seamlessly into the next page/chapter's
// content (no snap), and currentPosition() tracks whichever page/chapter is
// dominantly visible as the reader scrolls.
class ScrollPageTurnTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void pdfCurrentPageTracksScrollPosition();
    void pdfWheelScrollMovesViewportContinuously();
    void pdfDoesNotScrollPastLastPage();
    void pdfDoesNotScrollBeforeFirstPage();
    void pdfArrowDownScrollsViewport();
    void pdfArrowUpAtTopStaysAtTop();
    void pdfGoToPageScrollsToPageTop();
    void pdfZoomPreservesCurrentPage();

    void epubScrollingNearBottomLoadsNextChapter();
    void epubScrollingNearTopLoadsPreviousChapter();
    void epubGoToChapterResetsLoadedWindow();
    void epubHighlightSurvivesAcrossChapterWindow();

private:
    std::unique_ptr<PdfView> makePdfView();
    std::unique_ptr<EpubView> makeEpubView();
};

void ScrollPageTurnTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
    QSettings().clear(); // fresh state -- epubHighlightSurvivesAcrossChapterWindow persists a highlight
}

void ScrollPageTurnTest::cleanupTestCase()
{
    QSettings().clear(); // don't leave test settings behind on disk
}

std::unique_ptr<PdfView> ScrollPageTurnTest::makePdfView()
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test_multipage.pdf"), &error);
    Q_ASSERT_X(doc, "makePdfView", qPrintable(error));
    auto view = std::make_unique<PdfView>(std::move(doc), fixturePath("test_multipage.pdf"));
    view->resize(400, 300); // smaller than a rendered page, so there's real scroll range to test
    // The scroll area's viewport size / scrollbar range isn't fully
    // realized until the widget is actually shown.
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

void ScrollPageTurnTest::pdfCurrentPageTracksScrollPosition()
{
    auto view = makePdfView();
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);
    auto *stackView = view->findChild<PdfPageStackView *>();
    QVERIFY(stackView);
    QCOMPARE(stackView->pageCount(), 3); // test_multipage.pdf's page count

    QCOMPARE(view->currentPosition(), 0);

    // Scroll so page 1 dominates the viewport.
    scrollArea->verticalScrollBar()->setValue(int(stackView->pageOffsetY(1)));
    QCOMPARE(view->currentPosition(), 1);

    // ... and page 2's.
    scrollArea->verticalScrollBar()->setValue(int(stackView->pageOffsetY(2)));
    QCOMPARE(view->currentPosition(), 2);

    // Scrolling back up updates it again -- not a one-way ratchet.
    scrollArea->verticalScrollBar()->setValue(int(stackView->pageOffsetY(0)));
    QCOMPARE(view->currentPosition(), 0);
}

void ScrollPageTurnTest::pdfWheelScrollMovesViewportContinuously()
{
    auto view = makePdfView();
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);
    QVERIFY(scrollArea->verticalScrollBar()->maximum() > 0); // page taller than the 300px viewport

    const int before = scrollArea->verticalScrollBar()->value();
    sendWheelScroll(scrollArea->viewport(), -120); // scroll down
    QVERIFY(scrollArea->verticalScrollBar()->value() > before); // moved, and nothing snapped it back
}

void ScrollPageTurnTest::pdfDoesNotScrollPastLastPage()
{
    auto view = makePdfView();
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);

    scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());
    QCOMPARE(view->currentPosition(), 2); // test_multipage.pdf's last page (0-based)

    sendWheelScroll(scrollArea->viewport(), -120);
    QCoreApplication::processEvents();

    QCOMPARE(scrollArea->verticalScrollBar()->value(), scrollArea->verticalScrollBar()->maximum());
    QCOMPARE(view->currentPosition(), 2); // stayed put, no crash
}

void ScrollPageTurnTest::pdfDoesNotScrollBeforeFirstPage()
{
    auto view = makePdfView();
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);

    scrollArea->verticalScrollBar()->setValue(0);
    sendWheelScroll(scrollArea->viewport(), 120); // scroll up
    QCoreApplication::processEvents();

    QCOMPARE(scrollArea->verticalScrollBar()->value(), 0);
    QCOMPARE(view->currentPosition(), 0);
}

void ScrollPageTurnTest::pdfArrowDownScrollsViewport()
{
    auto view = makePdfView();
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);

    scrollArea->verticalScrollBar()->setValue(0);
    sendKeyPress(view.get(), Qt::Key_Down);

    QVERIFY(scrollArea->verticalScrollBar()->value() > 0);
}

void ScrollPageTurnTest::pdfArrowUpAtTopStaysAtTop()
{
    auto view = makePdfView();
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);

    scrollArea->verticalScrollBar()->setValue(0);
    sendKeyPress(view.get(), Qt::Key_Up);

    QCOMPARE(scrollArea->verticalScrollBar()->value(), 0); // clamped, no crash
    QCOMPARE(view->currentPosition(), 0);
}

void ScrollPageTurnTest::pdfGoToPageScrollsToPageTop()
{
    auto view = makePdfView();
    auto *scrollArea = view->findChild<QScrollArea *>();
    QVERIFY(scrollArea);
    auto *stackView = view->findChild<PdfPageStackView *>();
    QVERIFY(stackView);

    view->goToPage(2);
    QCOMPARE(view->currentPosition(), 2);

    // goToPage()'s scrollbar positioning is synchronous -- PdfPageStackView's
    // page offsets are plain analytic math, not layout geometry that needs a
    // tick to settle.
    QCOMPARE(scrollArea->verticalScrollBar()->value(), int(stackView->pageOffsetY(2)));
}

void ScrollPageTurnTest::pdfZoomPreservesCurrentPage()
{
    auto view = makePdfView();
    view->goToPage(1);
    QCoreApplication::processEvents();
    QTest::qWait(10);

    QCOMPARE(view->currentPosition(), 1);
    view->zoomIn();
    QCoreApplication::processEvents();
    QTest::qWait(10);

    QCOMPARE(view->currentPosition(), 1); // still on the same page after re-layout
}

void ScrollPageTurnTest::epubScrollingNearBottomLoadsNextChapter()
{
    auto view = makeEpubView();
    QCOMPARE(view->currentPosition(), 0);

    auto *browser = view->findChild<QTextBrowser *>();
    QVERIFY(browser);

    // This fixture's chapters are short enough that even both combined still
    // fit in the 300px viewport (min == max == 0 throughout) -- any
    // scroll-down gesture should still load the next chapter into the
    // document, exercising onScrolled()'s degenerate-range fallback (see
    // PdfView/EpubView::eventFilter's comment on it), even though nothing
    // actually scrolls far enough to change which chapter dominates the
    // viewport (that's covered separately for PDF, where the fixture has
    // real scroll range; see pdfCurrentPageTracksScrollPosition()).
    sendWheelScroll(browser->viewport(), -120);
    QTest::qWait(50); // let the deferred onScrolled() safety-net timer fire

    // Chapter 1's content is now actually loaded into the browser (grown,
    // not replaced).
    QString error;
    auto referenceDoc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY(referenceDoc);
    QTextDocument chapter1Reference;
    chapter1Reference.setHtml(referenceDoc->chapterHtml(1));
    const QString chapter1Snippet = chapter1Reference.toPlainText().trimmed().left(20);
    QVERIFY(!chapter1Snippet.isEmpty());
    QVERIFY(browser->toPlainText().contains(chapter1Snippet));
}

void ScrollPageTurnTest::epubScrollingNearTopLoadsPreviousChapter()
{
    auto view = makeEpubView();
    view->goToChapter(1);
    QCOMPARE(view->currentPosition(), 1);

    auto *browser = view->findChild<QTextBrowser *>();
    QVERIFY(browser);

    sendWheelScroll(browser->viewport(), 120); // scroll up
    QTest::qWait(50); // let the deferred onScrolled() safety-net timer fire

    QCOMPARE(view->currentPosition(), 0);
}

void ScrollPageTurnTest::epubGoToChapterResetsLoadedWindow()
{
    auto view = makeEpubView();
    auto *browser = view->findChild<QTextBrowser *>();
    QVERIFY(browser);

    QString error;
    auto referenceDoc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY(referenceDoc);
    QTextDocument chapter1Reference;
    chapter1Reference.setHtml(referenceDoc->chapterHtml(1));
    const QString chapter1Snippet = chapter1Reference.toPlainText().trimmed().left(20);
    QVERIFY(!chapter1Snippet.isEmpty());

    // Grow the window to include chapter 1 by scrolling down.
    sendWheelScroll(browser->viewport(), -120);
    QTest::qWait(50); // let the deferred onScrolled() safety-net timer fire
    QVERIFY(browser->toPlainText().contains(chapter1Snippet));

    // An explicit TOC-style jump back to chapter 0 should reset cleanly --
    // chapter 1's content shouldn't still be sitting in the document.
    view->goToChapter(0);
    QCOMPARE(view->currentPosition(), 0);
    QVERIFY(!browser->toPlainText().contains(chapter1Snippet));
}

void ScrollPageTurnTest::epubHighlightSurvivesAcrossChapterWindow()
{
    // Figure out a real snippet of chapter 0's text to highlight, via a
    // throwaway EpubDocument/QTextDocument pair (independent of the view).
    QString error;
    auto referenceDoc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY(referenceDoc);
    QTextDocument chapter0Reference;
    chapter0Reference.setHtml(referenceDoc->chapterHtml(0));
    // QTextDocument::find() doesn't match text spanning a paragraph/block
    // boundary (blocks are joined internally by U+2029, not '\n', even
    // though toPlainText() renders both as '\n') -- skip past the first
    // paragraph break so the snippet stays within a single block.
    const QString chapter0Text = chapter0Reference.toPlainText();
    const int afterFirstBreak = chapter0Text.indexOf(QLatin1Char('\n')) + 1;
    const QString snippet = chapter0Text.mid(afterFirstBreak, 15).trimmed();
    QVERIFY(!snippet.isEmpty());

    // Persist a highlight for chapter 0 directly via HighlightStore (the
    // same store EpubView::addHighlightForSelection writes to), keyed by
    // this fixture's content hash, then have the view pick it up.
    Highlight highlight;
    highlight.targetIndex = 0;
    highlight.text = snippet;
    highlight.createdAt = QDateTime::currentDateTime();
    const QString bookHash = FileIdentity::contentHash(fixturePath("test.epub"));
    HighlightStore::addHighlight(bookHash, highlight);

    auto view = makeEpubView();
    view->refreshHighlights();
    auto *browser = view->findChild<QTextBrowser *>();
    QVERIFY(browser);
    QVERIFY(!browser->extraSelections().isEmpty()); // applied while chapter 0 is still the only loaded chapter

    // Grow the window into chapter 1 -- the chapter-0 highlight should still
    // be applied as an extra selection, not dropped just because chapter 0
    // is no longer the only loaded chapter.
    sendWheelScroll(browser->viewport(), -120);
    QTest::qWait(50); // let the deferred onScrolled() safety-net timer fire

    QVERIFY(!browser->extraSelections().isEmpty());
}

QTEST_MAIN(ScrollPageTurnTest)
#include "ScrollPageTurnTest.moc"
