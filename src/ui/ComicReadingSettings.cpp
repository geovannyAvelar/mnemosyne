#include "ComicReadingSettings.h"

#include <QSettings>

namespace ComicReadingSettings {

bool doublePageMode()
{
    return QSettings().value(QStringLiteral("comicDoublePageMode"), false).toBool();
}

void setDoublePageMode(bool enabled)
{
    QSettings().setValue(QStringLiteral("comicDoublePageMode"), enabled);
}

bool rightToLeft()
{
    return QSettings().value(QStringLiteral("comicRightToLeft"), false).toBool();
}

void setRightToLeft(bool enabled)
{
    QSettings().setValue(QStringLiteral("comicRightToLeft"), enabled);
}

} // namespace ComicReadingSettings
