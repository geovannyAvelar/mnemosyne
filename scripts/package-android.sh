#!/usr/bin/env bash
# Locates the .apk androiddeployqt already produced during the Android
# build (see src/CMakeLists.txt's `if(ANDROID)` block) and copies it to a
# conventional release-asset name, matching package-macos.sh/
# package-windows.sh's precedent for the other platforms.
#
# Debug-signed, not release-signed: adb install (and this repo's whole
# Android testing story so far, see docs/android-build.md) has only ever
# exercised a debug build -- Release produces an unsigned APK that needs a
# real signing keystore this project doesn't have set up yet. That's a
# follow-up, not part of this pass.
set -euo pipefail

BUILD_DIR="${1:-build-android}"
ABI="${2:-x86_64}"

APK_SRC="$BUILD_DIR/src/android-build/build/outputs/apk/debug/android-build-debug.apk"
APK_DEST="$(pwd)/$BUILD_DIR/Mnemosyne-android-$ABI.apk"

if [[ ! -f "$APK_SRC" ]]; then
    echo "error: $APK_SRC not found -- did the Android build/deploy step run?" >&2
    exit 1
fi

echo "==> Copying $APK_SRC -> $APK_DEST"
cp "$APK_SRC" "$APK_DEST"

echo "==> Done: $APK_DEST"
