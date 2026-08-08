#include "DeviceIdentity.h"

#include <QSettings>
#include <QSysInfo>
#include <QUuid>

namespace DeviceIdentity {

QString id()
{
    QSettings settings;
    QString existing = settings.value(QStringLiteral("Device/id")).toString();
    if (!existing.isEmpty()) {
        return existing;
    }
    existing = QUuid::createUuid().toString(QUuid::WithoutBraces);
    settings.setValue(QStringLiteral("Device/id"), existing);
    return existing;
}

QString name()
{
    const QString hostName = QSysInfo::machineHostName();
    return hostName.isEmpty() ? QStringLiteral("Unknown Device") : hostName;
}

} // namespace DeviceIdentity
