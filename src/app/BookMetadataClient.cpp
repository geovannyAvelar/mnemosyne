#include "BookMetadataClient.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>

namespace {

QString cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/booklookup");
}

QString metadataCachePath(const QString &isbn)
{
    return cacheDir() + QLatin1Char('/') + isbn + QStringLiteral(".json");
}

QString coverCachePath(const QString &isbn)
{
    return cacheDir() + QLatin1Char('/') + isbn + QStringLiteral(".jpg");
}

BookMetadata metadataFromJson(const QJsonObject &data)
{
    BookMetadata metadata;
    metadata.title = data.value(QStringLiteral("title")).toString();
    metadata.publishDate = data.value(QStringLiteral("publish_date")).toString();
    for (const QJsonValue &authorValue : data.value(QStringLiteral("authors")).toArray()) {
        const QString name = authorValue.toObject().value(QStringLiteral("name")).toString();
        if (!name.isEmpty()) {
            metadata.authors.append(name);
        }
    }
    const QJsonArray publishers = data.value(QStringLiteral("publishers")).toArray();
    if (!publishers.isEmpty()) {
        metadata.publisher = publishers.first().toObject().value(QStringLiteral("name")).toString();
    }
    // jscmd=data has no dedicated description field; "excerpts" is the
    // closest it offers, and even that's frequently absent -- left empty
    // rather than guessed at when it's not there, since MainWindow falls
    // back to the document's own local description in that case anyway.
    const QJsonArray excerpts = data.value(QStringLiteral("excerpts")).toArray();
    if (!excerpts.isEmpty()) {
        metadata.description = excerpts.first().toObject().value(QStringLiteral("text")).toString();
    }
    return metadata;
}

} // namespace

BookMetadataClient::BookMetadataClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

void BookMetadataClient::lookup(const QString &isbn)
{
    if (isbn.isEmpty()) {
        return;
    }

    const auto cached = m_memoryCache.constFind(isbn);
    if (cached != m_memoryCache.constEnd()) {
        emit metadataReady(isbn, cached.value());
        return;
    }
    if (m_inFlight.contains(isbn)) {
        return;
    }

    QFile jsonFile(metadataCachePath(isbn));
    if (jsonFile.exists() && jsonFile.open(QIODevice::ReadOnly)) {
        BookMetadata metadata = metadataFromJson(QJsonDocument::fromJson(jsonFile.readAll()).object());

        // A cover file that exists but is empty means an earlier lookup
        // confirmed Open Library has none for this ISBN -- see
        // fetchCover()'s error branch -- so it's left as a null QImage
        // rather than treated as "not yet checked" and re-fetched.
        QFile coverFile(coverCachePath(isbn));
        if (coverFile.size() > 0) {
            metadata.cover.load(coverFile.fileName());
        }

        m_memoryCache.insert(isbn, metadata);
        emit metadataReady(isbn, metadata);
        return;
    }

    m_inFlight.insert(isbn);

    QUrl url(QStringLiteral("https://openlibrary.org/api/books"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("bibkeys"), QStringLiteral("ISBN:") + isbn);
    query.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("jscmd"), QStringLiteral("data"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, isbn, reply] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            m_inFlight.remove(isbn);
            emit lookupFailed(isbn, reply->errorString());
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject data = root.value(QStringLiteral("ISBN:") + isbn).toObject();
        if (data.isEmpty()) {
            m_inFlight.remove(isbn);
            emit lookupFailed(isbn, tr("No book information found for this ISBN."));
            return;
        }

        QDir().mkpath(cacheDir());
        QFile jsonFile(metadataCachePath(isbn));
        if (jsonFile.open(QIODevice::WriteOnly)) {
            jsonFile.write(QJsonDocument(data).toJson(QJsonDocument::Compact));
        }

        fetchCover(isbn, metadataFromJson(data));
    });
}

void BookMetadataClient::fetchCover(const QString &isbn, BookMetadata metadata)
{
    QUrl url(QStringLiteral("https://covers.openlibrary.org/b/isbn/%1-M.jpg").arg(isbn));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("default"), QStringLiteral("false"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, isbn, metadata, reply]() mutable {
        reply->deleteLater();

        QDir().mkpath(cacheDir());
        QFile coverFile(coverCachePath(isbn));

        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray bytes = reply->readAll();
            metadata.cover.loadFromData(bytes);
            if (coverFile.open(QIODevice::WriteOnly)) {
                coverFile.write(bytes);
            }
        } else {
            // Empty marker file: "checked, no cover available" (see
            // lookup()'s cache-read branch), so this isn't retried forever.
            coverFile.open(QIODevice::WriteOnly);
        }

        m_inFlight.remove(isbn);
        m_memoryCache.insert(isbn, metadata);
        emit metadataReady(isbn, metadata);
    });
}
