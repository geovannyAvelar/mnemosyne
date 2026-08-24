#pragma once

#include <QObject>
#include <QString>

// Wraps Android's Storage Access Framework document picker
// (ACTION_OPEN_DOCUMENT) so QML can ask the user to choose a PDF/EPUB from
// anywhere on the device (local storage, cloud-backed providers, etc.)
// without needing broad filesystem permissions. The returned content://
// URI's read permission is made persistent (ContentResolver
// .takePersistableUriPermission) so it survives app restarts and device
// reboots — callers should store the URI string itself as the file's
// identity, the same way desktop code stores a filesystem path.
class AndroidStorageAccess : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Shows the system document picker. Results (or cancellation) arrive
    // asynchronously via the signals below, since the picker is a separate
    // Activity and this call returns immediately.
    Q_INVOKABLE void pickDocument();

signals:
    // uri is a content:// string suitable for QFile and for use as a
    // stable identity key (see FileIdentity::contentHash). displayName is
    // the picked file's name as reported by its content provider — not
    // necessarily accurate for identity/dedup, just for showing the user
    // something readable before the file has actually been opened.
    void documentPicked(const QString &uri, const QString &displayName);
    void pickCancelled();
};
