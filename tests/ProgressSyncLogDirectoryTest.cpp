#include "app/ProgressSyncLog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

// Exercises ProgressSyncLog::appendEntryToDirectory/latestFromDirectory
// directly against an arbitrary directory — the same logic SyncFolder-based
// sync uses (see SyncTest.cpp), but this is also what GoogleDriveSync's
// local staging directory relies on, so it's tested independent of both.
class ProgressSyncLogDirectoryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void findsLatestEntryAcrossDevices();
    void excludesGivenDeviceId();
    void excludesEntriesNamedLocalhost();
    void filtersByBookHash();
    void returnsNulloptForEmptyDirOrHash();
    void tolerateMalformedLine();
    void lamportClockWinsOverMisleadingTimestamp();
    void googleDriveNewerWithNoLocalFolderAnswer();
    void googleDriveNewerByLamportDespiteOlderTimestamp();
    void googleDriveNotNewerWhenLocalFolderHasHigherLamport();
    void googleDriveNewerByTimestampWhenNeitherHasLamport();

private:
    std::unique_ptr<QTemporaryDir> m_dir;
};

void ProgressSyncLogDirectoryTest::initTestCase()
{
    // appendEntryToDirectory() now ticks LamportClock internally, which is
    // QSettings-backed (see LamportClock.cpp) -- isolate from the real
    // app's settings the same way every other sync test does.
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void ProgressSyncLogDirectoryTest::init()
{
    QSettings().clear();
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void ProgressSyncLogDirectoryTest::findsLatestEntryAcrossDevices()
{
    ProgressSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-a"), QStringLiteral("Device A"),
                                             QStringLiteral("book1"), QStringLiteral("Book One"), 5, 1.0);
    QThread::msleep(10);
    ProgressSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-b"), QStringLiteral("Device B"),
                                             QStringLiteral("book1"), QStringLiteral("Book One"), 9, 1.5);

    const auto latest = ProgressSyncLog::latestFromDirectory(m_dir->path(), QStringLiteral("book1"),
                                                               QStringLiteral("some-other-device"));
    QVERIFY(latest.has_value());
    QCOMPARE(latest->position, 9);
    QCOMPARE(latest->deviceId, QStringLiteral("device-b"));
}

void ProgressSyncLogDirectoryTest::excludesGivenDeviceId()
{
    ProgressSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-a"), QStringLiteral("Device A"),
                                             QStringLiteral("book2"), QStringLiteral("Book Two"), 7, 1.0);

    const auto latest =
        ProgressSyncLog::latestFromDirectory(m_dir->path(), QStringLiteral("book2"), QStringLiteral("device-a"));
    QVERIFY(!latest.has_value());
}

void ProgressSyncLogDirectoryTest::excludesEntriesNamedLocalhost()
{
    // Dev/test machines often resolve their hostname to "localhost" (see
    // DeviceIdentity::name()); such entries should never surface as a jump
    // prompt, matched case-insensitively.
    ProgressSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-a"), QStringLiteral("localhost"),
                                             QStringLiteral("book3"), QStringLiteral("Book Three"), 3, 1.0);

    QVERIFY(!ProgressSyncLog::latestFromDirectory(m_dir->path(), QStringLiteral("book3"), QStringLiteral("nobody"))
                 .has_value());

    QThread::msleep(10);
    ProgressSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-b"), QStringLiteral("Localhost"),
                                             QStringLiteral("book3"), QStringLiteral("Book Three"), 4, 1.0);
    QThread::msleep(10);
    ProgressSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-c"), QStringLiteral("Device C"),
                                             QStringLiteral("book3"), QStringLiteral("Book Three"), 8, 1.0);

    const auto latest =
        ProgressSyncLog::latestFromDirectory(m_dir->path(), QStringLiteral("book3"), QStringLiteral("nobody"));
    QVERIFY(latest.has_value());
    QCOMPARE(latest->deviceId, QStringLiteral("device-c"));
    QCOMPARE(latest->position, 8);
}

void ProgressSyncLogDirectoryTest::filtersByBookHash()
{
    ProgressSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-a"), QStringLiteral("A"),
                                             QStringLiteral("book-a"), QStringLiteral("A"), 1, 1.0);
    ProgressSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-a"), QStringLiteral("A"),
                                             QStringLiteral("book-b"), QStringLiteral("B"), 2, 1.0);

    const auto latest =
        ProgressSyncLog::latestFromDirectory(m_dir->path(), QStringLiteral("book-a"), QStringLiteral("nobody"));
    QVERIFY(latest.has_value());
    QCOMPARE(latest->position, 1);
    QCOMPARE(latest->title, QStringLiteral("A"));
}

void ProgressSyncLogDirectoryTest::returnsNulloptForEmptyDirOrHash()
{
    QVERIFY(!ProgressSyncLog::latestFromDirectory(QString(), QStringLiteral("book"), QStringLiteral("nobody"))
                 .has_value());
    QVERIFY(!ProgressSyncLog::latestFromDirectory(m_dir->path(), QString(), QStringLiteral("nobody")).has_value());
}

void ProgressSyncLogDirectoryTest::tolerateMalformedLine()
{
    QFile file(QDir(m_dir->path()).filePath(QStringLiteral("some-other-device.jsonl")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not valid json at all\n");
    file.write("{\"bookHash\":\"book-c\",\"position\":9,\"zoom\":1.0,"
               "\"timestamp\":\"2026-01-01T00:00:00.000Z\",\"deviceId\":\"x\",\"deviceName\":\"X\"}\n");
    file.close();

    const auto latest =
        ProgressSyncLog::latestFromDirectory(m_dir->path(), QStringLiteral("book-c"), QStringLiteral("nobody"));
    QVERIFY(latest.has_value());
    QCOMPARE(latest->position, 9);
}

void ProgressSyncLogDirectoryTest::lamportClockWinsOverMisleadingTimestamp()
{
    // Hand-written lines (appendEntryToDirectory() always ticks a fresh
    // Lamport value internally, so it can't be used to inject a specific
    // one) simulating two devices whose wall clocks disagree with the true
    // order of events: device-a's line has the later timestamp but the
    // lower lamportClock; device-b's has an earlier timestamp but the
    // higher lamportClock. If Lamport ordering (not wall-clock) decides,
    // device-b's entry wins despite its "older" timestamp.
    QFile file(QDir(m_dir->path()).filePath(QStringLiteral("device-a.jsonl")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{\"bookHash\":\"book-skew\",\"position\":1,\"zoom\":1.0,"
               "\"timestamp\":\"2026-01-01T00:00:10.000Z\",\"lamportClock\":5,"
               "\"deviceId\":\"device-a\",\"deviceName\":\"Device A\"}\n");
    file.close();

    QFile file2(QDir(m_dir->path()).filePath(QStringLiteral("device-b.jsonl")));
    QVERIFY(file2.open(QIODevice::WriteOnly));
    file2.write("{\"bookHash\":\"book-skew\",\"position\":9,\"zoom\":1.5,"
                "\"timestamp\":\"2026-01-01T00:00:05.000Z\",\"lamportClock\":6,"
                "\"deviceId\":\"device-b\",\"deviceName\":\"Device B\"}\n");
    file2.close();

    const auto latest =
        ProgressSyncLog::latestFromDirectory(m_dir->path(), QStringLiteral("book-skew"), QStringLiteral("nobody"));
    QVERIFY(latest.has_value());
    QCOMPARE(latest->deviceId, QStringLiteral("device-b"));
    QCOMPARE(latest->position, 9);
}

void ProgressSyncLogDirectoryTest::googleDriveNewerWithNoLocalFolderAnswer()
{
    ProgressSyncLog::RemoteEntry googleRemote;
    googleRemote.deviceId = QStringLiteral("device-drive");
    googleRemote.timestamp = QDateTime::currentDateTimeUtc();
    QVERIFY(ProgressSyncLog::isGoogleDriveNewer(googleRemote, std::nullopt));
}

void ProgressSyncLogDirectoryTest::googleDriveNewerByLamportDespiteOlderTimestamp()
{
    ProgressSyncLog::RemoteEntry googleRemote;
    googleRemote.deviceId = QStringLiteral("device-drive");
    googleRemote.timestamp = QDateTime::currentDateTimeUtc().addSecs(-3600); // looks older by wall clock
    googleRemote.lamportClock = 10;

    ProgressSyncLog::RemoteEntry localFolderRemote;
    localFolderRemote.deviceId = QStringLiteral("device-local-folder");
    localFolderRemote.timestamp = QDateTime::currentDateTimeUtc(); // looks newer by wall clock
    localFolderRemote.lamportClock = 5;

    QVERIFY(ProgressSyncLog::isGoogleDriveNewer(googleRemote, localFolderRemote));
}

void ProgressSyncLogDirectoryTest::googleDriveNotNewerWhenLocalFolderHasHigherLamport()
{
    ProgressSyncLog::RemoteEntry googleRemote;
    googleRemote.deviceId = QStringLiteral("device-drive");
    googleRemote.timestamp = QDateTime::currentDateTimeUtc();
    googleRemote.lamportClock = 5;

    ProgressSyncLog::RemoteEntry localFolderRemote;
    localFolderRemote.deviceId = QStringLiteral("device-local-folder");
    localFolderRemote.timestamp = QDateTime::currentDateTimeUtc().addSecs(-3600);
    localFolderRemote.lamportClock = 10;

    QVERIFY(!ProgressSyncLog::isGoogleDriveNewer(googleRemote, localFolderRemote));
}

void ProgressSyncLogDirectoryTest::googleDriveNewerByTimestampWhenNeitherHasLamport()
{
    ProgressSyncLog::RemoteEntry googleRemote;
    googleRemote.deviceId = QStringLiteral("device-drive");
    googleRemote.timestamp = QDateTime::currentDateTimeUtc();
    // lamportClock left at its default (0) -- simulates a pre-migration entry.

    ProgressSyncLog::RemoteEntry localFolderRemote;
    localFolderRemote.deviceId = QStringLiteral("device-local-folder");
    localFolderRemote.timestamp = QDateTime::currentDateTimeUtc().addSecs(-3600);

    QVERIFY(ProgressSyncLog::isGoogleDriveNewer(googleRemote, localFolderRemote));
}

QTEST_MAIN(ProgressSyncLogDirectoryTest)
#include "ProgressSyncLogDirectoryTest.moc"
