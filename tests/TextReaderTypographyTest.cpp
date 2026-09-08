#include "ui/TextReaderTypography.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTest>

class TextReaderTypographyTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void defaultsMatchNoOverride();
    void fontFamilyRoundTrips();
    void lineSpacingRoundTrips();
    void marginRoundTrips();

    void bodyCssEmptyAtDefaults();
    void bodyCssIncludesFontFamilyWhenSet();
    void bodyCssIncludesLineHeightWhenNotDefault();
    void bodyCssCombinesBothWhenBothSet();
    void bodyCssTargetsSameSelectorScopeAsDarkModeOverride();
};

void TextReaderTypographyTest::initTestCase()
{
    // Isolate from the real app's settings (organizationName "Mnemosyne") so
    // this test can never read or clobber actual user data on the machine
    // it runs on -- see AppPersistenceTest for the same pattern.
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void TextReaderTypographyTest::init()
{
    QSettings().clear(); // fresh state for each test function
}

void TextReaderTypographyTest::cleanupTestCase()
{
    QSettings().clear(); // don't leave test settings behind on disk
}

void TextReaderTypographyTest::defaultsMatchNoOverride()
{
    QVERIFY(TextReaderTypography::fontFamily().isEmpty());
    QCOMPARE(TextReaderTypography::lineSpacingPercent(), 100);
    // Not 0: QTextDocument::setDocumentMargin()'s own built-in default is 4
    // -- see TextReaderTypography.cpp's own doc comment for why "Narrow"
    // reproduces that instead of an unmargined flush edge.
    QCOMPARE(TextReaderTypography::marginPx(), 4);
}

void TextReaderTypographyTest::fontFamilyRoundTrips()
{
    TextReaderTypography::setFontFamily(QStringLiteral("Georgia"));
    QCOMPARE(TextReaderTypography::fontFamily(), QStringLiteral("Georgia"));

    TextReaderTypography::setFontFamily(QString());
    QVERIFY(TextReaderTypography::fontFamily().isEmpty());
}

void TextReaderTypographyTest::lineSpacingRoundTrips()
{
    for (int percent : TextReaderTypography::lineSpacingPercentSteps()) {
        TextReaderTypography::setLineSpacingPercent(percent);
        QCOMPARE(TextReaderTypography::lineSpacingPercent(), percent);
    }
}

void TextReaderTypographyTest::marginRoundTrips()
{
    for (int px : TextReaderTypography::marginPxSteps()) {
        TextReaderTypography::setMarginPx(px);
        QCOMPARE(TextReaderTypography::marginPx(), px);
    }
}

void TextReaderTypographyTest::bodyCssEmptyAtDefaults()
{
    QVERIFY(TextReaderTypography::bodyCss().isEmpty());
}

void TextReaderTypographyTest::bodyCssIncludesFontFamilyWhenSet()
{
    TextReaderTypography::setFontFamily(QStringLiteral("Times New Roman"));
    const QString css = TextReaderTypography::bodyCss();
    QVERIFY2(css.contains(QStringLiteral("font-family:'Times New Roman'")), qPrintable(css));
    QVERIFY2(!css.contains(QStringLiteral("line-height")), qPrintable(css)); // spacing still at default
}

void TextReaderTypographyTest::bodyCssIncludesLineHeightWhenNotDefault()
{
    TextReaderTypography::setLineSpacingPercent(150);
    const QString css = TextReaderTypography::bodyCss();
    QVERIFY2(css.contains(QStringLiteral("line-height:150%")), qPrintable(css));
    QVERIFY2(!css.contains(QStringLiteral("font-family")), qPrintable(css)); // family still at default
}

void TextReaderTypographyTest::bodyCssCombinesBothWhenBothSet()
{
    TextReaderTypography::setFontFamily(QStringLiteral("Georgia"));
    TextReaderTypography::setLineSpacingPercent(125);
    const QString css = TextReaderTypography::bodyCss();
    QVERIFY2(css.contains(QStringLiteral("font-family:'Georgia'")), qPrintable(css));
    QVERIFY2(css.contains(QStringLiteral("line-height:125%")), qPrintable(css));
}

void TextReaderTypographyTest::bodyCssTargetsSameSelectorScopeAsDarkModeOverride()
{
    // EpubView/MobiView's dark-mode color override uses this exact selector
    // list (see chapterHtmlFragment()/renderCurrentPart()) specifically so
    // it outranks the book's own same-specificity CSS in the cascade --
    // bodyCss() has to match it, or a custom font-family/line-height would
    // lose that same fight against the book's own styling.
    TextReaderTypography::setFontFamily(QStringLiteral("Georgia"));
    QVERIFY(TextReaderTypography::bodyCss().startsWith(QStringLiteral("body,p,div,span{")));
}

QTEST_APPLESS_MAIN(TextReaderTypographyTest)
#include "TextReaderTypographyTest.moc"
