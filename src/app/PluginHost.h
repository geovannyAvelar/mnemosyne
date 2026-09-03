#pragma once

#include "HighlightExporter.h" // HighlightExporter::ExportEntry, used by value in CommandContext

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>

#include <functional>
#include <optional>

// Loads and runs QuickJS plugins from pluginsDirectory(). A plugin is a
// folder containing a manifest.json ({id, name, version, description,
// main}) and the JS entry file "main" points to. Each enabled plugin gets
// its own JSRuntime + JSContext (isolated globals, torn down and recreated
// together on reload()) with one bound global, `mnemosyne`:
//
//   mnemosyne.registerExporter({id, label, fileFilter, defaultExtension,
//                                format: function(bookTitle, entries) {...}})
//   mnemosyne.registerCommand({id, label, run: function(context) {...}})
//   mnemosyne.registerCssInjector({id, formats, css: function() {...}})
//   mnemosyne.on(eventName, function(payload) {...})
//   mnemosyne.log(...)
//   mnemosyne.showMessage(text)
//   mnemosyne.showForm({title, fields: [{id, type, label, ...}, ...]})
//
// That's the *entire* API surface in this pass -- no file, network, or
// process access is bound, so a plugin can only do what these functions
// let it (QuickJS itself grants no ambient authority beyond what's
// explicitly exposed). A plugin that throws while loading, or a hook
// invocation (event listener / exporter format function) that throws or
// runs past a ~250ms watchdog budget, is logged via qWarning and otherwise
// ignored -- one broken plugin can't take down another plugin's hooks or
// the app itself.
namespace PluginHost {

struct PluginInfo
{
    QString id;
    QString name;
    QString version;
    QString description;
    QString path; // the plugin's own directory, containing manifest.json
};

struct PluginExporter
{
    QString pluginId;
    QString id; // globally unique: "<pluginId>.<the id the plugin gave>"
    QString label;
    QString fileFilter;
    QString defaultExtension;
};

struct PluginCommand
{
    QString pluginId;
    QString id; // globally unique: "<pluginId>.<the id the plugin gave>"
    QString label;
};

// The currently-open book, passed to a command's run(context) -- absent
// (JS receives null) when the Library tab is active rather than a book.
struct CommandContext
{
    QString bookHash;
    QString title;
    QVector<HighlightExporter::ExportEntry> highlights;
};

// pluginsDirectory()/<some-dir>/manifest.json -- created on first use.
QString pluginsDirectory();

// Rescans pluginsDirectory(), tearing down every previously loaded plugin's
// JS context and recreating one for each plugin currently enabled (see
// setEnabled()). Call once at startup and after every setEnabled() call.
void reload();

// Every plugin manifest.json found under pluginsDirectory(), enabled or
// not -- for a Plugins management UI.
QVector<PluginInfo> discoveredPlugins();

bool isEnabled(const QString &pluginId);
// Persists the setting; does not call reload() itself.
void setEnabled(const QString &pluginId, bool enabled);

// Every exporter registered by a currently-loaded (enabled) plugin.
QVector<PluginExporter> registeredExporters();

// Runs exporterId's format(bookTitle, entries) and returns the string it
// produced. Returns a null QString on a bad id, a thrown JS exception, or a
// watchdog abort -- errorMessage, if given, is filled with a
// user-presentable reason in that case.
QString runExporter(const QString &exporterId, const QString &bookTitle,
                     const QVector<HighlightExporter::ExportEntry> &entries, QString *errorMessage = nullptr);

// Every command registered by a currently-loaded (enabled) plugin.
QVector<PluginCommand> registeredCommands();

// Runs commandId's run(context) -- context is JS null when std::nullopt.
// Errors/timeouts are logged and swallowed, same as emitEvent(); a command
// has no return value of its own, only whatever it does via
// mnemosyne.showMessage() or a registered exporter/listener.
void runCommand(const QString &commandId, const std::optional<CommandContext> &context);

// Backs mnemosyne.showMessage(text): PluginHost has no UI of its own (it's
// Widgets-free, see src/CMakeLists.txt's mnemosynecore), so the desktop UI
// layer supplies how a message actually gets shown. Unset (or before this
// is called), showMessage() just logs via qWarning instead of doing
// nothing silently.
void setMessageHandler(std::function<void(const QString &)> handler);

// Backs mnemosyne.showForm(schema): same rationale as setMessageHandler()
// above. The handler renders schema (a form description -- see
// ui/PluginFormDialog.h for the field types it supports) modally and
// returns the entered values as a JSON object, or
// QJsonValue(QJsonValue::Null) if the user cancelled (also what an unset
// handler returns, so a build with no handler installed just always
// cancels rather than crashing). showForm() blocks until the handler
// returns, same as any other synchronous plugin call.
void setFormHandler(std::function<QJsonValue(const QJsonValue &)> handler);

// Fans out to every loaded plugin's mnemosyne.on(name, ...) listener(s), if
// any. Never throws or otherwise propagates a plugin's error to the caller.
void emitEvent(const QString &name, const QJsonObject &payload);

// Concatenated CSS (one rule block per matching injector, in registration
// order) from every enabled plugin's mnemosyne.registerCssInjector() whose
// `formats` list contains format (matched case-insensitively, e.g. "epub",
// "mobi", "markdown") -- empty if none apply. Each injector's css() is
// called fresh on every call (not cached), under the same watchdog/
// exception handling as any other hook, so keep it cheap.
QString cssForFormat(const QString &format);

} // namespace PluginHost
