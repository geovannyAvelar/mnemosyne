#include "pdf/PopplerPdfDocument.h"

#include <QTest>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}
} // namespace

// tests/fixtures/test-password.pdf is tests/fixtures/test.pdf re-saved with
// both its owner and user password set to "secret123" (via pypdf -- see the
// commit this file landed with for the exact script), so its actual page
// content/text is identical to test.pdf's own tests.
class PopplerPdfDocumentTest : public QObject
{
    Q_OBJECT

private slots:
    void unencryptedPdfIsNotPasswordProtected();
    void encryptedPdfIsPasswordProtected();
    void loadingEncryptedPdfWithNoPasswordFails();
    void loadingEncryptedPdfWithWrongPasswordFails();
    void loadingEncryptedPdfWithCorrectPasswordSucceeds();
};

void PopplerPdfDocumentTest::unencryptedPdfIsNotPasswordProtected()
{
    QVERIFY(!PopplerPdfDocument::isPasswordProtected(fixturePath("test.pdf")));
}

void PopplerPdfDocumentTest::encryptedPdfIsPasswordProtected()
{
    QVERIFY(PopplerPdfDocument::isPasswordProtected(fixturePath("test-password.pdf")));
}

void PopplerPdfDocumentTest::loadingEncryptedPdfWithNoPasswordFails()
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test-password.pdf"), &error);
    QVERIFY(!doc);
    QVERIFY2(error.contains(QStringLiteral("password"), Qt::CaseInsensitive), qPrintable(error));
}

void PopplerPdfDocumentTest::loadingEncryptedPdfWithWrongPasswordFails()
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test-password.pdf"), &error, QStringLiteral("not-it"));
    QVERIFY(!doc);
    QVERIFY2(error.contains(QStringLiteral("password"), Qt::CaseInsensitive), qPrintable(error));
}

void PopplerPdfDocumentTest::loadingEncryptedPdfWithCorrectPasswordSucceeds()
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test-password.pdf"), &error, QStringLiteral("secret123"));
    QVERIFY2(doc, qPrintable(error));
    QCOMPARE(doc->pageCount(), 1);

    std::unique_ptr<IPage> page = doc->page(0);
    QVERIFY(page);
    QCOMPARE(page->text().trimmed(), QStringLiteral("Searchable PDF fixture text for full text search testing."));
}

QTEST_MAIN(PopplerPdfDocumentTest)
#include "PopplerPdfDocumentTest.moc"
