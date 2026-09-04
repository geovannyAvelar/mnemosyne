#include "LamportClock.h"

#include <QSettings>

namespace LamportClock {

quint64 tick()
{
    QSettings settings;
    const quint64 next = settings.value(QStringLiteral("Device/lamportClock"), 0).toULongLong() + 1;
    settings.setValue(QStringLiteral("Device/lamportClock"), next);
    return next;
}

void observe(quint64 remoteValue)
{
    QSettings settings;
    const quint64 current = settings.value(QStringLiteral("Device/lamportClock"), 0).toULongLong();
    if (remoteValue > current) {
        settings.setValue(QStringLiteral("Device/lamportClock"), remoteValue);
    }
}

} // namespace LamportClock
