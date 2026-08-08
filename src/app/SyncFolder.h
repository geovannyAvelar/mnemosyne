#pragma once

#include <QString>

// The user-chosen folder to read/write sync data in — expected to be a
// folder some other service (iCloud Drive, Google Drive, Dropbox, ...)
// already syncs across devices. This app never talks to any cloud API
// directly; it just treats that folder as a plain local directory and lets
// the OS/vendor's own sync client do the rest.
namespace SyncFolder {

// Empty if the user hasn't configured one yet (sync disabled).
QString path();
void setPath(const QString &path);
bool isConfigured();

// path()/MnemosyneSync — created on first use. Keeping our files in a
// dedicated subfolder avoids cluttering whatever top-level folder (an
// entire iCloud Drive root, say) the user pointed us at.
QString dataDirectory();

} // namespace SyncFolder
