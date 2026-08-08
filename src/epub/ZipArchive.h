#pragma once

#include <QByteArray>
#include <QString>

#include <memory>

struct zip; // libzip opaque handle (zip_t)

// Thin RAII wrapper around libzip for reading EPUB (zip) archive entries.
class ZipArchive
{
public:
    ~ZipArchive();

    ZipArchive(const ZipArchive &) = delete;
    ZipArchive &operator=(const ZipArchive &) = delete;

    static std::unique_ptr<ZipArchive> open(const QString &filePath, QString *errorMessage);

    bool hasEntry(const QString &entryPath) const;
    QByteArray readEntry(const QString &entryPath, bool *ok = nullptr) const;

private:
    explicit ZipArchive(zip *archive);

    zip *m_archive;
};
