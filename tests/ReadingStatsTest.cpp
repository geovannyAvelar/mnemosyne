#include "app/ReadingSessionTracker.h"
#include "app/ReadingStatsStore.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTest>
#include <QThread>

// ReadingSessionTracker records real wall-clock elapsed time (QDateTime::
// currentDateTime() at start()/stop()), so these tests sleep briefly around
// start()/stop() calls rather than mocking the clock -- short (tens of ms)
// sleeps, checked with a >= 0 or >= 1 lower bound rather than an exact
// value, since real elapsed time is never going to be deterministic to the
// second.
class ReadingStatsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void recordSessionAccumulatesForSameBookAndDate();
    void recordSessionKeepsBooksAndDatesSeparate();
    void recordSessionIgnoresZeroActivity();
    void totalsAggregateAcrossBooks();
    void currentStreakCountsConsecutiveDaysEndingToday();
    void currentStreakStillCountsIfTodayHasNoActivityYet();
    void currentStreakIsZeroAfterAGapDay();
    void longestStreakFindsTheBestRunEvenIfNotCurrent();
    void averagePagesPerDayCountsInactiveDaysAsZero();

    void sessionTrackerRecordsElapsedTimeAndPageDelta();
    void sessionTrackerClampsBackwardNavigationToZeroPages();
    void sessionTrackerStopWithNoActiveSessionIsNoOp();
    void sessionTrackerSecondStopIsNoOp();
};

void ReadingStatsTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void ReadingStatsTest::init()
{
    QSettings().clear();
}

void ReadingStatsTest::cleanupTestCase()
{
    QSettings().clear();
}

void ReadingStatsTest::recordSessionAccumulatesForSameBookAndDate()
{
    const QDate today = QDate::currentDate();
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today, 60, 5);
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today, 30, 2);

    QCOMPARE(ReadingStatsStore::totalSecondsOn(today), 90);
    QCOMPARE(ReadingStatsStore::totalPagesOn(today), 7);
}

void ReadingStatsTest::recordSessionKeepsBooksAndDatesSeparate()
{
    const QDate today = QDate::currentDate();
    const QDate yesterday = today.addDays(-1);
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today, 60, 5);
    ReadingStatsStore::recordSession(QStringLiteral("book-2"), yesterday, 100, 10);

    QCOMPARE(ReadingStatsStore::totalSecondsForBook(QStringLiteral("book-1")), 60);
    QCOMPARE(ReadingStatsStore::totalSecondsForBook(QStringLiteral("book-2")), 100);
    QCOMPARE(ReadingStatsStore::totalSecondsOn(today), 60);
    QCOMPARE(ReadingStatsStore::totalSecondsOn(yesterday), 100);
}

void ReadingStatsTest::recordSessionIgnoresZeroActivity()
{
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), QDate::currentDate(), 0, 0);
    QVERIFY(ReadingStatsStore::activeDates().isEmpty());
}

void ReadingStatsTest::totalsAggregateAcrossBooks()
{
    const QDate today = QDate::currentDate();
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today, 60, 5);
    ReadingStatsStore::recordSession(QStringLiteral("book-2"), today, 40, 3);

    QCOMPARE(ReadingStatsStore::totalSecondsAllTime(), 100);
    QCOMPARE(ReadingStatsStore::totalPagesAllTime(), 8);
    QCOMPARE(ReadingStatsStore::totalSecondsOn(today), 100);
    QCOMPARE(ReadingStatsStore::totalPagesOn(today), 8);
}

void ReadingStatsTest::currentStreakCountsConsecutiveDaysEndingToday()
{
    const QDate today = QDate::currentDate();
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today, 60, 1);
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today.addDays(-1), 60, 1);
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today.addDays(-2), 60, 1);

    QCOMPARE(ReadingStatsStore::currentStreakDays(), 3);
}

void ReadingStatsTest::currentStreakStillCountsIfTodayHasNoActivityYet()
{
    const QDate today = QDate::currentDate();
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today.addDays(-1), 60, 1);
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today.addDays(-2), 60, 1);

    // Today itself has nothing recorded yet -- the streak shouldn't look
    // broken first thing in the morning before the reader has opened
    // anything (see currentStreakDays()'s own doc comment).
    QCOMPARE(ReadingStatsStore::currentStreakDays(), 2);
}

void ReadingStatsTest::currentStreakIsZeroAfterAGapDay()
{
    const QDate today = QDate::currentDate();
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today.addDays(-3), 60, 1);
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today.addDays(-4), 60, 1);

    // Neither today nor yesterday has activity -- the streak is broken,
    // regardless of older activity further back.
    QCOMPARE(ReadingStatsStore::currentStreakDays(), 0);
}

void ReadingStatsTest::longestStreakFindsTheBestRunEvenIfNotCurrent()
{
    const QDate today = QDate::currentDate();
    // A 3-day run long ago...
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today.addDays(-20), 60, 1);
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today.addDays(-19), 60, 1);
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today.addDays(-18), 60, 1);
    // ...and a shorter, but current, 1-day run.
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today, 60, 1);

    QCOMPARE(ReadingStatsStore::currentStreakDays(), 1);
    QCOMPARE(ReadingStatsStore::longestStreakDays(), 3);
}

void ReadingStatsTest::averagePagesPerDayCountsInactiveDaysAsZero()
{
    const QDate today = QDate::currentDate();
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today, 60, 10);
    // 9 other days in a 10-day window have nothing recorded.

    QCOMPARE(ReadingStatsStore::averagePagesPerDay(10), 1.0);
}

void ReadingStatsTest::sessionTrackerRecordsElapsedTimeAndPageDelta()
{
    ReadingSessionTracker tracker;
    QVERIFY(!tracker.isActive());

    tracker.start(QStringLiteral("book-1"), 10);
    QVERIFY(tracker.isActive());
    QThread::msleep(1100); // so secsTo() reports at least 1 whole second

    tracker.stop(15);
    QVERIFY(!tracker.isActive());

    QCOMPARE(ReadingStatsStore::totalPagesForBook(QStringLiteral("book-1")), 5);
    QVERIFY(ReadingStatsStore::totalSecondsForBook(QStringLiteral("book-1")) >= 1);
}

void ReadingStatsTest::sessionTrackerClampsBackwardNavigationToZeroPages()
{
    ReadingSessionTracker tracker;
    tracker.start(QStringLiteral("book-1"), 50);
    QThread::msleep(1100);
    tracker.stop(20); // moved backward -- shouldn't count as negative pages

    QCOMPARE(ReadingStatsStore::totalPagesForBook(QStringLiteral("book-1")), 0);
    // Time spent still counts even though the reader navigated backward.
    QVERIFY(ReadingStatsStore::totalSecondsForBook(QStringLiteral("book-1")) >= 1);
}

void ReadingStatsTest::sessionTrackerStopWithNoActiveSessionIsNoOp()
{
    ReadingSessionTracker tracker;
    tracker.stop(10); // never started -- must not crash or record anything
    QVERIFY(ReadingStatsStore::activeDates().isEmpty());
}

void ReadingStatsTest::sessionTrackerSecondStopIsNoOp()
{
    ReadingSessionTracker tracker;
    tracker.start(QStringLiteral("book-1"), 0);
    QThread::msleep(1100);
    tracker.stop(5);
    const int secondsAfterFirstStop = ReadingStatsStore::totalSecondsForBook(QStringLiteral("book-1"));

    tracker.stop(100); // no session open anymore -- must not record again
    QCOMPARE(ReadingStatsStore::totalSecondsForBook(QStringLiteral("book-1")), secondsAfterFirstStop);
    QCOMPARE(ReadingStatsStore::totalPagesForBook(QStringLiteral("book-1")), 5);
}

QTEST_MAIN(ReadingStatsTest)
#include "ReadingStatsTest.moc"
