#pragma once

#include "core/Document.h"
#include "core/PdfFormField.h"

#include <poppler-qt6.h>

#include <map>
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

    // Every editable AcroForm field across the whole document (see
    // core/PdfFormField.h), in page then on-page order. The QVector itself
    // is recomputed fresh each call -- cheap enough (a plain page walk, no
    // rendering) -- but see formPage() below for why the underlying
    // Poppler::Page objects backing it are cached rather than reconstructed
    // every time. Push-button and signature fields are omitted: neither has
    // a value a side-panel field list can usefully show or edit.
    QVector<PdfFormField> formFields() const;
    // Each Set*() re-locates the field by (pageIndex, fieldIndex) -- see
    // PdfFormField::fieldIndex's own doc comment -- rather than holding a
    // Poppler::FormField pointer from an earlier formFields() call, since
    // those don't outlive the Poppler::Page that produced them. A no-op if
    // the field can't be found, or is the wrong type for the setter.
    void setFieldText(int pageIndex, int fieldIndex, const QString &value);
    void setFieldChecked(int pageIndex, int fieldIndex, bool checked);
    void setFieldChoiceIndex(int pageIndex, int fieldIndex, int choiceIndex);

    // Saves every field value set above into a *new* PDF at outputPath --
    // this never overwrites the file this document was opened from (same
    // never-mutate-the-original principle as Highlight/InkStroke, just
    // enforced by the caller choosing outputPath rather than by this class).
    // Uses Poppler::Document::pdfConverter() with WithChanges, the only save
    // path this Poppler-Qt6 version exposes (there's no plain
    // Document::save()).
    bool saveFilledFormAs(const QString &outputPath) const;

private:
    explicit PopplerPdfDocument(std::unique_ptr<Poppler::Document> doc, QString fallbackTitle);

    // The Poppler::Page backing form-field access for pageIndex, creating
    // and caching one on first use. A field edit (setFieldText() and
    // friends) mutates the specific Poppler::FormField/FormWidget objects
    // owned by whichever Poppler::Page produced them -- on at least one
    // Poppler build seen in CI (Homebrew's, on macOS; not reproducible
    // against the apt-packaged Poppler this was developed against), that
    // mutation didn't survive a later m_doc->page(index) call fetching a
    // *fresh* Page wrapper for the same index, so a set immediately
    // followed by a re-read (or by saveFilledFormAs()) silently saw the old
    // value. Every form-field method below goes through this cache instead
    // of calling m_doc->page() directly, so a page's edits and the reads/
    // save that follow all operate on the exact same Page object.
    Poppler::Page *formPage(int index) const;

    std::unique_ptr<Poppler::Document> m_doc;
    QString m_fallbackTitle;
    mutable std::map<int, std::unique_ptr<Poppler::Page>> m_formPageCache;
};
