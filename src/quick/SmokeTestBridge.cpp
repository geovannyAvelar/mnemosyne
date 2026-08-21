#include "SmokeTestBridge.h"

#include "core/Document.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QStandardPaths>

QString SmokeTestBridge::openSamplePdfSummary() const
{
    const QString destPath = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                                  .filePath("smoke_test_sample.pdf");

    QFile::remove(destPath);
    if (!QFile::copy(QStringLiteral(":/test_multipage.pdf"), destPath)) {
        return QStringLiteral("FAILED: could not extract bundled sample PDF");
    }

    QString errorMessage;
    std::unique_ptr<IDocument> document = openDocument(destPath, &errorMessage);
    if (!document) {
        return QStringLiteral("FAILED: openDocument() — %1").arg(errorMessage);
    }

    std::unique_ptr<IPage> firstPage = document->page(0);
    if (!firstPage) {
        return QStringLiteral("FAILED: page(0) returned null");
    }

    const QImage rendered = firstPage->renderToImage(1.0);
    if (rendered.isNull()) {
        return QStringLiteral("FAILED: renderToImage() produced a null image");
    }

    return QStringLiteral("OK — %1 pages, title \"%2\", page 0 rendered %3x%4")
        .arg(document->pageCount())
        .arg(document->title())
        .arg(rendered.width())
        .arg(rendered.height());
}
