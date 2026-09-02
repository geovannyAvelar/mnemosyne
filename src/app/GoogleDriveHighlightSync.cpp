#include "GoogleDriveHighlightSync.h"

#include "DeviceIdentity.h"
#include "GoogleAuth.h"
#include "IHttpClient.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QVector>

#include <memory>

namespace GoogleDriveHighlightSync {

namespace {

const auto kFilesEndpoint = QStringLiteral("https://www.googleapis.com/drive/v3/files");
const auto kUploadEndpoint = QStringLiteral("https://www.googleapis.com/upload/drive/v3/files");

std::unique_ptr<IHttpClient> &httpClientOverride()
{
    static std::unique_ptr<IHttpClient> override;
    return override;
}

IHttpClient &httpClient()
{
    if (httpClientOverride()) {
        return *httpClientOverride();
    }
    static QtHttpClient defaultClient;
    return defaultClient;
}

QString stagingDirectory()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/GoogleDriveHighlightSync");
    QDir().mkpath(dir);
    return dir;
}

QString authHeader(const QString &token)
{
    return QStringLiteral("Bearer ") + token;
}

QString ownFileName()
{
    return DeviceIdentity::id() + QStringLiteral("-highlights.jsonl");
}

// Drive file ID for this device's own <deviceId>-highlights.jsonl, once
// known — avoids a lookup-by-name round trip on every append after the
// first one this run.
QString &ownFileId()
{
    static QString id;
    return id;
}

// modifiedTime last seen per remote file name, so entriesForBook doesn't
// re-download files that haven't changed since the last check.
QHash<QString, QString> &remoteModifiedTimes()
{
    static QHash<QString, QString> times;
    return times;
}

void patchMedia(const QString &fileId, const QByteArray &content)
{
    GoogleAuth::withAccessToken([fileId, content](const QString &token) {
        if (token.isEmpty()) {
            return;
        }
        const QString url = kUploadEndpoint + QStringLiteral("/%1?uploadType=media").arg(fileId);
        httpClient().patch(url, {{QStringLiteral("Authorization"), authHeader(token)}}, content,
                            QStringLiteral("text/plain"), [](const IHttpClient::Response &) {
                                // Best-effort: a failed upload just means this device's
                                // highlights won't reach other devices until the next append.
                            });
    });
}

void createFile(const QString &name, const QByteArray &content)
{
    GoogleAuth::withAccessToken([name, content](const QString &token) {
        if (token.isEmpty()) {
            return;
        }

        QJsonObject metadata;
        metadata["name"] = name;
        metadata["parents"] = QJsonArray{QStringLiteral("appDataFolder")};
        const QByteArray metadataJson = QJsonDocument(metadata).toJson(QJsonDocument::Compact);

        const QByteArray boundary = "mnemosyne_highlight_sync_boundary";
        QByteArray body;
        body += "--" + boundary + "\r\n";
        body += "Content-Type: application/json; charset=UTF-8\r\n\r\n";
        body += metadataJson + "\r\n";
        body += "--" + boundary + "\r\n";
        body += "Content-Type: text/plain\r\n\r\n";
        body += content + "\r\n";
        body += "--" + boundary + "--";

        httpClient().post(
            kUploadEndpoint + QStringLiteral("?uploadType=multipart"), {{QStringLiteral("Authorization"), authHeader(token)}},
            body, QStringLiteral("multipart/related; boundary=") + QString::fromLatin1(boundary),
            [name](const IHttpClient::Response &response) {
                const QJsonDocument doc = QJsonDocument::fromJson(response.body);
                const QString id = doc.object().value(QStringLiteral("id")).toString();
                if (!id.isEmpty() && name == ownFileName()) {
                    ownFileId() = id;
                }
            });
    });
}

void uploadOwnFile(const QByteArray &content)
{
    if (!ownFileId().isEmpty()) {
        patchMedia(ownFileId(), content);
        return;
    }

    const QString name = ownFileName();
    GoogleAuth::withAccessToken([name, content](const QString &token) {
        if (token.isEmpty()) {
            return;
        }

        QUrl url(kFilesEndpoint);
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("appDataFolder"));
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id,name)"));
        query.addQueryItem(QStringLiteral("q"), QStringLiteral("name='%1' and trashed=false").arg(name));
        url.setQuery(query);

        httpClient().get(url.toString(), {{QStringLiteral("Authorization"), authHeader(token)}},
                          [name, content](const IHttpClient::Response &response) {
                              const QJsonDocument doc = QJsonDocument::fromJson(response.body);
                              const QJsonArray files = doc.object().value(QStringLiteral("files")).toArray();
                              if (!files.isEmpty()) {
                                  ownFileId() = files.first().toObject().value(QStringLiteral("id")).toString();
                                  patchMedia(ownFileId(), content);
                              } else {
                                  createFile(name, content);
                              }
                          });
    });
}

void appendAndUpload(const QString &bookHash, const QString &highlightId, HighlightSyncLog::Op op,
                      const Highlight &highlight)
{
    const QString dir = stagingDirectory();
    HighlightSyncLog::appendEntryToDirectory(dir, DeviceIdentity::id(), DeviceIdentity::name(), bookHash,
                                              highlightId, op, highlight);

    QFile file(QDir(dir).filePath(ownFileName()));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    uploadOwnFile(file.readAll());
}

} // namespace

bool isEnabled()
{
    return GoogleAuth::isSignedIn();
}

void appendUpsert(const QString &bookHash, const Highlight &highlight)
{
    if (!isEnabled()) {
        return;
    }
    appendAndUpload(bookHash, highlight.id, HighlightSyncLog::Op::Upsert, highlight);
}

void appendDelete(const QString &bookHash, const QString &highlightId)
{
    if (!isEnabled()) {
        return;
    }
    appendAndUpload(bookHash, highlightId, HighlightSyncLog::Op::Delete, {});
}

void entriesForBook(const QString &bookHash, const QString &excludeDeviceId,
                     std::function<void(QVector<HighlightSyncLog::RemoteEntry>)> callback)
{
    if (!isEnabled() || bookHash.isEmpty()) {
        callback({});
        return;
    }

    GoogleAuth::withAccessToken([bookHash, excludeDeviceId, callback](const QString &token) {
        if (token.isEmpty()) {
            callback({});
            return;
        }

        QUrl url(kFilesEndpoint);
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("appDataFolder"));
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id,name,modifiedTime)"));
        query.addQueryItem(QStringLiteral("q"), QStringLiteral("trashed=false"));
        url.setQuery(query);

        httpClient().get(
            url.toString(), {{QStringLiteral("Authorization"), authHeader(token)}},
            [bookHash, excludeDeviceId, callback, token](const IHttpClient::Response &response) {
                const QJsonDocument doc = QJsonDocument::fromJson(response.body);
                const QJsonArray files = doc.object().value(QStringLiteral("files")).toArray();

                const QString excludeFileName = excludeDeviceId + QStringLiteral("-highlights.jsonl");
                const QString dir = stagingDirectory();

                struct PendingFile
                {
                    QString id;
                    QString name;
                };
                QVector<PendingFile> toDownload;

                for (const QJsonValue &value : files) {
                    const QJsonObject obj = value.toObject();
                    const QString name = obj.value(QStringLiteral("name")).toString();
                    if (name == excludeFileName || !name.endsWith(QStringLiteral("-highlights.jsonl"))) {
                        continue;
                    }

                    const QString modifiedTime = obj.value(QStringLiteral("modifiedTime")).toString();
                    if (remoteModifiedTimes().value(name) == modifiedTime) {
                        continue; // unchanged since the last check
                    }
                    remoteModifiedTimes()[name] = modifiedTime;
                    toDownload.push_back({obj.value(QStringLiteral("id")).toString(), name});
                }

                if (toDownload.isEmpty()) {
                    callback(HighlightSyncLog::entriesFromDirectory(dir, bookHash, excludeDeviceId));
                    return;
                }

                auto pending = std::make_shared<int>(toDownload.size());
                for (const PendingFile &pf : toDownload) {
                    const QString fileUrl = kFilesEndpoint + QStringLiteral("/%1?alt=media").arg(pf.id);
                    httpClient().get(
                        fileUrl, {{QStringLiteral("Authorization"), authHeader(token)}},
                        [pf, dir, pending, bookHash, excludeDeviceId, callback](const IHttpClient::Response &fileResponse) {
                            QFile file(QDir(dir).filePath(pf.name));
                            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                                file.write(fileResponse.body);
                                // Close (flushing to disk) before the merge below opens a
                                // separate QFile handle to read it back — otherwise buffered
                                // writes may not be visible yet to that second handle.
                                file.close();
                            }
                            if (--(*pending) == 0) {
                                callback(HighlightSyncLog::entriesFromDirectory(dir, bookHash, excludeDeviceId));
                            }
                        });
                }
            });
    });
}

void setHttpClientForTesting(std::unique_ptr<IHttpClient> client)
{
    httpClientOverride() = std::move(client);
}

} // namespace GoogleDriveHighlightSync
