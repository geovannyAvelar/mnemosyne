#include "GoogleAuth.h"

#include "IHttpClient.h"
#include "platform/TokenStore.h"

#ifdef Q_OS_ANDROID
#include "platform/OAuthRedirect_android.h"
#include <QTimer>
#else
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#endif

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QList>
#include <QObject>
#include <QRandomGenerator>
#include <QSettings>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

namespace GoogleAuth {

namespace {

const auto kRefreshTokenKey = QStringLiteral("GoogleDrive/refreshToken");
const auto kAuthEndpoint = QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth");
const auto kTokenEndpoint = QStringLiteral("https://oauth2.googleapis.com/token");
const auto kScope = QStringLiteral("https://www.googleapis.com/auth/drive.appdata openid email");

#ifdef Q_OS_ANDROID
// Android needs its own, separately-registered OAuth client (Google's
// "Android" client type, tied to this app's package name + signing
// certificate -- see docs/google-drive-setup.md's Android section; the
// desktop client below can't be reused here). Left empty until a
// maintainer provisions one and fills it in, same as desktop's own
// "skip this step, no setup" from-source build.
const auto kBundledClientId = QStringLiteral("");
#else
// Mnemosyne's own Google Cloud "Desktop app" OAuth client, shared by every
// build so users never have to create their own. Fill these in once (see
// docs/google-drive-setup.md) before shipping a release; from-source builds
// that skip this step simply can't offer Google sign-in (hasClientCredentials()
// returns false and the UI hides the option).
const auto kBundledClientId =
    QStringLiteral("226504444824-bdg1t0brf4il13ssqt2t3ulnf6njv6e0.apps.googleusercontent.com");
const auto kBundledClientSecret = QStringLiteral("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");
#endif

struct AccessTokenCache
{
    QString token;
    QDateTime expiry;
};

AccessTokenCache &accessTokenCache()
{
    static AccessTokenCache cache;
    return cache;
}

std::unique_ptr<IHttpClient> &httpClientOverride()
{
    static std::unique_ptr<IHttpClient> override;
    return override;
}

IHttpClient &httpClient()
{
    if (httpClientOverride()) {
        return *httpClientOverride();
    }
    static QtHttpClient defaultClient;
    return defaultClient;
}

QString base64UrlEncode(const QByteArray &data)
{
    return QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QByteArray base64UrlDecode(QString value)
{
    while (value.size() % 4 != 0) {
        value.append(QLatin1Char('='));
    }
    return QByteArray::fromBase64(value.toLatin1(), QByteArray::Base64UrlEncoding);
}

void postForm(const QUrlQuery &body, IHttpClient::Callback callback)
{
    httpClient().post(kTokenEndpoint, {}, body.query(QUrl::FullyEncoded).toUtf8(),
                       QStringLiteral("application/x-www-form-urlencoded"), std::move(callback));
}

void applyTokenResponse(const detail::TokenResponse &parsed)
{
    accessTokenCache().token = parsed.accessToken;
    accessTokenCache().expiry = QDateTime::currentDateTimeUtc().addSecs(qMax(0, parsed.expiresInSeconds - 30));

    // Google only returns a refresh_token on the very first authorization
    // (or when one is re-issued); a plain token refresh usually omits it,
    // so only overwrite what's stored when a new one actually arrives.
    if (!parsed.refreshToken.isEmpty()) {
        TokenStore::save(kRefreshTokenKey, parsed.refreshToken);
    }
    if (!parsed.idToken.isEmpty()) {
        const QString email = detail::emailFromIdToken(parsed.idToken);
        if (!email.isEmpty()) {
            QSettings().setValue(QStringLiteral("GoogleDrive/AccountEmail"), email);
        }
    }
}

} // namespace

bool hasClientCredentials()
{
    return !kBundledClientId.isEmpty();
}

namespace {

QString clientId()
{
    return kBundledClientId;
}

QString clientSecret()
{
#ifdef Q_OS_ANDROID
    return QString(); // Google's "Android" OAuth client type has no secret at all
#else
    return kBundledClientSecret;
#endif
}

// Google's "Android"/"iOS" OAuth client types are public clients with no
// secret at all — omitting the field entirely (rather than sending it
// empty) is the technically correct thing to do for a client type that
// doesn't have one.
void addClientSecretIfPresent(QUrlQuery &body)
{
    const QString secret = clientSecret();
    if (!secret.isEmpty()) {
        body.addQueryItem(QStringLiteral("client_secret"), secret);
    }
}

// Shared by both platforms' startSignIn() once a redirect (loopback HTTP
// request on desktop, OAuthRedirectActivity intent on Android) has actually
// delivered an authorization code.
void exchangeCodeForToken(const QString &code, const QString &verifier, const QString &redirectUri,
                           std::function<void(bool ok, const QString &error)> onDone)
{
    QUrlQuery body;
    body.addQueryItem(QStringLiteral("client_id"), clientId());
    addClientSecretIfPresent(body);
    body.addQueryItem(QStringLiteral("code"), code);
    body.addQueryItem(QStringLiteral("code_verifier"), verifier);
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    body.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);

    postForm(body, [onDone](const IHttpClient::Response &response) {
        const detail::TokenResponse parsed = detail::parseTokenResponse(response.body);
        if (parsed.accessToken.isEmpty()) {
            onDone(false,
                   parsed.error.isEmpty() ? QObject::tr("Google did not return a valid sign-in token.") : parsed.error);
            return;
        }
        applyTokenResponse(parsed);
        onDone(true, QString());
    });
}

} // namespace

bool isSignedIn()
{
    return !TokenStore::load(kRefreshTokenKey).isEmpty();
}

QString accountEmail()
{
    return QSettings().value(QStringLiteral("GoogleDrive/AccountEmail")).toString();
}

void signOut()
{
    TokenStore::remove(kRefreshTokenKey);
    QSettings().remove(QStringLiteral("GoogleDrive/AccountEmail"));
    accessTokenCache() = AccessTokenCache{};
}

#ifdef Q_OS_ANDROID
// Android sandboxes each app in its own process with no way to bind a
// loopback port another app's browser could reach — the desktop flow below
// has no equivalent here. Instead the auth URL's redirect_uri is a custom
// URI scheme (mnemosyne://oauth2redirect) that AndroidManifest.xml
// registers OAuthRedirectActivity for; when the system browser follows the
// redirect, Android launches that Activity, which hands the code/state/
// error straight back into this process via a small JNI bridge (see
// platform/OAuthRedirect_android.{h,cpp}) — no HTTP listener needed. Known
// accepted limitation: if Android kills this app's process for memory
// while the browser tab is open, the pending verifier/state (held only in
// this static, not persisted) is lost and the sign-in silently never
// completes — same category of gap as any other in-memory OAuth state on a
// platform that can reclaim backgrounded processes.
void startSignInAndroid(std::function<void(bool ok, const QString &error)> onDone)
{
    const QString verifier = detail::generateRandomUrlSafeString(32);
    const QString challenge = detail::codeChallengeS256(verifier);
    const QString state = detail::generateRandomUrlSafeString(16);
    const QString redirectUri = QStringLiteral("mnemosyne://oauth2redirect");

    QUrl authUrl(kAuthEndpoint);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), clientId());
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), kScope);
    query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    query.addQueryItem(QStringLiteral("code_challenge"), challenge);
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("state"), state);
    authUrl.setQuery(query);

    // A timer, not just a one-shot callback, so a user who backs out of the
    // browser without completing sign-in doesn't leave onDone dangling
    // forever — there's no cancellation signal to catch that directly.
    auto timeoutTimer = std::make_shared<QTimer>();
    timeoutTimer->setSingleShot(true);

    auto complete = std::make_shared<bool>(false);
    auto finish = [complete, timeoutTimer, onDone](bool ok, const QString &error) {
        if (*complete) {
            return;
        }
        *complete = true;
        timeoutTimer->stop();
        OAuthRedirect::setPendingCallback(nullptr);
        onDone(ok, error);
    };

    OAuthRedirect::setPendingCallback(
        [verifier, state, redirectUri, finish](const QString &code, const QString &returnedState, const QString &error) {
            if (!error.isEmpty()) {
                finish(false, error);
                return;
            }
            if (returnedState != state) {
                finish(false, QObject::tr("Sign-in response failed a security check. Please try again."));
                return;
            }
            if (code.isEmpty()) {
                finish(false, QObject::tr("Sign-in was cancelled."));
                return;
            }
            exchangeCodeForToken(code, verifier, redirectUri, finish);
        });

    QObject::connect(timeoutTimer.get(), &QTimer::timeout, [finish] {
        finish(false, QObject::tr("Sign-in timed out."));
    });
    timeoutTimer->start(10 * 60 * 1000);

    if (!QDesktopServices::openUrl(authUrl)) {
        finish(false, QObject::tr("Could not open a browser for sign-in."));
    }
}
#endif

void startSignIn(std::function<void(bool ok, const QString &error)> onDone)
{
    if (!hasClientCredentials()) {
        onDone(false, QObject::tr("This build wasn't compiled with Google sign-in support."));
        return;
    }

#ifdef Q_OS_ANDROID
    startSignInAndroid(std::move(onDone));
    return;
#else
    const QString verifier = detail::generateRandomUrlSafeString(32);
    const QString challenge = detail::codeChallengeS256(verifier);
    const QString state = detail::generateRandomUrlSafeString(16);

    auto *server = new QTcpServer();
    if (!server->listen(QHostAddress::LocalHost, 0)) {
        onDone(false, QObject::tr("Could not start the local sign-in listener."));
        delete server;
        return;
    }

    const QString redirectUri = QStringLiteral("http://127.0.0.1:%1").arg(server->serverPort());

    QUrl authUrl(kAuthEndpoint);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), clientId());
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), kScope);
    query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    query.addQueryItem(QStringLiteral("code_challenge"), challenge);
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("state"), state);
    authUrl.setQuery(query);

    QObject::connect(
        server, &QTcpServer::newConnection, server,
        [server, verifier, state, redirectUri, onDone]() {
            QTcpSocket *socket = server->nextPendingConnection();

            // Detach the socket from the server before tearing the server
            // down, so the in-flight request can finish on its own.
            socket->setParent(nullptr);

            // The loopback flow only needs to handle one redirect, ever —
            // stop listening as soon as we have a connection so a stray
            // second request (browsers sometimes probe /favicon.ico) can't
            // confuse the flow.
            server->close();
            server->deleteLater();

            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, verifier, state, redirectUri, onDone]() {
                const QByteArray requestLine = socket->readLine();
                const QList<QByteArray> parts = requestLine.split(' ');

                QString code;
                QString returnedState;
                QString errorParam;
                if (parts.size() >= 2) {
                    const QUrl requestUrl(QStringLiteral("http://127.0.0.1") + QString::fromUtf8(parts.at(1)));
                    const QUrlQuery requestQuery(requestUrl);
                    code = requestQuery.queryItemValue(QStringLiteral("code"), QUrl::FullyDecoded);
                    returnedState = requestQuery.queryItemValue(QStringLiteral("state"));
                    errorParam = requestQuery.queryItemValue(QStringLiteral("error"));
                }

                const QByteArray html = "<html><body>You can close this tab and return to Mnemosyne.</body></html>";
                const QByteArray response = "HTTP/1.1 200 OK\r\n"
                                             "Content-Type: text/html; charset=utf-8\r\n"
                                             "Content-Length: "
                    + QByteArray::number(html.size()) + "\r\n"
                    + "Connection: close\r\n\r\n" + html;
                socket->write(response);
                socket->flush();
                socket->disconnectFromHost();

                if (!errorParam.isEmpty()) {
                    onDone(false, errorParam);
                    return;
                }
                if (returnedState != state) {
                    onDone(false, QObject::tr("Sign-in response failed a security check. Please try again."));
                    return;
                }
                if (code.isEmpty()) {
                    onDone(false, QObject::tr("Sign-in was cancelled."));
                    return;
                }

                exchangeCodeForToken(code, verifier, redirectUri, onDone);
            });

            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
        });

    if (!QDesktopServices::openUrl(authUrl)) {
        server->close();
        server->deleteLater();
        onDone(false, QObject::tr("Could not open a browser for sign-in."));
    }
#endif
}

void withAccessToken(std::function<void(const QString &token)> onToken)
{
    if (!isSignedIn()) {
        onToken(QString());
        return;
    }

    if (!accessTokenCache().token.isEmpty() && QDateTime::currentDateTimeUtc() < accessTokenCache().expiry) {
        onToken(accessTokenCache().token);
        return;
    }

    const QString refreshToken = TokenStore::load(kRefreshTokenKey);
    if (refreshToken.isEmpty()) {
        onToken(QString());
        return;
    }

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("client_id"), clientId());
    addClientSecretIfPresent(body);
    body.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));

    postForm(body, [onToken](const IHttpClient::Response &response) {
        const detail::TokenResponse parsed = detail::parseTokenResponse(response.body);
        if (parsed.accessToken.isEmpty()) {
            onToken(QString());
            return;
        }
        applyTokenResponse(parsed);
        onToken(parsed.accessToken);
    });
}

void setHttpClientForTesting(std::unique_ptr<IHttpClient> client)
{
    httpClientOverride() = std::move(client);
}

void setTokensForTesting(const QString &accessToken, const QString &refreshToken, int expiresInSeconds)
{
    accessTokenCache().token = accessToken;
    accessTokenCache().expiry = QDateTime::currentDateTimeUtc().addSecs(expiresInSeconds);
    if (!refreshToken.isEmpty()) {
        TokenStore::save(kRefreshTokenKey, refreshToken);
    }
}

namespace detail {

QString generateRandomUrlSafeString(int byteCount)
{
    QByteArray bytes(byteCount, Qt::Uninitialized);
    for (int i = 0; i < byteCount; ++i) {
        bytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return base64UrlEncode(bytes);
}

QString codeChallengeS256(const QString &codeVerifier)
{
    const QByteArray hash = QCryptographicHash::hash(codeVerifier.toUtf8(), QCryptographicHash::Sha256);
    return base64UrlEncode(hash);
}

TokenResponse parseTokenResponse(const QByteArray &json)
{
    TokenResponse result;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        result.error = QString::fromUtf8(json);
        return result;
    }

    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("error"))) {
        result.error = obj.value(QStringLiteral("error_description")).toString(obj.value(QStringLiteral("error")).toString());
        return result;
    }

    result.accessToken = obj.value(QStringLiteral("access_token")).toString();
    result.refreshToken = obj.value(QStringLiteral("refresh_token")).toString();
    result.idToken = obj.value(QStringLiteral("id_token")).toString();
    result.expiresInSeconds = obj.value(QStringLiteral("expires_in")).toInt();
    return result;
}

QString emailFromIdToken(const QString &idToken)
{
    const QStringList segments = idToken.split(QLatin1Char('.'));
    if (segments.size() != 3) {
        return {};
    }

    const QByteArray payload = base64UrlDecode(segments.at(1));
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }

    return doc.object().value(QStringLiteral("email")).toString();
}

} // namespace detail

} // namespace GoogleAuth
