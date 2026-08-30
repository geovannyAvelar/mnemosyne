#include "app/ProgressSyncLog.h"

#include <QDir>
#include <QFile>
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
    void init();

    void findsLatestEntryAcrossDevices();
    void excludesGivenDeviceId();
    void excludesEntriesNamedLocalhost();
    void filtersByBookHash();
    void returnsNulloptForEmptyDirOrHash();
    void tolerateMalformedLine();

private:
    std::unique_ptr<QTemporaryDir> m_dir;
};

void ProgressSyncLogDirectoryTest::init()
{
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

QTEST_MAIN(ProgressSyncLogDirectoryTest)
#include "ProgressSyncLogDirectoryTest.moc"
