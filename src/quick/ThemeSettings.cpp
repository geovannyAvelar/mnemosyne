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
