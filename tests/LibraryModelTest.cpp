#include "quick/LibraryModel.h"

#include <QCoreApplication>
#include <QSettings>
#include <QTest>

// LibraryModel is a thin QAbstractListModel wrapper over app/RecentFiles.h,
// the same store AppPersistenceTest already exercises directly -- this
// covers the model-specific part instead: role data and refresh()/
// recordOpened()/removeEntry() keeping the model in sync with the store.
class LibraryModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void recordOpenedAddsARowWithCorrectRoleData();
    void titleFallsBackToFilePathWhenEmpty();
    void removeEntryDropsTheRow();
};

void LibraryModelTest::initTestCase()
{
    // Isolate from the real app's settings, same as AppPersistenceTest.
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void LibraryModelTest::init()
{
    QSettings().clear();
}

void LibraryModelTest::cleanupTestCase()
{
    QSettings().clear();
}

void LibraryModelTest::recordOpenedAddsARowWithCorrectRoleData()
{
    LibraryModel model;
    model.recordOpened(QStringLiteral("/tmp/a.pdf"), QStringLiteral("Book A"), QStringLiteral("pdf"));

    QCOMPARE(model.rowCount(), 1);
    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, LibraryModel::FilePathRole).toString(), QStringLiteral("/tmp/a.pdf"));
    QCOMPARE(model.data(index, LibraryModel::TitleRole).toString(), QStringLiteral("Book A"));
    QCOMPARE(model.data(index, LibraryModel::FormatRole).toString(), QStringLiteral("pdf"));
}

void LibraryModelTest::titleFallsBackToFilePathWhenEmpty()
{
    LibraryModel model;
    model.recordOpened(QStringLiteral("/tmp/untitled.epub"), QString(), QStringLiteral("epub"));

    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, LibraryModel::TitleRole).toString(), QStringLiteral("/tmp/untitled.epub"));
}

void LibraryModelTest::removeEntryDropsTheRow()
{
    LibraryModel model;
    model.recordOpened(QStringLiteral("/tmp/a.pdf"), QStringLiteral("A"), QStringLiteral("pdf"));
    model.recordOpened(QStringLiteral("/tmp/b.epub"), QStringLiteral("B"), QStringLiteral("epub"));
    QCOMPARE(model.rowCount(), 2);

    model.removeEntry(QStringLiteral("/tmp/b.epub"));

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::FilePathRole).toString(), QStringLiteral("/tmp/a.pdf"));
}

QTEST_MAIN(LibraryModelTest)
#include "LibraryModelTest.moc"
