#include "quick/ReadingStatsModel.h"

#include "app/ReadingStatsStore.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTest>

// ReadingStatsModel is a thin Q_INVOKABLE pass-through over
// app/ReadingStatsStore.h (already covered in detail by ReadingStatsTest.cpp)
// -- this just confirms each method actually forwards to the right
// ReadingStatsStore call with the right arguments (e.g. "today", not some
// other date).
class ReadingStatsModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void todayFiguresReflectStore();
    void allTimeFiguresReflectStore();
    void streakAndPaceFiguresReflectStore();
};

void ReadingStatsModelTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void ReadingStatsModelTest::init()
{
    QSettings().clear();
}

void ReadingStatsModelTest::cleanupTestCase()
{
    QSettings().clear();
}

void ReadingStatsModelTest::todayFiguresReflectStore()
{
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), QDate::currentDate(), 90, 7);
    // A different day shouldn't leak into "today"'s figures.
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), QDate::currentDate().addDays(-1), 500, 50);

    ReadingStatsModel model;
    QCOMPARE(model.todaySeconds(), 90);
    QCOMPARE(model.todayPages(), 7);
}

void ReadingStatsModelTest::allTimeFiguresReflectStore()
{
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), QDate::currentDate(), 90, 7);
    ReadingStatsStore::recordSession(QStringLiteral("book-2"), QDate::currentDate().addDays(-5), 30, 3);

    ReadingStatsModel model;
    QCOMPARE(model.allTimeSeconds(), 120);
    QCOMPARE(model.allTimePages(), 10);
}

void ReadingStatsModelTest::streakAndPaceFiguresReflectStore()
{
    const QDate today = QDate::currentDate();
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today, 60, 10);
    ReadingStatsStore::recordSession(QStringLiteral("book-1"), today.addDays(-1), 60, 10);

    ReadingStatsModel model;
    QCOMPARE(model.currentStreakDays(), 2);
    QCOMPARE(model.longestStreakDays(), 2);
    // 20 pages spread over the last 30 days.
    QCOMPARE(model.averagePagesPerDay(), ReadingStatsStore::averagePagesPerDay(30));
}

QTEST_MAIN(ReadingStatsModelTest)
#include "ReadingStatsModelTest.moc"
