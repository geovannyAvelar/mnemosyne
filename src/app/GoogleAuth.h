#pragma once

#include <QByteArray>
#include <QString>

#include <functional>
#include <memory>

class QWidget;
class IHttpClient;

// OAuth 2.0 Authorization Code + PKCE sign-in for a single Google account,
// using the loopback-redirect flow Google requires for installed apps (a
// system-browser consent screen + a short-lived local HTTP listener — no
// embedded webview). The refresh token is the only long-lived secret and is
// stored via TokenStore (a real OS secret store); the access token lives in
// memory only and is refreshed on demand via withAccessToken().
//
// Mnemosyne ships its own OAuth client (see kBundledClientId/Secret in
// GoogleAuth.cpp) so users can just click "Sign in with Google Drive" with
// no setup of their own. Google treats an installed app's client "secret"
// as a public identifier rather than a confidential value — the loopback
// redirect + PKCE flow below is what actually secures the exchange — so
// baking it into the app is the model Google documents for desktop/mobile
// clients. See docs/google-drive-setup.md for maintainer notes on
// provisioning that client.
namespace GoogleAuth {

// True once the app has a usable Client ID compiled in (i.e. a maintainer
// has configured kBundledClientId in GoogleAuth.cpp). False only in
// from-source builds where that step was skipped.
bool hasClientCredentials();

bool isSignedIn();
QString accountEmail(); // cached from the last sign-in; empty if never signed in

// Opens the system browser for consent. onDone is invoked once (success or
// failure/cancellation) with a human-readable error on failure.
void startSignIn(QWidget *parent, std::function<void(bool ok, const QString &error)> onDone);

void signOut();

// Every Drive API call goes through this: returns a currently-valid access
// token, refreshing it first if necessary. Calls onToken(QString()) if not
// signed in or the refresh fails.
void withAccessToken(std::function<void(const QString &token)> onToken);

// Replaces the QNetworkAccessManager-backed HTTP layer with a test double.
// Passing nullptr restores the default.
void setHttpClientForTesting(std::unique_ptr<IHttpClient> client);

// Lets tests exercise withAccessToken()/GoogleDriveSync without driving an
// actual browser-based sign-in.
void setTokensForTesting(const QString &accessToken, const QString &refreshToken, int expiresInSeconds);

// Pure helpers factored out of the stateful flow above so they can be unit
// tested directly.
namespace detail {

QString generateRandomUrlSafeString(int byteCount);
QString codeChallengeS256(const QString &codeVerifier);

struct TokenResponse
{
    QString accessToken;
    QString refreshToken; // empty on a refresh response — Google only returns it once
    QString idToken;
    int expiresInSeconds = 0;
    QString error;
};

TokenResponse parseTokenResponse(const QByteArray &json);

// "email" claim from a Google ID token's (unverified, base64url-decoded)
// payload segment. Safe here only because the token just arrived directly
// from Google over TLS in the same request that produced it.
QString emailFromIdToken(const QString &idToken);

} // namespace detail

} // namespace GoogleAuth
