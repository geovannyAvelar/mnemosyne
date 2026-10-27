#include "PrintSupport.h"

#include "core/Document.h"

#include <QImage>
#include <QPageLayout>
#include <QPainter>
#include <QPrinter>
#include <QTextDocument>

#include <algorithm>

namespace PrintSupport {

bool printPages(QPrinter *printer, const IDocument &document)
{
    const int count = document.pageCount();
    if (count <= 0) {
        return false;
    }

    // fromPage()/toPage() are 1-based; 0 means "all".
    const int first = printer->fromPage() > 0 ? std::clamp(printer->fromPage() - 1, 0, count - 1) : 0;
    const int last = printer->toPage() > 0 ? std::clamp(printer->toPage() - 1, first, count - 1) : count - 1;

    QPainter painter;
    if (!painter.begin(printer)) {
        return false;
    }

    const QRectF areaPoints = printer->pageRect(QPrinter::Point);
    const QRect areaDevice = printer->pageRect(QPrinter::DevicePixel).toRect();
    const qreal dpiScale = printer->resolution() / 72.0;

    for (int i = first; i <= last; ++i) {
        if (i > first) {
            printer->newPage();
        }
        const std::unique_ptr<IPage> page = document.page(i);
        if (!page) {
            continue;
        }
        const QSizeF size = page->sizePoints();
        if (size.isEmpty()) {
            continue;
        }
        const qreal fit = std::min(areaPoints.width() / size.width(), areaPoints.height() / size.height());
        const QImage image = page->renderToImage(fit * dpiScale);
        if (image.isNull()) {
            continue;
        }
        const QPoint topLeft((areaDevice.width() - image.width()) / 2, (areaDevice.height() - image.height()) / 2);
        painter.drawImage(topLeft, image);
    }

    painter.end();
    return true;
}

bool printTextDocument(QPrinter *printer, const QTextDocument &document)
{
    const std::unique_ptr<QTextDocument> copy(document.clone());
    copy->print(printer);
    return true;
}

} // namespace PrintSupport
