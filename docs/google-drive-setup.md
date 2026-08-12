# Setting up Google Drive sign-in

Mnemosyne can sync your reading progress through your own Google Drive
account instead of (or alongside) a locally-synced folder. Mnemosyne ships
no Google credentials of its own — you create a small, free OAuth client in
your own Google Cloud project and paste its Client ID/Secret into the app
once. This takes about five minutes.

Mnemosyne only ever requests access to its own hidden data folder in your
Drive (the `drive.appdata` scope) plus your email address for display in
the Sync menu — never your other files.

## 1. Create a Google Cloud project

1. Go to the [Google Cloud Console](https://console.cloud.google.com/).
2. Create a new project (or reuse an existing personal one). Any name is
   fine, e.g. "Mnemosyne Sync".

## 2. Enable the Drive API

1. In the project, go to **APIs & Services → Library**.
2. Search for **Google Drive API** and click **Enable**.

## 3. Configure the OAuth consent screen

1. Go to **APIs & Services → OAuth consent screen**.
2. Choose **External** (unless you have a Google Workspace org and want
   **Internal**), and fill in the required app name/support email fields.
3. Add scopes: `.../auth/drive.appdata`, `openid`, and `.../auth/userinfo.email`.
4. Under **Test users** (while the app is unverified), add the Google
   account(s) you'll actually sign in with in Mnemosyne. Google limits
   unverified apps to a small list of test users, which is fine for
   personal/family use — verification is only needed to remove that limit
   for a public audience.

## 4. Create the OAuth client

1. Go to **APIs & Services → Credentials → Create Credentials → OAuth
   client ID**.
2. Application type: **Desktop app**.
3. Give it a name (e.g. "Mnemosyne Desktop") and click **Create**.
4. Copy the **Client ID** and **Client Secret** shown — you'll paste both
   into Mnemosyne next. (Google issues a secret even for Desktop app
   clients, but doesn't treat it as confidential the way a server secret
   would be — it's fine to store it locally.)

## 5. Enter the credentials in Mnemosyne

1. In Mnemosyne, open the **Sync** menu → **Sign in with Google Drive...**.
2. The first time, you'll be prompted to paste the Client ID and Client
   Secret from step 4.
3. Mnemosyne opens your system browser to the Google consent screen. Sign
   in and approve access.
4. Once approved, the browser tab tells you to return to Mnemosyne, and the
   Sync menu shows "Google Drive: signed in as \<your email\>".

From then on, reading progress is synced through your Drive's hidden app
data folder automatically. Sign out any time from the same menu.
