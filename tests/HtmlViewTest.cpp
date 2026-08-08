#include "ui/HtmlView.h"

#include <QCoreApplication>
#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}
} // namespace

class HtmlViewTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void fallsBackToFilenameTitleBeforeLoadCompletes();
    void realPageTitleLoadsAsynchronously();
    void zoomInIncreasesZoomFactor();
    void zoomOutDecreasesZoomFactor();
    void zoomIsClampedAtBounds();
    void hasNoChapterNavigationOrToc();
    void searchAlwaysReturnsEmpty();
};

void HtmlViewTest::initTestCase()
{
    // QWebEngineView's default profile persists cache/storage scoped by the
    // app's org/app name; isolate from the real app the same way the other
    // QSettings-backed tests do, as a precaution.
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void HtmlViewTest::fallsBackToFilenameTitleBeforeLoadCompletes()
{
    HtmlView view(fixturePath("test.html"));
    // Immediately after construction the page hasn't loaded yet, so this can
    // only be the filename-derived fallback, never the real <title>.
    QCOMPARE(view.documentTitle(), QStringLiteral("test"));
}

void HtmlViewTest::realPageTitleLoadsAsynchronously()
{
    HtmlView view(fixturePath("test.html"));

    // Proves WebEngine actually loaded the real file from disk (not just
    // that the widget constructed without crashing): the fixture's <title>
    // contains an entity that must be decoded, same as the other formats.
    QTRY_COMPARE_WITH_TIMEOUT(view.documentTitle(), QStringLiteral("Test HTML & Friends"), 15000);
}

void HtmlViewTest::zoomInIncreasesZoomFactor()
{
    HtmlView view(fixturePath("test.html"));
    const qreal baseline = view.currentZoomFactor();

    view.zoomIn();
    QVERIFY(view.currentZoomFactor() > baseline);
}

void HtmlViewTest::zoomOutDecreasesZoomFactor()
{
    HtmlView view(fixturePath("test.html"));
    const qreal baseline = view.currentZoomFactor();

    view.zoomOut();
    QVERIFY(view.currentZoomFactor() < baseline);
}

void HtmlViewTest::zoomIsClampedAtBounds()
{
    HtmlView view(fixturePath("test.html"));

    for (int i = 0; i < 30; ++i) {
        view.zoomIn();
    }
    const qreal maxReached = view.currentZoomFactor();
    view.zoomIn();
    QCOMPARE(view.currentZoomFactor(), maxReached);

    for (int i = 0; i < 40; ++i) {
        view.zoomOut();
    }
    const qreal minReached = view.currentZoomFactor();
    view.zoomOut();
    QCOMPARE(view.currentZoomFactor(), minReached);
}

void HtmlViewTest::hasNoChapterNavigationOrToc()
{
    HtmlView view(fixturePath("test.html"));
    QVERIFY(view.tableOfContents().isEmpty());
    QCOMPARE(view.currentPosition(), 0);

    TocNode node;
    node.pageNumber = 0;
    view.goToTocNode(node); // must not crash; it's a documented no-op
}

void HtmlViewTest::searchAlwaysReturnsEmpty()
{
    HtmlView view(fixturePath("test.html"));
    // Documented limitation: WebEngine has no synchronous text-extraction
    // API, so search() can't satisfy IReaderView's synchronous contract.
    QVERIFY(view.search(QStringLiteral("zebras")).isEmpty());
}

QTEST_MAIN(HtmlViewTest)
#include "HtmlViewTest.moc"
