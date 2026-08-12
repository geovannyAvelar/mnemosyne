#include "app/GoogleAuth.h"
#include "platform/TokenStore.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

class GoogleAuthTest : public QObject
{
    Q_OBJECT

private slots:
    void codeVerifierIsUrlSafeAndUnique();
    void codeChallengeMatchesRfc7636Vector();
    void parseTokenResponseReadsSuccessFields();
    void parseTokenResponseReadsErrorField();
    void parseTokenResponseHandlesGarbageBody();
    void emailFromIdTokenReadsClaim();
    void emailFromIdTokenReturnsEmptyForMalformedToken();
    void tokenStoreRoundTrips();
};

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

QTEST_MAIN(GoogleAuthTest)
#include "GoogleAuthTest.moc"
