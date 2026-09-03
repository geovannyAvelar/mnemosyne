#pragma once

class QWidget;

// Lists every plugin PluginHost::discoveredPlugins() found, each with a
// checkbox toggling PluginHost::setEnabled() (applied immediately, calling
// PluginHost::reload() -- there's no separate OK/Apply step, same spirit as
// the Sync menu's toggles), plus a button to open the plugins folder in the
// OS file manager. See app/PluginHost.h for what a plugin actually is.
namespace PluginsDialog {

void show(QWidget *parent);

} // namespace PluginsDialog
