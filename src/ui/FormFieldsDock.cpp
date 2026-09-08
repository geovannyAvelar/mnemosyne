#include "FormFieldsDock.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

FormFieldsDock::FormFieldsDock(QWidget *parent)
    : QDockWidget(tr("Form Fields"), parent)
{
    auto *outer = new QWidget(this);
    auto *outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(4, 4, 4, 4);

    auto *scrollArea = new QScrollArea(outer);
    scrollArea->setWidgetResizable(true);
    m_container = new QWidget(scrollArea);
    m_formLayout = new QFormLayout(m_container);
    scrollArea->setWidget(m_container);

    m_saveButton = new QPushButton(tr("Save Filled Form As..."), outer);
    connect(m_saveButton, &QPushButton::clicked, this, &FormFieldsDock::saveAsRequested);

    outerLayout->addWidget(scrollArea, 1);
    outerLayout->addWidget(m_saveButton);

    setWidget(outer);
    hide(); // nothing to show until setFields() is called with a non-empty list
}

void FormFieldsDock::setFields(const QVector<PdfFormField> &fields)
{
    while (m_formLayout->rowCount() > 0) {
        m_formLayout->removeRow(0); // deletes that row's label + input widget
    }

    if (fields.isEmpty()) {
        hide();
        return;
    }
    show();

    for (const PdfFormField &field : fields) {
        const int pageIndex = field.pageIndex;
        const int fieldIndex = field.fieldIndex;
        const QString label = (field.name.isEmpty() ? tr("Field") : field.name) + tr(" (p. %1)").arg(pageIndex + 1);

        switch (field.type) {
        case PdfFormField::Type::Text: {
            auto *edit = new QLineEdit(field.textValue, m_container);
            edit->setReadOnly(field.readOnly);
            connect(edit, &QLineEdit::editingFinished, this, [this, edit, pageIndex, fieldIndex] {
                emit textFieldEdited(pageIndex, fieldIndex, edit->text());
            });
            m_formLayout->addRow(label, edit);
            break;
        }
        case PdfFormField::Type::CheckBox:
        case PdfFormField::Type::RadioButton: {
            // Radio buttons are shown as an independent checkbox too, same
            // as a plain checkbox -- Poppler's setState() toggles only the
            // one widget, with no automatic same-group exclusivity, so
            // giving these their own QRadioButton-with-grouping UI would
            // promise a mutual-exclusion behavior this doesn't actually
            // enforce.
            auto *checkBox = new QCheckBox(m_container);
            checkBox->setChecked(field.checked);
            checkBox->setEnabled(!field.readOnly);
            connect(checkBox, &QCheckBox::toggled, this, [this, pageIndex, fieldIndex](bool checked) {
                emit checkBoxToggled(pageIndex, fieldIndex, checked);
            });
            m_formLayout->addRow(label, checkBox);
            break;
        }
        case PdfFormField::Type::ComboBox:
        case PdfFormField::Type::ListBox: {
            auto *combo = new QComboBox(m_container);
            combo->addItems(field.choices);
            combo->setCurrentIndex(field.currentChoiceIndex);
            combo->setEnabled(!field.readOnly);
            connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [this, pageIndex, fieldIndex](int index) { emit choiceSelected(pageIndex, fieldIndex, index); });
            m_formLayout->addRow(label, combo);
            break;
        }
        }
    }
}

void FormFieldsDock::clear()
{
    setFields({});
}
