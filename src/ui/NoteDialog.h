#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

#include <optional>

// Combined "note text + marker color" dialog used by every reader view
// (EPUB/PDF/MOBI/TXT/Markdown), for both creating a new note and editing an
// existing one — both are chosen on one screen instead of two prompts.
namespace NoteDialog {

struct Result
{
    QString note;
    QColor color;
};

// Pre-filled with initialNote/initialColor (an empty note and
// kDefaultHighlightColor when creating a new one). Returns std::nullopt if
// the user cancels; the caller decides whether an empty note is acceptable.
std::optional<Result> show(QWidget *parent, const QString &initialNote, const QColor &initialColor);

} // namespace NoteDialog
