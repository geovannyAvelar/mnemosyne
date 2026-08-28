#pragma once

#include "core/Highlight.h"

#include <QDockWidget>
#include <QVector>

class QListWidget;

// Sidebar tab listing every highlight that has a note attached — there's no
// separate "notes" store; a note is simply a Highlight with a non-empty
// note (see core/Highlight.h). Replaces the old Bookmarks dock: unlike a
// bookmark, a note has no "current position" to add one from, so unlike
// BookmarksDock this has no "+ Add" action — notes are only created via the
// "Add Note..." selection context menu in the document itself.
class NotesDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit NotesDock(QWidget *parent = nullptr);

    // highlights: the full list for the current book, as returned by
    // HighlightStore::highlightsFor() — filtered down to notes internally,
    // but indices emitted by the signals below are into this full list, so
    // callers can pass them straight to HighlightStore::setNote()/setColor()/
    // removeHighlight().
    void setHighlights(const QVector<Highlight> &highlights);
    void clear();

signals:
    void noteActivated(int targetIndex);
    void editNoteRequested(int index); // index into the list passed to setHighlights
    void removeNoteRequested(int index); // ditto

private:
    QListWidget *m_list;
};
