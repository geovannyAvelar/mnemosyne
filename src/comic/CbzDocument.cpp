#include "CbzDocument.h"

#include "epub/ZipArchive.h"

#include <QBuffer>
#include <QCollator>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QObject>

#include <algorithm>

namespace {

bool isImageEntry(const QString &entryPath)
{
    // Skip directories, macOS zip junk, and any non-image metadata a comic
    // archive might carry (e.g. ComicInfo.xml) — anything QImageReader
    // doesn't recognize by extension is assumed not to be a page.
    if (entryPath.endsWith(QLatin1Char('/'))) {
        return false;
    }
    const QString fileName = entryPath.mid(entryPath.lastIndexOf(QLatin1Char('/')) + 1);
    if (fileName.startsWith(QLatin1Char('.'))) {
        return false;
    }

    static const QStringList kImageSuffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"), QStringLiteral("bmp"),  QStringLiteral("webp"),
    };
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    if (dot < 0) {
        return false;
    }
    return kImageSuffixes.contains(fileName.mid(dot + 1).toLower());
}

} // namespace

CbzPage::CbzPage(QByteArray imageData)
    : m_imageData(std::move(imageData))
{
}

QSizeF CbzPage::sizePoints() const
{
    QBuffer buffer(const_cast<QByteArray *>(&m_imageData));
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    // 1 image pixel == 1 "point" at scale 1.0 — there's no inherent DPI for
    // a comic page the way there is for a PDF, so this just treats native
    // resolution as the zoom baseline.
    return QSizeF(reader.size());
}

QImage CbzPage::renderToImage(qreal scale) const
{
    const QImage image = QImage::fromData(m_imageData);
    if (image.isNull() || scale == 1.0) {
        return image;
    }
    return image.scaled(image.size() * scale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

CbzDocument::CbzDocument() = default;
CbzDocument::~CbzDocument() = default;

std::unique_ptr<CbzDocument> CbzDocument::load(const QString &filePath, QString *errorMessage)
{
    std::unique_ptr<ZipArchive> archive = ZipArchive::open(filePath, errorMessage);
    if (!archive) {
        return nullptr;
    }

    QStringList pageEntries;
    for (const QString &entry : archive->entryNames()) {
        if (isImageEntry(entry)) {
            pageEntries.append(entry);
        }
    }

    if (pageEntries.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("No images found in archive: %1").arg(filePath);
        }
        return nullptr;
    }

    // Numeric-aware sort, so "page2.jpg" sorts before "page10.jpg" instead
    // of after it.
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(pageEntries.begin(), pageEntries.end(), collator);

    auto document = std::unique_ptr<CbzDocument>(new CbzDocument());
    document->m_archive = std::move(archive);
    document->m_pageEntries = std::move(pageEntries);
    document->m_title = QFileInfo(filePath).completeBaseName();
    return document;
}

int CbzDocument::pageCount() const
{
    return m_pageEntries.size();
}

std::unique_ptr<IPage> CbzDocument::page(int index) const
{
    if (index < 0 || index >= m_pageEntries.size()) {
        return nullptr;
    }
    bool ok = false;
    QByteArray data = m_archive->readEntry(m_pageEntries.at(index), &ok);
    if (!ok) {
        return nullptr;
    }
    return std::make_unique<CbzPage>(std::move(data));
}
