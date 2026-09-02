#include "quick/HighlightsModel.h"

#include "app/HighlightStore.h"
#include "app/HighlightSyncLog.h"
#include "app/SyncFolder.h"

#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

// HighlightsModel is the one place mobile triggers cross-device highlight
// sync (see HighlightsModel.h's comment on why, unlike desktop, this isn't
// spread across PdfDocumentModel/EpubReaderModel): setBookHash() is the
// mobile "book just opened" signal, mirroring where each desktop view calls
// HighlightSync::pull(). The merge algorithm itself is already covered by
// HighlightSyncTest.cpp; this only checks that setBookHash() actually wires
// into it and refreshes the model (a QAbstractListModel reset) when it does.
class HighlightsModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void setBookHashPullsRemoteHighlightsAndResetsModel();
    void setBookHashWithNoRemoteDataLeavesModelAsIs();

private:
    std::unique_ptr<QTemporaryDir> m_syncDir;
};

void HighlightsModelTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void HighlightsModelTest::init()
{
    QSettings().clear();
    m_syncDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_syncDir->isValid());
    SyncFolder::setPath(m_syncDir->path());
}

void HighlightsModelTest::setBookHashPullsRemoteHighlightsAndResetsModel()
{
    const QString bookHash = QStringLiteral("book-mobile-remote");

    Highlight remote;
    remote.id = QStringLiteral("hl-mobile-1");
    remote.targetIndex = 0;
    remote.text = QStringLiteral("highlighted on another device");
    remote.createdAt = QDateTime::currentDateTimeUtc();
    remote.updatedAt = remote.createdAt;
    HighlightSyncLog::appendEntryToDirectory(SyncFolder::dataDirectory(), QStringLiteral("device-b"),
                                              QStringLiteral("Device B"), bookHash, remote.id,
                                              HighlightSyncLog::Op::Upsert, remote);

    HighlightsModel model;
    QCOMPARE(model.rowCount(), 0); // nothing local yet

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.setBookHash(bookHash);
    QVERIFY(resetSpy.count() >= 2); // once for the initial (empty) refresh, again once the pull merges

    QCOMPARE(model.rowCount(), 1);
    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, HighlightsModel::TextRole).toString(),
              QStringLiteral("highlighted on another device"));
}

void HighlightsModelTest::setBookHashWithNoRemoteDataLeavesModelAsIs()
{
    const QString bookHash = QStringLiteral("book-mobile-local-only");
    HighlightStore::addHighlight(bookHash, Highlight{0, QRectF(), QStringLiteral("local only"),
                                                       QDateTime::currentDateTime()});

    HighlightsModel model;
    model.setBookHash(bookHash);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), HighlightsModel::TextRole).toString(), QStringLiteral("local only"));
}

QTEST_MAIN(HighlightsModelTest)
#include "HighlightsModelTest.moc"
