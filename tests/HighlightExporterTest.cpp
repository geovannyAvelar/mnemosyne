#include "app/HighlightExporter.h"

#include <QTest>

using HighlightExporter::BookExport;
using HighlightExporter::ExportEntry;

class HighlightExporterTest : public QObject
{
    Q_OBJECT

private slots:
    void markdownIncludesTitleAndEveryEntry();
    void markdownOmitsNoteWhenAbsentAndLabelWhenEmpty();
    void markdownEmptyEntriesStillHasTitle();

    void ankiTsvExcludesEntriesWithNoNote();
    void ankiTsvIsFrontTabBackPerLine();
    void ankiTsvFlattensEmbeddedTabsAndNewlines();
    void ankiTsvEmptyInputIsEmptyString();

    void libraryMarkdownHasOneHeadingPerBook();
    void libraryAnkiTsvPrefixesFrontWithBookTitle();
    void libraryExportersHandleEmptyLibrary();

private:
    static Highlight makeHighlight(const QString &text, const QString &note = QString());
};

Highlight HighlightExporterTest::makeHighlight(const QString &text, const QString &note)
{
    Highlight h;
    h.targetIndex = 0;
    h.text = text;
    h.note = note;
    h.createdAt = QDateTime(QDate(2026, 9, 2), QTime(10, 0));
    return h;
}

void HighlightExporterTest::markdownIncludesTitleAndEveryEntry()
{
    QVector<ExportEntry> entries;
    entries.append({makeHighlight(QStringLiteral("first passage"), QStringLiteral("first note")),
                     QStringLiteral("Page 1")});
    entries.append({makeHighlight(QStringLiteral("second passage")), QStringLiteral("Page 2")});

    const QString markdown = HighlightExporter::toMarkdown(QStringLiteral("My Book"), entries);

    QVERIFY(markdown.contains(QStringLiteral("# My Book")));
    QVERIFY(markdown.contains(QStringLiteral("first passage")));
    QVERIFY(markdown.contains(QStringLiteral("first note")));
    QVERIFY(markdown.contains(QStringLiteral("second passage"))); // included even with no note
    QVERIFY(markdown.contains(QStringLiteral("Page 1")));
    QVERIFY(markdown.contains(QStringLiteral("Page 2")));
}

void HighlightExporterTest::markdownOmitsNoteWhenAbsentAndLabelWhenEmpty()
{
    QVector<ExportEntry> entries;
    entries.append({makeHighlight(QStringLiteral("bare passage")), QString()});

    const QString markdown = HighlightExporter::toMarkdown(QStringLiteral("Book"), entries);

    QVERIFY(markdown.contains(QStringLiteral("bare passage")));
    QVERIFY(!markdown.contains(QStringLiteral("**"))); // no bolded position label rendered
}

void HighlightExporterTest::markdownEmptyEntriesStillHasTitle()
{
    const QString markdown = HighlightExporter::toMarkdown(QStringLiteral("Empty Book"), {});
    QVERIFY(markdown.contains(QStringLiteral("# Empty Book")));
}

void HighlightExporterTest::ankiTsvExcludesEntriesWithNoNote()
{
    QVector<ExportEntry> entries;
    entries.append({makeHighlight(QStringLiteral("no note here")), QString()});
    entries.append({makeHighlight(QStringLiteral("has a note"), QStringLiteral("the note")), QString()});

    const QString tsv = HighlightExporter::toAnkiTsv(entries);

    QVERIFY(!tsv.contains(QStringLiteral("no note here")));
    QVERIFY(tsv.contains(QStringLiteral("has a note")));
}

void HighlightExporterTest::ankiTsvIsFrontTabBackPerLine()
{
    QVector<ExportEntry> entries;
    entries.append({makeHighlight(QStringLiteral("front text"), QStringLiteral("back text")), QString()});

    const QString tsv = HighlightExporter::toAnkiTsv(entries);

    QCOMPARE(tsv, QStringLiteral("front text\tback text\n"));
}

void HighlightExporterTest::ankiTsvFlattensEmbeddedTabsAndNewlines()
{
    QVector<ExportEntry> entries;
    entries.append(
        {makeHighlight(QStringLiteral("line one\nline two"), QStringLiteral("note\twith tab")), QString()});

    const QString tsv = HighlightExporter::toAnkiTsv(entries);

    // Still exactly one row (one newline, at the very end) and the correct
    // number of tab-separated columns -- a stray embedded tab or newline
    // would otherwise split a row into a bogus extra field/line.
    QCOMPARE(tsv.count(QLatin1Char('\n')), 1);
    QCOMPARE(tsv.count(QLatin1Char('\t')), 1);
    QVERIFY(tsv.startsWith(QStringLiteral("line one line two\tnote with tab")));
}

void HighlightExporterTest::ankiTsvEmptyInputIsEmptyString()
{
    QCOMPARE(HighlightExporter::toAnkiTsv({}), QString());
}

void HighlightExporterTest::libraryMarkdownHasOneHeadingPerBook()
{
    QVector<BookExport> books;
    books.append({QStringLiteral("Book A"), {{makeHighlight(QStringLiteral("passage a")), QString()}}});
    books.append({QStringLiteral("Book B"), {{makeHighlight(QStringLiteral("passage b")), QString()}}});

    const QString markdown = HighlightExporter::toMarkdownForLibrary(books);

    QVERIFY(markdown.contains(QStringLiteral("# Book A")));
    QVERIFY(markdown.contains(QStringLiteral("# Book B")));
    QVERIFY(markdown.contains(QStringLiteral("passage a")));
    QVERIFY(markdown.contains(QStringLiteral("passage b")));
    // Book A's heading comes before Book B's -- library order is preserved.
    QVERIFY(markdown.indexOf(QStringLiteral("# Book A")) < markdown.indexOf(QStringLiteral("# Book B")));
}

void HighlightExporterTest::libraryAnkiTsvPrefixesFrontWithBookTitle()
{
    QVector<BookExport> books;
    books.append({QStringLiteral("Title One"),
                   {{makeHighlight(QStringLiteral("passage"), QStringLiteral("note")), QString()}}});

    const QString tsv = HighlightExporter::toAnkiTsvForLibrary(books);

    QCOMPARE(tsv, QStringLiteral("[Title One] passage\tnote\n"));
}

void HighlightExporterTest::libraryExportersHandleEmptyLibrary()
{
    QCOMPARE(HighlightExporter::toMarkdownForLibrary({}), QString());
    QCOMPARE(HighlightExporter::toAnkiTsvForLibrary({}), QString());
}

QTEST_MAIN(HighlightExporterTest)
#include "HighlightExporterTest.moc"
