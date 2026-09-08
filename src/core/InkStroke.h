#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>

// A single freehand pen stroke drawn on a PDF page (see PdfPageStackView's
// draw mode) -- stored entirely within Mnemosyne via InkStore, never written
// into the PDF file itself, the same way Highlight already works. points are
// page-space (points, 1/72in, same convention as Highlight::pageRect), so a
// stroke scales correctly with zoom without needing to be re-captured.
struct InkStroke
{
    int targetIndex = -1; // PDF page index
    QVector<QPointF> points; // in drawing order; at least 2 once committed
    QColor color = Qt::black;
    qreal width = 2.0; // points, pre-zoom -- multiplied by the current zoom when painted
    QString id; // stable UUID; not used for sync yet, but keeps that door open (mirrors Highlight::id)
};
