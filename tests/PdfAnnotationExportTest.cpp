#include "core/Highlight.h"
#include "core/InkStroke.h"
#include "pdf/PopplerPdfDocument.h"

#include <poppler-qt6.h>

#include <QColor>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}
} // namespace

// PopplerPdfDocument::exportAnnotated() writes Highlight/InkStroke entries
// into a new PDF as real Poppler::HighlightAnnotation/InkAnnotation objects
// -- unlike every other test in this suite, verifying that means opening
// the *output* file with raw Poppler-Qt (not through PopplerPdfDocument's
// own IDocument wrapper, which doesn't expose annotations() at all), the
// same way any other PDF reader would see it.
class PdfAnnotationExportTest : public QObject
{
    Q_OBJECT

private slots:
    void exportedHighlightAppearsAsRealAnnotationWithCorrectPositionAndColor();
    void highlightNoteBecomesAnnotationContents();
    void exportedInkStrokeAppearsAsRealInkAnnotation();
    void entriesTargetingOutOfRangePageAreSkippedNotCrashed();
    void originalFileIsNeverModified();

private:
    std::unique_ptr<PopplerPdfDocument> loadFixture(const QString &name = QStringLiteral("test.pdf"));
};

std::unique_ptr<PopplerPdfDocument> PdfAnnotationExportTest::loadFixture(const QString &name)
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath(name), &error);
    Q_ASSERT_X(doc, "loadFixture", qPrintable(error));
    return doc;
}

void PdfAnnotationExportTest::exportedHighlightAppearsAsRealAnnotationWithCorrectPositionAndColor()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString outputPath = tempDir.filePath(QStringLiteral("annotated.pdf"));

    auto doc = loadFixture();
    const std::unique_ptr<IPage> page0 = doc->page(0);
    QVERIFY(page0);
    const QSizeF sizePoints = page0->sizePoints();

    Highlight highlight;
    highlight.targetIndex = 0;
    highlight.pageRect = QRectF(sizePoints.width() * 0.1, sizePoints.height() * 0.2, sizePoints.width() * 0.3,
                                 sizePoints.height() * 0.05);
    highlight.color = QColor(255, 0, 0);
    highlight.text = QStringLiteral("some text");
    highlight.createdAt = QDateTime::currentDateTime();

    QVERIFY(doc->exportAnnotated(outputPath, {highlight}, {}));
    QVERIFY(QFile::exists(outputPath));

    auto reopened = Poppler::Document::load(outputPath);
    QVERIFY(reopened);
    const std::unique_ptr<Poppler::Page> poppPage = reopened->page(0);
    QVERIFY(poppPage);

    const std::vector<std::unique_ptr<Poppler::Annotation>> annotations = poppPage->annotations();
    QCOMPARE(static_cast<int>(annotations.size()), 1);
    QCOMPARE(annotations.front()->subType(), Poppler::Annotation::AHighlight);

    auto *highlightAnnotation = static_cast<Poppler::HighlightAnnotation *>(annotations.front().get());
    const QList<Poppler::HighlightAnnotation::Quad> quads = highlightAnnotation->highlightQuads();
    QCOMPARE(quads.size(), 1);

    // Normalized coordinates (0,0 top-left, 1,1 bottom-right) -- the quad's
    // top-left corner should land back at the ratio the input rect was
    // placed at, within a small tolerance for the round trip through
    // Poppler's own internal fixed-point/matrix representation.
    const QPointF topLeft = quads.first().points[0];
    QVERIFY(qAbs(topLeft.x() - 0.1) < 0.01);
    QVERIFY(qAbs(topLeft.y() - 0.2) < 0.01);

    QCOMPARE(highlightAnnotation->style().color(), QColor(255, 0, 0));
}

void PdfAnnotationExportTest::highlightNoteBecomesAnnotationContents()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString outputPath = tempDir.filePath(QStringLiteral("annotated.pdf"));

    auto doc = loadFixture();
    const std::unique_ptr<IPage> page0 = doc->page(0);
    QVERIFY(page0);
    const QSizeF sizePoints = page0->sizePoints();

    Highlight highlight;
    highlight.targetIndex = 0;
    highlight.pageRect = QRectF(10, 10, 100, 20);
    highlight.note = QStringLiteral("Remember this passage");
    Q_UNUSED(sizePoints);

    QVERIFY(doc->exportAnnotated(outputPath, {highlight}, {}));

    auto reopened = Poppler::Document::load(outputPath);
    QVERIFY(reopened);
    const std::unique_ptr<Poppler::Page> poppPage = reopened->page(0);
    QVERIFY(poppPage);
    const std::vector<std::unique_ptr<Poppler::Annotation>> annotations = poppPage->annotations();
    QCOMPARE(static_cast<int>(annotations.size()), 1);
    QCOMPARE(annotations.front()->contents(), QStringLiteral("Remember this passage"));
}

void PdfAnnotationExportTest::exportedInkStrokeAppearsAsRealInkAnnotation()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString outputPath = tempDir.filePath(QStringLiteral("annotated.pdf"));

    auto doc = loadFixture();

    InkStroke stroke;
    stroke.targetIndex = 0;
    stroke.points = {QPointF(20, 20), QPointF(40, 40), QPointF(60, 20)};
    stroke.color = QColor(0, 128, 0);
    stroke.width = 3.0;

    QVERIFY(doc->exportAnnotated(outputPath, {}, {stroke}));

    auto reopened = Poppler::Document::load(outputPath);
    QVERIFY(reopened);
    const std::unique_ptr<Poppler::Page> poppPage = reopened->page(0);
    QVERIFY(poppPage);
    const std::vector<std::unique_ptr<Poppler::Annotation>> annotations = poppPage->annotations();
    QCOMPARE(static_cast<int>(annotations.size()), 1);
    QCOMPARE(annotations.front()->subType(), Poppler::Annotation::AInk);

    auto *inkAnnotation = static_cast<Poppler::InkAnnotation *>(annotations.front().get());
    QCOMPARE(inkAnnotation->inkPaths().size(), 1);
    QCOMPARE(inkAnnotation->inkPaths().first().size(), 3);
    QCOMPARE(inkAnnotation->style().color(), QColor(0, 128, 0));
}

void PdfAnnotationExportTest::entriesTargetingOutOfRangePageAreSkippedNotCrashed()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString outputPath = tempDir.filePath(QStringLiteral("annotated.pdf"));

    auto doc = loadFixture(); // 1 page

    Highlight offPageHighlight;
    offPageHighlight.targetIndex = 5; // doesn't exist
    offPageHighlight.pageRect = QRectF(10, 10, 100, 20);

    InkStroke offPageStroke;
    offPageStroke.targetIndex = 5;
    offPageStroke.points = {QPointF(0, 0), QPointF(10, 10)};

    QVERIFY(doc->exportAnnotated(outputPath, {offPageHighlight}, {offPageStroke}));
    QVERIFY(QFile::exists(outputPath));

    auto reopened = Poppler::Document::load(outputPath);
    QVERIFY(reopened);
    QCOMPARE(reopened->numPages(), 1);
    const std::unique_ptr<Poppler::Page> poppPage = reopened->page(0);
    QVERIFY(poppPage);
    QVERIFY(poppPage->annotations().empty());
}

void PdfAnnotationExportTest::originalFileIsNeverModified()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString outputPath = tempDir.filePath(QStringLiteral("annotated.pdf"));
    const QString originalPath = fixturePath(QStringLiteral("test.pdf"));

    QFile original(originalPath);
    const qint64 originalSize = original.size();

    auto doc = loadFixture();
    Highlight highlight;
    highlight.targetIndex = 0;
    highlight.pageRect = QRectF(10, 10, 100, 20);
    QVERIFY(doc->exportAnnotated(outputPath, {highlight}, {}));

    QCOMPARE(QFile(originalPath).size(), originalSize);
    QVERIFY(outputPath != originalPath);
}

QTEST_MAIN(PdfAnnotationExportTest)
#include "PdfAnnotationExportTest.moc"
