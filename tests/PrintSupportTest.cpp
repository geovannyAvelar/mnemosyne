#include "core/Document.h"
#include "ui/PrintSupport.h"

#include <QFile>
#include <QImage>
#include <QPageSize>
#include <QPrinter>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>

namespace {

class FakePage : public IPage
{
public:
    QSizeF sizePoints() const override { return {200, 300}; }
    QImage renderToImage(qreal scale) const override
    {
        QImage image(QSize(int(200 * scale), int(300 * scale)), QImage::Format_RGB32);
        image.fill(Qt::black);
        return image;
    }
    QString text() const override { return {}; }
    QVector<TextWord> words() const override { return {}; }
};

class FakeDocument : public IDocument
{
public:
    explicit FakeDocument(int pages) : m_pages(pages) { }
    int pageCount() const override { return m_pages; }
    std::unique_ptr<IPage> page(int) const override { return std::make_unique<FakePage>(); }
    QVector<TocNode> tableOfContents() const override { return {}; }
    QString title() const override { return QStringLiteral("fake"); }

private:
    int m_pages;
};

// Counts "/Type /Page" objects (not "/Pages") in an uncompressed-enough PDF.
int pdfPageCount(const QByteArray &pdf)
{
    int count = 0;
    for (int pos = pdf.indexOf("/Type /Page"); pos >= 0; pos = pdf.indexOf("/Type /Page", pos + 1)) {
        if (pdf.mid(pos + 11, 1) != "s") {
            ++count;
        }
    }
    return count;
}

} // namespace

class PrintSupportTest : public QObject
{
    Q_OBJECT

private slots:
    void printsEveryPageToPdf();
    void honorsPageRange();
    void printsTextDocument();
};

static QByteArray printToPdf(QTemporaryDir &dir, const std::function<void(QPrinter &)> &fn)
{
    QPrinter printer(QPrinter::ScreenResolution);
    printer.setResolution(72);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setPageSize(QPageSize(QPageSize::A4));
    const QString path = dir.filePath(QStringLiteral("out.pdf"));
    printer.setOutputFileName(path);
    fn(printer);
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void PrintSupportTest::printsEveryPageToPdf()
{
    QTemporaryDir dir;
    FakeDocument doc(3);
    const QByteArray pdf = printToPdf(dir, [&](QPrinter &p) { QVERIFY(PrintSupport::printPages(&p, doc)); });
    QVERIFY(pdf.startsWith("%PDF"));
    QCOMPARE(pdfPageCount(pdf), 3);
}

void PrintSupportTest::honorsPageRange()
{
    QTemporaryDir dir;
    FakeDocument doc(5);
    const QByteArray pdf = printToPdf(dir, [&](QPrinter &p) {
        p.setFromTo(2, 3);
        QVERIFY(PrintSupport::printPages(&p, doc));
    });
    QCOMPARE(pdfPageCount(pdf), 2);
}

void PrintSupportTest::printsTextDocument()
{
    QTemporaryDir dir;
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("hello print"));
    const QByteArray pdf = printToPdf(dir, [&](QPrinter &p) { QVERIFY(PrintSupport::printTextDocument(&p, doc)); });
    QVERIFY(pdf.startsWith("%PDF"));
    QVERIFY(pdfPageCount(pdf) >= 1);
}

QTEST_MAIN(PrintSupportTest)
#include "PrintSupportTest.moc"
