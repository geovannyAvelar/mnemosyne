#include "EpubSearch.h"

#include "EpubDocument.h"

#include "core/SearchUtil.h"

#include <QTextDocument>

EpubSearchCancelToken makeSearchCancelToken()
{
    return std::make_shared<std::atomic_bool>(false);
}

void searchEpubFile(const QString &filePath, const QString &query,
                     const std::function<void(const SearchResult &)> &onResult,
                     const EpubSearchCancelToken &cancelToken)
{
    if (query.trimmed().isEmpty()) {
        return;
    }

    QString error;
    const std::unique_ptr<EpubDocument> document = EpubDocument::load(filePath, &error);
    if (!document) {
        return;
    }

    for (int i = 0; i < document->spineCount(); ++i) {
        if (cancelToken && cancelToken->load()) {
            return;
        }

        QTextDocument doc;
        doc.setHtml(document->chapterHtml(i));
        const QString text = doc.toPlainText();
        if (text.contains(query, Qt::CaseInsensitive)) {
            SearchResult result;
            result.targetIndex = i;
            result.label = QStringLiteral("Chapter %1").arg(i + 1);
            result.snippet = makeSearchSnippet(text, query);
            onResult(result);
        }
    }
}
