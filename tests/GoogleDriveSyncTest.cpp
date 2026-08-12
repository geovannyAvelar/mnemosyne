#include "app/DeviceIdentity.h"
#include "app/GoogleAuth.h"
#include "app/GoogleDriveSync.h"
#include "app/IHttpClient.h"
#include "app/ProgressSyncLog.h"

#include <QCoreApplication>
#include <QDir>
#include <QPair>
#include <QStandardPaths>
#include <QTest>
#include <QVector>

#include <memory>
#include <optional>

namespace {

// Responds to every call with the first response whose registered substring
// appears in the request URL, or a bare "{}" if nothing matches. Real Drive
// API calls never happen — everything resolves synchronously in the same
// call stack, so tests don't need an event loop.
class FakeHttpClient : public IHttpClient
{
public:
    struct Call
    {
        QString method;
        QString url;
        QByteArray body;
        QString contentType;
    };

    QVector<Call> calls;
    QVector<QPair<QString, Response>> responses;

    void get(const QString &url, const Headers &, Callback callback) override
    {
        calls.push_back({QStringLiteral("GET"), url, {}, {}});
        callback(responseFor(url));
    }

    void post(const QString &url, const Headers &, const QByteArray &body, const QString &contentType,
              Callback callback) override
    {
        calls.push_back({QStringLiteral("POST"), url, body, contentType});
        callback(responseFor(url));
    }

    void patch(const QString &url, const Headers &, const QByteArray &body, const QString &contentType,
               Callback callback) override
    {
        calls.push_back({QStringLiteral("PATCH"), url, body, contentType});
        callback(responseFor(url));
    }

private:
    Response responseFor(const QString &url) const
    {
        for (const auto &entry : responses) {
            if (url.contains(entry.first)) {
                return entry.second;
            }
        }
        return Response{200, QByteArray("{}"), QString()};
    }
};

QString stagingDirectoryForTest()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/GoogleDriveSync");
}

} // namespace

class GoogleDriveSyncTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void appendEntryCreatesThenPatchesTheRemoteFile();
    void latestFromOtherDevicesDownloadsAndSkipsUnchangedFiles();
    void latestFromOtherDevicesReturnsNulloptWhenSignedOut();
};

void GoogleDriveSyncTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));

    // A long-lived cached access token means withAccessToken() resolves
    // synchronously without any network refresh call, keeping these tests
    // free of any real HTTP traffic.
    GoogleAuth::setTokensForTesting(QStringLiteral("test-access-token"), QStringLiteral("test-refresh-token"), 3600);
}

void GoogleDriveSyncTest::cleanupTestCase()
{
    GoogleAuth::signOut();
    GoogleDriveSync::setHttpClientForTesting(nullptr);
    QDir(stagingDirectoryForTest()).removeRecursively();
}

void GoogleDriveSyncTest::appendEntryCreatesThenPatchesTheRemoteFile()
{
    {
        auto fake = std::make_unique<FakeHttpClient>();
        FakeHttpClient *fakePtr = fake.get();
        fakePtr->responses.push_back(
            {QStringLiteral("uploadType=multipart"),
             IHttpClient::Response{200, QByteArray(R"({"id":"created-file-id"})"), QString()}});
        GoogleDriveSync::setHttpClientForTesting(std::move(fake));

        GoogleDriveSync::appendEntry(QStringLiteral("book-append"), QStringLiteral("Book Append"), 3, 1.0);

        // No Drive file was known for this device yet, so the first append
        // must look it up, find nothing, and create it via a multipart POST.
        QCOMPARE(fakePtr->calls.size(), 2);
        QCOMPARE(fakePtr->calls.at(0).method, QStringLiteral("GET")); // find-by-name lookup
        QCOMPARE(fakePtr->calls.at(1).method, QStringLiteral("POST"));
        QVERIFY(fakePtr->calls.at(1).url.contains(QStringLiteral("uploadType=multipart")));
        QVERIFY(fakePtr->calls.at(1).contentType.contains(QStringLiteral("multipart/related")));
        QVERIFY(fakePtr->calls.at(1).body.contains("appDataFolder"));
        QVERIFY(fakePtr->calls.at(1).body.contains("book-append"));
    }

    {
        auto fake = std::make_unique<FakeHttpClient>();
        FakeHttpClient *fakePtr = fake.get();
        GoogleDriveSync::setHttpClientForTesting(std::move(fake));

        GoogleDriveSync::appendEntry(QStringLiteral("book-append"), QStringLiteral("Book Append"), 4, 1.0);

        // The Drive file id from the create above is now cached in-process,
        // so this append should go straight to a content-only PATCH.
        QCOMPARE(fakePtr->calls.size(), 1);
        QCOMPARE(fakePtr->calls.first().method, QStringLiteral("PATCH"));
        QVERIFY(fakePtr->calls.first().url.contains(QStringLiteral("created-file-id")));
        QVERIFY(fakePtr->calls.first().body.contains("book-append"));
    }
}

void GoogleDriveSyncTest::latestFromOtherDevicesDownloadsAndSkipsUnchangedFiles()
{
    const QByteArray remoteLine = QByteArray("{\"bookHash\":\"book-remote\",\"title\":\"Remote Book\",\"position\":42,"
                                              "\"zoom\":1.25,\"timestamp\":\"2026-01-01T00:00:00.000Z\","
                                              "\"deviceId\":\"device-remote\",\"deviceName\":\"Remote Device\"}\n");
    const QByteArray listing =
        R"({"files":[{"id":"remote-file-id","name":"device-remote.jsonl","modifiedTime":"2026-01-01T00:00:00.000Z"}]})";

    {
        auto fake = std::make_unique<FakeHttpClient>();
        FakeHttpClient *fakePtr = fake.get();
        fakePtr->responses.push_back({QStringLiteral("alt=media"), IHttpClient::Response{200, remoteLine, QString()}});
        fakePtr->responses.push_back(
            {QStringLiteral("spaces=appDataFolder"), IHttpClient::Response{200, listing, QString()}});
        GoogleDriveSync::setHttpClientForTesting(std::move(fake));

        bool called = false;
        std::optional<ProgressSyncLog::RemoteEntry> result;
        GoogleDriveSync::latestFromOtherDevices(
            QStringLiteral("book-remote"), QStringLiteral("this-device"),
            [&](std::optional<ProgressSyncLog::RemoteEntry> entry) {
                called = true;
                result = entry;
            });

        QVERIFY(called);
        QVERIFY(result.has_value());
        QCOMPARE(result->position, 42);
        QCOMPARE(result->deviceId, QStringLiteral("device-remote"));
        QCOMPARE(fakePtr->calls.size(), 2); // list + one download
    }

    {
        // Same modifiedTime as before: the file must not be re-downloaded,
        // but the merge should still find it via the locally staged copy.
        auto fake = std::make_unique<FakeHttpClient>();
        FakeHttpClient *fakePtr = fake.get();
        fakePtr->responses.push_back(
            {QStringLiteral("spaces=appDataFolder"), IHttpClient::Response{200, listing, QString()}});
        GoogleDriveSync::setHttpClientForTesting(std::move(fake));

        bool called = false;
        std::optional<ProgressSyncLog::RemoteEntry> result;
        GoogleDriveSync::latestFromOtherDevices(
            QStringLiteral("book-remote"), QStringLiteral("this-device"),
            [&](std::optional<ProgressSyncLog::RemoteEntry> entry) {
                called = true;
                result = entry;
            });

        QVERIFY(called);
        QVERIFY(result.has_value());
        QCOMPARE(result->position, 42);
        QCOMPARE(fakePtr->calls.size(), 1); // list only, no re-download
    }
}

void GoogleDriveSyncTest::latestFromOtherDevicesReturnsNulloptWhenSignedOut()
{
    GoogleAuth::signOut();

    bool called = false;
    std::optional<ProgressSyncLog::RemoteEntry> result;
    GoogleDriveSync::latestFromOtherDevices(QStringLiteral("book-remote"), QStringLiteral("this-device"),
                                             [&](std::optional<ProgressSyncLog::RemoteEntry> entry) {
                                                 called = true;
                                                 result = entry;
                                             });

    QVERIFY(called);
    QVERIFY(!result.has_value());

    // Restore signed-in state in case QTest ever runs slots out of declaration order.
    GoogleAuth::setTokensForTesting(QStringLiteral("test-access-token"), QStringLiteral("test-refresh-token"), 3600);
}

QTEST_MAIN(GoogleDriveSyncTest)
#include "GoogleDriveSyncTest.moc"
