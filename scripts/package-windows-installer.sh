#!/usr/bin/env bash
# Run inside an MSYS2 MINGW64 or CLANGARM64 shell, after
# scripts/package-windows.sh has already staged a working copy of the app
# (Mnemosyne.exe + Qt + MinGW/MSYS2 DLLs) — this script doesn't restage
# anything, it just wraps that existing staging directory into a proper
# installer via NSIS (Start Menu shortcut, optional Desktop shortcut, and a
# real uninstaller in Add/Remove Programs).
set -euo pipefail

case "${MSYSTEM:-}" in
    MINGW64) ARCH=x86_64 ;;
    CLANGARM64) ARCH=arm64 ;;
    *) echo "error: unsupported MSYSTEM '${MSYSTEM:-}' (expected MINGW64 or CLANGARM64)" >&2; exit 1 ;;
esac

BUILD_DIR="${1:-build}"
STAGE_DIR="$(pwd)/$BUILD_DIR/windows-stage"
OUT_FILE="$(pwd)/$BUILD_DIR/Mnemosyne-Setup-$ARCH.exe"

if [[ ! -f "$STAGE_DIR/Mnemosyne.exe" ]]; then
    echo "error: $STAGE_DIR/Mnemosyne.exe not found — run scripts/package-windows.sh first" >&2
    exit 1
fi

VERSION=$(grep -oE 'VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | head -1 | awk '{print $2}')
if [[ -z "$VERSION" ]]; then
    echo "error: couldn't read the project version from CMakeLists.txt" >&2
    exit 1
fi

# NSIS has no CLANGARM64 build in MSYS2, so makensis always comes from the
# MINGW64 environment — it's a build-time packaging tool only, doesn't need
# to match the target arch, and runs fine under Windows 11 ARM's x64
# emulation. It won't be on PATH in a CLANGARM64 shell, so call it by full
# path rather than relying on `command -v`.
MAKENSIS=/mingw64/bin/makensis
if [[ ! -x "$MAKENSIS" ]]; then
    echo "==> Installing NSIS"
    pacman -S --noconfirm mingw-w64-x86_64-nsis
fi

echo "==> Building installer for version $VERSION"
# NSIS's /r "STAGE_DIR\*.*" needs a native Windows path, not the POSIX one
# this MSYS2 shell otherwise uses everywhere else.
STAGE_DIR_WIN=$(cygpath -w "$STAGE_DIR")
OUT_FILE_WIN=$(cygpath -w "$OUT_FILE")

# makensis resolves its plugin dirs (nsDialogs etc., used by MultiUser.nsh)
# from NSISDIR. mingw-w64-nsis 3.13-2 started failing with "Plugin not found,
# cannot call nsDialogs::Create" / empty "Plugin directories:", so point it at
# the install explicitly (Windows-style path, since makensis is a native exe)
# and list what's there to make any future breakage diagnosable from the log.
NSISDIR_POSIX=/mingw64/share/nsis
export NSISDIR=$(cygpath -w "$NSISDIR_POSIX")
echo "==> NSISDIR=$NSISDIR"
ls "$NSISDIR_POSIX/Plugins" "$NSISDIR_POSIX"/Plugins/* 2>&1 | head -30

"$MAKENSIS" "-DSTAGE_DIR=$STAGE_DIR_WIN" "-DVERSION=$VERSION" "-DOUT_FILE=$OUT_FILE_WIN" \
    scripts/windows-installer.nsi

echo "==> Done: $OUT_FILE"
