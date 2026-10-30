#include "TextParts.h"

#include "core/SearchUtil.h"
#include "markdown/MarkdownDocument.h"
#include "txt/TxtDocument.h"

#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QTextDocument>

#include <algorithm>

namespace {

constexpr int kTxtPartChars = 30000;

const QString kViewportMeta = QStringLiteral("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");

QVector<TextPart> markdownParts(const MarkdownDocument &document)
{
    QVector<TextPart> parts;
    for (const MarkdownDocument::Section &section : document.sections()) {
        if (section.headingIndex < 0) {
            if (!section.text.trimmed().isEmpty()) {
                parts.append({section.label, section.text, -1});
            }
            continue;
        }
        // sections() leaves the heading line itself out of text and doesn't
        // keep its level, hence the fixed "##".
        parts.append({section.label, QStringLiteral("## %1\n\n%2").arg(section.label, section.text),
                      section.headingIndex});
    }
    return parts;
}

QVector<TextPart> txtParts(const QString &text)
{
    QVector<TextPart> parts;
    int start = 0;
    while (start < text.size()) {
        int end = std::min<int>(start + kTxtPartChars, text.size());
        if (end < text.size()) {
            const int lineEnd = text.lastIndexOf(QLatin1Char('\n'), end - 1);
            if (lineEnd > start) {
                end = lineEnd + 1;
            } else if (text.at(end - 1).isHighSurrogate()) {
                --end; // no line break to use; don't split a surrogate pair
            }
        }
        parts.append({QObject::tr("Part %1").arg(parts.size() + 1), text.mid(start, end - start), start});
        start = end;
    }
    return parts;
}

} // namespace

std::optional<TextParts> loadTextParts(const QString &filePath, QString *errorMessage)
{
    TextParts result;
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QLatin1String("md") || suffix == QLatin1String("markdown")) {
        const std::unique_ptr<MarkdownDocument> document = MarkdownDocument::load(filePath, errorMessage);
        if (!document) {
            return std::nullopt;
        }
        result.title = document->title();
        result.isMarkdown = true;
        result.parts = markdownParts(*document);
    } else {
        const std::unique_ptr<TxtDocument> document = TxtDocument::load(filePath, errorMessage);
        if (!document) {
            return std::nullopt;
        }
        result.title = document->title();
        result.parts = txtParts(document->text());
    }

    if (result.parts.isEmpty()) {
        result.parts.append({result.title, QString(), 0});
    }
    return result;
}

int partIndexForPosition(const QVector<TextPart> &parts, int position)
{
    int index = 0;
    for (int i = 0; i < parts.size() && parts.at(i).position <= position; ++i) {
        index = i;
    }
    return index;
}

QString textPartHtml(const TextPart &part, bool isMarkdown)
{
    if (!isMarkdown) {
        return QStringLiteral("<!DOCTYPE html><html><head>%1</head><body style=\"white-space:pre-wrap;"
                              "overflow-wrap:break-word\">%2</body></html>")
            .arg(kViewportMeta, part.source.toHtmlEscaped());
    }

    QTextDocument document;
    document.setMarkdown(part.source);
    QString html = document.toHtml();
    html.replace(QStringLiteral("<head>"), QStringLiteral("<head>") + kViewportMeta);
    // toHtml() pins the body to the app's default font and point size, which
    // would override the WebView's own size for phone screens.
    static const QRegularExpression bodyStyle(QStringLiteral("<body style=\"[^\"]*\">"));
    html.replace(bodyStyle, QStringLiteral("<body>"));
    return html;
}

void searchTextFile(const QString &filePath, const QString &query,
                     const std::function<void(const SearchResult &)> &onResult,
                     const EpubSearchCancelToken &cancelToken)
{
    const std::optional<TextParts> loaded = loadTextParts(filePath, nullptr);
    if (!loaded) {
        return;
    }
    for (int i = 0; i < loaded->parts.size(); ++i) {
        if (cancelToken && cancelToken->load()) {
            return;
        }
        const TextPart &part = loaded->parts.at(i);
        const QString snippet = makeSearchSnippet(part.source, query);
        if (!snippet.isEmpty()) {
            onResult({i, part.label, snippet});
        }
    }
}
