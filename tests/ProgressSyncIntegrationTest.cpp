#include "app/DeviceIdentity.h"
#include "app/FileIdentity.h"
#include "app/ProgressSyncLog.h"
#include "app/SyncFolder.h"
#include "epub/EpubDocument.h"
#include "ui/EpubView.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

// Exercises the glue inside EpubView::restoreProgressAndCheckSync() — the
// one piece of the sync feature not already covered by SyncTest's
// lower-level WAL tests: does a real view actually notice another device's
// entry and surface the prompt, at the right times and not others? The
// EPUB fixture (2 chapters) is used rather than the 1-page PDF fixture,
// since a genuine "different position" case needs more than one position to
// choose between. The full click-to-jump button interaction is verified
// live in the running app instead, same as other UI-interaction-dependent
// behavior in this codebase.
class ProgressSyncIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void showsPromptWhenAnotherDeviceIsOnADifferentChapter();
    void doesNotPromptWhenNoRemoteEntryExists();
    void doesNotPromptWhenRemoteMatchesDefaultPosition();
    void doesNotPromptForEntriesWrittenByThisSameDevice();
    void jumpingNavigatesToTheRemoteChapter();

private:
    static QString fixturePath(const QString &name);
    static std::unique_ptr<EpubView> makeView();
    // Writes a JSONL entry as if some other, real device had written it —
    // deliberately bypassing ProgressSyncLog::appendEntry()/DeviceIdentity,
    // since a genuine other device would never touch this process's own
    // device-id settings.
    void writeOtherDeviceEntry(const QString &bookHash, int position);

    std::unique_ptr<QTemporaryDir> m_syncDir;
};

QString ProgressSyncIntegrationTest::fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}

std::unique_ptr<EpubView> ProgressSyncIntegrationTest::makeView()
{
    QString error;
    auto doc = EpubDocument::load(fixturePath("test.epub"), &error);
    Q_ASSERT_X(doc, "makeView", qPrintable(error));
    return std::make_unique<EpubView>(std::move(doc), fixturePath("test.epub"));
}

void ProgressSyncIntegrationTest::writeOtherDeviceEntry(const QString &bookHash, int position)
{
    const QString dir = SyncFolder::dataDirectory();
    QFile file(QDir(dir).filePath(QStringLiteral("some-other-real-device.jsonl")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QString line = QStringLiteral(
                              "{\"bookHash\":\"%1\",\"title\":\"Test Book\",\"position\":%2,\"zoom\":0.0,"
                              "\"timestamp\":\"2026-01-01T00:00:00.000Z\",\"deviceId\":\"some-other-real-device\","
                              "\"deviceName\":\"Other Device\"}\n")
                              .arg(bookHash)
                              .arg(position);
    file.write(line.toUtf8());
}

void ProgressSyncIntegrationTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void ProgressSyncIntegrationTest::init()
{
    QSettings().clear();
    m_syncDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_syncDir->isValid());
    SyncFolder::setPath(m_syncDir->path());
}

void ProgressSyncIntegrationTest::cleanupTestCase()
{
    QSettings().clear();
}

void ProgressSyncIntegrationTest::showsPromptWhenAnotherDeviceIsOnADifferentChapter()
{
    const QString bookHash = FileIdentity::contentHash(fixturePath("test.epub"));
    QVERIFY(!bookHash.isEmpty());

    writeOtherDeviceEntry(bookHash, /*position=*/1); // chapter 2, different from the default chapter 1

    auto view = makeView();
    QVERIFY(view->hasPendingSyncPrompt());
}

void ProgressSyncIntegrationTest::doesNotPromptWhenNoRemoteEntryExists()
{
    auto view = makeView();
    QVERIFY(!view->hasPendingSyncPrompt());
}

void ProgressSyncIntegrationTest::doesNotPromptWhenRemoteMatchesDefaultPosition()
{
    const QString bookHash = FileIdentity::contentHash(fixturePath("test.epub"));
    writeOtherDeviceEntry(bookHash, /*position=*/0); // chapter 1, same as the default

    auto view = makeView();
    QVERIFY(!view->hasPendingSyncPrompt());
}

void ProgressSyncIntegrationTest::doesNotPromptForEntriesWrittenByThisSameDevice()
{
    const QString bookHash = FileIdentity::contentHash(fixturePath("test.epub"));
    QVERIFY(!bookHash.isEmpty());

    // appendEntry() stamps whatever DeviceIdentity::id() returns for this
    // process — exactly what the view under test will also use to identify
    // itself and exclude its own writes.
    ProgressSyncLog::appendEntry(bookHash, QStringLiteral("Test Book"), /*position=*/1, 0.0);

    auto view = makeView();
    QVERIFY(!view->hasPendingSyncPrompt());
}

void ProgressSyncIntegrationTest::jumpingNavigatesToTheRemoteChapter()
{
    const QString bookHash = FileIdentity::contentHash(fixturePath("test.epub"));
    writeOtherDeviceEntry(bookHash, /*position=*/1);

    auto view = makeView();
    QVERIFY(view->hasPendingSyncPrompt());
    QCOMPARE(view->currentPosition(), 0); // opened at the default chapter, not yet jumped

    // goToChapter() is exactly what the prompt bar's "Jump" button is wired
    // to call with the remote entry's position (see EpubView::restoreProgressAndCheckSync).
    view->goToChapter(1);
    QCOMPARE(view->currentPosition(), 1);
}

QTEST_MAIN(ProgressSyncIntegrationTest)
#include "ProgressSyncIntegrationTest.moc"
