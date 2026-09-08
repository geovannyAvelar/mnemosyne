#include "app/FileIdentity.h"
#include "app/InkStore.h"
#include "core/InkStroke.h"
#include "pdf/PopplerPdfDocument.h"
#include "ui/PdfPageStackView.h"
#include "ui/PdfView.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {

QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}

void sendDrag(QWidget *target, const QPoint &from, const QPoint &mid, const QPoint &to)
{
    QMouseEvent press(QEvent::MouseButtonPress, from, target->mapToGlobal(from), Qt::LeftButton, Qt::LeftButton,
                       Qt::NoModifier);
    QCoreApplication::sendEvent(target, &press);

    QMouseEvent move(QEvent::MouseMove, mid, target->mapToGlobal(mid), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(target, &move);

    QMouseEvent release(QEvent::MouseButtonRelease, to, target->mapToGlobal(to), Qt::LeftButton, Qt::LeftButton,
                         Qt::NoModifier);
    QCoreApplication::sendEvent(target, &release);
}

} // namespace

// Freehand ink annotations (see core/InkStroke.h, app/InkStore.h) are
// app-side only -- never written into the PDF file itself, same as
// highlights/notes. These tests exercise the real mouse-drag path through
// PdfPageStackView's draw mode end to end, the same way
// PdfTextSelectionTest.cpp does for text selection.
class PdfInkAnnotationTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void dragInDrawModeEmitsStrokeWithAtLeastTwoPoints();
    void dragWithoutDrawModeSelectsTextInstead();
    void turningOffDrawModeMidDragDropsInProgressStroke();
    void pdfViewPersistsDrawnStrokeAndReloadsIt();
    void clearPageDrawingsRemovesOnlyCurrentPage();
    void togglingDrawModeOnClearsAnyActiveTextSelection();

private:
    std::unique_ptr<PdfView> makePdfView();
    QPoint toViewportPoint(PdfPageStackView *view, int pageIndex, const QPoint &pageLocalPoint);
};

void PdfInkAnnotationTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
    QSettings().clear();
}

void PdfInkAnnotationTest::cleanupTestCase()
{
    QSettings().clear();
}

std::unique_ptr<PdfView> PdfInkAnnotationTest::makePdfView()
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test.pdf"), &error);
    Q_ASSERT_X(doc, "makePdfView", qPrintable(error));
    auto view = std::make_unique<PdfView>(std::move(doc), fixturePath("test.pdf"));
    view->resize(900, 700);
    view->show();
    static_cast<void>(QTest::qWaitForWindowExposed(view.get()));
    return view;
}

QPoint PdfInkAnnotationTest::toViewportPoint(PdfPageStackView *view, int pageIndex, const QPoint &pageLocalPoint)
{
    return pageLocalPoint + QPoint(int(view->pageXOffset(pageIndex)), int(view->pageOffsetY(pageIndex)));
}

void PdfInkAnnotationTest::dragInDrawModeEmitsStrokeWithAtLeastTwoPoints()
{
    auto view = makePdfView();
    auto *stackView = view->findChild<PdfPageStackView *>();
    QVERIFY(stackView);

    stackView->setDrawMode(true);
    QVERIFY(stackView->drawMode());

    QSignalSpy strokeSpy(stackView, &PdfPageStackView::inkStrokeDrawn);

    const QPoint start = toViewportPoint(stackView, 0, QPoint(50, 60));
    const QPoint mid = toViewportPoint(stackView, 0, QPoint(80, 90));
    const QPoint end = toViewportPoint(stackView, 0, QPoint(120, 60));
    sendDrag(stackView, start, mid, end);

    QCOMPARE(strokeSpy.count(), 1);
    const QList<QVariant> args = strokeSpy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 0); // page index
    const QVector<QPointF> points = args.at(1).value<QVector<QPointF>>();
    QVERIFY(points.size() >= 2);

    // A plain drag doesn't leave a live text selection behind -- draw mode
    // replaces selection entirely rather than doing both at once.
    QVERIFY(view->selectedText().isEmpty());
}

void PdfInkAnnotationTest::dragWithoutDrawModeSelectsTextInstead()
{
    auto view = makePdfView();
    auto *stackView = view->findChild<PdfPageStackView *>();
    QVERIFY(stackView);
    QVERIFY(!stackView->drawMode());

    QSignalSpy strokeSpy(stackView, &PdfPageStackView::inkStrokeDrawn);

    sendDrag(stackView, toViewportPoint(stackView, 0, QPoint(0, 0)), toViewportPoint(stackView, 0, QPoint(500, 0)),
             toViewportPoint(stackView, 0, QPoint(2000, 2000)));

    QCOMPARE(strokeSpy.count(), 0);
    QVERIFY(!view->selectedText().isEmpty());
}

void PdfInkAnnotationTest::turningOffDrawModeMidDragDropsInProgressStroke()
{
    auto view = makePdfView();
    auto *stackView = view->findChild<PdfPageStackView *>();
    QVERIFY(stackView);

    stackView->setDrawMode(true);
    QSignalSpy strokeSpy(stackView, &PdfPageStackView::inkStrokeDrawn);

    const QPoint start = toViewportPoint(stackView, 0, QPoint(50, 60));
    const QPoint mid = toViewportPoint(stackView, 0, QPoint(80, 90));

    QMouseEvent press(QEvent::MouseButtonPress, start, stackView->mapToGlobal(start), Qt::LeftButton,
                       Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(stackView, &press);
    QMouseEvent move(QEvent::MouseMove, mid, stackView->mapToGlobal(mid), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(stackView, &move);

    // Mode toggled off mid-drag, before release -- see
    // PdfPageStackView::setDrawMode()'s own doc comment for why this drops
    // the stroke instead of committing a possibly-incomplete one.
    stackView->setDrawMode(false);

    QMouseEvent release(QEvent::MouseButtonRelease, mid, stackView->mapToGlobal(mid), Qt::LeftButton,
                         Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(stackView, &release);

    QCOMPARE(strokeSpy.count(), 0);
}

void PdfInkAnnotationTest::pdfViewPersistsDrawnStrokeAndReloadsIt()
{
    const QString bookHash = FileIdentity::contentHash(fixturePath("test.pdf"));
    InkStore::clearPage(bookHash, 0); // leftover state from an earlier test function in this file

    auto view = makePdfView();
    auto *stackView = view->findChild<PdfPageStackView *>();
    QVERIFY(stackView);

    stackView->setDrawMode(true);
    sendDrag(stackView, toViewportPoint(stackView, 0, QPoint(50, 60)), toViewportPoint(stackView, 0, QPoint(80, 90)),
             toViewportPoint(stackView, 0, QPoint(120, 60)));

    const QVector<InkStroke> persisted = InkStore::strokesFor(bookHash);
    QCOMPARE(persisted.size(), 1);
    QCOMPARE(persisted[0].targetIndex, 0);
    QVERIFY(persisted[0].points.size() >= 2);

    // Reopening the same file should show the same stroke -- PdfView loads
    // via InkStore::strokesFor() in its constructor, same as highlights.
    view.reset();
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test.pdf"), &error);
    Q_ASSERT_X(doc, "test", qPrintable(error));
    auto reopened = std::make_unique<PdfView>(std::move(doc), fixturePath("test.pdf"));
    reopened->resize(900, 700);
    reopened->show();
    QVERIFY(QTest::qWaitForWindowExposed(reopened.get()));

    QCOMPARE(InkStore::strokesFor(bookHash).size(), 1);
}

void PdfInkAnnotationTest::clearPageDrawingsRemovesOnlyCurrentPage()
{
    const QString bookHash = FileIdentity::contentHash(fixturePath("test_multipage.pdf"));

    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test_multipage.pdf"), &error);
    Q_ASSERT_X(doc, "test", qPrintable(error));
    auto view = std::make_unique<PdfView>(std::move(doc), fixturePath("test_multipage.pdf"));
    view->resize(900, 700);
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view.get()));

    auto *stackView = view->findChild<PdfPageStackView *>();
    QVERIFY(stackView);

    InkStroke onPage0;
    onPage0.targetIndex = 0;
    onPage0.points = {QPointF(1, 1), QPointF(5, 5)};
    InkStore::addStroke(bookHash, onPage0);
    InkStroke onPage1;
    onPage1.targetIndex = 1;
    onPage1.points = {QPointF(1, 1), QPointF(5, 5)};
    InkStore::addStroke(bookHash, onPage1);
    stackView->setInkStrokes(InkStore::strokesFor(bookHash));

    QCOMPARE(view->currentPosition(), 0); // opens on page 0
    view->clearPageDrawings();

    const QVector<InkStroke> remaining = InkStore::strokesFor(bookHash);
    QCOMPARE(remaining.size(), 1);
    QCOMPARE(remaining[0].targetIndex, 1);
}

void PdfInkAnnotationTest::togglingDrawModeOnClearsAnyActiveTextSelection()
{
    auto view = makePdfView();
    auto *stackView = view->findChild<PdfPageStackView *>();
    QVERIFY(stackView);

    sendDrag(stackView, toViewportPoint(stackView, 0, QPoint(0, 0)), toViewportPoint(stackView, 0, QPoint(500, 0)),
             toViewportPoint(stackView, 0, QPoint(2000, 2000)));
    QVERIFY(!view->selectedText().isEmpty());

    view->toggleDrawMode(true);
    QVERIFY(view->selectedText().isEmpty());
}

QTEST_MAIN(PdfInkAnnotationTest)
#include "PdfInkAnnotationTest.moc"
