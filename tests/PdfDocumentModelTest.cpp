#include "quick/PdfDocumentModel.h"
#include "quick/PdfPageImageProvider.h"

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {

QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}

} // namespace

// PdfDocumentModel is the QML-facing counterpart to what IReaderView/PdfView
// give MainWindow on desktop -- this exercises it the same way the reader
// screen does: open a real PDF through the real (unmodified) Poppler
// backend, navigate/zoom, and confirm ReadingProgressStore persistence
// round-trips exactly as it does on desktop, just reached through this
// model's API instead of PdfView's.
class PdfDocumentModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void opensRealPdfAndReportsPageCount();
    void currentPageClampsToValidRange();
    void zoomClampsToBounds();
    void progressPersistsAcrossCloseAndReopen();
};

void PdfDocumentModelTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void PdfDocumentModelTest::init()
{
    QSettings().clear();
}

void PdfDocumentModelTest::cleanupTestCase()
{
    QSettings().clear();
}

void PdfDocumentModelTest::opensRealPdfAndReportsPageCount()
{
    PdfPageImageProvider provider;
    PdfDocumentModel model(&provider);

    QSignalSpy documentChangedSpy(&model, &PdfDocumentModel::documentChanged);
    QVERIFY(model.open(fixturePath(QStringLiteral("test_multipage.pdf")), QStringLiteral("pdf")));

    QVERIFY(model.isOpen());
    QCOMPARE(model.pageCount(), 3);
    QVERIFY(!model.bookHash().isEmpty());
    QVERIFY(documentChangedSpy.count() >= 1);
}

void PdfDocumentModelTest::currentPageClampsToValidRange()
{
    PdfPageImageProvider provider;
    PdfDocumentModel model(&provider);
    QVERIFY(model.open(fixturePath(QStringLiteral("test_multipage.pdf")), QStringLiteral("pdf")));

    model.setCurrentPage(-5);
    QCOMPARE(model.currentPage(), 0);

    model.setCurrentPage(999);
    QCOMPARE(model.currentPage(), model.pageCount() - 1);

    model.setCurrentPage(1);
    QCOMPARE(model.currentPage(), 1);
}

void PdfDocumentModelTest::zoomClampsToBounds()
{
    PdfPageImageProvider provider;
    PdfDocumentModel model(&provider);
    QVERIFY(model.open(fixturePath(QStringLiteral("test_multipage.pdf")), QStringLiteral("pdf")));

    model.setZoom(0.01);
    QCOMPARE(model.zoom(), 0.25);

    model.setZoom(100.0);
    QCOMPARE(model.zoom(), 4.0);

    model.setZoom(2.0);
    QCOMPARE(model.zoom(), 2.0);
}

void PdfDocumentModelTest::progressPersistsAcrossCloseAndReopen()
{
    PdfPageImageProvider provider;
    PdfDocumentModel model(&provider);
    const QString path = fixturePath(QStringLiteral("test_multipage.pdf"));

    QVERIFY(model.open(path, QStringLiteral("pdf")));
    model.setCurrentPage(2);
    model.setZoom(1.75);
    model.close(); // flushes the pending debounced save synchronously, same as the reader screen's Component.onDestruction

    QVERIFY(model.open(path, QStringLiteral("pdf")));
    QCOMPARE(model.currentPage(), 2);
    QCOMPARE(model.zoom(), 1.75);
}

QTEST_MAIN(PdfDocumentModelTest)
#include "PdfDocumentModelTest.moc"
