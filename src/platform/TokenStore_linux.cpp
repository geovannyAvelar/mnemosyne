// libsecret pulls in glib/gio headers (gdbusintrospection.h) that use
// "signals" as a plain struct field name. Qt's <QString> defines "signals"
// as a macro (part of the signals/slots/emit convenience keywords), so
// libsecret must be included before any Qt header pulls that macro in, or
// the glib header fails to parse.
#include <libsecret/secret.h>

#include "TokenStore.h"

#include <QByteArray>

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

    secret_password_store_sync(schema(), SECRET_COLLECTION_DEFAULT, "Mnemosyne token", secretUtf8.constData(),
                                nullptr, nullptr, "key", keyUtf8.constData(), nullptr);
}

QString load(const QString &key)
{
    const QByteArray keyUtf8 = key.toUtf8();

    gchar *password = secret_password_lookup_sync(schema(), nullptr, nullptr, "key", keyUtf8.constData(), nullptr);
    if (!password) {
        return {};
    }

    const QString secret = QString::fromUtf8(password);
    secret_password_free(password);
    return secret;
}

void remove(const QString &key)
{
    const QByteArray keyUtf8 = key.toUtf8();
    secret_password_clear_sync(schema(), nullptr, nullptr, "key", keyUtf8.constData(), nullptr);
}

} // namespace TokenStore
