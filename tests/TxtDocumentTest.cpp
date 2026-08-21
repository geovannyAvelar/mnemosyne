#include "txt/TxtDocument.h"

#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

namespace {
QString writeFixture(QTemporaryDir &dir, const QString &name, const QString &content)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << content;
    }
    return path;
}
} // namespace

class TxtDocumentTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsTextAndFilenameTitle();
    void failsGracefullyOnMissingFile();
};

void TxtDocumentTest::loadsTextAndFilenameTitle()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFixture(dir, QStringLiteral("notes.txt"), QStringLiteral("Line one.\nLine two.\n"));

    QString error;
    auto doc = TxtDocument::load(path, &error);
    QVERIFY2(doc, qPrintable(error));

    // No metadata is possible for plain text, so the title always falls
    // back to the filename.
    QCOMPARE(doc->title(), QStringLiteral("notes"));
    QCOMPARE(doc->text(), QStringLiteral("Line one.\nLine two.\n"));
}

void TxtDocumentTest::failsGracefullyOnMissingFile()
{
    QString error;
    auto doc = TxtDocument::load(QStringLiteral("/does/not/exist.txt"), &error);
    QVERIFY(!doc);
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(TxtDocumentTest)
#include "TxtDocumentTest.moc"
