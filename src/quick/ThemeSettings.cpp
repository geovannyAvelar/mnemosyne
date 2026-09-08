#include "ThemeSettings.h"

#include <QSettings>

bool ThemeSettings::dark() const
{
    return QSettings().value(QStringLiteral("darkMode"), false).toBool();
}

void ThemeSettings::setDark(bool dark)
{
    if (dark == this->dark()) {
        return;
    }
    QSettings().setValue(QStringLiteral("darkMode"), dark);
    emit darkChanged();
}

bool ThemeSettings::pdfPageDark() const
{
    return QSettings().value(QStringLiteral("pdfPageInvertColors"), false).toBool();
}

void ThemeSettings::setPdfPageDark(bool dark)
{
    if (dark == pdfPageDark()) {
        return;
    }
    QSettings().setValue(QStringLiteral("pdfPageInvertColors"), dark);
    emit pdfPageDarkChanged();
}
