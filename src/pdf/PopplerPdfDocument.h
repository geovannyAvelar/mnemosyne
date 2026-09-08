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
    // Returns nullptr and fills errorMessage on failure -- including a
    // wrong/missing password against an encrypted file (see
    // isPasswordProtected() for how a caller distinguishes that case up
    // front, before ever calling this, in order to prompt instead of just
    // showing an error). password is tried as both the owner and user
    // password (Poppler::Document::load()'s two separate password
    // arguments) since callers here only ever collect one password from
    // the reader -- covers the common case where a file sets both to the
    // same value, and either "just a user password" or "just an owner
    // password" alone.
    static std::unique_ptr<PopplerPdfDocument> load(const QString &filePath, QString *errorMessage,
                                                      const QString &password = QString());

    // True if filePath is a PDF whose content is encrypted and can't be
    // read with an empty password -- i.e. load() with no password will
    // fail specifically because one is needed, not for some other reason
    // (corrupt file, wrong extension, etc.). Cheap: only reads the file's
    // header/trailer, no page parsing.
    static bool isPasswordProtected(const QString &filePath);

    int pageCount() const override;
    std::unique_ptr<IPage> page(int index) const override;
    QVector<TocNode> tableOfContents() const override;
    QString title() const override;

private:
    explicit PopplerPdfDocument(std::unique_ptr<Poppler::Document> doc, QString fallbackTitle);

    std::unique_ptr<Poppler::Document> m_doc;
    QString m_fallbackTitle;
};
