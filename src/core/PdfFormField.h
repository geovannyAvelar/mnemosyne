#pragma once

#include <QString>
#include <QStringList>

// One AcroForm field on a PDF page, as reported by
// PopplerPdfDocument::formFields() and edited via its setFieldText()/
// setFieldChecked()/setFieldChoiceIndex() -- see FormFieldsDock, the
// side-panel that lists and edits these. Values are written straight into
// the live Poppler document (unlike Highlight/InkStroke, which are
// Mnemosyne-only sidecars): a form field's whole purpose is to become part
// of the PDF, via PopplerPdfDocument::saveFilledFormAs().
struct PdfFormField
{
    enum class Type
    {
        Text,
        CheckBox,
        RadioButton,
        ComboBox,
        ListBox,
    };

    int pageIndex = -1;
    // Index into that page's own Poppler formFields() vector -- the stable
    // identity PopplerPdfDocument::setFieldText()/setFieldChecked()/
    // setFieldChoiceIndex() take to find this field again later, since
    // Poppler::FormField objects themselves aren't retained between calls.
    int fieldIndex = -1;
    Type type = Type::Text;
    QString name; // fullyQualifiedName(), for display
    bool readOnly = false;

    QString textValue; // Type::Text
    bool checked = false; // Type::CheckBox / RadioButton
    QStringList choices; // Type::ComboBox / ListBox
    int currentChoiceIndex = -1; // ditto; -1 if nothing selected
};
