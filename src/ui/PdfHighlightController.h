#pragma once

#include "core/Highlight.h"

#include <QVector>

// Owns PdfView's highlight/note persistence against HighlightStore -- the
// CRUD operations and the current-book cache that used to live directly on
// PdfView (m_highlights, plus the HighlightStore::* calls duplicated across
// addHighlightForSelection/addNoteForSelection/showCanvasContextMenu's
// edit-note and remove-highlight actions).
//
// PdfView still builds/execs the actual context menu (it needs QMenu/
// NoteDialog) and still emits highlightsChanged() itself after calling into
// this controller -- this class is the data layer underneath that UI, not a
// replacement for it.
class PdfHighlightController
{
public:
    void setBookHash(const QString &bookHash) { m_bookHash = bookHash; }

    // Re-pulls HighlightStore::highlightsFor(bookHash) into the cache --
    // call after any change (local or synced) that could have altered it.
    void reload();
    const QVector<Highlight> &highlights() const { return m_highlights; }

    // Index into highlights() of the persisted highlight containing
    // pagePoint on pageIndex, or -1 if none.
    int indexAtPagePoint(const QPointF &pagePoint, int pageIndex) const;

    void addHighlight(int pageIndex, const QRectF &pageRect, const QString &text);
    void addNote(int pageIndex, const QRectF &pageRect, const QString &text, const QString &note,
                 const QColor &color);
    void setNote(int index, const QString &note, const QColor &color);
    void removeHighlight(int index);

private:
    QString m_bookHash;
    QVector<Highlight> m_highlights;
};
