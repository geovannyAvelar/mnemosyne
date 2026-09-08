#include "core/EmbeddedImageHtml.h"

#include <QBuffer>
#include <QImage>
#include <QTest>

namespace {

// A tiny real (not just arbitrary bytes) PNG, so QImage::fromData() inside
// rewriteImgTagWithDataUri() actually decodes it -- needed to exercise the
// width-capping path, which only triggers off a successfully decoded image.
QByteArray pngBytes(int width, int height)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::red);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

} // namespace

// Pure regex/string logic behind MobiDocument's embedded-image support (see
// MobiDocument.cpp's embedMobiImages()) -- covers the two real-world <img>
// reference schemes (Mobipocket/KF7 recindex, KF8 kindle:embed) and the
// data-URI rewrite itself, all without needing a real parsed MOBI file (no
// available test fixture has actual body-referenced image resources; see
// the MobiDocument.cpp commit this test landed with for why). The glue that
// resolves a reference to a real MOBIPart via libmobi (resolveImageResource()
// in MobiDocument.cpp) isn't covered here for that same reason, but is a
// thin, directly-libmobi-API-based function with little room for its own
// bugs independent of this.
class EmbeddedImageHtmlTest : public QObject
{
    Q_OBJECT

private slots:
    void mobiImageReference_recindexAttribute();
    void mobiImageReference_kindleEmbedSrc();
    void mobiImageReference_kindleEmbedStripsLeadingZeros();
    void mobiImageReference_plainImgHasNoReference();
    void rewriteImgTagWithDataUri_insertsSrcWhenTagHasNone();
    void rewriteImgTagWithDataUri_replacesExistingSrc();
    void rewriteImgTagWithDataUri_removesRecindexAttribute();
    void rewriteImgTagWithDataUri_capsOversizedImage();
    void rewriteImgTagWithDataUri_leavesSmallImageUncapped();
};

void EmbeddedImageHtmlTest::mobiImageReference_recindexAttribute()
{
    QCOMPARE(EmbeddedImageHtml::mobiImageReference(QStringLiteral("<img recindex=\"00003\" width=\"200\">")),
             QStringLiteral("00003"));
}

void EmbeddedImageHtmlTest::mobiImageReference_kindleEmbedSrc()
{
    QCOMPARE(EmbeddedImageHtml::mobiImageReference(
                 QStringLiteral("<img src=\"kindle:embed:0007?mime=image/jpeg\" alt=\"\">")),
             QStringLiteral("7"));
}

void EmbeddedImageHtmlTest::mobiImageReference_kindleEmbedStripsLeadingZeros()
{
    QCOMPARE(EmbeddedImageHtml::mobiImageReference(QStringLiteral("<img src=\"kindle:embed:0000\">")),
             QStringLiteral("0"));
}

void EmbeddedImageHtmlTest::mobiImageReference_plainImgHasNoReference()
{
    QVERIFY(EmbeddedImageHtml::mobiImageReference(QStringLiteral("<img src=\"cover.jpg\">")).isEmpty());
    QVERIFY(EmbeddedImageHtml::mobiImageReference(QStringLiteral("<img>")).isEmpty());
}

void EmbeddedImageHtmlTest::rewriteImgTagWithDataUri_insertsSrcWhenTagHasNone()
{
    const QString result = EmbeddedImageHtml::rewriteImgTagWithDataUri(QStringLiteral("<img recindex=\"00001\">"),
                                                                        pngBytes(10, 10), QStringLiteral("image/png"),
                                                                        720);
    QVERIFY2(result.contains(QStringLiteral("src=\"data:image/png;base64,")), qPrintable(result));
}

void EmbeddedImageHtmlTest::rewriteImgTagWithDataUri_replacesExistingSrc()
{
    const QString result = EmbeddedImageHtml::rewriteImgTagWithDataUri(
        QStringLiteral("<img src=\"kindle:embed:0001?mime=image/jpeg\">"), pngBytes(10, 10),
        QStringLiteral("image/jpeg"), 720);
    QVERIFY2(!result.contains(QStringLiteral("kindle:embed")), qPrintable(result));
    QVERIFY2(result.contains(QStringLiteral("src=\"data:image/jpeg;base64,")), qPrintable(result));
    // Exactly one src attribute -- the original wasn't left behind alongside
    // the new one.
    QCOMPARE(result.count(QStringLiteral("src=")), 1);
}

void EmbeddedImageHtmlTest::rewriteImgTagWithDataUri_removesRecindexAttribute()
{
    const QString result = EmbeddedImageHtml::rewriteImgTagWithDataUri(
        QStringLiteral("<img recindex=\"00001\" alt=\"a photo\">"), pngBytes(10, 10), QStringLiteral("image/png"),
        720);
    QVERIFY2(!result.contains(QStringLiteral("recindex")), qPrintable(result));
    QVERIFY2(result.contains(QStringLiteral("alt=\"a photo\"")), qPrintable(result)); // untouched sibling attribute
}

void EmbeddedImageHtmlTest::rewriteImgTagWithDataUri_capsOversizedImage()
{
    const QString result = EmbeddedImageHtml::rewriteImgTagWithDataUri(QStringLiteral("<img recindex=\"00001\">"),
                                                                        pngBytes(1440, 900), QStringLiteral("image/png"),
                                                                        720);
    QVERIFY2(result.contains(QStringLiteral("width=\"720\"")), qPrintable(result));
    QVERIFY2(result.contains(QStringLiteral("height=\"450\"")), qPrintable(result)); // 900 * 720/1440, aspect preserved
}

void EmbeddedImageHtmlTest::rewriteImgTagWithDataUri_leavesSmallImageUncapped()
{
    const QString result = EmbeddedImageHtml::rewriteImgTagWithDataUri(QStringLiteral("<img recindex=\"00001\">"),
                                                                        pngBytes(100, 50), QStringLiteral("image/png"),
                                                                        720);
    QVERIFY2(!result.contains(QStringLiteral("width=")), qPrintable(result));
    QVERIFY2(!result.contains(QStringLiteral("height=")), qPrintable(result));
}

QTEST_APPLESS_MAIN(EmbeddedImageHtmlTest)
#include "EmbeddedImageHtmlTest.moc"
