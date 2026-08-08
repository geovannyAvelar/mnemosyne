#include "HtmlAttrUtil.h"

#include <QRegularExpression>

QString extractHtmlAttr(const QString &tag, const QString &attrName)
{
    QRegularExpression re(QStringLiteral("\\b%1\\s*=\\s*(\"([^\"]*)\"|'([^']*)')").arg(QRegularExpression::escape(attrName)),
                           QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(tag);
    if (!m.hasMatch()) {
        return {};
    }
    return m.captured(2).isNull() ? m.captured(3) : m.captured(2);
}
