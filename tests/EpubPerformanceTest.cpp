#include "epub/EpubDocument.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTest>

namespace {
QString fixturePath(const QString &name)
{
    return QStringLiteral(FIXTURES_DIR) + QLatin1Char('/') + name;
}

std::unique_ptr<EpubDocument> loadTestEpub()
{
    QString error;
    auto doc = EpubDocument::load(fixturePath("test.epub"), &error);
    Q_ASSERT_X(doc, "loadTestEpub", qPrintable(error));
    return doc;
}
}

class EpubPerformanceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void chapterHtmlCachingIsFast();
    void videoPowersCachingFast();
};

void EpubPerformanceTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void EpubPerformanceTest::chapterHtmlCachingIsFast()
{
    auto doc = loadTestEpub();
    if (doc->spineCount() == 0) {
        QSKIP("No chapters in test EPUB");
    }

    QElapsedTimer timer;

    timer.start();
    QString html1 = doc->chapterHtml(0);
    qint64 firstCall = timer.elapsed();

    timer.start();
    QString html2 = doc->chapterHtml(0);
    qint64 secondCall = timer.elapsed();

    QCOMPARE(html1, html2);

    qDebug() << "First chapterHtml() call:" << firstCall << "ms";
    qDebug() << "Cached chapterHtml() call:" << secondCall << "ms";

    QVERIFY2(secondCall < firstCall, "Cached call should be significantly faster than first call");
}

void EpubPerformanceTest::videoPowersCachingFast()
{
    auto doc = loadTestEpub();
    if (doc->spineCount() == 0) {
        QSKIP("No chapters in test EPUB");
    }

    QElapsedTimer timer;

    timer.start();
    QString html = doc->chapterHtml(0);
    qint64 htmlTime = timer.elapsed();

    timer.start();
    for (int i = 0; i < 100; ++i) {
        QVector<QString> paths = doc->chapterVideoPaths(0);
    }
    qint64 videosTime = timer.elapsed();

    qDebug() << "chapterHtml() first call:" << htmlTime << "ms";
    qDebug() << "100 chapterVideoPaths() calls on cached chapter:" << videosTime << "ms";

    QVERIFY2(videosTime <= 5, "Multiple video path calls should be instant from cache");
}

QTEST_MAIN(EpubPerformanceTest)
#include "EpubPerformanceTest.moc"
