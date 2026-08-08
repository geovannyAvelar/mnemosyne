#include "core/CoordinateUtil.h"

#include <QTest>

class CoordinateUtilTest : public QObject
{
    Q_OBJECT

private slots:
    void convertsPixelsToPointsAtUnitScale();
    void convertsPixelsToPointsAtNonUnitScale();
    void preservesPositionNotJustSize();
    void returnsNullRectForNonPositiveScale();
    void pageToPixelIsInverseOfPixelToPage();
    void pageToPixelReturnsNullRectForNonPositiveScale();
};

void CoordinateUtilTest::convertsPixelsToPointsAtUnitScale()
{
    // scale 1.0 == 72 DPI, so pixels and points coincide exactly.
    const QRectF result = pixelRectToPageRect(QRect(10, 20, 100, 50), 1.0);
    QCOMPARE(result, QRectF(10, 20, 100, 50));
}

void CoordinateUtilTest::convertsPixelsToPointsAtNonUnitScale()
{
    // At the app's default 1.5x render scale, a 150x300 pixel selection
    // covers a 100x200 point region of the actual page.
    const QRectF result = pixelRectToPageRect(QRect(0, 0, 150, 300), 1.5);
    QCOMPARE(result, QRectF(0, 0, 100, 200));
}

void CoordinateUtilTest::preservesPositionNotJustSize()
{
    const QRectF result = pixelRectToPageRect(QRect(300, 600, 150, 300), 1.5);
    QCOMPARE(result, QRectF(200, 400, 100, 200));
}

void CoordinateUtilTest::returnsNullRectForNonPositiveScale()
{
    QCOMPARE(pixelRectToPageRect(QRect(0, 0, 10, 10), 0.0), QRectF());
    QCOMPARE(pixelRectToPageRect(QRect(0, 0, 10, 10), -1.0), QRectF());
}

void CoordinateUtilTest::pageToPixelIsInverseOfPixelToPage()
{
    const QRect original(300, 600, 150, 300);
    const qreal scale = 1.5;
    const QRectF pageRect = pixelRectToPageRect(original, scale);
    const QRect roundTripped = pageRectToPixelRect(pageRect, scale);
    QCOMPARE(roundTripped, original);
}

void CoordinateUtilTest::pageToPixelReturnsNullRectForNonPositiveScale()
{
    QCOMPARE(pageRectToPixelRect(QRectF(0, 0, 10, 10), 0.0), QRect());
    QCOMPARE(pageRectToPixelRect(QRectF(0, 0, 10, 10), -1.0), QRect());
}

QTEST_MAIN(CoordinateUtilTest)
#include "CoordinateUtilTest.moc"
