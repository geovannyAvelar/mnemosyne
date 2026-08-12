#include "TokenStore.h"

#include <windows.h>

#include <wincred.h>

#include <QByteArray>

namespace {

// Credential Manager has one flat namespace per user, so prefix our own
// keys to avoid colliding with anything else on the machine.
std::wstring targetNameFor(const QString &key)
{
    return (QStringLiteral("Mnemosyne/") + key).toStdWString();
}

} // namespace

namespace TokenStore {

void save(const QString &key, const QString &secret)
{
    const QByteArray data = secret.toUtf8();
    const std::wstring target = targetNameFor(key);

    CREDENTIALW credential = {};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(target.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(data.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(data.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

    CredWriteW(&credential, 0);
}

QString load(const QString &key)
{
    const std::wstring target = targetNameFor(key);

    PCREDENTIALW credential = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        return {};
    }

    QString secret = QString::fromUtf8(reinterpret_cast<const char *>(credential->CredentialBlob),
                                        credential->CredentialBlobSize);
    CredFree(credential);
    return secret;
}

void remove(const QString &key)
{
    const std::wstring target = targetNameFor(key);
    CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
}

} // namespace TokenStore
