#include "HighlightSyncLog.h"

#include "DeviceIdentity.h"
#include "LamportClock.h"
#include "SyncFolder.h"
#include "SyncOrdering.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>

namespace HighlightSyncLog {

namespace {

QString opToString(Op op)
{
    return op == Op::Delete ? QStringLiteral("delete") : QStringLiteral("upsert");
}

Op opFromString(const QString &s)
{
    return s == QStringLiteral("delete") ? Op::Delete : Op::Upsert;
}

} // namespace

void appendEntryToDirectory(const QString &dir, const QString &deviceId, const QString &deviceName,
                             const QString &bookHash, const QString &highlightId, Op op, const Highlight &highlight)
{
    if (dir.isEmpty() || bookHash.isEmpty() || highlightId.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj["id"] = highlightId;
    obj["bookHash"] = bookHash;
    obj["op"] = opToString(op);
    if (op == Op::Upsert) {
        obj["targetIndex"] = highlight.targetIndex;
        obj["rectX"] = highlight.pageRect.x();
        obj["rectY"] = highlight.pageRect.y();
        obj["rectW"] = highlight.pageRect.width();
        obj["rectH"] = highlight.pageRect.height();
        obj["text"] = highlight.text;
        obj["note"] = highlight.note;
        obj["colorRgba"] = static_cast<qint64>(highlight.color.rgba());
        obj["createdAt"] = highlight.createdAt.toUTC().toString(Qt::ISODateWithMs);
    }
    const QDateTime updatedAt = highlight.updatedAt.isValid() ? highlight.updatedAt : QDateTime::currentDateTimeUtc();
    obj["updatedAt"] = updatedAt.toUTC().toString(Qt::ISODateWithMs);
    obj["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    // Upsert: reuse the highlight's own Lamport value (already ticked by
    // HighlightStore when it was edited) rather than ticking again here --
    // this is what lets the value round-trip through a receiving device
    // and back into its own local storage unchanged (see
    // entriesFromDirectory() below). Delete has no highlight content to
    // draw one from, so it ticks fresh.
    obj["lamportClock"] =
        static_cast<qint64>(op == Op::Upsert ? highlight.lamportClock : LamportClock::tick());
    obj["deviceId"] = deviceId;
    obj["deviceName"] = deviceName;

    const QString filePath = QDir(dir).filePath(deviceId + QStringLiteral("-highlights.jsonl"));
    QFile file(filePath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    file.write("\n");
}

QVector<RemoteEntry> entriesFromDirectory(const QString &dir, const QString &bookHash,
                                           const QString &excludeDeviceId)
{
    QVector<RemoteEntry> result;
    if (dir.isEmpty() || bookHash.isEmpty()) {
        return result;
    }

    quint64 maxObservedLamport = 0;

    const QStringList logFiles =
        QDir(dir).entryList(QStringList() << QStringLiteral("*-highlights.jsonl"), QDir::Files);
    for (const QString &fileName : logFiles) {
        const QString deviceId = fileName.chopped(QStringLiteral("-highlights.jsonl").size());
        if (deviceId == excludeDeviceId) {
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
            entry.id = obj.value(QStringLiteral("id")).toString();
            entry.bookHash = bookHash;
            entry.op = opFromString(obj.value(QStringLiteral("op")).toString());
            entry.timestamp = QDateTime::fromString(obj.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
            entry.lamportClock = static_cast<quint64>(obj.value(QStringLiteral("lamportClock")).toDouble());
            entry.deviceId = obj.value(QStringLiteral("deviceId")).toString();
            entry.deviceName = obj.value(QStringLiteral("deviceName")).toString();

            if (entry.id.isEmpty() || !entry.timestamp.isValid()) {
                continue;
            }

            if (entry.op == Op::Upsert) {
                Highlight &h = entry.highlight;
                h.id = entry.id;
                h.targetIndex = obj.value(QStringLiteral("targetIndex")).toInt(-1);
                const double x = obj.value(QStringLiteral("rectX")).toDouble();
                const double y = obj.value(QStringLiteral("rectY")).toDouble();
                const double w = obj.value(QStringLiteral("rectW")).toDouble();
                const double hgt = obj.value(QStringLiteral("rectH")).toDouble();
                if (w > 0 && hgt > 0) {
                    h.pageRect = QRectF(x, y, w, hgt);
                }
                h.text = obj.value(QStringLiteral("text")).toString();
                h.note = obj.value(QStringLiteral("note")).toString();
                h.color = QColor::fromRgba(static_cast<QRgb>(obj.value(QStringLiteral("colorRgba")).toDouble()));
                h.createdAt = QDateTime::fromString(obj.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
                h.updatedAt = QDateTime::fromString(obj.value(QStringLiteral("updatedAt")).toString(), Qt::ISODateWithMs);
                // Propagates the wire-level Lamport value into the embedded
                // highlight so a receiving device's local copy carries it
                // once merged in (see HighlightSync::mergeEntries) --
                // without this, every highlight a device only *receives*
                // (never edits itself) would keep lamportClock == 0
                // forever, forcing wall-clock fallback for it indefinitely.
                h.lamportClock = entry.lamportClock;
            }

            maxObservedLamport = std::max(maxObservedLamport, entry.lamportClock);
            result.append(entry);
        }
    }

    if (maxObservedLamport > 0) {
        LamportClock::observe(maxObservedLamport);
    }

    return result;
}

void appendUpsert(const QString &bookHash, const Highlight &highlight)
{
    const QString dir = SyncFolder::dataDirectory();
    if (dir.isEmpty()) {
        return;
    }
    appendEntryToDirectory(dir, DeviceIdentity::id(), DeviceIdentity::name(), bookHash, highlight.id, Op::Upsert,
                            highlight);
}

void appendDelete(const QString &bookHash, const QString &highlightId)
{
    const QString dir = SyncFolder::dataDirectory();
    if (dir.isEmpty()) {
        return;
    }
    appendEntryToDirectory(dir, DeviceIdentity::id(), DeviceIdentity::name(), bookHash, highlightId, Op::Delete, {});
}

QVector<RemoteEntry> entriesForBook(const QString &bookHash, const QString &excludeDeviceId)
{
    const QString dir = SyncFolder::dataDirectory();
    if (dir.isEmpty()) {
        return {};
    }
    return entriesFromDirectory(dir, bookHash, excludeDeviceId);
}

} // namespace HighlightSyncLog
