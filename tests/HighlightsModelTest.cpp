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

    void highlightsForTargetFiltersByPageAndIncludesNote();
    void highlightsForTargetRowMatchesRemoveHighlightAt();

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

void HighlightsModelTest::highlightsForTargetFiltersByPageAndIncludesNote()
{
    // See qml/components/PdfContinuousPageItem.qml's tap-to-view-note hit
    // test (NotePopup) -- it filters this Q_INVOKABLE's own return value by
    // the "note" field, so that field has to actually be populated, not
    // just "row"/"pageRect"/"text" (the fields SelectionToolbar's highlight
    // rendering already exercised before this was added).
    const QString bookHash = QStringLiteral("book-mobile-notes");
    HighlightStore::addHighlight(
        bookHash, Highlight{0, QRectF(1, 2, 30, 40), QStringLiteral("page 0, no note"), QDateTime::currentDateTime()});
    HighlightStore::addHighlight(
        bookHash, Highlight{0, QRectF(5, 6, 30, 40), QStringLiteral("page 0, with note"), QDateTime::currentDateTime()});
    HighlightStore::addHighlight(
        bookHash, Highlight{1, QRectF(0, 0, 30, 40), QStringLiteral("page 1"), QDateTime::currentDateTime()});
    HighlightStore::setNote(bookHash, 1, QStringLiteral("a note on the second page-0 highlight"));

    HighlightsModel model;
    model.setBookHash(bookHash);

    const QVariantList page0 = model.highlightsForTarget(0);
    QCOMPARE(page0.size(), 2);

    bool sawNoted = false;
    for (const QVariant &entry : page0) {
        const QVariantMap map = entry.toMap();
        if (map["text"].toString() == QStringLiteral("page 0, with note")) {
            QCOMPARE(map["note"].toString(), QStringLiteral("a note on the second page-0 highlight"));
            sawNoted = true;
        } else {
            QVERIFY(map["note"].toString().isEmpty());
        }
    }
    QVERIFY(sawNoted);

    QCOMPARE(model.highlightsForTarget(1).size(), 1);
    QVERIFY(model.highlightsForTarget(2).isEmpty()); // no highlights on this page
}

void HighlightsModelTest::highlightsForTargetRowMatchesRemoveHighlightAt()
{
    const QString bookHash = QStringLiteral("book-mobile-remove");
    HighlightStore::addHighlight(bookHash,
                                  Highlight{0, QRectF(), QStringLiteral("keep me"), QDateTime::currentDateTime()});
    HighlightStore::addHighlight(bookHash,
                                  Highlight{0, QRectF(), QStringLiteral("remove me"), QDateTime::currentDateTime()});

    HighlightsModel model;
    model.setBookHash(bookHash);

    const QVariantList page0 = model.highlightsForTarget(0);
    QCOMPARE(page0.size(), 2);
    const QVariantMap toRemove = page0[1].toMap();
    QCOMPARE(toRemove["text"].toString(), QStringLiteral("remove me"));

    model.removeHighlightAt(toRemove["row"].toInt());

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), HighlightsModel::TextRole).toString(), QStringLiteral("keep me"));
}

QTEST_MAIN(HighlightsModelTest)
#include "HighlightsModelTest.moc"
