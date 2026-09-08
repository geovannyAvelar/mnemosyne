#include "PopplerPdfDocument.h"

#include <poppler-form.h>

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

QString PopplerPdfPage::text() const
{
    if (!m_page) {
        return {};
    }
    return m_page->text(QRectF()); // null rect => whole page
}

QVector<TextWord> PopplerPdfPage::words() const
{
    QVector<TextWord> result;
    if (!m_page) {
        return result;
    }

    const std::vector<std::unique_ptr<Poppler::TextBox>> boxes = m_page->textList();
    result.reserve(static_cast<int>(boxes.size()));
    for (const auto &box : boxes) {
        TextWord word;
        word.text = box->text();
        word.boundingBox = box->boundingBox();
        word.hasSpaceAfter = box->hasSpaceAfter();
        result.append(word);
    }
    return result;
}

PopplerPdfDocument::PopplerPdfDocument(std::unique_ptr<Poppler::Document> doc, QString fallbackTitle)
    : m_doc(std::move(doc))
    , m_fallbackTitle(std::move(fallbackTitle))
{
    m_doc->setRenderHint(Poppler::Document::Antialiasing, true);
    m_doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
}

std::unique_ptr<PopplerPdfDocument> PopplerPdfDocument::load(const QString &filePath, QString *errorMessage,
                                                               const QString &password)
{
    const QByteArray passwordUtf8 = password.toUtf8();
    std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(filePath, passwordUtf8, passwordUtf8);

    if (!doc) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Failed to open PDF: %1").arg(filePath);
        }
        return nullptr;
    }

    if (doc->isLocked()) {
        if (errorMessage) {
            *errorMessage = password.isEmpty()
                                ? QObject::tr("PDF is password-protected: %1").arg(filePath)
                                : QObject::tr("Incorrect password for: %1").arg(filePath);
        }
        return nullptr;
    }

    QString fallbackTitle = QFileInfo(filePath).completeBaseName();
    return std::unique_ptr<PopplerPdfDocument>(new PopplerPdfDocument(std::move(doc), std::move(fallbackTitle)));
}

bool PopplerPdfDocument::isPasswordProtected(const QString &filePath)
{
    const std::unique_ptr<Poppler::Document> doc = Poppler::Document::load(filePath);
    return doc && doc->isLocked();
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

QVector<PdfFormField> PopplerPdfDocument::formFields() const
{
    QVector<PdfFormField> result;
    const int pages = m_doc->numPages();
    for (int p = 0; p < pages; ++p) {
        const std::unique_ptr<Poppler::Page> page = m_doc->page(p);
        if (!page) {
            continue;
        }
        const std::vector<std::unique_ptr<Poppler::FormField>> fields = page->formFields();
        for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
            Poppler::FormField *field = fields[i].get();

            PdfFormField info;
            info.pageIndex = p;
            info.fieldIndex = i;
            info.name = field->fullyQualifiedName();
            info.readOnly = field->isReadOnly();

            switch (field->type()) {
            case Poppler::FormField::FormText: {
                auto *text = static_cast<Poppler::FormFieldText *>(field);
                info.type = PdfFormField::Type::Text;
                info.textValue = text->text();
                break;
            }
            case Poppler::FormField::FormButton: {
                auto *button = static_cast<Poppler::FormFieldButton *>(field);
                if (button->buttonType() == Poppler::FormFieldButton::Push) {
                    continue; // no value to show/edit -- see this method's doc comment
                }
                info.type = button->buttonType() == Poppler::FormFieldButton::Radio ? PdfFormField::Type::RadioButton
                                                                                     : PdfFormField::Type::CheckBox;
                info.checked = button->state();
                break;
            }
            case Poppler::FormField::FormChoice: {
                auto *choice = static_cast<Poppler::FormFieldChoice *>(field);
                info.type =
                    choice->choiceType() == Poppler::FormFieldChoice::ListBox ? PdfFormField::Type::ListBox
                                                                               : PdfFormField::Type::ComboBox;
                info.choices = choice->choices();
                const QList<int> current = choice->currentChoices();
                info.currentChoiceIndex = current.isEmpty() ? -1 : current.first();
                break;
            }
            case Poppler::FormField::FormSignature:
            default:
                continue; // no editable value here either
            }

            result.append(info);
        }
    }
    return result;
}

void PopplerPdfDocument::setFieldText(int pageIndex, int fieldIndex, const QString &value)
{
    const std::unique_ptr<Poppler::Page> page = m_doc->page(pageIndex);
    if (!page) {
        return;
    }
    const std::vector<std::unique_ptr<Poppler::FormField>> fields = page->formFields();
    if (fieldIndex < 0 || fieldIndex >= static_cast<int>(fields.size())) {
        return;
    }
    Poppler::FormField *field = fields[fieldIndex].get();
    // type()-then-static_cast, not dynamic_cast -- matches formFields()'s
    // own pattern above, and sidesteps a known pitfall where dynamic_cast
    // across a prebuilt shared library boundary can misbehave if that
    // library's RTTI/typeinfo visibility doesn't line up with the
    // consuming binary's.
    if (field->type() == Poppler::FormField::FormText) {
        static_cast<Poppler::FormFieldText *>(field)->setText(value);
    }
}

void PopplerPdfDocument::setFieldChecked(int pageIndex, int fieldIndex, bool checked)
{
    const std::unique_ptr<Poppler::Page> page = m_doc->page(pageIndex);
    if (!page) {
        return;
    }
    const std::vector<std::unique_ptr<Poppler::FormField>> fields = page->formFields();
    if (fieldIndex < 0 || fieldIndex >= static_cast<int>(fields.size())) {
        return;
    }
    Poppler::FormField *field = fields[fieldIndex].get();
    if (field->type() == Poppler::FormField::FormButton) {
        static_cast<Poppler::FormFieldButton *>(field)->setState(checked);
    }
}

void PopplerPdfDocument::setFieldChoiceIndex(int pageIndex, int fieldIndex, int choiceIndex)
{
    const std::unique_ptr<Poppler::Page> page = m_doc->page(pageIndex);
    if (!page) {
        return;
    }
    const std::vector<std::unique_ptr<Poppler::FormField>> fields = page->formFields();
    if (fieldIndex < 0 || fieldIndex >= static_cast<int>(fields.size())) {
        return;
    }
    Poppler::FormField *field = fields[fieldIndex].get();
    if (field->type() == Poppler::FormField::FormChoice) {
        static_cast<Poppler::FormFieldChoice *>(field)->setCurrentChoices(choiceIndex < 0 ? QList<int>()
                                                                                           : QList<int>{choiceIndex});
    }
}

bool PopplerPdfDocument::saveFilledFormAs(const QString &outputPath) const
{
    const std::unique_ptr<Poppler::PDFConverter> converter = m_doc->pdfConverter();
    if (!converter) {
        return false;
    }
    converter->setOutputFileName(outputPath);
    converter->setPDFOptions(Poppler::PDFConverter::WithChanges);
    return converter->convert();
}
