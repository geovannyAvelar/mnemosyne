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
