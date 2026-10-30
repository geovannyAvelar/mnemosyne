#pragma once

#include "epub/EpubSearch.h"

#include <QString>
#include <QVector>

#include <functional>
#include <optional>

// Markdown and plain text have no spine of chapters, but a WebView can't
// load a whole large file comfortably and the mobile reader screen navigates
// by discrete parts, so both are sliced here: Markdown at its headings
// (MarkdownDocument::sections()), plain text into fixed-size runs broken at
// line ends.
struct TextPart
{
    QString label;
    QString source; // raw Markdown or plain text
    // What ReadingProgressStore holds for this format on every device:
    // desktop MarkdownView's heading index (-1 before the first heading),
    // desktop TxtView's character offset. See partIndexForPosition().
    int position = 0;
};

struct TextParts
{
    QString title;
    bool isMarkdown = false;
    QVector<TextPart> parts; // never empty on success
};

// Picks the format from filePath's suffix. Returns nullopt and fills
// errorMessage on failure.
std::optional<TextParts> loadTextParts(const QString &filePath, QString *errorMessage);

// The part holding a position saved by any device, so a desktop position
// partway through a part still lands on it.
int partIndexForPosition(const QVector<TextPart> &parts, int position);

// Self-contained HTML for WebView.loadHtml().
QString textPartHtml(const TextPart &part, bool isMarkdown);

// searchEpubFile()'s contract (see epub/EpubSearch.h), one result per
// matching part, with SearchResult::targetIndex as the part's index.
void searchTextFile(const QString &filePath, const QString &query,
                     const std::function<void(const SearchResult &)> &onResult,
                     const EpubSearchCancelToken &cancelToken);
