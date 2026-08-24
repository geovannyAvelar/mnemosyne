#include "ContentUriCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace ContentUriCache {

QString resolveToLocalFile(const QString &filePathOrUri, const QString &format, QString *errorMessage)
{
    if (!filePathOrUri.startsWith(QStringLiteral("content://"))) {
        return filePathOrUri; // already a real filesystem path (desktop, or a non-SAF Android path)
    }

    const QDir cacheDir(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                             .filePath(QStringLiteral("opened_docs")));
    if (!cacheDir.exists() && !QDir().mkpath(cacheDir.absolutePath())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create cache directory for opened documents");
        }
        return {};
    }

    const QByteArray uriHash =
        QCryptographicHash::hash(filePathOrUri.toUtf8(), QCryptographicHash::Sha256).toHex();
    const QString extension = format == QStringLiteral("epub") ? QStringLiteral("epub") : QStringLiteral("pdf");
    const QString localPath =
        cacheDir.filePath(QString::fromLatin1(uriHash) + QLatin1Char('.') + extension);

    if (QFileInfo::exists(localPath)) {
        return localPath; // reuse the previously copied file for this URI
    }

    QFile source(filePathOrUri);
    if (!source.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not open picked document: %1").arg(source.errorString());
        }
        return {};
    }

    const QString tempPath = localPath + QStringLiteral(".part");
    QFile dest(tempPath);
    if (!dest.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create local cache file: %1").arg(dest.errorString());
        }
        return {};
    }

    constexpr qint64 kChunkSize = 1 << 20; // 1 MB
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(kChunkSize);
        if (dest.write(chunk) != chunk.size()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not write local cache file: %1").arg(dest.errorString());
            }
            dest.close();
            QFile::remove(tempPath);
            return {};
        }
    }
    dest.close();

    QFile::remove(localPath); // no-op if absent; guards a partial leftover from a previous crash
    if (!QFile::rename(tempPath, localPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not finalize local cache file");
        }
        QFile::remove(tempPath);
        return {};
    }

    return localPath;
}

} // namespace ContentUriCache
