#include "SearchUtil.h"

#include <algorithm>

QString makeSearchSnippet(const QString &haystack, const QString &needle, int contextChars)
{
    const int matchIndex = haystack.indexOf(needle, 0, Qt::CaseInsensitive);
    if (matchIndex < 0) {
        return {};
    }

    const int start = std::max(0, matchIndex - contextChars);
    const int end = std::min(haystack.size(), matchIndex + needle.size() + contextChars);

    QString snippet = haystack.mid(start, end - start).simplified();
    if (start > 0) {
        snippet.prepend(QStringLiteral("…"));
    }
    if (end < haystack.size()) {
        snippet.append(QStringLiteral("…"));
    }
    return snippet;
}
