#pragma once

#include "core/Document.h" // TextWord

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

// Finds the word in words (reading order, from IPage::words()) whose box is
// closest to point, in page-space (points): vertical distance dominates so
// the right line is picked first, then the closest word on that line.
// Returns -1 if words is empty.
int nearestWordIndex(const QVector<TextWord> &words, const QPointF &point);

struct TextSelectionResult
{
    QString text;
    QVector<QRectF> wordRects; // page-space (points), in reading order
};

// Snaps anchorPoint and focusPoint (page-space points — e.g. a click-drag's
// or a touch long-press-drag's start and current position, divided through
// by the render scale first) to their nearest words via nearestWordIndex,
// then returns the concatenated text and per-word rects of every word from
// the earlier snapped word to the later one: a newline where the words'
// vertical centers differ by more than half a word's height (a line break),
// otherwise a space when the earlier word's hasSpaceAfter is set. Order of
// anchorPoint/focusPoint doesn't matter — the result always runs from the
// earlier word to the later one. Returns an empty result if words is empty.
//
// This is desktop PdfView's original click-drag selection logic
// (updateSelectionFromDrag/nearestWordIndex), extracted so a touch-driven
// QML selection (long-press + drag, see PdfSelectionController) can call
// the identical algorithm rather than reimplement it.
TextSelectionResult selectWordRange(const QVector<TextWord> &words, const QPointF &anchorPoint,
                                     const QPointF &focusPoint);
