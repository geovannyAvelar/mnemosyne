#pragma once

#include <QString>

// Extracts an HTML attribute's value from a single tag's source text, e.g.
// extractHtmlAttr("<img src=\"x.png\">", "src") == "x.png". Returns an empty
// string if the attribute isn't present.
QString extractHtmlAttr(const QString &tag, const QString &attrName);
