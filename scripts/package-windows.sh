#!/usr/bin/env bash
# Run inside an MSYS2 MINGW64 shell.
#
# Bundles Mnemosyne.exe with Qt (windeployqt6) and the remaining MinGW/MSYS2
# DLLs it depends on (Poppler-Qt6, libzip, and their transitive deps), then
# zips it into a portable package. HTML/WebEngine support isn't part of the
# Windows build at all (see MNEMOSYNE_ENABLE_HTML in CMakeLists.txt), so
# there's no Chromium runtime to worry about here, unlike macOS/Linux.
set -euo pipefail

BUILD_DIR="${1:-build}"
STAGE_DIR="$BUILD_DIR/windows-stage"
ZIP_PATH="$(pwd)/$BUILD_DIR/Mnemosyne-windows.zip"

echo "==> Installing a clean copy of the app"
rm -rf "$STAGE_DIR"
cmake --install "$BUILD_DIR" --prefix "$STAGE_DIR"

EXE="$STAGE_DIR/Mnemosyne.exe"
if [[ ! -f "$EXE" ]]; then
    echo "error: $EXE not found after install" >&2
    exit 1
fi

echo "==> Bundling Qt DLLs and plugins"
windeployqt6 --release "$EXE"

if ! command -v ntldd >/dev/null 2>&1; then
    echo "==> Installing ntldd (walks DLL dependencies)"
    pacman -S --noconfirm mingw-w64-x86_64-ntldd
fi

echo "==> Bundling remaining MinGW/MSYS2 DLLs (Poppler, libzip, runtime, ...)"
# ntldd -R prints the full transitive dependency closure. Anything living
# under /mingw64 is either the MinGW runtime or one of our libs — Windows
# system DLLs resolve elsewhere and are left alone. Qt DLLs windeployqt
# already placed are silently skipped via `cp -n`.
#
# ntldd commonly exits non-zero (e.g. an optional dependency it can't
# resolve) even though its output is still useful, and grep exits non-zero
# on no matches — neither should be allowed to abort the script here.
DLL_LIST=$(ntldd -R "$EXE" 2>/dev/null || true)
MATCHED_DLLS=$(printf '%s\n' "$DLL_LIST" | grep -oE '/mingw64/[^ ]+\.dll' || true)
if [[ -n "$MATCHED_DLLS" ]]; then
    printf '%s\n' "$MATCHED_DLLS" | sort -u | while read -r dll; do
        cp -n "$dll" "$STAGE_DIR/"
    done
fi

if ! command -v zip >/dev/null 2>&1; then
    echo "==> Installing zip"
    pacman -S --noconfirm zip
fi

echo "==> Zipping portable package"
rm -f "$ZIP_PATH"
(cd "$STAGE_DIR" && zip -r "$ZIP_PATH" .)

echo "==> Done: $ZIP_PATH"
