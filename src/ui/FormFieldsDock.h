#pragma once

#include "core/PdfFormField.h"

#include <QDockWidget>
#include <QVector>

class QFormLayout;
class QPushButton;
class QWidget;

// Sidebar tab listing every AcroForm field on the current PDF tab (see
// core/PdfFormField.h), each with a live input widget wired straight to
// PopplerPdfDocument -- a QLineEdit for a text field, a QCheckBox for a
// checkbox/radio button, a QComboBox for a choice field. A side-panel field
// list rather than an on-page overlay: it works the same way regardless of
// zoom/scroll position, and reuses this app's existing dock architecture
// (TocDock/SearchDock/NotesDock) instead of a second, WYSIWYG input
// mechanism living on top of PdfPageStackView's rendered page images.
class FormFieldsDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit FormFieldsDock(QWidget *parent = nullptr);

    // fields: every field on the current tab's document, as returned by
    // PopplerPdfDocument::formFields(). Unlike NotesDock/TocDock/SearchDock,
    // this dock hides itself entirely (see BookInfoDock's same convention)
    // rather than showing an empty state, on the assumption that most tabs
    // -- every non-PDF one, and most PDFs -- have nothing to fill in here.
    void setFields(const QVector<PdfFormField> &fields);
    void clear(); // same as setFields({}) -- hides the dock

signals:
    // Emitted the instant the reader edits a field's input widget --
    // (pageIndex, fieldIndex) is PdfFormField's own identity pair, passed
    // straight through to PopplerPdfDocument::setFieldText()/
    // setFieldChecked()/setFieldChoiceIndex() (see PdfView's wiring).
    void textFieldEdited(int pageIndex, int fieldIndex, const QString &value);
    void checkBoxToggled(int pageIndex, int fieldIndex, bool checked);
    void choiceSelected(int pageIndex, int fieldIndex, int choiceIndex);
    // "Save Filled Form As..." button -- PdfView owns the actual file
    // dialog/save call, this dock only reports the button was pressed.
    void saveAsRequested();

private:
    QWidget *m_container;
    QFormLayout *m_formLayout;
    QPushButton *m_saveButton;
};
