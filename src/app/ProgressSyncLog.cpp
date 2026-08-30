#include "ProgressSyncLog.h"

#include "DeviceIdentity.h"
#include "SyncFolder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace ProgressSyncLog {

void appendEntry(const QString &bookHash, const QString &title, int position, qreal zoom)
{
    const QString dir = SyncFolder::dataDirectory();
    if (dir.isEmpty()) {
        return;
    }
    appendEntryToDirectory(dir, DeviceIdentity::id(), DeviceIdentity::name(), bookHash, title, position, zoom);
}

std::optional<RemoteEntry> latestFromOtherDevices(const QString &bookHash, const QString &excludeDeviceId)
{
    const QString dir = SyncFolder::dataDirectory();
    if (dir.isEmpty()) {
        return std::nullopt;
    }
    return latestFromDirectory(dir, bookHash, excludeDeviceId);
}

void appendEntryToDirectory(const QString &dir, const QString &deviceId, const QString &deviceName,
                             const QString &bookHash, const QString &title, int position, qreal zoom)
{
    if (dir.isEmpty() || bookHash.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["bookHash"] = bookHash;
    obj["title"] = title;
    obj["position"] = position;
    obj["zoom"] = zoom;
    obj["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    obj["deviceId"] = deviceId;
    obj["deviceName"] = deviceName;

    const QString filePath = QDir(dir).filePath(deviceId + QStringLiteral(".jsonl"));
    QFile file(filePath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.write("\n");
}

std::optional<RemoteEntry> latestFromDirectory(const QString &dir, const QString &bookHash,
                                                const QString &excludeDeviceId)
{
    if (dir.isEmpty() || bookHash.isEmpty()) {
        return std::nullopt;
    }

    std::optional<RemoteEntry> latest;

    const QStringList logFiles = QDir(dir).entryList(QStringList() << QStringLiteral("*.jsonl"), QDir::Files);
    for (const QString &fileName : logFiles) {
        if (QFileInfo(fileName).completeBaseName() == excludeDeviceId) {
            continue;
        }

        QFile file(QDir(dir).filePath(fileName));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        while (!file.atEnd()) {
            const QByteArray line = file.readLine();
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                continue; // tolerate a partially-written line from an in-progress cloud sync
            }

            const QJsonObject obj = doc.object();
            if (obj.value(QStringLiteral("bookHash")).toString() != bookHash) {
                continue;
            }

            RemoteEntry entry;
            entry.bookHash = bookHash;
            entry.title = obj.value(QStringLiteral("title")).toString();
            entry.position = obj.value(QStringLiteral("position")).toInt();
            entry.zoom = obj.value(QStringLiteral("zoom")).toDouble(1.0);
            entry.timestamp = QDateTime::fromString(obj.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
            entry.deviceId = obj.value(QStringLiteral("deviceId")).toString();
            entry.deviceName = obj.value(QStringLiteral("deviceName")).toString();

            if (!entry.timestamp.isValid()) {
                continue;
            }
            if (entry.deviceName.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) {
                continue; // dev/test machines often resolve their hostname to "localhost"; never worth a jump prompt
            }
            if (!latest || entry.timestamp > latest->timestamp) {
                latest = entry;
            }
        }
    }

    return latest;
}

} // namespace ProgressSyncLog
