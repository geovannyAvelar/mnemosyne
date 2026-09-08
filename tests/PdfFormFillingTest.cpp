#include "core/PdfFormField.h"
#include "pdf/PopplerPdfDocument.h"
#include "ui/PdfView.h"

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

// Returns a copy of the matching field, not a pointer into `fields` --
// every call site here passes a just-returned-by-value QVector (e.g.
// findField(view->formFields(), ...)) straight through, and a pointer into
// that temporary would dangle the instant this call's full expression
// ends. A previous pointer-returning version of this shipped that exact
// bug: it read back fine on Linux's allocator (freed memory left
// coincidentally intact) but reliably came back empty on macOS's,
// surfacing only in CI.
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

// PopplerPdfDocumentTest.cpp already covers the field enumeration/mutation
// logic itself in detail; these exercise PdfView's own thin forwarding layer
// (see PdfView::formFields()/setFormFieldText()/etc.) -- the API
// FormFieldsDock actually calls through MainWindow's wiring -- end to end
// against a real PdfView, the same way PdfTextSelectionTest.cpp does for
// highlights.
class PdfFormFillingTest : public QObject
{
    Q_OBJECT

private slots:
    void pdfWithNoAcroFormReturnsNoFieldsFromPdfView();
    void pdfViewReturnsFieldsForFormPdf();
    void pdfViewSetFormFieldTextForwardsToDocument();
    void pdfViewSetFormFieldCheckedForwardsToDocument();
    void pdfViewSetFormFieldChoiceIndexForwardsToDocument();
    void pdfViewSaveFilledFormAsWritesNewFile();

private:
    std::unique_ptr<PdfView> makePdfView(const QString &fixtureName);
};

std::unique_ptr<PdfView> PdfFormFillingTest::makePdfView(const QString &fixtureName)
{
    QString error;
    auto doc = PopplerPdfDocument::load(fixturePath(fixtureName), &error);
    Q_ASSERT_X(doc, "makePdfView", qPrintable(error));
    auto view = std::make_unique<PdfView>(std::move(doc), fixturePath(fixtureName));
    view->resize(900, 700);
    view->show();
    static_cast<void>(QTest::qWaitForWindowExposed(view.get()));
    return view;
}

void PdfFormFillingTest::pdfWithNoAcroFormReturnsNoFieldsFromPdfView()
{
    auto view = makePdfView(QStringLiteral("test.pdf"));
    QVERIFY(view->formFields().isEmpty());
}

void PdfFormFillingTest::pdfViewReturnsFieldsForFormPdf()
{
    auto view = makePdfView(QStringLiteral("test_form.pdf"));
    const QVector<PdfFormField> fields = view->formFields();
    QCOMPARE(fields.size(), 3);
    QVERIFY(findField(fields, QStringLiteral("full_name")));
    QVERIFY(findField(fields, QStringLiteral("subscribe")));
    QVERIFY(findField(fields, QStringLiteral("country")));
}

void PdfFormFillingTest::pdfViewSetFormFieldTextForwardsToDocument()
{
    auto view = makePdfView(QStringLiteral("test_form.pdf"));
    std::optional<PdfFormField> before = findField(view->formFields(), QStringLiteral("full_name"));
    QVERIFY(before);

    view->setFormFieldText(before->pageIndex, before->fieldIndex, QStringLiteral("Ada Lovelace"));

    std::optional<PdfFormField> after = findField(view->formFields(), QStringLiteral("full_name"));
    QVERIFY(after);
    QCOMPARE(after->textValue, QStringLiteral("Ada Lovelace"));
}

void PdfFormFillingTest::pdfViewSetFormFieldCheckedForwardsToDocument()
{
    auto view = makePdfView(QStringLiteral("test_form.pdf"));
    std::optional<PdfFormField> before = findField(view->formFields(), QStringLiteral("subscribe"));
    QVERIFY(before);
    QVERIFY(!before->checked);

    view->setFormFieldChecked(before->pageIndex, before->fieldIndex, true);

    std::optional<PdfFormField> after = findField(view->formFields(), QStringLiteral("subscribe"));
    QVERIFY(after);
    QVERIFY(after->checked);
}

void PdfFormFillingTest::pdfViewSetFormFieldChoiceIndexForwardsToDocument()
{
    auto view = makePdfView(QStringLiteral("test_form.pdf"));
    std::optional<PdfFormField> before = findField(view->formFields(), QStringLiteral("country"));
    QVERIFY(before);

    view->setFormFieldChoiceIndex(before->pageIndex, before->fieldIndex, 1); // "Canada"

    std::optional<PdfFormField> after = findField(view->formFields(), QStringLiteral("country"));
    QVERIFY(after);
    QCOMPARE(after->currentChoiceIndex, 1);
}

void PdfFormFillingTest::pdfViewSaveFilledFormAsWritesNewFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString outputPath = tempDir.filePath(QStringLiteral("filled.pdf"));

    auto view = makePdfView(QStringLiteral("test_form.pdf"));
    std::optional<PdfFormField> field = findField(view->formFields(), QStringLiteral("full_name"));
    QVERIFY(field);
    view->setFormFieldText(field->pageIndex, field->fieldIndex, QStringLiteral("Grace Hopper"));

    QVERIFY(view->saveFilledFormAs(outputPath));
    QVERIFY(QFile::exists(outputPath));

    QString error;
    auto reopened = PopplerPdfDocument::load(outputPath, &error);
    QVERIFY2(reopened, qPrintable(error));
    std::optional<PdfFormField> saved = findField(reopened->formFields(), QStringLiteral("full_name"));
    QVERIFY(saved);
    QCOMPARE(saved->textValue, QStringLiteral("Grace Hopper"));
}

QTEST_MAIN(PdfFormFillingTest)
#include "PdfFormFillingTest.moc"
