#include "app/HighlightStore.h"
#include "app/HighlightSync.h"
#include "app/HighlightSyncLog.h"
#include "app/SyncFolder.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

// Covers HighlightSync::pull()'s per-highlight last-write-wins merge — the
// part of cross-device highlight sync that's more than "latest timestamp
// wins for the whole book" (see ProgressSyncLog/SyncTest.cpp for that
// simpler case). Remote entries are injected by writing directly into
// SyncFolder::dataDirectory() under a fake device id, exactly as another
// device's own HighlightSyncLog::appendUpsert/appendDelete would.
//
// Local state is seeded via HighlightStore::replaceMerged() rather than
// addHighlight(), so each test can pin a highlight's id and updatedAt
// precisely instead of racing the wall clock against the remote entry's
// timestamp (always "now" at the moment it's appended).
class HighlightSyncTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void remoteOnlyHighlightAppears();
    void localOnlyHighlightStaysUntouched();
    void sameIdEditedBothSidesNewerWins();
    void remoteDeleteNewerThanLocalRemovesIt();
    void remoteDeleteOlderThanLocalIsIgnored();
    void lamportClockWinsOverMisleadingTimestamp();
    void receivedHighlightCarriesRemoteLamportClock();

private:
    std::unique_ptr<QTemporaryDir> m_syncDir;

    void appendRemote(const QString &bookHash, const QString &id, HighlightSyncLog::Op op, const Highlight &h = {});
    bool pullSync(const QString &bookHash);
};

void HighlightSyncTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void HighlightSyncTest::init()
{
    QSettings().clear();
    m_syncDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_syncDir->isValid());
    SyncFolder::setPath(m_syncDir->path());
}

void HighlightSyncTest::appendRemote(const QString &bookHash, const QString &id, HighlightSyncLog::Op op,
                                      const Highlight &h)
{
    HighlightSyncLog::appendEntryToDirectory(SyncFolder::dataDirectory(), QStringLiteral("device-b"),
                                              QStringLiteral("Device B"), bookHash, id, op, h);
}

bool HighlightSyncTest::pullSync(const QString &bookHash)
{
    bool changed = false;
    bool called = false;
    HighlightSync::pull(bookHash, [&](bool c) {
        called = true;
        changed = changed || c;
    });
    // Both backends (local-folder, and Drive when signed out) resolve
    // synchronously in this build/test setup, so the callback has already
    // fired by the time pull() returns.
    return called && changed;
}

void HighlightSyncTest::remoteOnlyHighlightAppears()
{
    const QString bookHash = QStringLiteral("book-remote-only");

    Highlight remote;
    remote.id = QStringLiteral("hl-1");
    remote.targetIndex = 2;
    remote.text = QStringLiteral("remote passage");
    remote.note = QStringLiteral("remote note");
    remote.color = QColor(1, 2, 3);
    remote.createdAt = QDateTime::currentDateTimeUtc();
    remote.updatedAt = remote.createdAt;
    appendRemote(bookHash, remote.id, HighlightSyncLog::Op::Upsert, remote);

    QVERIFY(pullSync(bookHash));

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(bookHash);
    QCOMPARE(highlights.size(), 1);
    QCOMPARE(highlights.first().id, QStringLiteral("hl-1"));
    QCOMPARE(highlights.first().note, QStringLiteral("remote note"));
    QCOMPARE(highlights.first().text, QStringLiteral("remote passage"));
}

void HighlightSyncTest::localOnlyHighlightStaysUntouched()
{
    const QString bookHash = QStringLiteral("book-local-only");
    HighlightStore::addHighlight(bookHash, Highlight{1, QRectF(0, 0, 10, 10), QStringLiteral("local only"),
                                                       QDateTime::currentDateTime()});

    pullSync(bookHash); // no remote entries for this book at all

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(bookHash);
    QCOMPARE(highlights.size(), 1);
    QCOMPARE(highlights.first().text, QStringLiteral("local only"));
}

void HighlightSyncTest::sameIdEditedBothSidesNewerWins()
{
    const QString bookHash = QStringLiteral("book-conflict-remote-wins");

    Highlight local;
    local.id = QStringLiteral("hl-conflict");
    local.targetIndex = 0;
    local.note = QStringLiteral("local note");
    local.color = QColor(255, 0, 0);
    local.createdAt = QDateTime::currentDateTimeUtc();
    local.updatedAt = local.createdAt.addSecs(-60); // clearly older than the remote entry appended below
    HighlightStore::replaceMerged(bookHash, {local});

    Highlight remote = local;
    remote.note = QStringLiteral("remote note");
    remote.color = QColor(0, 255, 0);
    appendRemote(bookHash, local.id, HighlightSyncLog::Op::Upsert, remote);

    QVERIFY(pullSync(bookHash));

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(bookHash);
    QCOMPARE(highlights.size(), 1);
    QCOMPARE(highlights.first().note, QStringLiteral("remote note"));
    QCOMPARE(highlights.first().color, QColor(0, 255, 0));
}

void HighlightSyncTest::remoteDeleteNewerThanLocalRemovesIt()
{
    const QString bookHash = QStringLiteral("book-remote-delete-wins");

    Highlight local;
    local.id = QStringLiteral("hl-to-delete");
    local.createdAt = QDateTime::currentDateTimeUtc();
    local.updatedAt = local.createdAt.addSecs(-60); // older than the delete appended below
    HighlightStore::replaceMerged(bookHash, {local});

    appendRemote(bookHash, local.id, HighlightSyncLog::Op::Delete);

    QVERIFY(pullSync(bookHash));

    QVERIFY(HighlightStore::highlightsFor(bookHash).isEmpty());
}

void HighlightSyncTest::remoteDeleteOlderThanLocalIsIgnored()
{
    const QString bookHash = QStringLiteral("book-local-edit-wins");

    Highlight local;
    local.id = QStringLiteral("hl-survives");
    local.note = QStringLiteral("kept locally");
    local.createdAt = QDateTime::currentDateTimeUtc();
    local.updatedAt = local.createdAt.addSecs(3600); // "in the future" relative to the delete's real-time timestamp
    HighlightStore::replaceMerged(bookHash, {local});

    appendRemote(bookHash, local.id, HighlightSyncLog::Op::Delete);

    pullSync(bookHash); // may report no change, since the delete is stale and gets ignored

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(bookHash);
    QCOMPARE(highlights.size(), 1);
    QCOMPARE(highlights.first().id, QStringLiteral("hl-survives"));
    QCOMPARE(highlights.first().note, QStringLiteral("kept locally"));
}

void HighlightSyncTest::lamportClockWinsOverMisleadingTimestamp()
{
    // Mirrors sameIdEditedBothSidesNewerWins(), but with the wall-clock
    // relationship deliberately reversed (local's updatedAt is pushed into
    // the future, so it *looks* newer than the remote entry's real-time
    // wire timestamp) while giving both sides an explicit, correctly-
    // ordered Lamport value. If Lamport clocks -- not timestamps -- are
    // deciding the merge, the remote edit still wins despite looking
    // "older" by wall clock; this is exactly the clock-skew scenario the
    // whole change exists to fix.
    const QString bookHash = QStringLiteral("book-conflict-lamport-wins");

    Highlight local;
    local.id = QStringLiteral("hl-conflict-lamport");
    local.note = QStringLiteral("local note");
    local.createdAt = QDateTime::currentDateTimeUtc();
    local.updatedAt = local.createdAt.addSecs(3600); // wall clock looks newer than the remote entry below
    local.lamportClock = 5;
    HighlightStore::replaceMerged(bookHash, {local});

    Highlight remote = local;
    remote.note = QStringLiteral("remote note");
    remote.lamportClock = 6; // genuinely happened after, in Lamport terms
    appendRemote(bookHash, local.id, HighlightSyncLog::Op::Upsert, remote);

    QVERIFY(pullSync(bookHash));

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(bookHash);
    QCOMPARE(highlights.size(), 1);
    QCOMPARE(highlights.first().note, QStringLiteral("remote note"));
    QCOMPARE(highlights.first().lamportClock, quint64(6));
}

void HighlightSyncTest::receivedHighlightCarriesRemoteLamportClock()
{
    // Regression test for a bug caught during design review: a receiving
    // device must persist the sender's Lamport value into its own local
    // copy, not just use it transiently during this merge -- otherwise
    // every highlight a device only ever *receives* (never edits itself)
    // would carry lamportClock == 0 forever, forcing every future
    // comparison for it back onto wall-clock time.
    const QString bookHash = QStringLiteral("book-remote-lamport-propagates");

    Highlight remote;
    remote.id = QStringLiteral("hl-propagate");
    remote.targetIndex = 0;
    remote.text = QStringLiteral("remote passage");
    remote.createdAt = QDateTime::currentDateTimeUtc();
    remote.updatedAt = remote.createdAt;
    remote.lamportClock = 42; // simulates a value the sending device already ticked
    appendRemote(bookHash, remote.id, HighlightSyncLog::Op::Upsert, remote);

    QVERIFY(pullSync(bookHash));

    const QVector<Highlight> highlights = HighlightStore::highlightsFor(bookHash);
    QCOMPARE(highlights.size(), 1);
    QCOMPARE(highlights.first().lamportClock, quint64(42));
}

QTEST_MAIN(HighlightSyncTest)
#include "HighlightSyncTest.moc"
