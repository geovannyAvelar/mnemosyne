#pragma once

#include <QPoint>
#include <QString>

#include <functional>

class QWidget;

// Lightweight, non-modal "here's the note" popup shown when the reader
// clicks a highlight that has one -- distinct from NoteDialog (a modal
// QDialog for actually writing/editing a note's text and color): this is
// meant to be glanced at and dismissed, not to block the rest of the
// reader while it's open. Offers Edit/Remove as a convenience so the
// reader doesn't have to fall back to the right-click context menu for
// those, but doesn't duplicate NoteDialog's editing UI itself -- onEdit is
// expected to open that.
namespace NotePopup {

void show(QWidget *parent, const QPoint &globalPos, const QString &noteText, std::function<void()> onEdit,
          std::function<void()> onRemove);

} // namespace NotePopup
