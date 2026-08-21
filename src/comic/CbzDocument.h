#pragma once

#include "core/Document.h"

#include <QStringList>

#include <memory>

class ZipArchive;

// A CBZ comic is just a zip of page images in reading order, with no
// manifest telling us what's inside (unlike EPUB's OPF) — pages are
// discovered by listing the whole archive, filtering to image files, and
// sorting naturally (so "page2.jpg" comes before "page10.jpg"). No text
// layer exists, so IPage::text()/words() are always empty; search and
// highlights are meaningless for this format and aren't wired up for it.
class CbzPage : public IPage
{
public:
    explicit CbzPage(QByteArray imageData);

    QSizeF sizePoints() const override;
    QImage renderToImage(qreal scale) const override;
    QString text() const override { return {}; }
    QVector<TextWord> words() const override { return {}; }

private:
    QByteArray m_imageData;
};

class CbzDocument : public IDocument
{
public:
    ~CbzDocument();

    // Returns nullptr and fills errorMessage on failure — including when
    // the archive contains no recognizable image files.
    static std::unique_ptr<CbzDocument> load(const QString &filePath, QString *errorMessage);

    int pageCount() const override;
    std::unique_ptr<IPage> page(int index) const override;
    QVector<TocNode> tableOfContents() const override { return {}; }
    QString title() const override { return m_title; }

private:
    CbzDocument();

    std::unique_ptr<ZipArchive> m_archive;
    QStringList m_pageEntries; // sorted image entry paths, in reading order
    QString m_title;
};
