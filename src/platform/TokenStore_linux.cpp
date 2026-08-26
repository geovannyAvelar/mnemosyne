// libsecret pulls in glib/gio headers (gdbusintrospection.h) that use
// "signals" as a plain struct field name. Qt's <QString> defines "signals"
// as a macro (part of the signals/slots/emit convenience keywords), so
// libsecret must be included before any Qt header pulls that macro in, or
// the glib header fails to parse.
#include <libsecret/secret.h>

#include "TokenStore.h"

#include <QByteArray>
#include <QDebug>

namespace {

const SecretSchema *schema()
{
    static const SecretSchema kSchema = {
        "com.geovannyavelar.Mnemosyne.TokenStore",
        SECRET_SCHEMA_NONE,
        {
            {"key", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SecretSchemaAttributeType(0)},
        },
    };
    return &kSchema;
}

} // namespace

namespace TokenStore {

void save(const QString &key, const QString &secret)
{
    const QByteArray keyUtf8 = key.toUtf8();
    const QByteArray secretUtf8 = secret.toUtf8();

    GError *error = nullptr;
    const gboolean ok = secret_password_store_sync(schema(), SECRET_COLLECTION_DEFAULT, "Mnemosyne token",
                                                     secretUtf8.constData(), nullptr, &error, "key",
                                                     keyUtf8.constData(), nullptr);
    if (!ok) {
        // Most commonly: no Secret Service is running, or the user's
        // keyring collection is locked and couldn't be unlocked (no
        // prompter, or the prompt was dismissed) -- either way the token
        // silently isn't persisted, so the next load() will come back
        // empty and look like a sign-out.
        qWarning("TokenStore: failed to save secret for \"%s\": %s", qUtf8Printable(key),
                 error ? error->message : "unknown error");
    }
    if (error) {
        g_error_free(error);
    }
}

QString load(const QString &key)
{
    const QByteArray keyUtf8 = key.toUtf8();

    GError *error = nullptr;
    gchar *password = secret_password_lookup_sync(schema(), nullptr, &error, "key", keyUtf8.constData(), nullptr);
    if (!password) {
        if (error) {
            // Distinguish "no such secret" (error is null, this is a
            // normal not-signed-in state) from an actual Secret Service
            // failure -- e.g. the collection is locked -- which callers
            // otherwise can't tell apart from a real sign-out.
            qWarning("TokenStore: failed to load secret for \"%s\": %s", qUtf8Printable(key), error->message);
            g_error_free(error);
        }
        return {};
    }

    const QString secret = QString::fromUtf8(password);
    secret_password_free(password);
    return secret;
}

void remove(const QString &key)
{
    const QByteArray keyUtf8 = key.toUtf8();

    GError *error = nullptr;
    secret_password_clear_sync(schema(), nullptr, &error, "key", keyUtf8.constData(), nullptr);
    if (error) {
        qWarning("TokenStore: failed to remove secret for \"%s\": %s", qUtf8Printable(key), error->message);
        g_error_free(error);
    }
}

} // namespace TokenStore
