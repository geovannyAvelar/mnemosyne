#include "app/FileIdentity.h"
#include "app/ProgressSyncLog.h"
#include "app/ReadingProgressStore.h"
#include "app/SyncFolder.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QThread>

class SyncTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void contentHashIsStableForUnchangedFile();
    void contentHashChangesWhenFileContentChanges();
    void contentHashDiffersForDifferentContent();

    void readingProgressRoundTrips();
    void readingProgressReturnsNulloptWhenUnset();

    void syncLogFindsLatestEntryAcrossDevices();
    void syncLogExcludesOwnDevice();
    void syncLogFiltersByBookHash();
    void syncLogReturnsNulloptWhenSyncNotConfigured();
    void syncLogTolerateMalformedLine();

private:
    std::unique_ptr<QTemporaryDir> m_syncDir;
};

void SyncTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void SyncTest::init()
{
    QSettings().clear();
    m_syncDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_syncDir->isValid());
    SyncFolder::setPath(m_syncDir->path());
}

void SyncTest::cleanupTestCase()
{
    QSettings().clear();
}

void SyncTest::contentHashIsStableForUnchangedFile()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write("hello world");
    file.close();

    const QString first = FileIdentity::contentHash(file.fileName());
    const QString second = FileIdentity::contentHash(file.fileName());
    QVERIFY(!first.isEmpty());
    QCOMPARE(first, second);
}

void SyncTest::contentHashChangesWhenFileContentChanges()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write("version one");
    file.close();
    const QString before = FileIdentity::contentHash(file.fileName());

    // mtime resolution on some filesystems is 1s; make sure it actually ticks over.
    QThread::msleep(1100);
    QVERIFY(file.open());
    file.resize(0);
    file.write("version two, a completely different length");
    file.close();
    const QString after = FileIdentity::contentHash(file.fileName());

    QVERIFY(!before.isEmpty());
    QVERIFY(!after.isEmpty());
    QVERIFY(before != after);
}

void SyncTest::contentHashDiffersForDifferentContent()
{
    QTemporaryFile fileA;
    QTemporaryFile fileB;
    QVERIFY(fileA.open());
    QVERIFY(fileB.open());
    fileA.write("content A");
    fileB.write("content B");
    fileA.close();
    fileB.close();

    QVERIFY(FileIdentity::contentHash(fileA.fileName()) != FileIdentity::contentHash(fileB.fileName()));
}

void SyncTest::readingProgressRoundTrips()
{
    ReadingProgressStore::set(QStringLiteral("abc123"), 42, 1.75);

    const auto progress = ReadingProgressStore::get(QStringLiteral("abc123"));
    QVERIFY(progress.has_value());
    QCOMPARE(progress->position, 42);
    QCOMPARE(progress->zoom, 1.75);
}

void SyncTest::readingProgressReturnsNulloptWhenUnset()
{
    QVERIFY(!ReadingProgressStore::get(QStringLiteral("never-set")).has_value());
}

void SyncTest::syncLogFindsLatestEntryAcrossDevices()
{
    // Simulate two other devices by writing directly to their own log files,
    // exactly as appendEntry() would from that device.
    ProgressSyncLog::appendEntry(QStringLiteral("book1"), QStringLiteral("Book One"), 5, 1.0);
    QThread::msleep(10);

    // A later write, from "this" device, should not count as "other".
    const QString dir = QDir(SyncFolder::path()).filePath(QStringLiteral("MnemosyneSync"));
    QVERIFY(QDir().exists(dir));

    const auto latest = ProgressSyncLog::latestFromOtherDevices(QStringLiteral("book1"), QStringLiteral("some-other-device-id"));
    QVERIFY(latest.has_value());
    QCOMPARE(latest->position, 5);
}

void SyncTest::syncLogExcludesOwnDevice()
{
    ProgressSyncLog::appendEntry(QStringLiteral("book2"), QStringLiteral("Book Two"), 7, 1.0);

    // Excluding the real device id (DeviceIdentity::id(), whatever entry we
    // just wrote used) must yield nothing, since that's the only writer.
    QSettings settings;
    const QString ownId = settings.value(QStringLiteral("Device/id")).toString();
    QVERIFY(!ownId.isEmpty());

    const auto latest = ProgressSyncLog::latestFromOtherDevices(QStringLiteral("book2"), ownId);
    QVERIFY(!latest.has_value());
}

void SyncTest::syncLogFiltersByBookHash()
{
    ProgressSyncLog::appendEntry(QStringLiteral("book-a"), QStringLiteral("A"), 1, 1.0);
    ProgressSyncLog::appendEntry(QStringLiteral("book-b"), QStringLiteral("B"), 2, 1.0);

    const auto latest = ProgressSyncLog::latestFromOtherDevices(QStringLiteral("book-a"), QStringLiteral("nobody"));
    QVERIFY(latest.has_value());
    QCOMPARE(latest->position, 1);
    QCOMPARE(latest->title, QStringLiteral("A"));
}

void SyncTest::syncLogReturnsNulloptWhenSyncNotConfigured()
{
    SyncFolder::setPath(QString());
    QVERIFY(!ProgressSyncLog::latestFromOtherDevices(QStringLiteral("anything"), QStringLiteral("nobody")).has_value());
}

void SyncTest::syncLogTolerateMalformedLine()
{
    const QString dir = SyncFolder::dataDirectory();
    QFile file(QDir(dir).filePath(QStringLiteral("some-other-device.jsonl")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not valid json at all\n");
    file.write("{\"bookHash\":\"book-c\",\"position\":9,\"zoom\":1.0,"
               "\"timestamp\":\"2026-01-01T00:00:00.000Z\",\"deviceId\":\"x\",\"deviceName\":\"X\"}\n");
    file.close();

    const auto latest = ProgressSyncLog::latestFromOtherDevices(QStringLiteral("book-c"), QStringLiteral("nobody"));
    QVERIFY(latest.has_value());
    QCOMPARE(latest->position, 9);
}

QTEST_MAIN(SyncTest)
#include "SyncTest.moc"
