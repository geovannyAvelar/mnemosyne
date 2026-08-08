#include "ReadingProgressStore.h"

#include <QDateTime>
#include <QSettings>

namespace {
QString groupKey(const QString &bookHash)
{
    return QStringLiteral("ReadingProgress/%1").arg(bookHash);
}
} // namespace

namespace ReadingProgressStore {

std::optional<ReadingProgress> get(const QString &bookHash)
{
    if (bookHash.isEmpty()) {
        return std::nullopt;
    }

    QSettings settings;
    const QString group = groupKey(bookHash);
    if (!settings.contains(group + QStringLiteral("/position"))) {
        return std::nullopt;
    }

    ReadingProgress progress;
    progress.position = settings.value(group + QStringLiteral("/position"), 0).toInt();
    progress.zoom = settings.value(group + QStringLiteral("/zoom"), 1.0).toDouble();
    progress.updatedAt = settings.value(group + QStringLiteral("/updatedAt")).toDateTime();
    return progress;
}

void set(const QString &bookHash, int position, qreal zoom)
{
    if (bookHash.isEmpty()) {
        return;
    }

    QSettings settings;
    const QString group = groupKey(bookHash);
    settings.setValue(group + QStringLiteral("/position"), position);
    settings.setValue(group + QStringLiteral("/zoom"), zoom);
    settings.setValue(group + QStringLiteral("/updatedAt"), QDateTime::currentDateTimeUtc());
}

} // namespace ReadingProgressStore
