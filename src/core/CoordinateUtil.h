#pragma once

#include <QRect>
#include <QRectF>

// Converts a selection rectangle in rendered-image pixels back to page-space
// points (1/72 inch), given the render scale used to produce that image
// (renderToImage(scale) means scale 1.0 == 72 DPI, so pixels == points * scale).
QRectF pixelRectToPageRect(const QRect &pixelRect, qreal scale);

// Inverse of pixelRectToPageRect: converts a stored page-space rect (points)
// back to image pixels for the given render scale, e.g. to draw a persisted
// highlight over a freshly rendered page.
QRect pageRectToPixelRect(const QRectF &pageRect, qreal scale);
