#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace HighlightExporter {
struct ExportEntry;
}

// Loads and runs QuickJS plugins from pluginsDirectory(). A plugin is a
// folder containing a manifest.json ({id, name, version, description,
// main}) and the JS entry file "main" points to. Each enabled plugin gets
// its own JSRuntime + JSContext (isolated globals, torn down and recreated
// together on reload()) with one bound global, `mnemosyne`:
//
//   mnemosyne.registerExporter({id, label, fileFilter, defaultExtension,
//                                format: function(bookTitle, entries) {...}})
//   mnemosyne.on(eventName, function(payload) {...})
//   mnemosyne.log(...)
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

// Fans out to every loaded plugin's mnemosyne.on(name, ...) listener(s), if
// any. Never throws or otherwise propagates a plugin's error to the caller.
void emitEvent(const QString &name, const QJsonObject &payload);

} // namespace PluginHost
