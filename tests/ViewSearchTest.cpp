#include "epub/EpubDocument.h"
#include "pdf/PopplerPdfDocument.h"
#include "ui/EpubView.h"
#include "ui/PdfPageStackView.h"
#include "ui/PdfView.h"

#include <QCoreApplication>
#include <QTest>
#include <QTextBrowser>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}
} // namespace

// Exercises IReaderView::search() through real PdfView/EpubView instances
// wired to real documents, rather than mocking the interface — this is what
// SearchDock actually calls when the user searches.
class ViewSearchTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void pdfSearchFindsMatchAndReportsPageLabel();
    void pdfSearchReturnsEmptyForNoMatch();
    void epubSearchFindsMatchesAcrossChapters();
    void epubSearchIsCaseInsensitive();
    void epubSearchReturnsEmptyForNoMatch();

    void pdfSetSearchTermHighlightsMatchOnCurrentPage();
    void pdfSetSearchTermEmptyClearsHighlight();
    void epubSetSearchTermHighlightsMatchesInCurrentChapter();
    void epubSetSearchTermEmptyClearsHighlight();
};

void ViewSearchTest::initTestCase()
{
    // PdfView/EpubView now load highlights via HighlightStore (QSettings) on
    // construction; isolate from the real app's settings.
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void ViewSearchTest::pdfSearchFindsMatchAndReportsPageLabel()
{
    QString error;
    std::unique_ptr<IDocument> doc = PopplerPdfDocument::load(fixturePath("test.pdf"), &error);
    QVERIFY2(doc, qPrintable(error));

    PdfView view(std::move(doc), fixturePath("test.pdf"));
    const QVector<SearchResult> results = view.search(QStringLiteral("fixture"));

    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].targetIndex, 0);
    QCOMPARE(results[0].label, QStringLiteral("Page 1"));
    QVERIFY(results[0].snippet.contains(QStringLiteral("fixture"), Qt::CaseInsensitive));
}

void ViewSearchTest::pdfSearchReturnsEmptyForNoMatch()
{
    QString error;
    std::unique_ptr<IDocument> doc = PopplerPdfDocument::load(fixturePath("test.pdf"), &error);
    QVERIFY2(doc, qPrintable(error));

    PdfView view(std::move(doc), fixturePath("test.pdf"));
    QVERIFY(view.search(QStringLiteral("nonexistent-xyz")).isEmpty());
}

void ViewSearchTest::epubSearchFindsMatchesAcrossChapters()
{
    QString error;
    std::unique_ptr<EpubDocument> doc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY2(doc, qPrintable(error));

    EpubView view(std::move(doc), fixturePath("test.epub"));
    const QVector<SearchResult> results = view.search(QStringLiteral("chapter"));

    // Both "Chapter One" and "Chapter Two" headings contain the word.
    QCOMPARE(results.size(), 2);
    QCOMPARE(results[0].targetIndex, 0);
    QCOMPARE(results[0].label, QStringLiteral("Chapter 1"));
    QCOMPARE(results[1].targetIndex, 1);
    QCOMPARE(results[1].label, QStringLiteral("Chapter 2"));
}

void ViewSearchTest::epubSearchIsCaseInsensitive()
{
    QString error;
    std::unique_ptr<EpubDocument> doc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY2(doc, qPrintable(error));

    EpubView view(std::move(doc), fixturePath("test.epub"));

    // Chapter 1's body text contains "special <characters>" (only chapter 1).
    const QVector<SearchResult> results = view.search(QStringLiteral("SPECIAL"));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].targetIndex, 0);
}

void ViewSearchTest::epubSearchReturnsEmptyForNoMatch()
{
    QString error;
    std::unique_ptr<EpubDocument> doc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY2(doc, qPrintable(error));

    EpubView view(std::move(doc), fixturePath("test.epub"));
    QVERIFY(view.search(QStringLiteral("nonexistent-xyz")).isEmpty());
}

// Exercises IReaderView::setSearchTerm() — the yellow "dauber" overlay that
// marks every hit of the active search term, distinct from persisted
// Highlight annotations. See PdfPageStackView::searchRectsForPage() and
// EpubView::applyHighlightsToBrowser()'s search-format branch.
void ViewSearchTest::pdfSetSearchTermHighlightsMatchOnCurrentPage()
{
    QString error;
    std::unique_ptr<IDocument> doc = PopplerPdfDocument::load(fixturePath("test.pdf"), &error);
    QVERIFY2(doc, qPrintable(error));

    PdfView view(std::move(doc), fixturePath("test.pdf"));
    auto *stackView = view.findChild<PdfPageStackView *>();
    QVERIFY(stackView);
    QVERIFY(stackView->searchRectsForPage(0).isEmpty()); // nothing highlighted before searching

    // Fixture text: "Searchable PDF fixture text for full text search testing."
    // — "fixture" appears exactly once.
    view.setSearchTerm(QStringLiteral("fixture"));
    QCOMPARE(stackView->searchRectsForPage(0).size(), 1);
}

void ViewSearchTest::pdfSetSearchTermEmptyClearsHighlight()
{
    QString error;
    std::unique_ptr<IDocument> doc = PopplerPdfDocument::load(fixturePath("test.pdf"), &error);
    QVERIFY2(doc, qPrintable(error));

    PdfView view(std::move(doc), fixturePath("test.pdf"));
    auto *stackView = view.findChild<PdfPageStackView *>();
    QVERIFY(stackView);

    view.setSearchTerm(QStringLiteral("fixture"));
    QVERIFY(!stackView->searchRectsForPage(0).isEmpty());

    view.setSearchTerm(QString());
    QVERIFY(stackView->searchRectsForPage(0).isEmpty());
}

void ViewSearchTest::epubSetSearchTermHighlightsMatchesInCurrentChapter()
{
    QString error;
    std::unique_ptr<EpubDocument> doc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY2(doc, qPrintable(error));

    EpubView view(std::move(doc), fixturePath("test.epub"));
    auto *browser = view.findChild<QTextBrowser *>();
    QVERIFY(browser);
    QVERIFY(browser->extraSelections().isEmpty()); // nothing highlighted before searching

    // Chapter 1 contains "chapter" twice, case-insensitively: the "Chapter
    // One" heading and "the first chapter" in the intro paragraph.
    view.setSearchTerm(QStringLiteral("CHAPTER"));
    QCOMPARE(browser->extraSelections().size(), 2);
}

void ViewSearchTest::epubSetSearchTermEmptyClearsHighlight()
{
    QString error;
    std::unique_ptr<EpubDocument> doc = EpubDocument::load(fixturePath("test.epub"), &error);
    QVERIFY2(doc, qPrintable(error));

    EpubView view(std::move(doc), fixturePath("test.epub"));
    auto *browser = view.findChild<QTextBrowser *>();
    QVERIFY(browser);

    view.setSearchTerm(QStringLiteral("chapter"));
    QVERIFY(!browser->extraSelections().isEmpty());

    view.setSearchTerm(QString());
    QVERIFY(browser->extraSelections().isEmpty());
}

QTEST_MAIN(ViewSearchTest)
#include "ViewSearchTest.moc"
