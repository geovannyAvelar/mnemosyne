#include "quick/ContentUriCache.h"

#include <QTest>

// content:// URI caching (the part that actually copies bytes out of a
// SAF-provided stream) can't be exercised without a real Android
// ContentResolver, so this covers the one piece of resolveToLocalFile()
// that's genuinely portable and desktop-testable: the passthrough for
// anything that isn't a content:// URI, which is every desktop call site
// and any non-SAF Android path alike.
class ContentUriCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void plainFilePathPassesThroughUnchanged();
    void emptyPathPassesThroughUnchanged();
    void cachedFileKeepsTheFormatsSuffix();
};

void ContentUriCacheTest::plainFilePathPassesThroughUnchanged()
{
    QString error;
    const QString result = ContentUriCache::resolveToLocalFile(QStringLiteral("/tmp/book.pdf"), QStringLiteral("pdf"), &error);
    QCOMPARE(result, QStringLiteral("/tmp/book.pdf"));
    QVERIFY(error.isEmpty());
}

void ContentUriCacheTest::emptyPathPassesThroughUnchanged()
{
    const QString result = ContentUriCache::resolveToLocalFile(QString(), QStringLiteral("epub"), nullptr);
    QVERIFY(result.isEmpty());
}

// Every reader picks its backend from the cached file's suffix, so a format
// cached under the wrong one fails to open on Android only.
void ContentUriCacheTest::cachedFileKeepsTheFormatsSuffix()
{
    for (const char *format : {"epub", "cbz", "md", "markdown", "txt", "pdf"}) {
        QCOMPARE(ContentUriCache::localExtensionForFormat(QLatin1String(format)), QLatin1String(format));
    }
    QCOMPARE(ContentUriCache::localExtensionForFormat(QStringLiteral("unknown")), QStringLiteral("pdf"));
}

QTEST_MAIN(ContentUriCacheTest)
#include "ContentUriCacheTest.moc"
