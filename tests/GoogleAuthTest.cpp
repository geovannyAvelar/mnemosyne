#include "app/GoogleAuth.h"
#include "app/IHttpClient.h"
#include "platform/TokenStore.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <memory>

namespace {

// Same key GoogleAuth.cpp uses internally for the stored refresh token (see
// the example in TokenStore.h) -- duplicated here so tests can poke at it
// directly without GoogleAuth exposing internals just for testing.
const auto kRefreshTokenKey = QStringLiteral("GoogleDrive/refreshToken");

// Responds to every POST with a single canned response, ignoring the
// request body -- enough to drive withAccessToken()'s refresh path without
// any real network traffic.
class FakeHttpClient : public IHttpClient
{
public:
    explicit FakeHttpClient(IHttpClient::Response response) : m_response(std::move(response)) {}

    void get(const QString &, const Headers &, Callback callback) override { callback(m_response); }
    void post(const QString &, const Headers &, const QByteArray &, const QString &, Callback callback) override
    {
        callback(m_response);
    }
    void patch(const QString &, const Headers &, const QByteArray &, const QString &, Callback callback) override
    {
        callback(m_response);
    }

private:
    IHttpClient::Response m_response;
};

} // namespace

class GoogleAuthTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void codeVerifierIsUrlSafeAndUnique();
    void codeChallengeMatchesRfc7636Vector();
    void parseTokenResponseReadsSuccessFields();
    void parseTokenResponseReadsErrorField();
    void parseTokenResponseHandlesGarbageBody();
    void emailFromIdTokenReadsClaim();
    void emailFromIdTokenReturnsEmptyForMalformedToken();
    void tokenStoreRoundTrips();

    void isSignedInStaysTrueWhenTheStoredTokenBecomesUnreadable();
    void refreshFailureWithInvalidGrantSignsOut();
    void refreshFailureWithATransientErrorLeavesSignInStateAlone();
};

void GoogleAuthTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("MnemosyneTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MnemosyneTest"));
}

void GoogleAuthTest::codeVerifierIsUrlSafeAndUnique()
{
    const QString a = GoogleAuth::detail::generateRandomUrlSafeString(32);
    const QString b = GoogleAuth::detail::generateRandomUrlSafeString(32);

    QVERIFY(!a.isEmpty());
    QVERIFY(a != b);
    QVERIFY(!a.contains(QLatin1Char('+')));
    QVERIFY(!a.contains(QLatin1Char('/')));
    QVERIFY(!a.contains(QLatin1Char('=')));
}

void GoogleAuthTest::codeChallengeMatchesRfc7636Vector()
{
    // RFC 7636 appendix B example: known verifier -> known S256 challenge.
    const QString verifier = QStringLiteral("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");
    const QString expectedChallenge = QStringLiteral("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");

    QCOMPARE(GoogleAuth::detail::codeChallengeS256(verifier), expectedChallenge);
}

void GoogleAuthTest::parseTokenResponseReadsSuccessFields()
{
    const QByteArray json = R"({
        "access_token": "access-123",
        "refresh_token": "refresh-456",
        "id_token": "id-789",
        "expires_in": 3600
    })";

    const auto parsed = GoogleAuth::detail::parseTokenResponse(json);
    QCOMPARE(parsed.accessToken, QStringLiteral("access-123"));
    QCOMPARE(parsed.refreshToken, QStringLiteral("refresh-456"));
    QCOMPARE(parsed.idToken, QStringLiteral("id-789"));
    QCOMPARE(parsed.expiresInSeconds, 3600);
    QVERIFY(parsed.error.isEmpty());
}

void GoogleAuthTest::parseTokenResponseReadsErrorField()
{
    const QByteArray json = R"({"error": "invalid_grant", "error_description": "Token has been expired or revoked."})";

    const auto parsed = GoogleAuth::detail::parseTokenResponse(json);
    QVERIFY(parsed.accessToken.isEmpty());
    QCOMPARE(parsed.error, QStringLiteral("Token has been expired or revoked."));
    QCOMPARE(parsed.errorCode, QStringLiteral("invalid_grant"));
}

void GoogleAuthTest::parseTokenResponseHandlesGarbageBody()
{
    const auto parsed = GoogleAuth::detail::parseTokenResponse("not json");
    QVERIFY(parsed.accessToken.isEmpty());
    QVERIFY(!parsed.error.isEmpty());
}

namespace {

QString base64Url(const QByteArray &data)
{
    return QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

} // namespace

void GoogleAuthTest::emailFromIdTokenReadsClaim()
{
    const QByteArray header = R"({"alg":"RS256","typ":"JWT"})";
    const QByteArray payload = R"({"email":"reader@example.com","sub":"12345"})";
    const QString token = base64Url(header) + QLatin1Char('.') + base64Url(payload) + QLatin1Char('.') + QStringLiteral("signature");

    QCOMPARE(GoogleAuth::detail::emailFromIdToken(token), QStringLiteral("reader@example.com"));
}

void GoogleAuthTest::emailFromIdTokenReturnsEmptyForMalformedToken()
{
    QVERIFY(GoogleAuth::detail::emailFromIdToken(QStringLiteral("not-a-jwt")).isEmpty());
    QVERIFY(GoogleAuth::detail::emailFromIdToken(QString()).isEmpty());
}

void GoogleAuthTest::tokenStoreRoundTrips()
{
    const QString key = QStringLiteral("MnemosyneTest/tokenStoreRoundTrip");
    TokenStore::remove(key); // in case a previous failed run left this behind

    QVERIFY(TokenStore::load(key).isEmpty());

    TokenStore::save(key, QStringLiteral("secret-value"));
    QCOMPARE(TokenStore::load(key), QStringLiteral("secret-value"));

    TokenStore::save(key, QStringLiteral("replacement-value"));
    QCOMPARE(TokenStore::load(key), QStringLiteral("replacement-value"));

    TokenStore::remove(key);
    QVERIFY(TokenStore::load(key).isEmpty());
}

// Regression coverage for the Linux "signed out after some time" symptom:
// isSignedIn() must not flip to false just because the stored refresh token
// couldn't be read (e.g. a locked libsecret/gnome-keyring collection) --
// only an explicit signOut() or a real "invalid_grant" from Google should
// do that.
void GoogleAuthTest::isSignedInStaysTrueWhenTheStoredTokenBecomesUnreadable()
{
    GoogleAuth::setTokensForTesting(QStringLiteral("access-token"), QStringLiteral("refresh-token"), 3600);
    QVERIFY(GoogleAuth::isSignedIn());

    // Simulate the token becoming transiently unreachable in the OS secret
    // store (what a locked keyring collection looks like from the caller's
    // side: TokenStore::load() just comes back empty) without going through
    // GoogleAuth::signOut().
    TokenStore::remove(kRefreshTokenKey);
    QVERIFY(TokenStore::load(kRefreshTokenKey).isEmpty());

    QVERIFY(GoogleAuth::isSignedIn());

    GoogleAuth::signOut();
    GoogleAuth::setTokensForTesting(QStringLiteral("access-token"), QStringLiteral("refresh-token"), 3600);
}

void GoogleAuthTest::refreshFailureWithInvalidGrantSignsOut()
{
    GoogleAuth::setTokensForTesting(QString(), QStringLiteral("refresh-token"), -3600); // already-expired cache
    QVERIFY(GoogleAuth::isSignedIn());

    GoogleAuth::setHttpClientForTesting(std::make_unique<FakeHttpClient>(IHttpClient::Response{
        400, QByteArray(R"({"error": "invalid_grant", "error_description": "Token has been expired or revoked."})"),
        QString()}));

    bool called = false;
    QString token = QStringLiteral("unset");
    GoogleAuth::withAccessToken([&](const QString &t) {
        called = true;
        token = t;
    });

    QVERIFY(called);
    QVERIFY(token.isEmpty());
    QVERIFY(!GoogleAuth::isSignedIn());

    GoogleAuth::setHttpClientForTesting(nullptr);
    GoogleAuth::setTokensForTesting(QStringLiteral("access-token"), QStringLiteral("refresh-token"), 3600);
}

void GoogleAuthTest::refreshFailureWithATransientErrorLeavesSignInStateAlone()
{
    GoogleAuth::setTokensForTesting(QString(), QStringLiteral("refresh-token"), -3600); // already-expired cache
    QVERIFY(GoogleAuth::isSignedIn());

    // A transport-level failure -- no server involved at all, same shape as
    // "the machine is offline" or "the keyring couldn't be reached to even
    // send the request".
    GoogleAuth::setHttpClientForTesting(
        std::make_unique<FakeHttpClient>(IHttpClient::Response{0, QByteArray(), QStringLiteral("network error")}));

    bool called = false;
    QString token = QStringLiteral("unset");
    GoogleAuth::withAccessToken([&](const QString &t) {
        called = true;
        token = t;
    });

    QVERIFY(called);
    QVERIFY(token.isEmpty());
    QVERIFY(GoogleAuth::isSignedIn());
    QCOMPARE(TokenStore::load(kRefreshTokenKey), QStringLiteral("refresh-token"));

    GoogleAuth::setHttpClientForTesting(nullptr);
    GoogleAuth::setTokensForTesting(QStringLiteral("access-token"), QStringLiteral("refresh-token"), 3600);
}

QTEST_MAIN(GoogleAuthTest)
#include "GoogleAuthTest.moc"
