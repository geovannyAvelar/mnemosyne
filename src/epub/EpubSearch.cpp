#include "EpubSearch.h"

#include "EpubDocument.h"

#include "core/SearchUtil.h"

#include <QTextDocument>

QVector<SearchResult> searchEpubFile(const QString &filePath, const QString &query)
{
    QVector<SearchResult> results;
    if (query.trimmed().isEmpty()) {
        return results;
    }

    QString error;
    const std::unique_ptr<EpubDocument> document = EpubDocument::load(filePath, &error);
    if (!document) {
        return results;
    }

    for (int i = 0; i < document->spineCount(); ++i) {
        QTextDocument doc;
        doc.setHtml(document->chapterHtml(i));
        const QString text = doc.toPlainText();
        if (text.contains(query, Qt::CaseInsensitive)) {
            SearchResult result;
            result.targetIndex = i;
            result.label = QStringLiteral("Chapter %1").arg(i + 1);
            result.snippet = makeSearchSnippet(text, query);
            results.append(result);
        }
    }
    return results;
}
