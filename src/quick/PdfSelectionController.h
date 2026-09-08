#pragma once

#include "core/PdfSelectionModel.h"

#include <QObject>
#include <QString>
#include <QVariantList>

class PdfDocumentModel;

// Touch counterpart to desktop PdfPageCanvas's mouse click-drag selection:
// a long-press starts a selection at the nearest word, and dragging while
// still pressed extends it — same selectWordRange() word-snapping algorithm
// (see core/TextSelectionUtil.h) desktop's mouse-drag selection uses,
// just fed touch points instead of mouse events. The QML side (see
// qml/components/PdfContinuousPageItem.qml) is responsible for turning a
// long-press-then-drag gesture into begin/update calls with page-space
// points (pixel position divided by the render scale).
//
// One controller instance is shared by every page delegate (registered as
// the single "pdfSelectionController" QML context property), since a
// continuous-scroll reader can have several pages visible/touchable at
// once — begin/updateSelection() take an explicit page index rather than
// assuming a single global "current page". A drag isn't confined to the
// page it started on either: PdfContinuousPageItem.qml resolves whichever
// page the touch point currently sits over (which can differ from the page
// its own TapHandler lives on, once the drag crosses a page boundary) and
// passes that index to updateSelection() each move -- see there for how.
// selectionRectsForPage()/selectionPageIndices() let each delegate ask
// "does the current selection touch me, and if so with which rects" rather
// than there being a single "the" active page.
class PdfSelectionController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectionPageIndices READ selectionPageIndices NOTIFY selectionChanged)

public:
    explicit PdfSelectionController(PdfDocumentModel *documentModel, QObject *parent = nullptr);

    QString selectedText() const { return m_model.selectedText(); }
    // Pages the current selection spans, ascending; empty when there is none.
    QVariantList selectionPageIndices() const;
    // page-space QRectF entries (QML's "rect" type) for just one page's
    // portion of the selection, in reading order; empty if that page isn't
    // part of the current selection.
    Q_INVOKABLE QVariantList selectionRectsForPage(int pageIndex) const;
    // Just this page's slice of selectedText() -- see PdfSelectionModel's
    // own doc comment for why "add highlight" on a cross-page selection
    // needs this (one Highlight per page, each with its own matching
    // rect+text) rather than the full combined selectedText().
    Q_INVOKABLE QString selectionTextForPage(int pageIndex) const { return m_model.selectionTextForPage(pageIndex); }

    // pageX/pageY: a touch point in page-space (points) — QML divides
    // through by the render scale before calling these.
    Q_INVOKABLE void beginSelection(int pageIndex, qreal pageX, qreal pageY);
    Q_INVOKABLE void updateSelection(int pageIndex, qreal pageX, qreal pageY);
    Q_INVOKABLE void clearSelection();

signals:
    void selectionChanged();

private:
    PdfDocumentModel *m_documentModel; // non-owning
    PdfSelectionModel m_model;
};
