#include "AndroidStorageAccess.h"

#include <QCoreApplication>
#include <QJniObject>
#include <QMetaObject>
#include <QtCore/private/qandroidextras_p.h>
#include <QtCore/qcoreapplication_platform.h>

namespace {

// android.content.Intent constants (stable AOSP API, not looked up via JNI
// since they're guaranteed-stable ints/strings, same as every Qt-for-Android
// JNI snippet that touches Intent flags).
constexpr int kFlagGrantReadUriPermission = 0x00000001;
constexpr int kFlagGrantPersistableUriPermission = 0x00000040;
constexpr int kActivityResultOk = -1;
constexpr int kPickDocumentRequestCode = 42;

QString queryDisplayName(const QJniObject &context, const QJniObject &uri)
{
    QJniObject resolver = context.callObjectMethod(
        "getContentResolver", "()Landroid/content/ContentResolver;");
    if (!resolver.isValid()) {
        return {};
    }

    QJniObject cursor = resolver.callObjectMethod(
        "query",
        "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)"
        "Landroid/database/Cursor;",
        uri.object<jobject>(), nullptr, nullptr, nullptr, nullptr);
    if (!cursor.isValid()) {
        return {};
    }

    QString displayName;
    if (cursor.callMethod<jboolean>("moveToFirst")) {
        const jint columnIndex = cursor.callMethod<jint>(
            "getColumnIndex", "(Ljava/lang/String;)I",
            QJniObject::fromString(QStringLiteral("_display_name")).object<jstring>());
        if (columnIndex >= 0) {
            displayName = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", columnIndex)
                              .toString();
        }
    }
    cursor.callMethod<void>("close");
    return displayName;
}

} // namespace

void AndroidStorageAccess::pickDocument()
{
    QJniObject intent(
        "android/content/Intent", "(Ljava/lang/String;)V",
        QJniObject::fromString(QStringLiteral("android.intent.action.OPEN_DOCUMENT")).object<jstring>());
    intent.callObjectMethod(
        "addCategory", "(Ljava/lang/String;)Landroid/content/Intent;",
        QJniObject::fromString(QStringLiteral("android.intent.category.OPENABLE")).object<jstring>());
    intent.callObjectMethod(
        "setType", "(Ljava/lang/String;)Landroid/content/Intent;",
        QJniObject::fromString(QStringLiteral("*/*")).object<jstring>());
    intent.callObjectMethod(
        "addFlags", "(I)Landroid/content/Intent;",
        kFlagGrantReadUriPermission | kFlagGrantPersistableUriPermission);

    QtAndroidPrivate::startActivity(
        intent, kPickDocumentRequestCode,
        [this](int /*requestCode*/, int resultCode, const QJniObject &data) {
            if (resultCode != kActivityResultOk || !data.isValid()) {
                QMetaObject::invokeMethod(this, &AndroidStorageAccess::pickCancelled, Qt::QueuedConnection);
                return;
            }

            QJniObject uri = data.callObjectMethod("getData", "()Landroid/net/Uri;");
            if (!uri.isValid()) {
                QMetaObject::invokeMethod(this, &AndroidStorageAccess::pickCancelled, Qt::QueuedConnection);
                return;
            }

            const QJniObject context = QNativeInterface::QAndroidApplication::context();
            QJniObject resolver = context.callObjectMethod(
                "getContentResolver", "()Landroid/content/ContentResolver;");
            resolver.callMethod<void>(
                "takePersistableUriPermission", "(Landroid/net/Uri;I)V",
                uri.object<jobject>(), kFlagGrantReadUriPermission);

            const QString uriString = uri.toString();
            const QString displayName = queryDisplayName(context, uri);

            QMetaObject::invokeMethod(
                this,
                [this, uriString, displayName] { emit documentPicked(uriString, displayName); },
                Qt::QueuedConnection);
        });
}
