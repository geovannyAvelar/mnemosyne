#include "TextReaderTypography.h"

#include <QSettings>

namespace TextReaderTypography {

QString fontFamily()
{
    return QSettings().value(QStringLiteral("readerFontFamily")).toString();
}

void setFontFamily(const QString &family)
{
    QSettings().setValue(QStringLiteral("readerFontFamily"), family);
}

QVector<int> lineSpacingPercentSteps()
{
    return {100, 125, 150, 175};
}

int lineSpacingPercent()
{
    return QSettings().value(QStringLiteral("readerLineSpacingPercent"), 100).toInt();
}

void setLineSpacingPercent(int percent)
{
    QSettings().setValue(QStringLiteral("readerLineSpacingPercent"), percent);
}

QVector<int> marginPxSteps()
{
    // 4, not 0: QTextDocument::setDocumentMargin()'s own built-in default is
    // 4 -- "Narrow" reproduces that (today's existing look, before this
    // setting existed at all) rather than an unmargined flush edge nothing
    // ever actually rendered with before.
    return {4, 40, 80};
}

int marginPx()
{
    return QSettings().value(QStringLiteral("readerMarginPx"), 4).toInt();
}

void setMarginPx(int px)
{
    QSettings().setValue(QStringLiteral("readerMarginPx"), px);
}

QString bodyCss()
{
    QString rules;
    const QString family = fontFamily();
    if (!family.isEmpty()) {
        // Quoted: a family name with spaces ("Times New Roman") is only
        // valid CSS this way, and a quoted single-word name still parses
        // identically to an unquoted one.
        rules += QStringLiteral("font-family:'%1';").arg(family);
    }
    const int spacing = lineSpacingPercent();
    if (spacing != 100) {
        rules += QStringLiteral("line-height:%1%;").arg(spacing);
    }
    if (rules.isEmpty()) {
        return QString();
    }
    return QStringLiteral("body,p,div,span{%1}").arg(rules);
}

} // namespace TextReaderTypography
