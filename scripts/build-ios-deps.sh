#!/usr/bin/env bash
# Cross-compiles freetype, libzip, and Poppler-Qt6 for a single iOS
# architecture into one prefix, for MNEMOSYNE_IOS_DEPS_PREFIX (see
# top-level CMakeLists.txt and docs/ios-build.md). Mirrors
# scripts/build-android-deps.sh's recipe with two iOS-specific changes:
#
# - Static libs (BUILD_SHARED_LIBS=OFF), not shared: iOS app bundles can
#   embed dynamic frameworks, but that needs separate codesigning and an
#   "Embed Frameworks" build phase per library. Static avoids all of that
#   at the cost of a bigger binary -- fine for a single reader app.
# - Poppler's FONT_CONFIGURATION defaults to "fontconfig" for any non-
#   Android, non-Windows target (see poppler's CMakeLists.txt), which
#   would mean cross-compiling fontconfig too. There's no "ios" backend
#   the way there's an "android" one, so this uses "generic" instead.
#   "generic" has no *system* font substitution, but PDFs without their
#   own embedded fonts fall back to the 14 standard PostScript names
#   (Courier/Helvetica/Times/etc.) -- GlobalParams::setupBaseFonts(dir)
#   is the hook every non-fontconfig backend uses to satisfy those from
#   real files on disk, but it needs an actual dir; Mnemosyne bundles the
#   Ghostscript/URW Base35 fonts (resources/pdf-base14-fonts) as that dir
#   and calls setupBaseFonts() itself at startup (IOSStorageAccess's
#   sibling PopplerFontSetup.mm) -- without it, every substitution lookup
#   re-scans a nonexistent directory and logs 14 failures each time,
#   which is slow enough on a real multi-hundred-page book to trip iOS's
#   watchdog. ENABLE_UNSTABLE_API_ABI_HEADERS installs GlobalParams.h
#   (poppler-qt6's public headers don't expose this at all) so that code
#   can call it.
set -euo pipefail

PREFIX="${1:?usage: $0 <install-prefix> <ios-arch> <qt-ios-kit-dir> <qt-host-kit-dir>}"
ARCH="${2:?usage: $0 <install-prefix> <ios-arch> <qt-ios-kit-dir> <qt-host-kit-dir>}"
QT_IOS_KIT="${3:?usage: $0 <install-prefix> <ios-arch> <qt-ios-kit-dir> <qt-host-kit-dir>}"
QT_HOST_KIT="${4:?usage: $0 <install-prefix> <ios-arch> <qt-ios-kit-dir> <qt-host-kit-dir>}"

# arm64 -> device (iphoneos SDK); x86_64/arm64-simulator builds would use
# iphonesimulator instead -- not needed here since the target is a
# physical iPhone.
SYSROOT="iphoneos"
IOS_DEPLOYMENT_TARGET=15.0

FREETYPE_VERSION=2.13.3
LIBZIP_VERSION=1.10.1
POPPLER_VERSION=24.02.0

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

mkdir -p "$PREFIX"

echo "==> Downloading sources"
curl -fsSL "https://download.savannah.gnu.org/releases/freetype/freetype-$FREETYPE_VERSION.tar.xz" \
    -o "$WORK_DIR/freetype.tar.xz"
curl -fsSL "https://libzip.org/download/libzip-$LIBZIP_VERSION.tar.xz" \
    -o "$WORK_DIR/libzip.tar.xz"
curl -fsSL "https://poppler.freedesktop.org/poppler-$POPPLER_VERSION.tar.xz" \
    -o "$WORK_DIR/poppler.tar.xz"

for f in freetype libzip poppler; do
    tar -xf "$WORK_DIR/$f.tar.xz" -C "$WORK_DIR"
done

echo "==> Building freetype $FREETYPE_VERSION"
cmake -S "$WORK_DIR/freetype-$FREETYPE_VERSION" -B "$WORK_DIR/build-freetype" -G Ninja \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT="$SYSROOT" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$IOS_DEPLOYMENT_TARGET" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_SHARED_LIBS=OFF \
    -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_PNG=ON \
    -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_BROTLI=ON -DFT_DISABLE_ZLIB=OFF
cmake --build "$WORK_DIR/build-freetype"
cmake --install "$WORK_DIR/build-freetype"

echo "==> Building libzip $LIBZIP_VERSION"
cmake -S "$WORK_DIR/libzip-$LIBZIP_VERSION" -B "$WORK_DIR/build-libzip" -G Ninja \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT="$SYSROOT" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$IOS_DEPLOYMENT_TARGET" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_GNUTLS=OFF -DENABLE_MBEDTLS=OFF -DENABLE_OPENSSL=OFF -DENABLE_WINDOWS_CRYPTO=OFF \
    -DENABLE_BZIP2=OFF -DENABLE_LZMA=OFF -DENABLE_ZSTD=OFF \
    -DBUILD_TOOLS=OFF -DBUILD_REGRESS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOC=OFF
cmake --build "$WORK_DIR/build-libzip"
cmake --install "$WORK_DIR/build-libzip"

echo "==> Building poppler $POPPLER_VERSION (Qt6 frontend only)"
# freetype must already be installed at $PREFIX before this configure step.
# Configure through the iOS kit's qt-cmake wrapper (Xcode generator by
# default -- overridden to Ninja here to match freetype/libzip and avoid
# a second, unsigned Xcode project for a library that's never run
# directly) so Qt's own iOS toolchain defaults (sysroot, arch, deployment
# target already baked into qt.toolchain.cmake) line up with the app build.
# CMAKE_FIND_ROOT_PATH is needed alongside CMAKE_PREFIX_PATH -- Qt's iOS
# toolchain file sets CMAKE_FIND_ROOT_PATH_MODE_* to ONLY, the same
# restriction the Android NDK toolchain file applies, so freetype's
# just-installed headers/lib in $PREFIX aren't found without it (same
# gotcha as the Android recipe). QT_HOST_PATH points at a same-version
# desktop Qt kit -- a cross-compiled Qt6Config.cmake refuses to
# find_package(Qt6) at all without one, since it needs a native moc/rcc
# to build against (unlike Android, whose NDK toolchain file doesn't
# trigger this particular check).
"$QT_IOS_KIT/bin/qt-cmake" -S "$WORK_DIR/poppler-$POPPLER_VERSION" -B "$WORK_DIR/build-poppler" -G Ninja \
    -DQT_HOST_PATH="$QT_HOST_KIT" -DQT_HOST_PATH_CMAKE_DIR="$QT_HOST_KIT/lib/cmake" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" -DCMAKE_FIND_ROOT_PATH="$PREFIX" \
    -DBUILD_SHARED_LIBS=OFF \
    -DFONT_CONFIGURATION=generic \
    -DENABLE_UNSTABLE_API_ABI_HEADERS=ON \
    -DENABLE_QT5=OFF -DENABLE_QT6=ON \
    -DBUILD_QT6_TESTS=OFF -DBUILD_GTK_TESTS=OFF -DBUILD_CPP_TESTS=OFF -DBUILD_MANUAL_TESTS=OFF \
    -DENABLE_CPP=OFF -DENABLE_GLIB=OFF -DENABLE_GOBJECT_INTROSPECTION=OFF \
    -DENABLE_UTILS=OFF -DENABLE_BOOST=OFF \
    -DENABLE_LIBOPENJPEG=none -DENABLE_DCTDECODER=none -DENABLE_LCMS=OFF \
    -DENABLE_LIBCURL=OFF -DENABLE_LIBTIFF=OFF -DENABLE_NSS3=OFF -DENABLE_GPGME=OFF \
    -DRUN_GPERF_IF_PRESENT=OFF
cmake --build "$WORK_DIR/build-poppler"
cmake --install "$WORK_DIR/build-poppler"

echo "==> Done: $PREFIX"
