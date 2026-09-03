#include "PluginFormDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace PluginFormDialog {

namespace {

struct FieldWidget
{
    QString id;
    QString type;
    QWidget *widget = nullptr;
};

QStringList optionsFromField(const QJsonObject &field)
{
    QStringList options;
    for (const QJsonValue &value : field.value(QStringLiteral("options")).toArray()) {
        options << value.toString();
    }
    return options;
}

} // namespace

QJsonValue show(QWidget *parent, const QJsonValue &schema)
{
    const QJsonObject schemaObj = schema.toObject();

    QDialog dialog(parent);
    dialog.setWindowTitle(schemaObj.value(QStringLiteral("title")).toString(QObject::tr("Plugin")));

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    layout->addLayout(form);

    QVector<FieldWidget> fieldWidgets;
    for (const QJsonValue &fieldValue : schemaObj.value(QStringLiteral("fields")).toArray()) {
        const QJsonObject field = fieldValue.toObject();
        const QString id = field.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            continue; // a field with no id has nowhere to put its value in the result
        }
        const QString type = field.value(QStringLiteral("type")).toString(QStringLiteral("text"));
        const QString label = field.value(QStringLiteral("label")).toString(id);
        const QJsonValue defaultValue = field.value(QStringLiteral("default"));

        QWidget *widget = nullptr;
        if (type == QStringLiteral("multiline")) {
            auto *edit = new QPlainTextEdit(&dialog);
            edit->setPlainText(defaultValue.toString());
            widget = edit;
        } else if (type == QStringLiteral("number")) {
            auto *spin = new QDoubleSpinBox(&dialog);
            spin->setRange(-1.0e15, 1.0e15);
            spin->setDecimals(6);
            spin->setValue(defaultValue.toDouble());
            widget = spin;
        } else if (type == QStringLiteral("checkbox")) {
            auto *check = new QCheckBox(&dialog);
            check->setChecked(defaultValue.toBool());
            widget = check;
        } else if (type == QStringLiteral("choice")) {
            auto *combo = new QComboBox(&dialog);
            combo->addItems(optionsFromField(field));
            const int index = combo->findText(defaultValue.toString());
            if (index >= 0) {
                combo->setCurrentIndex(index);
            }
            widget = combo;
        } else { // "text", and the fallback for any type this dialog doesn't recognize
            auto *edit = new QLineEdit(&dialog);
            edit->setText(defaultValue.toString());
            widget = edit;
        }

        form->addRow(label, widget);
        fieldWidgets.append({id, type, widget});
    }

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dialog.exec() != QDialog::Accepted) {
        return QJsonValue();
    }

    QJsonObject result;
    for (const FieldWidget &fw : fieldWidgets) {
        if (fw.type == QStringLiteral("multiline")) {
            result[fw.id] = static_cast<QPlainTextEdit *>(fw.widget)->toPlainText();
        } else if (fw.type == QStringLiteral("number")) {
            result[fw.id] = static_cast<QDoubleSpinBox *>(fw.widget)->value();
        } else if (fw.type == QStringLiteral("checkbox")) {
            result[fw.id] = static_cast<QCheckBox *>(fw.widget)->isChecked();
        } else if (fw.type == QStringLiteral("choice")) {
            result[fw.id] = static_cast<QComboBox *>(fw.widget)->currentText();
        } else {
            result[fw.id] = static_cast<QLineEdit *>(fw.widget)->text();
        }
    }
    return result;
}

} // namespace PluginFormDialog
