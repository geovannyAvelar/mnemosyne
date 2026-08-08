#include "PopplerPdfDocument.h"

#include <QFileInfo>
#include <QObject>

namespace {

TocNode fromOutlineItemImpl(const Poppler::OutlineItem &item)
{
    TocNode node;
    node.title = item.name();

    if (auto dest = item.destination()) {
        node.pageNumber = dest->pageNumber() - 1; // Poppler destinations are 1-based
    }

    const QVector<Poppler::OutlineItem> children = item.children();
    node.children.reserve(children.size());
    for (const Poppler::OutlineItem &child : children) {
        node.children.append(fromOutlineItemImpl(child));
    }
    return node;
}

} // namespace

PopplerPdfPage::PopplerPdfPage(std::unique_ptr<Poppler::Page> page)
    : m_page(std::move(page))
{
}

QSizeF PopplerPdfPage::sizePoints() const
{
    if (!m_page) {
        return {};
    }
    return m_page->pageSizeF();
}

QImage PopplerPdfPage::renderToImage(qreal scale) const
{
    if (!m_page) {
        return {};
    }
    const double dpi = 72.0 * scale;
    return m_page->renderToImage(dpi, dpi);
}

QString PopplerPdfPage::text(const QRectF &rect) const
{
    if (!m_page) {
        return {};
    }
    return m_page->text(rect); // null rect => whole page
}

PopplerPdfDocument::PopplerPdfDocument(std::unique_ptr<Poppler::Document> doc, QString fallbackTitle)
    : m_doc(std::move(doc))
    , m_fallbackTitle(std::move(fallbackTitle))
{
    m_doc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
}

std::unique_ptr<PopplerPdfDocument> PopplerPdfDocument::load(const QString &filePath, QString *errorMessage)
{
    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(filePath);

    if (!doc) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to open PDF: %1").arg(filePath);
        }
        return nullptr;
    }

    if (doc->isLocked()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("PDF is password-protected: %1").arg(filePath);
        }
        return nullptr;
    }

    QString fallbackTitle = QFileInfo(filePath).completeBaseName();
    return std::unique_ptr<PopplerPdfDocument>(new PopplerPdfDocument(std::move(doc), std::move(fallbackTitle)));
}

int PopplerPdfDocument::pageCount() const
{
    return m_doc->numPages();
}

std::unique_ptr<IPage> PopplerPdfDocument::page(int index) const
{
    std::unique_ptr<Poppler::Page> p = m_doc->page(index);
    if (!p) {
        return nullptr;
    }
    return std::make_unique<PopplerPdfPage>(std::move(p));
}

QVector<TocNode> PopplerPdfDocument::tableOfContents() const
{
    QVector<TocNode> result;
    const QVector<Poppler::OutlineItem> outline = m_doc->outline();
    result.reserve(outline.size());
    for (const Poppler::OutlineItem &item : outline) {
        result.append(fromOutlineItemImpl(item));
    }
    return result;
}

QString PopplerPdfDocument::title() const
{
    const QString info = m_doc->info(QStringLiteral("Title"));
    return info.isEmpty() ? m_fallbackTitle : info;
}
