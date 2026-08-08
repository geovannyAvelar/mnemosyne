#pragma once

#include "core/Document.h"

#include <poppler-qt6.h>

#include <memory>

class PopplerPdfPage : public IPage
{
public:
    explicit PopplerPdfPage(std::unique_ptr<Poppler::Page> page);

    QSizeF sizePoints() const override;
    QImage renderToImage(qreal scale) const override;
    QString text() const override;
    QVector<TextWord> words() const override;

private:
    std::unique_ptr<Poppler::Page> m_page;
};

class PopplerPdfDocument : public IDocument
{
public:
    // Returns nullptr and fills errorMessage on failure.
    static std::unique_ptr<PopplerPdfDocument> load(const QString &filePath, QString *errorMessage);

    int pageCount() const override;
    std::unique_ptr<IPage> page(int index) const override;
    QVector<TocNode> tableOfContents() const override;
    QString title() const override;

private:
    explicit PopplerPdfDocument(std::unique_ptr<Poppler::Document> doc, QString fallbackTitle);

    std::unique_ptr<Poppler::Document> m_doc;
    QString m_fallbackTitle;
};
