#pragma once

#include "core/Document.h" // TextWord

#include <QObject>
#include <QRectF>
#include <QString>
#include <QVariantList>
#include <QVector>

class PdfDocumentModel;

// Touch counterpart to desktop PdfPageCanvas's mouse click-drag selection:
// a long-press starts a selection at the nearest word, and dragging while
// still pressed extends it — same selectWordRange() word-snapping algorithm
// (see core/TextSelectionUtil.h) desktop's mouse-drag selection uses,
// just fed touch points instead of mouse events. The QML side (see
// qml/components/PdfPageItem.qml) is responsible for turning a
// long-press-then-drag gesture into begin/update calls with page-space
// points (pixel position divided by the render scale).
class PdfSelectionController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectionRects READ selectionRects NOTIFY selectionChanged)

public:
    explicit PdfSelectionController(PdfDocumentModel *documentModel, QObject *parent = nullptr);

    QString selectedText() const { return m_selectedText; }
    // page-space QRectF entries (QML's "rect" type), in reading order.
    QVariantList selectionRects() const;

    // pageX/pageY: a touch point in page-space (points) — QML divides
    // through by the render scale before calling these.
    Q_INVOKABLE void beginSelection(qreal pageX, qreal pageY);
    Q_INVOKABLE void updateSelection(qreal pageX, qreal pageY);
    Q_INVOKABLE void clearSelection();

signals:
    void selectionChanged();

private:
    void applySelection(const QPointF &focusPoint);

    PdfDocumentModel *m_documentModel; // non-owning
    QVector<TextWord> m_words; // current page's words, cached for the active gesture
    QPointF m_anchorPoint;
    QString m_selectedText;
    QVector<QRectF> m_selectionRects;
};
