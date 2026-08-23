#include "OAuthRedirect_android.h"

#include <QCoreApplication>
#include <QMetaObject>

#include <jni.h>

namespace {

std::function<void(const QString &, const QString &, const QString &)> &pendingCallback()
{
    static std::function<void(const QString &, const QString &, const QString &)> callback;
    return callback;
}

QString jstringToQString(JNIEnv *env, jstring value)
{
    if (!value) {
        return {};
    }
    const char *chars = env->GetStringUTFChars(value, nullptr);
    const QString result = QString::fromUtf8(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

} // namespace

namespace OAuthRedirect {

void setPendingCallback(std::function<void(const QString &code, const QString &state, const QString &error)> callback)
{
    pendingCallback() = std::move(callback);
}

} // namespace OAuthRedirect

// Matches OAuthRedirectActivity.java's `external fun`-equivalent
// `native void nativeOnRedirect(...)` declaration by JNI naming convention
// (Java_<package_with_underscores>_<Class>_<method>) — picked up
// automatically by the JVM when that class is loaded, no explicit
// RegisterNatives call needed. Only reachable while Mnemosyne's own process
// (and therefore this native library) is already running — the redirect
// Activity launching into a fresh process after Android killed the app
// while the browser was open is the accepted gap GoogleAuth.cpp documents.
extern "C" JNIEXPORT void JNICALL Java_org_mnemosyne_OAuthRedirectActivity_nativeOnRedirect(JNIEnv *env, jobject /*thiz*/,
                                                                                              jstring code, jstring state,
                                                                                              jstring error)
{
    if (!pendingCallback()) {
        return;
    }

    const QString codeStr = jstringToQString(env, code);
    const QString stateStr = jstringToQString(env, state);
    const QString errorStr = jstringToQString(env, error);

    // Consume the pending callback here (not inside the queued lambda) so a
    // stray second delivery of the intent can't fire it twice.
    auto callback = pendingCallback();
    pendingCallback() = nullptr;

    // JNI upcalls arrive on whichever thread called nativeOnRedirect
    // (Android's main thread) -- QueuedConnection onto qApp hands off to
    // Qt's own event-loop thread regardless of what that turns out to be,
    // so GoogleAuth's callback always runs where the rest of its state
    // (QNetworkAccessManager, QTimer) expects to be touched from.
    QMetaObject::invokeMethod(
        qApp, [callback, codeStr, stateStr, errorStr] { callback(codeStr, stateStr, errorStr); }, Qt::QueuedConnection);
}
