#include "app/LamportClock.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTest>

class LamportClockTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void tickIsMonotonicAndPersists();
    void observeAdoptsHigherValue();
    void observeIgnoresLowerOrEqualValue();
};

void LamportClockTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void LamportClockTest::init()
{
    QSettings().clear();
}

void LamportClockTest::tickIsMonotonicAndPersists()
{
    QCOMPARE(LamportClock::tick(), quint64(1));
    QCOMPARE(LamportClock::tick(), quint64(2));
    QCOMPARE(LamportClock::tick(), quint64(3));

    // Persisted (QSettings), not just an in-process counter -- a fresh
    // QSettings read (as a new process would see) reflects the same value.
    QCOMPARE(QSettings().value(QStringLiteral("Device/lamportClock")).toULongLong(), quint64(3));
}

void LamportClockTest::observeAdoptsHigherValue()
{
    LamportClock::tick(); // local = 1
    LamportClock::observe(10);
    QCOMPARE(LamportClock::tick(), quint64(11)); // next tick exceeds the observed value
}

void LamportClockTest::observeIgnoresLowerOrEqualValue()
{
    for (int i = 0; i < 5; ++i) {
        LamportClock::tick(); // local = 5
    }
    LamportClock::observe(3); // lower than current -- no-op
    QCOMPARE(LamportClock::tick(), quint64(6));

    LamportClock::observe(6); // equal to current -- no-op
    QCOMPARE(LamportClock::tick(), quint64(7));
}

QTEST_MAIN(LamportClockTest)
#include "LamportClockTest.moc"
