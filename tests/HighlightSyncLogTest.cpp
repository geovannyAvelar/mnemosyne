#include "app/HighlightSyncLog.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

// Exercises HighlightSyncLog::appendEntryToDirectory/entriesFromDirectory
// directly against an arbitrary directory — the same logic SyncFolder-based
// sync uses, but also what GoogleDriveHighlightSync's local staging
// directory relies on. Mirrors ProgressSyncLogDirectoryTest.cpp; the
// per-id/last-write-wins merge itself is covered by HighlightSyncTest.cpp,
// not here — this only checks the log reads back what was written.
class HighlightSyncLogTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void readsBackUpsertFields();
    void readsBackDeleteAsTombstone();
    void excludesGivenDeviceId();
    void filtersByBookHash();
    void returnsEmptyForEmptyDirOrHashOrId();
    void tolerateMalformedLine();

private:
    std::unique_ptr<QTemporaryDir> m_dir;
};

void HighlightSyncLogTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void HighlightSyncLogTest::readsBackUpsertFields()
{
    Highlight h;
    h.id = QStringLiteral("hl-1");
    h.targetIndex = 3;
    h.pageRect = QRectF(1, 2, 30, 40);
    h.text = QStringLiteral("some passage");
    h.note = QStringLiteral("a comment");
    h.color = QColor(10, 20, 30, 40);
    h.createdAt = QDateTime::currentDateTimeUtc();
    h.updatedAt = h.createdAt;

    HighlightSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-a"), QStringLiteral("Device A"),
                                              QStringLiteral("book1"), h.id, HighlightSyncLog::Op::Upsert, h);

    const auto entries =
        HighlightSyncLog::entriesFromDirectory(m_dir->path(), QStringLiteral("book1"), QStringLiteral("nobody"));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().id, QStringLiteral("hl-1"));
    QCOMPARE(entries.first().op, HighlightSyncLog::Op::Upsert);
    QCOMPARE(entries.first().deviceId, QStringLiteral("device-a"));
    QCOMPARE(entries.first().highlight.targetIndex, 3);
    QCOMPARE(entries.first().highlight.pageRect, QRectF(1, 2, 30, 40));
    QCOMPARE(entries.first().highlight.text, QStringLiteral("some passage"));
    QCOMPARE(entries.first().highlight.note, QStringLiteral("a comment"));
    QCOMPARE(entries.first().highlight.color, QColor(10, 20, 30, 40));
}

void HighlightSyncLogTest::readsBackDeleteAsTombstone()
{
    HighlightSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-a"), QStringLiteral("Device A"),
                                              QStringLiteral("book2"), QStringLiteral("hl-2"),
                                              HighlightSyncLog::Op::Delete, {});

    const auto entries =
        HighlightSyncLog::entriesFromDirectory(m_dir->path(), QStringLiteral("book2"), QStringLiteral("nobody"));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().id, QStringLiteral("hl-2"));
    QCOMPARE(entries.first().op, HighlightSyncLog::Op::Delete);
}

void HighlightSyncLogTest::excludesGivenDeviceId()
{
    HighlightSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-a"), QStringLiteral("Device A"),
                                              QStringLiteral("book3"), QStringLiteral("hl-3"),
                                              HighlightSyncLog::Op::Upsert, Highlight{});

    const auto entries =
        HighlightSyncLog::entriesFromDirectory(m_dir->path(), QStringLiteral("book3"), QStringLiteral("device-a"));
    QVERIFY(entries.isEmpty());
}

void HighlightSyncLogTest::filtersByBookHash()
{
    HighlightSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-a"), QStringLiteral("A"),
                                              QStringLiteral("book-a"), QStringLiteral("hl-a"),
                                              HighlightSyncLog::Op::Upsert, Highlight{});
    HighlightSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-a"), QStringLiteral("A"),
                                              QStringLiteral("book-b"), QStringLiteral("hl-b"),
                                              HighlightSyncLog::Op::Upsert, Highlight{});

    const auto entries =
        HighlightSyncLog::entriesFromDirectory(m_dir->path(), QStringLiteral("book-a"), QStringLiteral("nobody"));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().id, QStringLiteral("hl-a"));
}

void HighlightSyncLogTest::returnsEmptyForEmptyDirOrHashOrId()
{
    QVERIFY(HighlightSyncLog::entriesFromDirectory(QString(), QStringLiteral("book"), QStringLiteral("nobody"))
                .isEmpty());
    QVERIFY(HighlightSyncLog::entriesFromDirectory(m_dir->path(), QString(), QStringLiteral("nobody")).isEmpty());

    HighlightSyncLog::appendEntryToDirectory(m_dir->path(), QStringLiteral("device-a"), QStringLiteral("A"),
                                              QStringLiteral("book"), QString(), HighlightSyncLog::Op::Upsert,
                                              Highlight{});
    QVERIFY(HighlightSyncLog::entriesFromDirectory(m_dir->path(), QStringLiteral("book"), QStringLiteral("nobody"))
                .isEmpty());
}

void HighlightSyncLogTest::tolerateMalformedLine()
{
    QFile file(QDir(m_dir->path()).filePath(QStringLiteral("some-other-device-highlights.jsonl")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not valid json at all\n");
    file.write("{\"id\":\"hl-x\",\"bookHash\":\"book-c\",\"op\":\"upsert\",\"targetIndex\":1,"
               "\"timestamp\":\"2026-01-01T00:00:00.000Z\",\"deviceId\":\"x\",\"deviceName\":\"X\"}\n");
    file.close();

    const auto entries =
        HighlightSyncLog::entriesFromDirectory(m_dir->path(), QStringLiteral("book-c"), QStringLiteral("nobody"));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().id, QStringLiteral("hl-x"));
}

QTEST_MAIN(HighlightSyncLogTest)
#include "HighlightSyncLogTest.moc"
