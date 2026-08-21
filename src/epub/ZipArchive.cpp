#include "ZipArchive.h"

#include <zip.h>

ZipArchive::ZipArchive(zip *archive)
    : m_archive(archive)
{
}

ZipArchive::~ZipArchive()
{
    if (m_archive) {
        zip_close(reinterpret_cast<zip_t *>(m_archive));
    }
}

std::unique_ptr<ZipArchive> ZipArchive::open(const QString &filePath, QString *errorMessage)
{
    int errorCode = 0;
    zip_t *archive = zip_open(filePath.toUtf8().constData(), ZIP_RDONLY, &errorCode);

    if (!archive) {
        if (errorMessage) {
            zip_error_t zipError;
            zip_error_init_with_code(&zipError, errorCode);
            *errorMessage = QString::fromUtf8(zip_error_strerror(&zipError));
            zip_error_fini(&zipError);
        }
        return nullptr;
    }

    return std::unique_ptr<ZipArchive>(new ZipArchive(reinterpret_cast<zip *>(archive)));
}

bool ZipArchive::hasEntry(const QString &entryPath) const
{
    auto *archive = reinterpret_cast<zip_t *>(m_archive);
    return zip_name_locate(archive, entryPath.toUtf8().constData(), 0) >= 0;
}

QByteArray ZipArchive::readEntry(const QString &entryPath, bool *ok) const
{
    auto *archive = reinterpret_cast<zip_t *>(m_archive);
    const QByteArray nameUtf8 = entryPath.toUtf8();

    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat(archive, nameUtf8.constData(), 0, &stat) != 0) {
        if (ok) {
            *ok = false;
        }
        return {};
    }

    zip_file_t *file = zip_fopen(archive, nameUtf8.constData(), 0);
    if (!file) {
        if (ok) {
            *ok = false;
        }
        return {};
    }

    QByteArray data;
    data.resize(static_cast<int>(stat.size));
    const zip_int64_t bytesRead = zip_fread(file, data.data(), stat.size);
    zip_fclose(file);

    if (bytesRead < 0 || static_cast<zip_uint64_t>(bytesRead) != stat.size) {
        if (ok) {
            *ok = false;
        }
        return {};
    }

    if (ok) {
        *ok = true;
    }
    return data;
}

QStringList ZipArchive::entryNames() const
{
    auto *archive = reinterpret_cast<zip_t *>(m_archive);
    const zip_int64_t count = zip_get_num_entries(archive, 0);

    QStringList names;
    names.reserve(static_cast<int>(count));
    for (zip_int64_t i = 0; i < count; ++i) {
        if (const char *name = zip_get_name(archive, static_cast<zip_uint64_t>(i), 0)) {
            names.append(QString::fromUtf8(name));
        }
    }
    return names;
}
