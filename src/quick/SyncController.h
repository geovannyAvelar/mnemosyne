#pragma once

#include <QObject>
#include <QString>

// QML-facing wrapper around GoogleAuth's sign-in flow — the mobile
// equivalent of desktop MainWindow's Sync menu (see ui/MainWindow.cpp's
// signInWithGoogle()/signOutOfGoogle()). Mnemosyne ships its own bundled
// OAuth client (see GoogleAuth.cpp's kBundledClientId) so there's no
// credential-entry step here, only sign-in/out — hasClientCredentials is
// false (and the sign-in button disabled) only in an unconfigured
// from-source Android build, same as desktop. Reading/writing progress
// sync itself happens directly in PdfDocumentModel/EpubReaderModel
// (mirroring where desktop's PdfView/EpubView call GoogleDriveSync) — this
// class is only the sign-in/settings surface, backing qml/SettingsScreen.qml.
class SyncController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasClientCredentials READ hasClientCredentials CONSTANT)
    Q_PROPERTY(bool isSignedIn READ isSignedIn NOTIFY signedInChanged)
    Q_PROPERTY(QString accountEmail READ accountEmail NOTIFY signedInChanged)
    Q_PROPERTY(bool signInInProgress READ signInInProgress NOTIFY signInInProgressChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    using QObject::QObject;

    bool hasClientCredentials() const;
    bool isSignedIn() const;
    QString accountEmail() const;
    bool signInInProgress() const { return m_signInInProgress; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void startSignIn();
    Q_INVOKABLE void signOut();

signals:
    void signedInChanged();
    void signInInProgressChanged();
    void lastErrorChanged();
    // Fired once per startSignIn() call, success or failure — SettingsScreen
    // uses this rather than polling isSignedIn after a fixed delay.
    void signInFinished(bool ok, const QString &error);

private:
    void setLastError(const QString &error);
    void setSignInInProgress(bool inProgress);

    bool m_signInInProgress = false;
    QString m_lastError;
};
