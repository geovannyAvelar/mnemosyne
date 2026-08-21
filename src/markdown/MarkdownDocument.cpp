#include "MarkdownDocument.h"

#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>

namespace {

// Builds a heading tree from ATX-style ("# Heading") lines, skipping any
// inside fenced code blocks (```/~~~) so a shell comment or Python "#"
// inside a code sample doesn't get mistaken for a heading. Setext-style
// ("Heading\n===") headings aren't recognized — rare enough in practice
// that a plain line scan (vs. a two-line lookahead parser) is worth it.
QVector<TocNode> buildTableOfContents(const QString &text)
{
    static const QRegularExpression headingPattern(QStringLiteral("^(#{1,6})\\s+(.+?)\\s*#*\\s*$"));

    QVector<TocNode> roots;

    // Headings currently "open" (haven't been closed by a same-or-shallower
    // heading yet), shallowest first. A new heading closes — i.e. attaches
    // as a finished subtree to its parent, or to roots if there is none —
    // every open heading at its level or deeper before opening itself.
    struct OpenNode
    {
        int level;
        TocNode node;
    };
    QVector<OpenNode> open;

    auto closeDownTo = [&](int level) {
        while (!open.isEmpty() && open.last().level >= level) {
            const TocNode finished = open.takeLast().node;
            if (!open.isEmpty()) {
                open.last().node.children.append(finished);
            } else {
                roots.append(finished);
            }
        }
    };

    bool insideFencedCode = false;
    int headingIndex = 0;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"))) {
            insideFencedCode = !insideFencedCode;
            continue;
        }
        if (insideFencedCode) {
            continue;
        }

        const QRegularExpressionMatch match = headingPattern.match(line);
        if (!match.hasMatch()) {
            continue;
        }

        const int level = match.captured(1).size();
        closeDownTo(level);

        TocNode node;
        node.title = match.captured(2).trimmed();
        node.pageNumber = headingIndex++;
        open.append({level, node});
    }
    closeDownTo(1); // flush everything still open, regardless of level

    return roots;
}

} // namespace

QVector<MarkdownDocument::Section> MarkdownDocument::sections() const
{
    static const QRegularExpression headingPattern(QStringLiteral("^(#{1,6})\\s+(.+?)\\s*#*\\s*$"));

    QVector<Section> result;
    Section current;
    current.label = m_title;

    QStringList bodyLines;
    auto flush = [&]() {
        current.text = bodyLines.join(QLatin1Char('\n'));
        result.append(current);
        bodyLines.clear();
    };

    bool insideFencedCode = false;
    int headingIndex = 0;
    for (const QString &line : m_markdownText.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"))) {
            insideFencedCode = !insideFencedCode;
            bodyLines.append(line);
            continue;
        }
        if (insideFencedCode) {
            bodyLines.append(line);
            continue;
        }

        const QRegularExpressionMatch match = headingPattern.match(line);
        if (match.hasMatch()) {
            flush();
            current = Section{};
            current.headingIndex = headingIndex++;
            current.label = match.captured(2).trimmed();
            continue;
        }
        bodyLines.append(line);
    }
    flush();

    return result;
}

std::unique_ptr<MarkdownDocument> MarkdownDocument::load(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not open file: %1").arg(filePath);
        }
        return nullptr;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString text = stream.readAll();

    auto document = std::unique_ptr<MarkdownDocument>(new MarkdownDocument());
    document->m_markdownText = text;
    document->m_toc = buildTableOfContents(text);
    document->m_title =
        document->m_toc.isEmpty() ? QFileInfo(filePath).completeBaseName() : document->m_toc.first().title;
    return document;
}
