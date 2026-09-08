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

// Concatenates the text and per-word rects of words[startIndex..endIndex]
// inclusive (both must be valid indices into words, startIndex <= endIndex):
// a newline where two consecutive words' vertical centers differ by more
// than half a word's height (a line break), otherwise a space when the
// earlier word's hasSpaceAfter is set. This is the shared tail end of
// selectWordRange() below and of PdfSelectionModel's own per-page range
// selection (see there for why a whole page's range — not just one snapped
// from a point — also needs this).
TextSelectionResult wordRangeSelection(const QVector<TextWord> &words, int startIndex, int endIndex);

// Snaps anchorPoint and focusPoint (page-space points — e.g. a click-drag's
// or a touch long-press-drag's start and current position, divided through
// by the render scale first) to their nearest words via nearestWordIndex,
// then returns wordRangeSelection() from the earlier snapped word to the
// later one. Order of anchorPoint/focusPoint doesn't matter — the result
// always runs from the earlier word to the later one. Returns an empty
// result if words is empty.
//
// This is desktop PdfView's original click-drag selection logic
// (updateSelectionFromDrag/nearestWordIndex), extracted so a touch-driven
// QML selection (long-press + drag, see PdfSelectionController) can call
// the identical algorithm rather than reimplement it.
TextSelectionResult selectWordRange(const QVector<TextWord> &words, const QPointF &anchorPoint,
                                     const QPointF &focusPoint);
