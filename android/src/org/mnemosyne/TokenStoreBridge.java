package org.mnemosyne;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import java.security.Key;
import java.security.KeyStore;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

// Called from src/platform/TokenStore_android.cpp to implement the same
// three-function TokenStore contract every other platform gets from a real
// OS secret store (macOS Keychain, Windows Credential Manager, Linux Secret
// Service). Android has no equivalent user-facing secret store API, so this
// rolls its own: an AES-256-GCM key generated inside the hardware-backed
// Android Keystore (the key material itself never leaves the Keystore --
// only Cipher.doFinal() output crosses back into this class) encrypts the
// refresh token, and the ciphertext + IV are persisted as base64 strings in
// an ordinary SharedPreferences file. Stolen ciphertext bytes alone are
// useless off this device.
public final class TokenStoreBridge {
    private static final String KEY_ALIAS = "mnemosyne_token_store_key";
    private static final String PREFS_NAME = "mnemosyne_token_store";
    private static final int GCM_TAG_BITS = 128;

    private TokenStoreBridge() {}

    private static Key getOrCreateKey() throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);

        Key existing = keyStore.getKey(KEY_ALIAS, null);
        if (existing != null) {
            return existing;
        }

        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        KeyGenParameterSpec spec =
                new KeyGenParameterSpec.Builder(KEY_ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                        .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                        .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                        .setKeySize(256)
                        .build();
        generator.init(spec);
        return generator.generateKey();
    }

    public static void save(Context context, String key, String secret) {
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, getOrCreateKey());
            byte[] ciphertext = cipher.doFinal(secret.getBytes("UTF-8"));
            byte[] iv = cipher.getIV();

            SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
            prefs.edit()
                    .putString(key + ".iv", Base64.encodeToString(iv, Base64.NO_WRAP))
                    .putString(key + ".ct", Base64.encodeToString(ciphertext, Base64.NO_WRAP))
                    .apply();
        } catch (Exception e) {
            // Best-effort, matching every other TokenStore backend's
            // contract: a failed save just means the next isSignedIn()
            // check reports signed-out, not a crash.
        }
    }

    public static String load(Context context, String key) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        String ivB64 = prefs.getString(key + ".iv", null);
        String ctB64 = prefs.getString(key + ".ct", null);
        if (ivB64 == null || ctB64 == null) {
            return "";
        }
        try {
            byte[] iv = Base64.decode(ivB64, Base64.NO_WRAP);
            byte[] ciphertext = Base64.decode(ctB64, Base64.NO_WRAP);
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, getOrCreateKey(), new GCMParameterSpec(GCM_TAG_BITS, iv));
            return new String(cipher.doFinal(ciphertext), "UTF-8");
        } catch (Exception e) {
            // Corrupted entry, or the Keystore key became unusable (e.g.
            // the device's lock-screen credential was removed, which
            // invalidates keys generated with a lock-bound spec -- this one
            // isn't lock-bound, but treat any failure the same way): a
            // refresh token that can't be decrypted is exactly as useless
            // as one that's absent, so report it as absent rather than
            // propagating an exception across the JNI boundary.
            return "";
        }
    }

    public static void remove(Context context, String key) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        prefs.edit().remove(key + ".iv").remove(key + ".ct").apply();
    }
}
