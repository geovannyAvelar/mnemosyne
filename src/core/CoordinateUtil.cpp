#include "CoordinateUtil.h"

QRectF pixelRectToPageRect(const QRect &pixelRect, qreal scale)
{
    if (scale <= 0) {
        return {};
    }
    return QRectF(pixelRect.x() / scale, pixelRect.y() / scale,
                   pixelRect.width() / scale, pixelRect.height() / scale);
}

QRect pageRectToPixelRect(const QRectF &pageRect, qreal scale)
{
    if (scale <= 0) {
        return {};
    }
    return QRectF(pageRect.x() * scale, pageRect.y() * scale,
                   pageRect.width() * scale, pageRect.height() * scale)
        .toRect();
}
