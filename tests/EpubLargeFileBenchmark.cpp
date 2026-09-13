#include "epub/EpubDocument.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTest>

class EpubLargeFileBenchmark : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void benchmarkLargeEpubChapterAccess();
    void benchmarkMultipleChapterAccess();
    void benchmarkVideoPaths();

private:
    std::unique_ptr<EpubDocument> m_doc;
};

void EpubLargeFileBenchmark::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));

    QString error;
    m_doc = EpubDocument::load(QStringLiteral("/Users/geovannyavelar/Desktop/Practical Electronics for Inventors.epub"), &error);

    if (!m_doc) {
        QSKIP(qPrintable(QString("Could not load large EPUB: %1").arg(error)));
    }

    qDebug() << "Loaded EPUB:" << m_doc->title();
    qDebug() << "Chapters:" << m_doc->spineCount();
}

void EpubLargeFileBenchmark::benchmarkLargeEpubChapterAccess()
{
    if (!m_doc || m_doc->spineCount() == 0) {
        QSKIP("No chapters in EPUB");
    }

    QElapsedTimer timer;
    qint64 firstAccessMs, secondAccessMs, thirdAccessMs;

    // First access - cold cache
    timer.start();
    QString html1 = m_doc->chapterHtml(5);
    firstAccessMs = timer.elapsed();
    qDebug().noquote() << QString("Chapter 5 - First access (cold):  %1 ms").arg(firstAccessMs, 5);

    // Second access - warm cache
    timer.start();
    QString html2 = m_doc->chapterHtml(5);
    secondAccessMs = timer.elapsed();
    qDebug().noquote() << QString("Chapter 5 - Second access (cached): %1 ms").arg(secondAccessMs, 5);

    // Third access - verify cache consistency
    timer.start();
    QString html3 = m_doc->chapterHtml(5);
    thirdAccessMs = timer.elapsed();
    qDebug().noquote() << QString("Chapter 5 - Third access (cached):  %1 ms").arg(thirdAccessMs, 5);

    QCOMPARE(html1, html2);
    QCOMPARE(html2, html3);

    qDebug() << "Speedup:" << (firstAccessMs > 0 ? QString::number(firstAccessMs / (secondAccessMs + 1)) + "x" : "instant");
}

void EpubLargeFileBenchmark::benchmarkMultipleChapterAccess()
{
    if (!m_doc || m_doc->spineCount() < 10) {
        QSKIP("Need at least 10 chapters");
    }

    QElapsedTimer timer;

    // Access 10 different chapters for first time
    timer.start();
    for (int i = 0; i < 10; ++i) {
        QString html = m_doc->chapterHtml(i);
        Q_UNUSED(html);
    }
    qint64 coldAccessMs = timer.elapsed();
    qDebug().noquote() << QString("10 chapters - Cold access: %1 ms").arg(coldAccessMs, 5);

    // Access same 10 chapters again (all cached)
    timer.start();
    for (int i = 0; i < 10; ++i) {
        QString html = m_doc->chapterHtml(i);
        Q_UNUSED(html);
    }
    qint64 cachedAccessMs = timer.elapsed();
    qDebug().noquote() << QString("10 chapters - Cached access: %1 ms").arg(cachedAccessMs, 5);

    // Access same 10 chapters 100 times more (all from cache)
    timer.start();
    for (int round = 0; round < 100; ++round) {
        for (int i = 0; i < 10; ++i) {
            QString html = m_doc->chapterHtml(i);
            Q_UNUSED(html);
        }
    }
    qint64 massAccessMs = timer.elapsed();
    qDebug().noquote() << QString("1000 cached accesses: %1 ms").arg(massAccessMs, 5);
}

void EpubLargeFileBenchmark::benchmarkVideoPaths()
{
    if (!m_doc || m_doc->spineCount() == 0) {
        QSKIP("No chapters in EPUB");
    }

    QElapsedTimer timer;

    // Load chapter first
    timer.start();
    QVector<QString> paths1 = m_doc->chapterVideoPaths(5);
    qint64 firstVideoMs = timer.elapsed();
    qDebug().noquote() << QString("Video paths - First call (triggers HTML processing): %1 ms").arg(firstVideoMs, 5);

    // Access video paths again (uses cache)
    timer.start();
    for (int i = 0; i < 1000; ++i) {
        QVector<QString> paths = m_doc->chapterVideoPaths(5);
        Q_UNUSED(paths);
    }
    qint64 cachedVideoMs = timer.elapsed();
    qDebug().noquote() << QString("Video paths - 1000 cached calls: %1 ms").arg(cachedVideoMs, 5);
}

QTEST_MAIN(EpubLargeFileBenchmark)
#include "EpubLargeFileBenchmark.moc"
