#include "ui/ReadAloud.h"
#ifdef MNEMOSYNE_ENABLE_TTS
#include "ui/ReadAloudController.h"
#endif

#include <QTest>
#include <QTextBrowser>

class ReadAloudTest : public QObject
{
    Q_OBJECT

private slots:
    void textFromViewportStartsAtTopAndRunsToEnd();
#ifdef MNEMOSYNE_ENABLE_TTS
    void controllerStartsInactiveAndStopIsSafe();
#endif
};

void ReadAloudTest::textFromViewportStartsAtTopAndRunsToEnd()
{
    QTextBrowser browser;
    browser.setPlainText(QStringLiteral("First paragraph.\nSecond paragraph.\nThird paragraph."));
    browser.resize(400, 300);
    browser.show();
    QVERIFY(QTest::qWaitForWindowExposed(&browser));

    const QString text = ReadAloud::textFromViewport(&browser);
    QVERIFY(text.startsWith(QStringLiteral("First paragraph.")));
    QVERIFY(text.contains(QStringLiteral("Third paragraph.")));
}

#ifdef MNEMOSYNE_ENABLE_TTS
void ReadAloudTest::controllerStartsInactiveAndStopIsSafe()
{
    ReadAloudController controller;
    QVERIFY(!controller.isActive());
    controller.stop(); // no-op when idle, must not touch the (possibly absent) engine
    QVERIFY(!controller.isActive());
}
#endif

QTEST_MAIN(ReadAloudTest)
#include "ReadAloudTest.moc"
