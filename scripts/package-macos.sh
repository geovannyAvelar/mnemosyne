#!/usr/bin/env bash
# Builds the Mnemosyne.app in BUILD_DIR into a redistributable .dmg.
#
# The app links against Homebrew-installed Qt, Poppler-Qt6, and libzip, none
# of which exist on a user's machine. This script installs a clean copy of
# the .app (with its rpath already pointed at Contents/Frameworks, see
# src/CMakeLists.txt), bundles in Qt (macdeployqt) and the remaining
# Homebrew dylibs (dylibbundler), then wraps it in a .dmg via hdiutil.
#
# cpack's DragNDrop generator is intentionally not used here: it re-runs the
# same install-time rpath rewrite that dylibbundler already performed,
# which fails because the "old" rpath it expects to find is gone by then.
set -euo pipefail

BUILD_DIR="${1:-build}"
STAGE_DIR="$BUILD_DIR/macos-stage"
ARCH="$(uname -m)"
DMG_PATH="$BUILD_DIR/Mnemosyne-macos-$ARCH.dmg"

echo "==> Installing a clean copy of the app"
rm -rf "$STAGE_DIR"
cmake --install "$BUILD_DIR" --prefix "$STAGE_DIR"

APP_BUNDLE="$STAGE_DIR/Mnemosyne.app"
if [[ ! -d "$APP_BUNDLE" ]]; then
    echo "error: $APP_BUNDLE not found after install" >&2
    exit 1
fi

QT_PREFIX="$(brew --prefix qt)"

echo "==> Bundling Qt frameworks and plugins"
"$QT_PREFIX/bin/macdeployqt" "$APP_BUNDLE"

if ! command -v dylibbundler >/dev/null 2>&1; then
    echo "==> Installing dylibbundler (bundles non-Qt dylibs: Poppler, libzip, ...)"
    brew install dylibbundler
fi

echo "==> Bundling non-Qt dylibs"
# -of/-cd (not -od): -od *erases* Contents/Frameworks first, which would
# wipe out everything macdeployqt just copied in.
dylibbundler -of -cd -b \
    -x "$APP_BUNDLE/Contents/MacOS/Mnemosyne" \
    -d "$APP_BUNDLE/Contents/Frameworks" \
    -p "@executable_path/../Frameworks"

echo "==> Building .dmg"
rm -f "$DMG_PATH"

# hdiutil create briefly mounts the image under this exact volume name to
# populate it -- a volume of the same name left mounted from an earlier
# run (a previous CI attempt that didn't get to unmount it, or a developer
# testing this script locally with the .dmg still open in Finder) makes it
# fail with "Resource busy" despite the stale DMG *file* itself already
# being removed above. Detaching it first is a no-op when nothing's
# mounted (hence the || true).
hdiutil detach "/Volumes/Mnemosyne" -quiet 2>/dev/null || true

# hdiutil create is known to intermittently fail with "Resource busy" on
# CI runners even with no genuinely stale mount (diskarbitrationd/mdworker
# transiently holding a lock) -- retrying a couple of times before giving
# up is the standard workaround, cheaper than chasing a root cause that's
# often just runner-environment timing.
for attempt in 1 2 3; do
    if hdiutil create -volname Mnemosyne -srcfolder "$STAGE_DIR" -ov -format UDZO "$DMG_PATH"; then
        break
    fi
    if [[ "$attempt" == 3 ]]; then
        echo "error: hdiutil create failed after 3 attempts" >&2
        exit 1
    fi
    echo "==> hdiutil create failed (attempt $attempt/3), retrying..." >&2
    sleep 5
done

echo "==> Done: $DMG_PATH"
