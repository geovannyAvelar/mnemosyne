#include "markdown/MarkdownDocument.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

namespace {

// Markdown fixtures are small enough to write inline rather than keeping
// separate fixture files on disk (unlike EPUB's binary zip archives).
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

class MarkdownDocumentTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsTitleAndNestedHeadings();
    void skipsHeadingsInsideFencedCodeBlocks();
    void sectionsSplitBodyTextByHeading();
    void titleFallsBackToFilenameWhenNoHeadings();
    void failsGracefullyOnMissingFile();
};

void MarkdownDocumentTest::loadsTitleAndNestedHeadings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFixture(dir, QStringLiteral("book.md"),
                                       QStringLiteral("# Book Title\n"
                                                       "\n"
                                                       "Intro text.\n"
                                                       "\n"
                                                       "## Chapter One\n"
                                                       "\n"
                                                       "Chapter one text.\n"
                                                       "\n"
                                                       "### Section 1.1\n"
                                                       "\n"
                                                       "Nested text.\n"
                                                       "\n"
                                                       "## Chapter Two\n"
                                                       "\n"
                                                       "Chapter two text.\n"));

    QString error;
    auto doc = MarkdownDocument::load(path, &error);
    QVERIFY2(doc, qPrintable(error));

    QCOMPARE(doc->title(), QStringLiteral("Book Title"));

    const QVector<TocNode> toc = doc->tableOfContents();
    // The single H1 is the document title, so the TOC's top level starts at
    // its two H2 children.
    QCOMPARE(toc.size(), 1);
    QCOMPARE(toc[0].title, QStringLiteral("Book Title"));
    QCOMPARE(toc[0].pageNumber, 0);
    QCOMPARE(toc[0].children.size(), 2);

    QCOMPARE(toc[0].children[0].title, QStringLiteral("Chapter One"));
    QCOMPARE(toc[0].children[0].pageNumber, 1);
    QCOMPARE(toc[0].children[0].children.size(), 1);
    QCOMPARE(toc[0].children[0].children[0].title, QStringLiteral("Section 1.1"));
    QCOMPARE(toc[0].children[0].children[0].pageNumber, 2);

    QCOMPARE(toc[0].children[1].title, QStringLiteral("Chapter Two"));
    QCOMPARE(toc[0].children[1].pageNumber, 3);
    QVERIFY(toc[0].children[1].children.isEmpty());
}

void MarkdownDocumentTest::skipsHeadingsInsideFencedCodeBlocks()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFixture(dir, QStringLiteral("readme.md"),
                                       QStringLiteral("# Real Heading\n"
                                                       "\n"
                                                       "```bash\n"
                                                       "# this looks like a heading but isn't one\n"
                                                       "echo hi\n"
                                                       "```\n"
                                                       "\n"
                                                       "## Another Real Heading\n"));

    QString error;
    auto doc = MarkdownDocument::load(path, &error);
    QVERIFY2(doc, qPrintable(error));

    const QVector<TocNode> toc = doc->tableOfContents();
    QCOMPARE(toc.size(), 1);
    QCOMPARE(toc[0].title, QStringLiteral("Real Heading"));
    QCOMPARE(toc[0].children.size(), 1);
    QCOMPARE(toc[0].children[0].title, QStringLiteral("Another Real Heading"));
}

void MarkdownDocumentTest::sectionsSplitBodyTextByHeading()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFixture(dir, QStringLiteral("book.md"),
                                       QStringLiteral("Preamble mentions unicorns.\n"
                                                       "\n"
                                                       "# Chapter One\n"
                                                       "\n"
                                                       "This chapter mentions dragons.\n"
                                                       "\n"
                                                       "# Chapter Two\n"
                                                       "\n"
                                                       "This chapter mentions griffins.\n"));

    QString error;
    auto doc = MarkdownDocument::load(path, &error);
    QVERIFY2(doc, qPrintable(error));

    const QVector<MarkdownDocument::Section> sections = doc->sections();
    QCOMPARE(sections.size(), 3);

    QCOMPARE(sections[0].headingIndex, -1);
    QVERIFY(sections[0].text.contains(QStringLiteral("unicorns")));

    QCOMPARE(sections[1].headingIndex, 0);
    QCOMPARE(sections[1].label, QStringLiteral("Chapter One"));
    QVERIFY(sections[1].text.contains(QStringLiteral("dragons")));
    QVERIFY(!sections[1].text.contains(QStringLiteral("griffins")));

    QCOMPARE(sections[2].headingIndex, 1);
    QCOMPARE(sections[2].label, QStringLiteral("Chapter Two"));
    QVERIFY(sections[2].text.contains(QStringLiteral("griffins")));
}

void MarkdownDocumentTest::titleFallsBackToFilenameWhenNoHeadings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFixture(dir, QStringLiteral("just-prose.md"),
                                       QStringLiteral("Just a paragraph, no headings at all.\n"));

    QString error;
    auto doc = MarkdownDocument::load(path, &error);
    QVERIFY2(doc, qPrintable(error));

    QCOMPARE(doc->title(), QStringLiteral("just-prose"));
    QVERIFY(doc->tableOfContents().isEmpty());
}

void MarkdownDocumentTest::failsGracefullyOnMissingFile()
{
    QString error;
    auto doc = MarkdownDocument::load(QStringLiteral("/does/not/exist.md"), &error);
    QVERIFY(!doc);
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(MarkdownDocumentTest)
#include "MarkdownDocumentTest.moc"
