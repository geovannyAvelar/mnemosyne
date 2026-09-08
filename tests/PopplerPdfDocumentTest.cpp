#include "pdf/PopplerPdfDocument.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <optional>

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt).
namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}

// Returns a copy of the matching field, not a pointer into `fields` -- most
// call sites here pass a just-returned-by-value QVector straight through
// (e.g. findField(doc->formFields(), ...)), and a pointer into that
// temporary would dangle the instant this call's full expression ends. A
// previous pointer-returning version of this shipped that exact bug: it
// read back fine on Linux's allocator (freed memory left coincidentally
// intact) but reliably came back empty on macOS's, surfacing only in CI.
std::optional<PdfFormField> findField(const QVector<PdfFormField> &fields, const QString &name)
{
    for (const PdfFormField &field : fields) {
        if (field.name == name) {
            return field;
        }
    }
    return std::nullopt;
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

    void pdfWithNoAcroFormHasNoFormFields();
    void formFieldsEnumeratesTextCheckboxAndChoiceFields();
    void setFieldTextUpdatesFormFields();
    void setFieldCheckedUpdatesFormFields();
    void setFieldChoiceIndexUpdatesFormFields();
    void saveFilledFormAsWritesValuesToNewFileWithoutTouchingOriginal();
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

void PopplerPdfDocumentTest::pdfWithNoAcroFormHasNoFormFields()
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test.pdf"), &error);
    QVERIFY2(doc, qPrintable(error));
    QVERIFY(doc->formFields().isEmpty());
}

void PopplerPdfDocumentTest::formFieldsEnumeratesTextCheckboxAndChoiceFields()
{
    // tests/fixtures/test_form.pdf was generated with reportlab's AcroForm
    // helpers (see the script this fixture was committed with): one text
    // field ("full_name"), one checkbox ("subscribe", unchecked), and one
    // combo box ("country", offering USA/Canada/Brazil, defaulting to USA).
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test_form.pdf"), &error);
    QVERIFY2(doc, qPrintable(error));

    const QVector<PdfFormField> fields = doc->formFields();
    QCOMPARE(fields.size(), 3);

    std::optional<PdfFormField> name = findField(fields, QStringLiteral("full_name"));
    QVERIFY(name);
    QCOMPARE(name->type, PdfFormField::Type::Text);
    QCOMPARE(name->pageIndex, 0);
    QVERIFY(name->textValue.isEmpty());
    QVERIFY(!name->readOnly);

    std::optional<PdfFormField> subscribe = findField(fields, QStringLiteral("subscribe"));
    QVERIFY(subscribe);
    QCOMPARE(subscribe->type, PdfFormField::Type::CheckBox);
    QVERIFY(!subscribe->checked);

    std::optional<PdfFormField> country = findField(fields, QStringLiteral("country"));
    QVERIFY(country);
    QCOMPARE(country->type, PdfFormField::Type::ComboBox);
    QCOMPARE(country->choices, QStringList({QStringLiteral("USA"), QStringLiteral("Canada"), QStringLiteral("Brazil")}));
    QCOMPARE(country->currentChoiceIndex, 0);
}

void PopplerPdfDocumentTest::setFieldTextUpdatesFormFields()
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test_form.pdf"), &error);
    QVERIFY2(doc, qPrintable(error));

    std::optional<PdfFormField> before = findField(doc->formFields(), QStringLiteral("full_name"));
    QVERIFY(before);
    doc->setFieldText(before->pageIndex, before->fieldIndex, QStringLiteral("Ada Lovelace"));

    std::optional<PdfFormField> after = findField(doc->formFields(), QStringLiteral("full_name"));
    QVERIFY(after);
    QCOMPARE(after->textValue, QStringLiteral("Ada Lovelace"));
}

void PopplerPdfDocumentTest::setFieldCheckedUpdatesFormFields()
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test_form.pdf"), &error);
    QVERIFY2(doc, qPrintable(error));

    std::optional<PdfFormField> before = findField(doc->formFields(), QStringLiteral("subscribe"));
    QVERIFY(before);
    QVERIFY(!before->checked);
    doc->setFieldChecked(before->pageIndex, before->fieldIndex, true);

    std::optional<PdfFormField> after = findField(doc->formFields(), QStringLiteral("subscribe"));
    QVERIFY(after);
    QVERIFY(after->checked);
}

void PopplerPdfDocumentTest::setFieldChoiceIndexUpdatesFormFields()
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test_form.pdf"), &error);
    QVERIFY2(doc, qPrintable(error));

    std::optional<PdfFormField> before = findField(doc->formFields(), QStringLiteral("country"));
    QVERIFY(before);
    doc->setFieldChoiceIndex(before->pageIndex, before->fieldIndex, 2); // "Brazil"

    std::optional<PdfFormField> after = findField(doc->formFields(), QStringLiteral("country"));
    QVERIFY(after);
    QCOMPARE(after->currentChoiceIndex, 2);
}

void PopplerPdfDocumentTest::saveFilledFormAsWritesValuesToNewFileWithoutTouchingOriginal()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString outputPath = tempDir.filePath(QStringLiteral("filled.pdf"));

    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath("test_form.pdf"), &error);
    QVERIFY2(doc, qPrintable(error));

    std::optional<PdfFormField> name = findField(doc->formFields(), QStringLiteral("full_name"));
    QVERIFY(name);
    doc->setFieldText(name->pageIndex, name->fieldIndex, QStringLiteral("Grace Hopper"));
    std::optional<PdfFormField> subscribe = findField(doc->formFields(), QStringLiteral("subscribe"));
    QVERIFY(subscribe);
    doc->setFieldChecked(subscribe->pageIndex, subscribe->fieldIndex, true);

    QVERIFY(doc->saveFilledFormAs(outputPath));
    QVERIFY(QFile::exists(outputPath));

    // The saved copy has the filled-in values...
    QString reopenError;
    auto reopened = PopplerPdfDocument::load(outputPath, &reopenError);
    QVERIFY2(reopened, qPrintable(reopenError));
    std::optional<PdfFormField> savedName = findField(reopened->formFields(), QStringLiteral("full_name"));
    QVERIFY(savedName);
    QCOMPARE(savedName->textValue, QStringLiteral("Grace Hopper"));
    std::optional<PdfFormField> savedSubscribe = findField(reopened->formFields(), QStringLiteral("subscribe"));
    QVERIFY(savedSubscribe);
    QVERIFY(savedSubscribe->checked);

    // ...but the original fixture file on disk was never touched -- a fresh
    // load of it still shows the untouched defaults, confirming
    // saveFilledFormAs() wrote a separate file rather than mutating the
    // source PDF in place.
    QString originalError;
    auto original = PopplerPdfDocument::load(fixturePath("test_form.pdf"), &originalError);
    QVERIFY2(original, qPrintable(originalError));
    std::optional<PdfFormField> originalName = findField(original->formFields(), QStringLiteral("full_name"));
    QVERIFY(originalName);
    QVERIFY(originalName->textValue.isEmpty());
    std::optional<PdfFormField> originalSubscribe = findField(original->formFields(), QStringLiteral("subscribe"));
    QVERIFY(originalSubscribe);
    QVERIFY(!originalSubscribe->checked);
}

QTEST_MAIN(PopplerPdfDocumentTest)
#include "PopplerPdfDocumentTest.moc"
