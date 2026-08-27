#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;

// Basic title/author/cover-art info for an ISBN, looked up against the
// free, keyless Open Library API (openlibrary.org). This is the app's only
// network call unrelated to Drive sync, so it's strictly opt-in -- see
// MainWindow's "Look Up Book Info Online" toggle; callers should only
// invoke lookup() once the user has enabled that.
// Also doubles as the shape for EpubDocument/MobiDocument's local metadata
// (see BookInfoDock) -- MainWindow merges an API result into the local one
// field-by-field, so the two need to line up.
struct BookMetadata
{
    QString title;
    QStringList authors;
    QString publisher;
    QString publishDate;
    QString description;
    QImage cover; // null if no cover art was found
};

class BookMetadataClient : public QObject
{
    Q_OBJECT

public:
    explicit BookMetadataClient(QObject *parent = nullptr);

    // isbn must already be normalized: bare digits, optionally a trailing
    // 'X' for an ISBN-10 checksum (see EpubDocument::isbn()/
    // MobiDocument::isbn()). Answers from an in-memory or on-disk cache
    // immediately (synchronously, via metadataReady()) when available;
    // otherwise queries Open Library asynchronously. Exactly one of the two
    // signals below fires per call, including for a cached answer.
    void lookup(const QString &isbn);

signals:
    void metadataReady(const QString &isbn, const BookMetadata &metadata);
    void lookupFailed(const QString &isbn, const QString &reason);

private:
    void fetchCover(const QString &isbn, BookMetadata metadata);

    QNetworkAccessManager *m_networkManager;
    QHash<QString, BookMetadata> m_memoryCache;
    QSet<QString> m_inFlight;
};
