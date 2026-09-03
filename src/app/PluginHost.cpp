#include "PluginHost.h"

#include "HighlightExporter.h"

#include <quickjs.h>

#include <QColor>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

#include <memory>
#include <vector>

namespace PluginHost {

namespace {

constexpr qint64 kWatchdogBudgetMs = 250;
constexpr size_t kMemoryLimitBytes = 64 * 1024 * 1024; // per plugin

struct EventListener
{
    QString eventName;
    JSValue callback; // owned (JS_DupValue'd on registration); freed on unload
};

// One entry per discovered plugin, whether or not it's enabled -- a
// disabled plugin has info filled in (for the Plugins dialog) but a null
// runtime/context and empty listeners/exporterFormatFns, since its JS never
// runs.
struct LoadedPlugin
{
    PluginInfo info;
    JSRuntime *runtime = nullptr;
    JSContext *context = nullptr;
    QVector<EventListener> listeners;
    QVector<PluginExporter> exporters;
    QHash<QString, JSValue> exporterFormatFns; // keyed by PluginExporter::id (already qualified)
    QElapsedTimer watchdogTimer;
    bool watchdogTripped = false;
};

// std::vector, not QVector/QList: QList<std::unique_ptr<T>> isn't fully
// supported in Qt6 (non-const begin()/detach() instantiates a copy-append
// path even when never taken, which fails to compile for a move-only
// element type). unique_ptr indirection keeps each LoadedPlugin's address
// stable as this vector grows while plugins are discovered one at a time --
// native JS callbacks hold that address via JS_SetContextOpaque.
std::vector<std::unique_ptr<LoadedPlugin>> &pluginRegistry()
{
    static std::vector<std::unique_ptr<LoadedPlugin>> plugins;
    return plugins;
}

QString jsValueToQString(JSContext *ctx, JSValueConst v)
{
    const char *s = JS_ToCString(ctx, v);
    if (!s) {
        return QString();
    }
    const QString result = QString::fromUtf8(s);
    JS_FreeCString(ctx, s);
    return result;
}

QString jsGetStringProp(JSContext *ctx, JSValueConst obj, const char *prop)
{
    const JSValue v = JS_GetPropertyStr(ctx, obj, prop);
    const QString result = JS_IsString(v) ? jsValueToQString(ctx, v) : QString();
    JS_FreeValue(ctx, v);
    return result;
}

JSValue qJsonToJs(JSContext *ctx, const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::Bool:
        return JS_NewBool(ctx, value.toBool());
    case QJsonValue::Double:
        return JS_NewFloat64(ctx, value.toDouble());
    case QJsonValue::String: {
        const QByteArray utf8 = value.toString().toUtf8();
        return JS_NewStringLen(ctx, utf8.constData(), static_cast<size_t>(utf8.size()));
    }
    case QJsonValue::Array: {
        const QJsonArray array = value.toArray();
        JSValue jsArray = JS_NewArray(ctx);
        for (int i = 0; i < array.size(); ++i) {
            JS_SetPropertyUint32(ctx, jsArray, static_cast<uint32_t>(i), qJsonToJs(ctx, array.at(i)));
        }
        return jsArray;
    }
    case QJsonValue::Object: {
        const QJsonObject object = value.toObject();
        JSValue jsObject = JS_NewObject(ctx);
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            JS_SetPropertyStr(ctx, jsObject, it.key().toUtf8().constData(), qJsonToJs(ctx, it.value()));
        }
        return jsObject;
    }
    case QJsonValue::Null:
    case QJsonValue::Undefined:
        break;
    }
    return JS_NULL;
}

QJsonArray exportEntriesToJson(const QVector<HighlightExporter::ExportEntry> &entries)
{
    QJsonArray array;
    for (const HighlightExporter::ExportEntry &entry : entries) {
        QJsonObject obj;
        obj["text"] = entry.highlight.text;
        obj["note"] = entry.highlight.note;
        obj["color"] = entry.highlight.color.name(QColor::HexArgb);
        obj["targetIndex"] = entry.highlight.targetIndex;
        obj["positionLabel"] = entry.positionLabel;
        obj["createdAt"] = entry.highlight.createdAt.toString(Qt::ISODate);
        array.append(obj);
    }
    return array;
}

void logJsException(LoadedPlugin &plugin, const QString &context)
{
    const JSValue exc = JS_GetException(plugin.context);
    const QString message = jsValueToQString(plugin.context, exc);
    JS_FreeValue(plugin.context, exc);

    if (plugin.watchdogTripped) {
        qWarning().noquote() << QStringLiteral("[plugin:%1] %2: stopped -- took longer than %3ms")
                                     .arg(plugin.info.id, context)
                                     .arg(kWatchdogBudgetMs);
    } else {
        qWarning().noquote() << QStringLiteral("[plugin:%1] %2: %3").arg(plugin.info.id, context, message);
    }
}

int interruptHandler(JSRuntime *, void *opaque)
{
    auto *plugin = static_cast<LoadedPlugin *>(opaque);
    if (plugin->watchdogTimer.isValid() && plugin->watchdogTimer.elapsed() > kWatchdogBudgetMs) {
        plugin->watchdogTripped = true;
        return 1; // abort the running script
    }
    return 0;
}

// Runs fn(args...) under the watchdog, logging (and swallowing) any
// exception or timeout. Frees result and every element of args before
// returning. Returns an owned JSValue (JS_UNDEFINED on failure) the caller
// must JS_FreeValue.
JSValue callWatched(LoadedPlugin &plugin, JSValueConst fn, const QString &context, int argc, JSValueConst *argv)
{
    plugin.watchdogTimer.restart();
    plugin.watchdogTripped = false;
    JSValue result = JS_Call(plugin.context, fn, JS_UNDEFINED, argc, argv);
    if (JS_IsException(result)) {
        logJsException(plugin, context);
        JS_FreeValue(plugin.context, result);
        return JS_UNDEFINED;
    }
    return result;
}

// ---- native functions bound as mnemosyne.* --------------------------------

JSValue jsRegisterExporter(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *plugin = static_cast<LoadedPlugin *>(JS_GetContextOpaque(ctx));
    if (!plugin || argc < 1 || !JS_IsObject(argv[0])) {
        return JS_ThrowTypeError(ctx, "registerExporter expects an object");
    }

    const QString id = jsGetStringProp(ctx, argv[0], "id");
    const JSValue formatFn = JS_GetPropertyStr(ctx, argv[0], "format");
    if (id.isEmpty() || !JS_IsFunction(ctx, formatFn)) {
        JS_FreeValue(ctx, formatFn);
        return JS_ThrowTypeError(ctx, "registerExporter requires a non-empty id and a format function");
    }

    PluginExporter exporter;
    exporter.pluginId = plugin->info.id;
    exporter.id = plugin->info.id + QLatin1Char('.') + id;
    exporter.label = jsGetStringProp(ctx, argv[0], "label");
    exporter.fileFilter = jsGetStringProp(ctx, argv[0], "fileFilter");
    exporter.defaultExtension = jsGetStringProp(ctx, argv[0], "defaultExtension");
    plugin->exporters.append(exporter);
    plugin->exporterFormatFns.insert(exporter.id, formatFn); // ownership: already a new reference from JS_GetPropertyStr

    return JS_UNDEFINED;
}

JSValue jsOn(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *plugin = static_cast<LoadedPlugin *>(JS_GetContextOpaque(ctx));
    if (!plugin || argc < 2 || !JS_IsFunction(ctx, argv[1])) {
        return JS_ThrowTypeError(ctx, "on(eventName, callback) requires a callback function");
    }
    const QString eventName = jsValueToQString(ctx, argv[0]);
    plugin->listeners.append({eventName, JS_DupValue(ctx, argv[1])});
    return JS_UNDEFINED;
}

JSValue jsLog(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    auto *plugin = static_cast<LoadedPlugin *>(JS_GetContextOpaque(ctx));
    QStringList parts;
    for (int i = 0; i < argc; ++i) {
        parts << jsValueToQString(ctx, argv[i]);
    }
    qDebug().noquote() << QStringLiteral("[plugin:%1] %2")
                               .arg(plugin ? plugin->info.id : QStringLiteral("?"), parts.join(QLatin1Char(' ')));
    return JS_UNDEFINED;
}

void bindGlobalApi(JSContext *ctx)
{
    const JSValue global = JS_GetGlobalObject(ctx);
    const JSValue mnemosyne = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, mnemosyne, "registerExporter", JS_NewCFunction(ctx, jsRegisterExporter, "registerExporter", 1));
    JS_SetPropertyStr(ctx, mnemosyne, "on", JS_NewCFunction(ctx, jsOn, "on", 2));
    JS_SetPropertyStr(ctx, mnemosyne, "log", JS_NewCFunction(ctx, jsLog, "log", 1));
    JS_SetPropertyStr(ctx, global, "mnemosyne", mnemosyne); // consumes mnemosyne
    JS_FreeValue(ctx, global);
}

void unloadAll()
{
    for (auto &plugin : pluginRegistry()) {
        if (!plugin->context) {
            continue;
        }
        for (EventListener &listener : plugin->listeners) {
            JS_FreeValue(plugin->context, listener.callback);
        }
        for (auto it = plugin->exporterFormatFns.begin(); it != plugin->exporterFormatFns.end(); ++it) {
            JS_FreeValue(plugin->context, it.value());
        }
        JS_FreeContext(plugin->context);
        JS_FreeRuntime(plugin->runtime);
    }
    pluginRegistry().clear();
}

} // namespace

QString pluginsDirectory()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/Plugins");
    QDir().mkpath(dir);
    return dir;
}

bool isEnabled(const QString &pluginId)
{
    return QSettings().value(QStringLiteral("Plugins/Enabled/%1").arg(pluginId), false).toBool();
}

void setEnabled(const QString &pluginId, bool enabled)
{
    QSettings().setValue(QStringLiteral("Plugins/Enabled/%1").arg(pluginId), enabled);
}

void reload()
{
    unloadAll();

    const QString dir = pluginsDirectory();
    const QStringList entries = QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entryName : entries) {
        const QString pluginDir = QDir(dir).filePath(entryName);
        QFile manifestFile(QDir(pluginDir).filePath(QStringLiteral("manifest.json")));
        if (!manifestFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();

        PluginInfo info;
        info.id = manifest.value(QStringLiteral("id")).toString();
        info.name = manifest.value(QStringLiteral("name")).toString(info.id);
        info.version = manifest.value(QStringLiteral("version")).toString();
        info.description = manifest.value(QStringLiteral("description")).toString();
        info.path = pluginDir;
        const QString mainFile = manifest.value(QStringLiteral("main")).toString();
        if (info.id.isEmpty() || mainFile.isEmpty()) {
            continue;
        }

        auto plugin = std::make_unique<LoadedPlugin>();
        plugin->info = info;

        if (isEnabled(info.id)) {
            QFile scriptFile(QDir(pluginDir).filePath(mainFile));
            if (scriptFile.open(QIODevice::ReadOnly)) {
                const QByteArray source = scriptFile.readAll();
                const QByteArray scriptPathUtf8 = scriptFile.fileName().toUtf8();

                plugin->runtime = JS_NewRuntime();
                JS_SetMemoryLimit(plugin->runtime, kMemoryLimitBytes);
                JS_SetInterruptHandler(plugin->runtime, interruptHandler, plugin.get());
                plugin->context = JS_NewContext(plugin->runtime);
                JS_SetContextOpaque(plugin->context, plugin.get());
                bindGlobalApi(plugin->context);

                plugin->watchdogTimer.restart();
                plugin->watchdogTripped = false;
                JSValue result = JS_Eval(plugin->context, source.constData(), static_cast<size_t>(source.size()),
                                          scriptPathUtf8.constData(), JS_EVAL_TYPE_GLOBAL);
                if (JS_IsException(result)) {
                    logJsException(*plugin, QStringLiteral("load"));
                }
                JS_FreeValue(plugin->context, result);
            } else {
                qWarning().noquote() << QStringLiteral("[plugin:%1] could not open %2").arg(info.id, mainFile);
            }
        }

        pluginRegistry().push_back(std::move(plugin));
    }
}

QVector<PluginInfo> discoveredPlugins()
{
    QVector<PluginInfo> result;
    for (const auto &plugin : pluginRegistry()) {
        result.append(plugin->info);
    }
    return result;
}

QVector<PluginExporter> registeredExporters()
{
    QVector<PluginExporter> result;
    for (const auto &plugin : pluginRegistry()) {
        result += plugin->exporters;
    }
    return result;
}

QString runExporter(const QString &exporterId, const QString &bookTitle,
                     const QVector<HighlightExporter::ExportEntry> &entries, QString *errorMessage)
{
    for (auto &plugin : pluginRegistry()) {
        const auto it = plugin->exporterFormatFns.find(exporterId);
        if (it == plugin->exporterFormatFns.end()) {
            continue;
        }

        const QByteArray titleUtf8 = bookTitle.toUtf8();
        const JSValue titleArg = JS_NewStringLen(plugin->context, titleUtf8.constData(),
                                                  static_cast<size_t>(titleUtf8.size()));
        const JSValue entriesArg = qJsonToJs(plugin->context, exportEntriesToJson(entries));
        JSValueConst args[2] = {titleArg, entriesArg};

        JSValue result = callWatched(*plugin, it.value(), QStringLiteral("exporter %1").arg(exporterId), 2, args);
        JS_FreeValue(plugin->context, titleArg);
        JS_FreeValue(plugin->context, entriesArg);

        if (!JS_IsString(result)) {
            JS_FreeValue(plugin->context, result);
            if (errorMessage) {
                *errorMessage = plugin->watchdogTripped
                    ? QObject::tr("The plugin took too long and was stopped.")
                    : QObject::tr("The plugin did not return any text to write.");
            }
            return QString();
        }

        const QString output = jsValueToQString(plugin->context, result);
        JS_FreeValue(plugin->context, result);
        return output;
    }

    if (errorMessage) {
        *errorMessage = QObject::tr("This plugin exporter is no longer available.");
    }
    return QString();
}

void emitEvent(const QString &name, const QJsonObject &payload)
{
    for (auto &plugin : pluginRegistry()) {
        if (!plugin->context) {
            continue;
        }
        for (const EventListener &listener : plugin->listeners) {
            if (listener.eventName != name) {
                continue;
            }
            const JSValue arg = qJsonToJs(plugin->context, payload);
            JSValueConst args[1] = {arg};
            JSValue result = callWatched(*plugin, listener.callback, QStringLiteral("event %1").arg(name), 1, args);
            JS_FreeValue(plugin->context, arg);
            JS_FreeValue(plugin->context, result);
        }
    }
}

} // namespace PluginHost
