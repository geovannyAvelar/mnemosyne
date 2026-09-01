#include "app/SingleInstanceGuard.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

class SingleInstanceGuardTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void firstGuardBecomesPrimary();
    void secondGuardForwardsFilesToPrimaryAndDoesNotBecomePrimary();
    void secondGuardWithNoFilesStillNotifiesPrimary();
};

void SingleInstanceGuardTest::initTestCase()
{
    // A distinct application name keeps this test's local socket from
    // colliding with (or stealing file-open requests from) an actual
    // Mnemosyne instance running on the same machine -- see serverName() in
    // SingleInstanceGuard.cpp.
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneSingleInstanceGuardTest"));
}

void SingleInstanceGuardTest::firstGuardBecomesPrimary()
{
    SingleInstanceGuard primary;
    QVERIFY(primary.tryBecomePrimary(QStringList()));
}

void SingleInstanceGuardTest::secondGuardForwardsFilesToPrimaryAndDoesNotBecomePrimary()
{
    SingleInstanceGuard primary;
    QVERIFY(primary.tryBecomePrimary(QStringList()));

    QSignalSpy filesReceivedSpy(&primary, &SingleInstanceGuard::filesReceived);

    SingleInstanceGuard secondary;
    const QStringList files = {"/tmp/one.pdf", "/tmp/two.epub"};
    QVERIFY(!secondary.tryBecomePrimary(files));

    QVERIFY(filesReceivedSpy.wait(1000));
    QCOMPARE(filesReceivedSpy.count(), 1);
    QCOMPARE(filesReceivedSpy.at(0).at(0).toStringList(), files);
}

void SingleInstanceGuardTest::secondGuardWithNoFilesStillNotifiesPrimary()
{
    SingleInstanceGuard primary;
    QVERIFY(primary.tryBecomePrimary(QStringList()));

    QSignalSpy filesReceivedSpy(&primary, &SingleInstanceGuard::filesReceived);

    SingleInstanceGuard secondary;
    QVERIFY(!secondary.tryBecomePrimary(QStringList()));

    QVERIFY(filesReceivedSpy.wait(1000));
    QCOMPARE(filesReceivedSpy.count(), 1);
    QVERIFY(filesReceivedSpy.at(0).at(0).toStringList().isEmpty());
}

QTEST_MAIN(SingleInstanceGuardTest)
#include "SingleInstanceGuardTest.moc"
