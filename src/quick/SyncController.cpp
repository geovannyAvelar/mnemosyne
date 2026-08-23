#include "SyncController.h"

#include "app/GoogleAuth.h"

bool SyncController::hasClientCredentials() const
{
    return GoogleAuth::hasClientCredentials();
}

bool SyncController::isSignedIn() const
{
    return GoogleAuth::isSignedIn();
}

QString SyncController::accountEmail() const
{
    return GoogleAuth::accountEmail();
}

void SyncController::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

void SyncController::setSignInInProgress(bool inProgress)
{
    if (m_signInInProgress == inProgress) {
        return;
    }
    m_signInInProgress = inProgress;
    emit signInInProgressChanged();
}

void SyncController::startSignIn()
{
    if (m_signInInProgress) {
        return;
    }
    setLastError(QString());
    setSignInInProgress(true);

    // SyncController is a context property owned for the app's whole
    // lifetime (see main_android.cpp), so capturing `this` raw is safe —
    // it outlives any sign-in attempt.
    GoogleAuth::startSignIn([this](bool ok, const QString &error) {
        setSignInInProgress(false);
        if (!ok) {
            setLastError(error);
        }
        emit signedInChanged();
        emit signInFinished(ok, error);
    });
}

void SyncController::signOut()
{
    GoogleAuth::signOut();
    emit signedInChanged();
}
