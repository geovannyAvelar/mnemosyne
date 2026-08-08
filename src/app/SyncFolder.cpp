#include "SyncFolder.h"

#include <QDir>
#include <QSettings>

namespace SyncFolder {

QString path()
{
    return QSettings().value(QStringLiteral("Sync/folderPath")).toString();
}

void setPath(const QString &path)
{
    QSettings().setValue(QStringLiteral("Sync/folderPath"), path);
}

bool isConfigured()
{
    return !path().isEmpty();
}

QString dataDirectory()
{
    const QString base = path();
    if (base.isEmpty()) {
        return {};
    }
    const QString dir = QDir(base).filePath(QStringLiteral("MnemosyneSync"));
    QDir().mkpath(dir);
    return dir;
}

} // namespace SyncFolder
