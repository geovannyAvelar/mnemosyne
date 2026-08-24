# Google Drive sign-in

Mnemosyne can sync your reading progress through your Google Drive account
instead of (or alongside) a locally-synced folder. Sign-in is one click —
open the **Sync** menu → **Sign in with Google Drive...**, approve access in
the browser tab that opens, and you're done. There's nothing to create or
paste in.

Mnemosyne only ever requests access to its own hidden data folder in your
Drive (the `drive.appdata` scope) plus your email address for display in the
Sync menu — never your other files. Sign out any time from the same menu.

## For maintainers: provisioning the shared OAuth client

The one-click flow above works because Mnemosyne ships with its own Google
OAuth client baked in, so individual users never have to create one. If
you're building Mnemosyne from source for your own use or a fork, and want
Google Drive sign-in to work, you need to provision that client once:

1. Go to the [Google Cloud Console](https://console.cloud.google.com/) and
   create a project (e.g. "Mnemosyne Sync").
2. **APIs & Services → Library** → enable the **Google Drive API**.
3. **APIs & Services → OAuth consent screen** → choose **External**, fill in
   the required app name/support email fields, and add scopes
   `.../auth/drive.appdata`, `openid`, `.../auth/userinfo.email`. Under
   **Test users** (while the app is unverified), add the Google account(s)
   you'll sign in with — Google limits unverified apps to a small test-user
   list, which is fine for personal use; verification is only needed to
   remove that limit for a public audience.
4. **APIs & Services → Credentials → Create Credentials → OAuth client ID**,
   application type **Desktop app**, and create it.
5. Copy the **Client ID** and **Client Secret** into
   [`src/app/GoogleAuth.cpp`](../src/app/GoogleAuth.cpp) as
   `kBundledClientId`/`kBundledClientSecret`, then build normally.

   (Google issues a "secret" even for Desktop app clients, but — unlike a
   server secret — doesn't treat it as confidential; the loopback-redirect +
   PKCE flow is what actually secures the exchange. That's why it's safe to
   compile into a binary everyone shares, per Google's own guidance for
   installed apps.)

If `kBundledClientId` is left empty, the app still builds, but the Sync menu
disables "Sign in with Google Drive..." rather than offering a broken flow.
The same is true on Android — see below.

## Android

The mobile app needs its own, separate OAuth client — the Desktop app
client above won't work there, since Android's redirect flow (a custom
`mnemosyne://oauth2redirect` URI scheme, not a loopback HTTP listener) only
works with Google's **Android** client type, which is tied to a specific
package name and signing certificate.

1. **APIs & Services → Credentials → Create Credentials → OAuth client ID**.
2. Application type: **Android**.
3. Package name: `org.mnemosyne` (must match exactly — this is
   `QT_ANDROID_PACKAGE_NAME` in `src/CMakeLists.txt`).
4. SHA-1 certificate fingerprint: the SHA-1 of whichever keystore signs your
   build (debug keystore for local builds — `keytool -list -v -keystore
   ~/.android/debug.keystore -alias androiddebugkey -storepass android
   -keypass android`; your own release keystore for a signed build).
5. Click **Create**. Google shows a Client ID and no secret at all — the
   Android client type is a public client by design, matching what
   `GoogleAuth.cpp` already expects (it omits `client_secret` from the
   token request whenever none is configured).
6. Copy the **Client ID** into `src/app/GoogleAuth.cpp`'s Android branch of
   `kBundledClientId` (right next to the desktop one, under `#ifdef
   Q_OS_ANDROID`), then rebuild — same one-time step as the desktop client
   above, no in-app credential entry.

Everything else (consent screen setup, test users, the `drive.appdata` +
`openid` + `email` scopes) is shared with the Desktop app client above; add
the same test-user accounts under the same OAuth consent screen rather than
creating a second one.
