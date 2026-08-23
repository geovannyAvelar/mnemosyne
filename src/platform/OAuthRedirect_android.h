#pragma once

#include <QString>

#include <functional>

// Android-only counterpart to GoogleAuth's desktop loopback-TCP-server
// redirect catcher: OAuthRedirectActivity.java is registered in
// AndroidManifest.xml for the mnemosyne://oauth2redirect custom URI scheme,
// and calls back into this process via JNI (see OAuthRedirect_android.cpp)
// when the system browser follows Google's redirect there.
namespace OAuthRedirect {

// Replaces the pending callback. GoogleAuth::startSignInAndroid() sets one
// before opening the browser and clears it (pass nullptr) once resolved —
// there is only ever at most one sign-in in flight. Invoked with
// (code, state, error) exactly as parsed from the redirect URI's query
// string; code and state are empty together if the user backs out of the
// browser without Google appending an error param either.
void setPendingCallback(std::function<void(const QString &code, const QString &state, const QString &error)> callback);

} // namespace OAuthRedirect
