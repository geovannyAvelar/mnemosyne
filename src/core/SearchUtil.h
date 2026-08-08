#pragma once

#include <QString>

// Builds a "...context around the first match..." snippet for search
// results. Returns an empty string if needle isn't found in haystack.
QString makeSearchSnippet(const QString &haystack, const QString &needle, int contextChars = 40);
