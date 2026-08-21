#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <memory>

struct zip; // libzip opaque handle (zip_t)

// Thin RAII wrapper around libzip for reading EPUB/CBZ (zip) archive entries.
class ZipArchive
{
public:
    ~ZipArchive();

    ZipArchive(const ZipArchive &) = delete;
    ZipArchive &operator=(const ZipArchive &) = delete;

    static std::unique_ptr<ZipArchive> open(const QString &filePath, QString *errorMessage);

    bool hasEntry(const QString &entryPath) const;
    QByteArray readEntry(const QString &entryPath, bool *ok = nullptr) const;

    // Every entry's full path within the archive, in whatever order libzip
    // enumerates them (not necessarily reading order) — EPUB never needs
    // this (it knows every path up front from the OPF manifest), but CBZ has
    // no manifest, so CbzDocument discovers its pages by listing the whole
    // archive and filtering/sorting from there.
    QStringList entryNames() const;

private:
    explicit ZipArchive(zip *archive);

    zip *m_archive;
};
