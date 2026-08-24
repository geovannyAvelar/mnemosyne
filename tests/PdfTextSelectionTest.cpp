#include "app/FileIdentity.h"
#include "app/HighlightStore.h"
#include "core/Highlight.h"
#include "pdf/PopplerPdfDocument.h"
#include "ui/PdfPageCanvas.h"
#include "ui/PdfView.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QSettings>
#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {

QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}

void sendDrag(QWidget *target, const QPoint &from, const QPoint &to)
{
    QMouseEvent press(QEvent::MouseButtonPress, from, target->mapToGlobal(from), Qt::LeftButton,
                       Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(target, &press);

    QMouseEvent move(QEvent::MouseMove, to, target->mapToGlobal(to), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(target, &move);

    QMouseEvent release(QEvent::MouseButtonRelease, to, target->mapToGlobal(to), Qt::LeftButton,
                         Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(target, &release);
}

} // namespace

// Selection is resolved word-by-word from Poppler's text boxes (see
// PdfView::updateSelectionFromDrag), not one rectangle spanning whatever
// pixels the drag happened to cover — these tests exercise that end to end
// through real synthetic mouse events on the actual canvas widget, the same
// way a user's click-drag arrives.
class PdfTextSelectionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void dragOverWholePageSelectsAllWordsInOrder();
    void clickOnSingleWordSelectsOnlyThatWord();
    void copySelectionPutsSelectedTextOnClipboard();
    void selectionMovesLiveDuringDragBeforeRelease();
    void addedHighlightCoversFullWordSnappedSelection();

private:
    std::unique_ptr<PdfView> makePdfView();
    // Center of the given word's bounding box, in canvas pixels, computed
    // independently via Poppler — not hardcoded — so the test stays valid if
    // the fixture text or rendering changes.
    QPoint wordCenterPixel(int wordIndex, qreal zoom = 1.5);
};

void PdfTextSelectionTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
    QSettings().clear(); // fresh state -- this file now persists highlights too
}

void PdfTextSelectionTest::cleanupTestCase()
{
    QSettings().clear(); // don't leave test settings behind on disk
}

std::unique_ptr<PdfView> PdfTextSelectionTest::makePdfView()
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

QPoint PdfTextSelectionTest::wordCenterPixel(int wordIndex, qreal zoom)
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test.pdf"), &error);
    Q_ASSERT_X(doc, "wordCenterPixel", qPrintable(error));
    std::unique_ptr<IPage> page = doc->page(0);
    Q_ASSERT(page);
    const QVector<TextWord> words = page->words();
    Q_ASSERT(wordIndex >= 0 && wordIndex < words.size());
    const QPointF center = words[wordIndex].boundingBox.center();
    return QPoint(static_cast<int>(center.x() * zoom), static_cast<int>(center.y() * zoom));
}

void PdfTextSelectionTest::dragOverWholePageSelectsAllWordsInOrder()
{
    auto view = makePdfView();
    auto *canvas = view->findChild<PdfPageCanvas *>();
    QVERIFY(canvas);

    sendDrag(canvas, QPoint(0, 0), QPoint(2000, 2000));

    // Fixture text (tests/fixtures/test.pdf):
    // "Searchable PDF fixture text for full text search testing."
    QCOMPARE(view->selectedText(), QStringLiteral("Searchable PDF fixture text for full text search testing."));
}

void PdfTextSelectionTest::clickOnSingleWordSelectsOnlyThatWord()
{
    auto view = makePdfView();
    auto *canvas = view->findChild<PdfPageCanvas *>();
    QVERIFY(canvas);

    // Word 0 is "Searchable". A small drag fully contained within its box
    // (real movement, but short of reaching word 1) should select just that
    // one word, not the whole line — this is the behavior a single
    // bounding-rectangle selection couldn't give you.
    const QPoint point = wordCenterPixel(0);
    sendDrag(canvas, point - QPoint(3, 0), point + QPoint(3, 0));

    QCOMPARE(view->selectedText(), QStringLiteral("Searchable"));
}

void PdfTextSelectionTest::copySelectionPutsSelectedTextOnClipboard()
{
    auto view = makePdfView();
    auto *canvas = view->findChild<PdfPageCanvas *>();
    QVERIFY(canvas);

    const QPoint point = wordCenterPixel(1); // "PDF"
    sendDrag(canvas, point - QPoint(3, 0), point + QPoint(3, 0));
    QCOMPARE(view->selectedText(), QStringLiteral("PDF"));

    QApplication::clipboard()->clear();
    view->copySelection();

    QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("PDF"));
}

void PdfTextSelectionTest::selectionMovesLiveDuringDragBeforeRelease()
{
    auto view = makePdfView();
    auto *canvas = view->findChild<PdfPageCanvas *>();
    QVERIFY(canvas);

    const QPoint start = wordCenterPixel(0); // "Searchable"
    const QPoint mid = wordCenterPixel(2); // "fixture"

    QMouseEvent press(QEvent::MouseButtonPress, start, canvas->mapToGlobal(start), Qt::LeftButton,
                       Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(canvas, &press);

    QMouseEvent move(QEvent::MouseMove, mid, canvas->mapToGlobal(mid), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(canvas, &move);

    // Still mid-drag — no release yet — but the selection should already
    // reflect words 0..2, not be empty until the button comes up.
    QCOMPARE(view->selectedText(), QStringLiteral("Searchable PDF fixture"));

    QMouseEvent release(QEvent::MouseButtonRelease, mid, canvas->mapToGlobal(mid), Qt::LeftButton,
                         Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(canvas, &release);

    QCOMPARE(view->selectedText(), QStringLiteral("Searchable PDF fixture"));
}

void PdfTextSelectionTest::addedHighlightCoversFullWordSnappedSelection()
{
    auto view = makePdfView();
    auto *canvas = view->findChild<PdfPageCanvas *>();
    QVERIFY(canvas);

    // Start the drag inside word 0's right half and end it inside word 2's
    // left half -- nearestWordIndex() still resolves this to the full
    // "Searchable PDF fixture" (words 0-2), same as a real user's imprecise
    // drag, but the *raw* pixel span of the drag is narrower than the union
    // of the three words' boxes: it starts to the right of word 0's left
    // edge and ends to the left of word 2's right edge. A highlight built
    // from that raw span (rather than the word-snapped selection) would
    // clip the outer edges of word 0 and word 2 -- the reported bug.
    const QPoint start = wordCenterPixel(0) + QPoint(4, 0);
    const QPoint end = wordCenterPixel(2) - QPoint(4, 0);
    sendDrag(canvas, start, end);
    QCOMPARE(view->selectedText(), QStringLiteral("Searchable PDF fixture"));

    view->addHighlightForSelection();

    const QString bookHash = FileIdentity::contentHash(fixturePath("test.pdf"));
    const QVector<Highlight> highlights = HighlightStore::highlightsFor(bookHash);
    QCOMPARE(highlights.size(), 1);

    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test.pdf"), &error);
    Q_ASSERT_X(doc, "test", qPrintable(error));
    std::unique_ptr<IPage> page = doc->page(0);
    Q_ASSERT(page);
    const QVector<TextWord> words = page->words();

    for (int i = 0; i <= 2; ++i) {
        QVERIFY2(highlights[0].pageRect.contains(words[i].boundingBox),
                 qPrintable(QStringLiteral("word %1's box isn't fully covered by the saved highlight").arg(i)));
    }
}

QTEST_MAIN(PdfTextSelectionTest)
#include "PdfTextSelectionTest.moc"
