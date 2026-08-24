#include "TokenStore.h"

#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>

// Bridges to org.mnemosyne.TokenStoreBridge (android/src/org/mnemosyne/
// TokenStoreBridge.java), which encrypts with an AES-256-GCM key generated
// inside the hardware-backed Android Keystore and stores the ciphertext in
// a plain SharedPreferences file — the key itself never leaves the
// Keystore, so the stored bytes alone are useless. Deliberately not
// androidx.security's EncryptedSharedPreferences: that needs an extra
// Gradle dependency fetched from Google's Maven at build time, where a
// hand-rolled Keystore Cipher call using only APIs already on the device
// gets the same guarantee (ciphertext unusable off-device) with nothing new
// to resolve.
namespace TokenStore {

namespace {

const char *kBridgeClass = "org/mnemosyne/TokenStoreBridge";

QJniObject androidContext()
{
    return QNativeInterface::QAndroidApplication::context();
}

} // namespace

void save(const QString &key, const QString &secret)
{
    QJniObject::callStaticMethod<void>(kBridgeClass, "save",
                                        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)V",
                                        androidContext().object<jobject>(), QJniObject::fromString(key).object<jstring>(),
                                        QJniObject::fromString(secret).object<jstring>());
}

QString load(const QString &key)
{
    const QJniObject result = QJniObject::callStaticObjectMethod(
        kBridgeClass, "load", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;",
        androidContext().object<jobject>(), QJniObject::fromString(key).object<jstring>());
    return result.isValid() ? result.toString() : QString();
}

void remove(const QString &key)
{
    QJniObject::callStaticMethod<void>(kBridgeClass, "remove", "(Landroid/content/Context;Ljava/lang/String;)V",
                                        androidContext().object<jobject>(), QJniObject::fromString(key).object<jstring>());
}

} // namespace TokenStore
