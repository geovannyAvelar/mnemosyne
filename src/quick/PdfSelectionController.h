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
// once — beginSelection() takes an explicit page index rather than
// assuming a single global "current page", and selectionPageIndex tells
// each delegate whether the active selection (if any) belongs to it.
class PdfSelectionController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectionRects READ selectionRects NOTIFY selectionChanged)
    Q_PROPERTY(int selectionPageIndex READ selectionPageIndex NOTIFY selectionChanged)

public:
    explicit PdfSelectionController(PdfDocumentModel *documentModel, QObject *parent = nullptr);

    QString selectedText() const { return m_model.selectedText(); }
    // page-space QRectF entries (QML's "rect" type), in reading order.
    QVariantList selectionRects() const;
    // The page a selection is currently active on, or -1 when there is none.
    int selectionPageIndex() const { return m_model.selectionPageIndex(); }

    // pageX/pageY: a touch point in page-space (points) — QML divides
    // through by the render scale before calling these.
    Q_INVOKABLE void beginSelection(int pageIndex, qreal pageX, qreal pageY);
    Q_INVOKABLE void updateSelection(qreal pageX, qreal pageY);
    Q_INVOKABLE void clearSelection();

signals:
    void selectionChanged();

private:
    PdfDocumentModel *m_documentModel; // non-owning
    PdfSelectionModel m_model;
};
