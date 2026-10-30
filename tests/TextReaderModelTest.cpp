#include "app/ReadingProgressStore.h"
#include "app/FileIdentity.h"
#include "quick/SearchResultsModel.h"
#include "quick/TextReaderModel.h"

#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

// Exercises the Markdown and plain text reader the way qml/EpubReaderScreen.qml
// does, including that positions stay in the form desktop's MarkdownView and
// TxtView read and write.
class TextReaderModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void markdownSplitsAtHeadings();
    void markdownSkipsEmptyPreamble();
    void markdownHtmlHasNoPinnedFontSize();
    void markdownRestoresDesktopHeadingPosition();
    void savingKeepsDesktopZoom();
    void txtPartsCoverTheWholeFileAtLineEnds();
    void txtRestoresDesktopCharacterOffset();
    void txtHtmlEscapesMarkup();
    void emptyFileStillHasOnePart();
    void missingFileFailsWithMessage();
    void indexClampsToValidRange();
    void searchTargetsPartIndex();

private:
    QString writeFile(const QString &name, const QByteArray &content);

    QTemporaryDir m_dir;
};

QString TextReaderModelTest::writeFile(const QString &name, const QByteArray &content)
{
    const QString path = m_dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(content) != content.size()) {
        qFatal("could not write fixture");
    }
    return path;
}

void TextReaderModelTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTextReaderTest"));
    QVERIFY(m_dir.isValid());
}

void TextReaderModelTest::init()
{
    QSettings().clear();
}

void TextReaderModelTest::markdownSplitsAtHeadings()
{
    TextReaderModel model;
    QVERIFY(model.open(writeFile(QStringLiteral("a.md"), "intro\n\n# One\n\nfirst\n\n## Two\n\nsecond\n"),
                       QStringLiteral("md")));
    QCOMPARE(model.spineCount(), 3);
    QVERIFY(model.currentChapterHtml().contains(QStringLiteral("intro")));
    model.nextChapter();
    QVERIFY(model.currentChapterHtml().contains(QStringLiteral("One")));
    QVERIFY(model.currentChapterHtml().contains(QStringLiteral("first")));
    QVERIFY(!model.currentChapterHtml().contains(QStringLiteral("second")));
    QCOMPARE(model.highlightTarget(), 0);
}

void TextReaderModelTest::markdownSkipsEmptyPreamble()
{
    TextReaderModel model;
    QVERIFY(model.open(writeFile(QStringLiteral("b.md"), "\n\n# One\n\nfirst\n"), QStringLiteral("md")));
    QCOMPARE(model.spineCount(), 1);
}

void TextReaderModelTest::markdownHtmlHasNoPinnedFontSize()
{
    TextReaderModel model;
    QVERIFY(model.open(writeFile(QStringLiteral("c.md"), "plain body\n"), QStringLiteral("md")));
    const QString html = model.currentChapterHtml();
    QVERIFY(html.contains(QStringLiteral("name=\"viewport\"")));
    QVERIFY(html.contains(QStringLiteral("<body>")));
}

void TextReaderModelTest::markdownRestoresDesktopHeadingPosition()
{
    const QString path = writeFile(QStringLiteral("d.md"), "intro\n\n# One\n\nx\n\n# Two\n\ny\n");
    const QString hash = FileIdentity::contentHash(path);

    ReadingProgressStore::set(hash, 1, 0); // desktop: second heading
    TextReaderModel model;
    QVERIFY(model.open(path, QStringLiteral("md")));
    QCOMPARE(model.currentSpineIndex(), 2);

    model.previousChapter();
    model.close();
    QCOMPARE(ReadingProgressStore::get(hash)->position, 0); // first heading

    model.open(path, QStringLiteral("md"));
    model.previousChapter();
    model.close();
    QCOMPARE(ReadingProgressStore::get(hash)->position, -1); // the text before it
    model.open(path, QStringLiteral("md"));
    QCOMPARE(model.currentSpineIndex(), 0);
}

void TextReaderModelTest::savingKeepsDesktopZoom()
{
    const QString path = writeFile(QStringLiteral("e.md"), "# One\n\nx\n\n# Two\n\ny\n");
    const QString hash = FileIdentity::contentHash(path);
    ReadingProgressStore::set(hash, 0, 3);

    TextReaderModel model;
    QVERIFY(model.open(path, QStringLiteral("md")));
    model.nextChapter();
    model.close();
    QCOMPARE(ReadingProgressStore::get(hash)->zoom, 3.0);
}

void TextReaderModelTest::txtPartsCoverTheWholeFileAtLineEnds()
{
    QByteArray content;
    for (int i = 0; i < 5000; ++i) {
        content += "line " + QByteArray::number(i) + " of the sample text\n";
    }
    const std::optional<TextParts> loaded =
        loadTextParts(writeFile(QStringLiteral("f.txt"), content), nullptr);
    QVERIFY(loaded);
    QVERIFY(loaded->parts.size() > 1);

    QString joined;
    for (const TextPart &part : loaded->parts) {
        QCOMPARE(part.position, joined.size());
        joined += part.source;
        QVERIFY(part.source.endsWith(QLatin1Char('\n')));
    }
    QCOMPARE(joined, QString::fromUtf8(content));
}

void TextReaderModelTest::txtRestoresDesktopCharacterOffset()
{
    QByteArray content;
    for (int i = 0; i < 5000; ++i) {
        content += "line of the sample text number " + QByteArray::number(i) + "\n";
    }
    const QString path = writeFile(QStringLiteral("g.txt"), content);
    const std::optional<TextParts> loaded = loadTextParts(path, nullptr);
    QVERIFY(loaded && loaded->parts.size() > 1);

    // Partway through the second part, as a desktop scroll position would be.
    ReadingProgressStore::set(FileIdentity::contentHash(path), loaded->parts.at(1).position + 10, 0);
    TextReaderModel model;
    QVERIFY(model.open(path, QStringLiteral("txt")));
    QCOMPARE(model.currentSpineIndex(), 1);
}

void TextReaderModelTest::txtHtmlEscapesMarkup()
{
    TextReaderModel model;
    QVERIFY(model.open(writeFile(QStringLiteral("h.txt"), "<script>alert(1)</script>\n"), QStringLiteral("txt")));
    QVERIFY(!model.currentChapterHtml().contains(QStringLiteral("<script>")));
    QVERIFY(model.currentChapterHtml().contains(QStringLiteral("&lt;script&gt;")));
}

void TextReaderModelTest::emptyFileStillHasOnePart()
{
    TextReaderModel model;
    QVERIFY(model.open(writeFile(QStringLiteral("i.txt"), ""), QStringLiteral("txt")));
    QCOMPARE(model.spineCount(), 1);
    QVERIFY(model.open(writeFile(QStringLiteral("i.md"), ""), QStringLiteral("md")));
    QCOMPARE(model.spineCount(), 1);
}

void TextReaderModelTest::missingFileFailsWithMessage()
{
    TextReaderModel model;
    QVERIFY(!model.open(m_dir.filePath(QStringLiteral("absent.md")), QStringLiteral("md")));
    QVERIFY(!model.errorMessage().isEmpty());
    QVERIFY(!model.isOpen());
}

void TextReaderModelTest::indexClampsToValidRange()
{
    TextReaderModel model;
    QVERIFY(model.open(writeFile(QStringLiteral("j.md"), "# One\n\nx\n\n# Two\n\ny\n"), QStringLiteral("md")));
    model.setCurrentSpineIndex(99);
    QCOMPARE(model.currentSpineIndex(), 1);
    model.setCurrentSpineIndex(-5);
    QCOMPARE(model.currentSpineIndex(), 0);
}

void TextReaderModelTest::searchTargetsPartIndex()
{
    const QString path = writeFile(QStringLiteral("k.md"), "intro\n\n# One\n\nneedle here\n\n# Two\n\nnothing\n");
    SearchResultsModel results;
    QSignalSpy finished(&results, &SearchResultsModel::isSearchingChanged);
    results.search(path, QStringLiteral("NEEDLE"));
    QTRY_VERIFY(!results.isSearching());
    QCoreApplication::processEvents();
    QTRY_COMPARE(results.rowCount(), 1);
    QCOMPARE(results.index(0).data(SearchResultsModel::TargetIndexRole).toInt(), 1);
    QCOMPARE(results.index(0).data(SearchResultsModel::LabelRole).toString(), QStringLiteral("One"));
}

QTEST_MAIN(TextReaderModelTest)
#include "TextReaderModelTest.moc"
