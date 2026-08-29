#pragma once

#include <QObject>
#include <QString>

// Wraps UIDocumentPickerViewController so QML can ask the user to choose a
// PDF/EPUB from anywhere on the device (Files app, iCloud Drive, other
// apps' document providers) the same way AndroidStorageAccess wraps
// Android's Storage Access Framework picker for the same QML call site.
//
// iOS's document picker hands back a security-scoped URL to the original
// file rather than a stable, freely re-readable identity the way Android's
// persistable content:// URI is -- so pickDocument() copies the picked
// file into this app's own Documents/Imported directory immediately and
// reports that copy's plain filesystem path, which every downstream
// consumer (FileIdentity, RecentFiles, PdfDocumentModel, EpubReaderModel)
// already knows how to treat exactly like a desktop path.
class IOSStorageAccess : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Shows the system document picker. Results (or cancellation) arrive
    // asynchronously via the signals below, since presenting a view
    // controller and copying the picked file both happen off this call.
    Q_INVOKABLE void pickDocument();

signals:
    // path is a plain filesystem path (this app's sandbox, not the
    // original picked location) suitable for QFile and as a RecentFiles
    // identity key. displayName is the originally picked file's name.
    void documentPicked(const QString &path, const QString &displayName);
    void pickCancelled();
};
