#include "FileIdentity.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace {

QString cacheGroupKey(const QString &filePath)
{
    const QByteArray hash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Md5).toHex();
    return QStringLiteral("FileHashCache/%1").arg(QString::fromLatin1(hash));
}

QString hashFileContent(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hasher(QCryptographicHash::Sha256);
    constexpr qint64 kChunkSize = 1 << 20; // 1 MB, so large PDFs aren't read into memory all at once
    while (!file.atEnd()) {
        hasher.addData(file.read(kChunkSize));
    }
    return QString::fromLatin1(hasher.result().toHex());
}

} // namespace

namespace FileIdentity {

QString contentHash(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists()) {
        return {};
    }

    const qint64 size = info.size();
    const qint64 mtime = info.lastModified().toMSecsSinceEpoch();

    QSettings settings;
    const QString group = cacheGroupKey(filePath);
    const qint64 cachedSize = settings.value(group + QStringLiteral("/size"), -1).toLongLong();
    const qint64 cachedMtime = settings.value(group + QStringLiteral("/mtime"), -1).toLongLong();
    const QString cachedHash = settings.value(group + QStringLiteral("/hash")).toString();

    if (!cachedHash.isEmpty() && cachedSize == size && cachedMtime == mtime) {
        return cachedHash;
    }

    const QString hash = hashFileContent(filePath);
    if (!hash.isEmpty()) {
        settings.setValue(group + QStringLiteral("/size"), size);
        settings.setValue(group + QStringLiteral("/mtime"), mtime);
        settings.setValue(group + QStringLiteral("/hash"), hash);
    }
    return hash;
}

} // namespace FileIdentity
