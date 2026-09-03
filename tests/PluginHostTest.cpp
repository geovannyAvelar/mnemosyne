#include "app/HighlightExporter.h"
#include "app/PluginHost.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QTest>
#include <QThread>

#include <algorithm>

namespace {

// Captures qDebug()/qWarning() output for assertions instead of letting it
// go to stderr -- the only way a plugin's mnemosyne.log() or a swallowed JS
// exception's warning is observable from outside PluginHost.
QStringList &capturedMessages()
{
    static QStringList messages;
    return messages;
}

void captureHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    capturedMessages().append(msg);
}

} // namespace

class PluginHostTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void registerExporterAndRunExporterRoundTrip();
    void unregisteredExporterIdReturnsNullWithError();
    void emitEventInvokesEnabledListener();
    void disabledPluginIsDiscoveredButNotLoaded();
    void pluginThatThrowsWhileLoadingDoesNotCrash();
    void listenerThatThrowsDoesNotBlockOtherPlugins();
    void watchdogAbortsRunawayListener();

    void registerCommandAppearsInRegisteredCommands();
    void runCommandPassesBookContext();
    void runCommandPassesNullContextWhenNoBookOpen();
    void showMessageCallsInstalledHandler();
    void showMessageWithNoHandlerLogsInstead();
    void runCommandForUnknownIdDoesNotCrash();

    void cssForFormatReturnsOnlyMatchingInjector();
    void cssForFormatMatchesCaseInsensitively();
    void cssForFormatCombinesMultipleInjectors();
    void cssForFormatWithNoMatchIsEmpty();

    void showFormPassesSchemaAndReturnsHandlerResult();
    void showFormReturnsNullWhenHandlerCancels();
    void showFormWithNoHandlerReturnsNullAndLogs();
    void showFormResetsWatchdogAfterSlowHandler();

private:
    QtMessageHandler m_previousHandler = nullptr;

    // Writes <pluginsDir>/<id>/{manifest.json, main.js}, sets its enabled
    // state, but does not reload() -- callers batch writes then reload once.
    static void writePlugin(const QString &id, const QString &script, bool enabled = true);
    static QVector<HighlightExporter::ExportEntry> sampleEntries();
};

void PluginHostTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void PluginHostTest::init()
{
    QSettings().clear();
    QDir(PluginHost::pluginsDirectory()).removeRecursively();
    capturedMessages().clear();
    m_previousHandler = qInstallMessageHandler(captureHandler);
    PluginHost::setMessageHandler(nullptr); // each test opts back in if it needs one
    PluginHost::setFormHandler(nullptr);
}

void PluginHostTest::cleanupTestCase()
{
    qInstallMessageHandler(m_previousHandler);
    PluginHost::setMessageHandler(nullptr);
    PluginHost::setFormHandler(nullptr);
    QDir(PluginHost::pluginsDirectory()).removeRecursively();
    QSettings().clear();
}

void PluginHostTest::writePlugin(const QString &id, const QString &script, bool enabled)
{
    const QString dir = QDir(PluginHost::pluginsDirectory()).filePath(id);
    QDir().mkpath(dir);

    QFile manifest(QDir(dir).filePath(QStringLiteral("manifest.json")));
    QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    manifest.write(QStringLiteral(R"({"id":"%1","name":"%1","version":"1.0","main":"main.js"})").arg(id).toUtf8());
    manifest.close();

    QFile entry(QDir(dir).filePath(QStringLiteral("main.js")));
    QVERIFY(entry.open(QIODevice::WriteOnly | QIODevice::Truncate));
    entry.write(script.toUtf8());
    entry.close();

    PluginHost::setEnabled(id, enabled);
}

QVector<HighlightExporter::ExportEntry> PluginHostTest::sampleEntries()
{
    Highlight h;
    h.text = QStringLiteral("a passage");
    h.note = QStringLiteral("a note");
    h.targetIndex = 0;
    return {{h, QStringLiteral("Page 1")}};
}

void PluginHostTest::registerExporterAndRunExporterRoundTrip()
{
    writePlugin(QStringLiteral("echo-plugin"), QStringLiteral(R"js(
        mnemosyne.registerExporter({
            id: "echo",
            label: "as Echo...",
            fileFilter: "Echo Files (*.echo)",
            defaultExtension: "echo",
            format: function(title, entries) {
                return title + ":" + entries.length + ":" + entries[0].text + ":" + entries[0].positionLabel;
            }
        });
    )js"));
    PluginHost::reload();

    const QVector<PluginHost::PluginExporter> exporters = PluginHost::registeredExporters();
    QCOMPARE(exporters.size(), 1);
    QCOMPARE(exporters.first().id, QStringLiteral("echo-plugin.echo"));
    QCOMPARE(exporters.first().label, QStringLiteral("as Echo..."));
    QCOMPARE(exporters.first().defaultExtension, QStringLiteral("echo"));

    QString errorMessage;
    const QString output =
        PluginHost::runExporter(QStringLiteral("echo-plugin.echo"), QStringLiteral("My Book"), sampleEntries(), &errorMessage);
    QCOMPARE(output, QStringLiteral("My Book:1:a passage:Page 1"));
    QVERIFY(errorMessage.isEmpty());
}

void PluginHostTest::unregisteredExporterIdReturnsNullWithError()
{
    PluginHost::reload(); // nothing written this test -- an empty registry

    QString errorMessage;
    const QString output = PluginHost::runExporter(QStringLiteral("nobody.nothing"), QStringLiteral("Title"),
                                                     sampleEntries(), &errorMessage);
    QVERIFY(output.isNull());
    QVERIFY(!errorMessage.isEmpty());
}

void PluginHostTest::emitEventInvokesEnabledListener()
{
    writePlugin(QStringLiteral("listener-plugin"), QStringLiteral(R"js(
        mnemosyne.on("highlightAdded", function(event) {
            mnemosyne.log("received:" + event.bookHash + ":" + event.text);
        });
    )js"));
    PluginHost::reload();

    QJsonObject payload;
    payload["bookHash"] = QStringLiteral("hash123");
    payload["text"] = QStringLiteral("hello");
    PluginHost::emitEvent(QStringLiteral("highlightAdded"), payload);

    QVERIFY(capturedMessages().join(QLatin1Char('\n')).contains(QStringLiteral("received:hash123:hello")));
}

void PluginHostTest::disabledPluginIsDiscoveredButNotLoaded()
{
    writePlugin(QStringLiteral("disabled-plugin"), QStringLiteral(R"js(
        mnemosyne.registerExporter({id: "x", label: "x", format: function() { return ""; }});
    )js"),
                /*enabled=*/false);
    PluginHost::reload();

    const QVector<PluginHost::PluginInfo> discovered = PluginHost::discoveredPlugins();
    QVERIFY(std::any_of(discovered.cbegin(), discovered.cend(),
                         [](const PluginHost::PluginInfo &info) { return info.id == QStringLiteral("disabled-plugin"); }));
    QVERIFY(!PluginHost::isEnabled(QStringLiteral("disabled-plugin")));

    const QVector<PluginHost::PluginExporter> exporters = PluginHost::registeredExporters();
    QVERIFY(std::none_of(exporters.cbegin(), exporters.cend(),
                          [](const PluginHost::PluginExporter &e) { return e.pluginId == QStringLiteral("disabled-plugin"); }));
}

void PluginHostTest::pluginThatThrowsWhileLoadingDoesNotCrash()
{
    writePlugin(QStringLiteral("broken-plugin"), QStringLiteral("throw new Error('boom');"));

    PluginHost::reload(); // must not crash

    const QVector<PluginHost::PluginExporter> exporters = PluginHost::registeredExporters();
    QVERIFY(std::none_of(exporters.cbegin(), exporters.cend(),
                          [](const PluginHost::PluginExporter &e) { return e.pluginId == QStringLiteral("broken-plugin"); }));
    QVERIFY(!capturedMessages().isEmpty()); // the load failure was logged, not silently swallowed
}

void PluginHostTest::listenerThatThrowsDoesNotBlockOtherPlugins()
{
    writePlugin(QStringLiteral("throwing-plugin"), QStringLiteral(R"js(
        mnemosyne.on("highlightAdded", function() { throw new Error("nope"); });
    )js"));
    writePlugin(QStringLiteral("healthy-plugin"), QStringLiteral(R"js(
        mnemosyne.on("highlightAdded", function() { mnemosyne.log("healthy-plugin ran"); });
    )js"));
    PluginHost::reload();

    PluginHost::emitEvent(QStringLiteral("highlightAdded"), QJsonObject{});

    QVERIFY(capturedMessages().join(QLatin1Char('\n')).contains(QStringLiteral("healthy-plugin ran")));
}

void PluginHostTest::watchdogAbortsRunawayListener()
{
    writePlugin(QStringLiteral("runaway-plugin"), QStringLiteral(R"js(
        mnemosyne.on("go", function() { while (true) {} });
    )js"));
    PluginHost::reload();

    QElapsedTimer timer;
    timer.start();
    PluginHost::emitEvent(QStringLiteral("go"), QJsonObject{});
    // Generous margin over the ~250ms watchdog budget -- this asserts the
    // process didn't hang, not that the timing is exact.
    QVERIFY(timer.elapsed() < 5000);
}

void PluginHostTest::registerCommandAppearsInRegisteredCommands()
{
    writePlugin(QStringLiteral("command-plugin"), QStringLiteral(R"js(
        mnemosyne.registerCommand({id: "greet", label: "Say Hello", run: function() {}});
    )js"));
    PluginHost::reload();

    const QVector<PluginHost::PluginCommand> commands = PluginHost::registeredCommands();
    QCOMPARE(commands.size(), 1);
    QCOMPARE(commands.first().id, QStringLiteral("command-plugin.greet"));
    QCOMPARE(commands.first().label, QStringLiteral("Say Hello"));
}

void PluginHostTest::runCommandPassesBookContext()
{
    writePlugin(QStringLiteral("context-plugin"), QStringLiteral(R"js(
        mnemosyne.registerCommand({
            id: "inspect",
            label: "Inspect",
            run: function(context) {
                mnemosyne.log("context:" + context.bookHash + ":" + context.title + ":" + context.highlights.length);
            }
        });
    )js"));
    PluginHost::reload();

    PluginHost::CommandContext context;
    context.bookHash = QStringLiteral("hash-abc");
    context.title = QStringLiteral("My Book");
    context.highlights = sampleEntries();
    PluginHost::runCommand(QStringLiteral("context-plugin.inspect"), context);

    QVERIFY(capturedMessages().join(QLatin1Char('\n')).contains(QStringLiteral("context:hash-abc:My Book:1")));
}

void PluginHostTest::runCommandPassesNullContextWhenNoBookOpen()
{
    writePlugin(QStringLiteral("null-context-plugin"), QStringLiteral(R"js(
        mnemosyne.registerCommand({
            id: "inspect",
            label: "Inspect",
            run: function(context) { mnemosyne.log("isNull:" + (context === null)); }
        });
    )js"));
    PluginHost::reload();

    PluginHost::runCommand(QStringLiteral("null-context-plugin.inspect"), std::nullopt);

    QVERIFY(capturedMessages().join(QLatin1Char('\n')).contains(QStringLiteral("isNull:true")));
}

void PluginHostTest::showMessageCallsInstalledHandler()
{
    QString received;
    PluginHost::setMessageHandler([&received](const QString &text) { received = text; });

    writePlugin(QStringLiteral("message-plugin"), QStringLiteral(R"js(
        mnemosyne.registerCommand({id: "greet", label: "Greet", run: function() { mnemosyne.showMessage("hello there"); }});
    )js"));
    PluginHost::reload();

    PluginHost::runCommand(QStringLiteral("message-plugin.greet"), std::nullopt);

    QCOMPARE(received, QStringLiteral("hello there"));
}

void PluginHostTest::showMessageWithNoHandlerLogsInstead()
{
    // init() already reset the handler to null for this test.
    writePlugin(QStringLiteral("no-handler-plugin"), QStringLiteral(R"js(
        mnemosyne.registerCommand({id: "greet", label: "Greet", run: function() { mnemosyne.showMessage("hi"); }});
    )js"));
    PluginHost::reload();

    PluginHost::runCommand(QStringLiteral("no-handler-plugin.greet"), std::nullopt); // must not crash

    QVERIFY(capturedMessages().join(QLatin1Char('\n')).contains(QStringLiteral("hi")));
}

void PluginHostTest::runCommandForUnknownIdDoesNotCrash()
{
    PluginHost::reload();
    PluginHost::runCommand(QStringLiteral("nobody.nothing"), std::nullopt); // must not crash
}

void PluginHostTest::cssForFormatReturnsOnlyMatchingInjector()
{
    writePlugin(QStringLiteral("css-plugin"), QStringLiteral(R"js(
        mnemosyne.registerCssInjector({
            id: "sepia",
            formats: ["epub"],
            css: function() { return "body{background:#f4ecd8;}"; }
        });
    )js"));
    PluginHost::reload();

    QVERIFY(PluginHost::cssForFormat(QStringLiteral("epub")).contains(QStringLiteral("#f4ecd8")));
    QVERIFY(PluginHost::cssForFormat(QStringLiteral("markdown")).isEmpty());
}

void PluginHostTest::cssForFormatMatchesCaseInsensitively()
{
    writePlugin(QStringLiteral("case-css-plugin"), QStringLiteral(R"js(
        mnemosyne.registerCssInjector({
            id: "sepia",
            formats: ["EPUB"],
            css: function() { return "body{background:#f4ecd8;}"; }
        });
    )js"));
    PluginHost::reload();

    QVERIFY(PluginHost::cssForFormat(QStringLiteral("epub")).contains(QStringLiteral("#f4ecd8")));
}

void PluginHostTest::cssForFormatCombinesMultipleInjectors()
{
    writePlugin(QStringLiteral("css-plugin-a"), QStringLiteral(R"js(
        mnemosyne.registerCssInjector({id: "a", formats: ["epub"], css: function() { return "body{color:red;}"; }});
    )js"));
    writePlugin(QStringLiteral("css-plugin-b"), QStringLiteral(R"js(
        mnemosyne.registerCssInjector({id: "b", formats: ["epub"], css: function() { return "h1{color:blue;}"; }});
    )js"));
    PluginHost::reload();

    const QString css = PluginHost::cssForFormat(QStringLiteral("epub"));
    QVERIFY(css.contains(QStringLiteral("color:red")));
    QVERIFY(css.contains(QStringLiteral("color:blue")));
}

void PluginHostTest::cssForFormatWithNoMatchIsEmpty()
{
    PluginHost::reload(); // nothing written this test
    QCOMPARE(PluginHost::cssForFormat(QStringLiteral("epub")), QString());
}

void PluginHostTest::showFormPassesSchemaAndReturnsHandlerResult()
{
    QJsonValue receivedSchema;
    PluginHost::setFormHandler([&receivedSchema](const QJsonValue &schema) {
        receivedSchema = schema;
        QJsonObject result;
        result["name"] = QStringLiteral("Alice");
        result["count"] = 3;
        return QJsonValue(result);
    });

    writePlugin(QStringLiteral("form-plugin"), QStringLiteral(R"js(
        mnemosyne.registerCommand({
            id: "ask",
            label: "Ask",
            run: function() {
                const result = mnemosyne.showForm({
                    title: "Test Form",
                    fields: [{id: "name", type: "text", label: "Name", default: "x"}]
                });
                mnemosyne.log("got:" + result.name + ":" + result.count);
            }
        });
    )js"));
    PluginHost::reload();

    PluginHost::runCommand(QStringLiteral("form-plugin.ask"), std::nullopt);

    QVERIFY(receivedSchema.isObject());
    QCOMPARE(receivedSchema.toObject().value(QStringLiteral("title")).toString(), QStringLiteral("Test Form"));
    const QJsonArray fields = receivedSchema.toObject().value(QStringLiteral("fields")).toArray();
    QCOMPARE(fields.size(), 1);
    QCOMPARE(fields.first().toObject().value(QStringLiteral("id")).toString(), QStringLiteral("name"));

    QVERIFY(capturedMessages().join(QLatin1Char('\n')).contains(QStringLiteral("got:Alice:3")));
}

void PluginHostTest::showFormReturnsNullWhenHandlerCancels()
{
    PluginHost::setFormHandler([](const QJsonValue &) { return QJsonValue(); }); // Null == cancelled

    writePlugin(QStringLiteral("cancel-form-plugin"), QStringLiteral(R"js(
        mnemosyne.registerCommand({
            id: "ask",
            label: "Ask",
            run: function() {
                const result = mnemosyne.showForm({title: "T", fields: []});
                mnemosyne.log("isNull:" + (result === null));
            }
        });
    )js"));
    PluginHost::reload();

    PluginHost::runCommand(QStringLiteral("cancel-form-plugin.ask"), std::nullopt);

    QVERIFY(capturedMessages().join(QLatin1Char('\n')).contains(QStringLiteral("isNull:true")));
}

void PluginHostTest::showFormWithNoHandlerReturnsNullAndLogs()
{
    // init() already reset the handler to null for this test.
    writePlugin(QStringLiteral("no-form-handler-plugin"), QStringLiteral(R"js(
        mnemosyne.registerCommand({
            id: "ask",
            label: "Ask",
            run: function() {
                const result = mnemosyne.showForm({title: "T", fields: []});
                mnemosyne.log("isNull:" + (result === null));
            }
        });
    )js"));
    PluginHost::reload();

    PluginHost::runCommand(QStringLiteral("no-form-handler-plugin.ask"), std::nullopt);

    const QString log = capturedMessages().join(QLatin1Char('\n'));
    QVERIFY(log.contains(QStringLiteral("showForm() with no handler installed")));
    QVERIFY(log.contains(QStringLiteral("isNull:true")));
}

void PluginHostTest::showFormResetsWatchdogAfterSlowHandler()
{
    PluginHost::setFormHandler([](const QJsonValue &) {
        QThread::msleep(400); // longer than the ~250ms watchdog budget
        QJsonObject result;
        result["ok"] = true;
        return QJsonValue(result);
    });

    writePlugin(QStringLiteral("slow-form-plugin"), QStringLiteral(R"js(
        mnemosyne.registerCommand({
            id: "ask",
            label: "Ask",
            run: function() {
                mnemosyne.showForm({title: "T", fields: []});
                mnemosyne.log("after form"); // must still run -- not aborted by the watchdog
            }
        });
    )js"));
    PluginHost::reload();

    PluginHost::runCommand(QStringLiteral("slow-form-plugin.ask"), std::nullopt);

    QVERIFY(capturedMessages().join(QLatin1Char('\n')).contains(QStringLiteral("after form")));
}

QTEST_MAIN(PluginHostTest)
#include "PluginHostTest.moc"
