#pragma once

#include <QString>

// Thin wrapper over each OS's secret store (macOS Keychain, Windows
// Credential Manager, Linux Secret Service via libsecret) for storing OAuth
// refresh tokens. Never falls back to plaintext — a given platform either
// has a real backend compiled in or the app must not offer sign-in there.
namespace TokenStore {

// key is a stable identifier for the secret, e.g. "GoogleDrive/refreshToken".
void save(const QString &key, const QString &secret);

// Empty if key isn't present or the platform store couldn't be read.
QString load(const QString &key);

void remove(const QString &key);

} // namespace TokenStore
