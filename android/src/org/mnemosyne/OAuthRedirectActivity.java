package org.mnemosyne;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

// Registered in AndroidManifest.xml for the mnemosyne://oauth2redirect
// custom URI scheme -- Google's OAuth consent screen redirects here after
// sign-in, and this Activity's only job is to hand the code/state/error
// query parameters straight back into the native library via JNI, then
// close itself without ever showing anything on screen (no layout is set).
//
// Relies on Mnemosyne's native library already being loaded in this
// process, i.e. that QtActivity started this app normally before the
// browser was opened -- true for the supported flow (app stays backgrounded
// while the browser has focus), not true if Android killed the process for
// memory while the browser tab was open. That gap is accepted, not handled
// here: see GoogleAuth.cpp's startSignInAndroid() comment.
public class OAuthRedirectActivity extends Activity {
    private native void nativeOnRedirect(String code, String state, String error);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        handleIntent(getIntent());
        finish();
    }

    private void handleIntent(Intent intent) {
        Uri uri = intent != null ? intent.getData() : null;
        String code = "";
        String state = "";
        String error = "";
        if (uri != null) {
            String value;
            value = uri.getQueryParameter("code");
            code = value != null ? value : "";
            value = uri.getQueryParameter("state");
            state = value != null ? value : "";
            value = uri.getQueryParameter("error");
            error = value != null ? value : "";
        }
        try {
            nativeOnRedirect(code, state, error);
        } catch (UnsatisfiedLinkError e) {
            // Native library isn't loaded in this process -- the accepted
            // process-death gap above. Nothing to recover to; the pending
            // sign-in callback on the C++ side will simply time out.
        }
    }
}
