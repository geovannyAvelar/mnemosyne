#include "HighlightExporter.h"

#include <QStringList>

namespace HighlightExporter {

namespace {

QString asBlockquote(const QString &text)
{
    QStringList quoted;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        quoted << QLatin1String("> ") + line;
    }
    return quoted.join(QLatin1Char('\n'));
}

// Collapses characters a raw TSV row can't represent (a literal tab would
// shift a field boundary; a newline would break the one-note-per-line
// contract Anki's plain import relies on) to a single space.
QString flattenForTsv(const QString &text)
{
    QString flattened = text;
    flattened.replace(QLatin1Char('\t'), QLatin1Char(' '));
    flattened.replace(QLatin1Char('\r'), QLatin1Char(' '));
    flattened.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return flattened;
}

} // namespace

QString toMarkdown(const QString &bookTitle, const QVector<ExportEntry> &entries)
{
    QStringList parts;
    parts << QLatin1String("# ") + bookTitle << QString();

    for (const ExportEntry &entry : entries) {
        const Highlight &h = entry.highlight;

        if (!entry.positionLabel.isEmpty()) {
            parts << QLatin1String("**") + entry.positionLabel + QLatin1String("**") << QString();
        }

        parts << asBlockquote(h.text) << QString();

        if (!h.note.isEmpty()) {
            parts << h.note << QString();
        }

        if (h.createdAt.isValid()) {
            parts << QLatin1String("*Highlighted ") + h.createdAt.toString(QStringLiteral("yyyy-MM-dd")) + QLatin1Char('*')
                  << QString();
        }

        parts << QLatin1String("---") << QString();
    }

    return parts.join(QLatin1Char('\n'));
}

QString toAnkiTsv(const QVector<ExportEntry> &entries)
{
    QStringList lines;
    for (const ExportEntry &entry : entries) {
        const Highlight &h = entry.highlight;
        if (h.note.isEmpty()) {
            continue;
        }
        lines << flattenForTsv(h.text) + QLatin1Char('\t') + flattenForTsv(h.note);
    }
    if (lines.isEmpty()) {
        return QString();
    }
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace HighlightExporter
