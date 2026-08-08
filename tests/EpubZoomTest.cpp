#include "epub/EpubDocument.h"
#include "ui/EpubView.h"

#include <QCoreApplication>
#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}

std::unique_ptr<EpubView> makeView()
{
    QString error;
    std::unique_ptr<EpubDocument> doc = EpubDocument::load(fixturePath("test.epub"), &error);
    Q_ASSERT_X(doc, "makeView", qPrintable(error));
    return std::make_unique<EpubView>(std::move(doc), fixturePath("test.epub"));
}
} // namespace

// Exercises EpubView's own zoom bookkeeping (m_fontZoomSteps): Qt's
// QTextEdit::zoomIn/zoomOut are well-established, but the persistence of the
// accumulated step count across chapter navigation — which needs setHtml()'s
// document reset handled explicitly — is custom logic worth verifying.
class EpubZoomTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void zoomInIncreasesFontSize();
    void zoomOutDecreasesFontSize();
    void zoomLevelPersistsAcrossChapterNavigation();
    void zoomInIsClampedAtUpperBound();
    void zoomOutIsClampedAtLowerBound();
};

void EpubZoomTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void EpubZoomTest::zoomInIncreasesFontSize()
{
    auto view = makeView();
    const qreal baseline = view->currentFontPointSize();

    view->zoomIn();
    QVERIFY(view->currentFontPointSize() > baseline);
}

void EpubZoomTest::zoomOutDecreasesFontSize()
{
    auto view = makeView();
    const qreal baseline = view->currentFontPointSize();

    view->zoomOut();
    QVERIFY(view->currentFontPointSize() < baseline);
}

void EpubZoomTest::zoomLevelPersistsAcrossChapterNavigation()
{
    auto view = makeView();
    view->zoomIn();
    view->zoomIn();
    view->zoomIn();
    const qreal zoomedSize = view->currentFontPointSize();

    // Chapter 2's HTML has different content but the same base font-size CSS,
    // so if the zoom survived setHtml() the resulting size should match.
    view->goToChapter(1);
    QCOMPARE(view->currentFontPointSize(), zoomedSize);

    view->goToChapter(0);
    QCOMPARE(view->currentFontPointSize(), zoomedSize);
}

void EpubZoomTest::zoomInIsClampedAtUpperBound()
{
    auto view = makeView();
    for (int i = 0; i < 30; ++i) {
        view->zoomIn();
    }
    const qreal clampedSize = view->currentFontPointSize();

    view->zoomIn(); // one more, past the clamp
    QCOMPARE(view->currentFontPointSize(), clampedSize);
}

void EpubZoomTest::zoomOutIsClampedAtLowerBound()
{
    auto view = makeView();
    for (int i = 0; i < 30; ++i) {
        view->zoomOut();
    }
    const qreal clampedSize = view->currentFontPointSize();

    view->zoomOut(); // one more, past the clamp
    QCOMPARE(view->currentFontPointSize(), clampedSize);
}

QTEST_MAIN(EpubZoomTest)
#include "EpubZoomTest.moc"
