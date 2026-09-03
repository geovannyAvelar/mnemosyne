#pragma once

#include <QJsonValue>

class QWidget;

// Renders a plugin's mnemosyne.showForm(schema) modally with Mnemosyne's
// own native Qt widgets -- deliberately not HTML/markup, so a plugin gets
// simple input UI (text/number/checkbox/choice fields) without any markup
// or script injection surface. See docs/plugins.md for the schema shape:
// {title, fields: [{id, type, label, default, options}, ...]}. Supported
// `type`s: "text" (QLineEdit, also the fallback for an unrecognized type),
// "multiline" (QPlainTextEdit), "number" (QDoubleSpinBox), "checkbox"
// (QCheckBox), "choice" (QComboBox, populated from `options`).
namespace PluginFormDialog {

// Returns the entered values as a JSON object keyed by field id, or
// QJsonValue(QJsonValue::Null) if the user cancelled.
QJsonValue show(QWidget *parent, const QJsonValue &schema);

} // namespace PluginFormDialog
