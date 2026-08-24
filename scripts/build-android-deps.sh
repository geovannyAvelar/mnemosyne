#!/usr/bin/env bash
# Cross-compiles freetype, libzip, and Poppler-Qt6 for a single Android ABI
# into one prefix, for MNEMOSYNE_ANDROID_DEPS_PREFIX (see top-level
# CMakeLists.txt and docs/android-build.md, which this script is a
# runnable, parameterized version of -- the exact recipe, versions, and
# flags were worked out and verified there first).
#
# Neither Poppler-Qt6 nor libzip have prebuilt Android packages, unlike
# every desktop platform Mnemosyne supports, so this is what CI's android
# job (.github/workflows/build.yml) runs on a cache miss -- see that job
# for the actions/cache wrapping this in normal runs, since a from-scratch
# Poppler build takes real time.
set -euo pipefail

PREFIX="${1:?usage: $0 <install-prefix> <android-abi> <android-platform> <qt-android-kit-dir>}"
ABI="${2:?usage: $0 <install-prefix> <android-abi> <android-platform> <qt-android-kit-dir>}"
PLATFORM="${3:?usage: $0 <install-prefix> <android-abi> <android-platform> <qt-android-kit-dir>}"
QT_ANDROID_KIT="${4:?usage: $0 <install-prefix> <android-abi> <android-platform> <qt-android-kit-dir>}"

: "${ANDROID_NDK_ROOT:?ANDROID_NDK_ROOT must be set}"

FREETYPE_VERSION=2.13.3
LIBZIP_VERSION=1.10.1
POPPLER_VERSION=24.02.0

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake"

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
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="$PLATFORM" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_SHARED_LIBS=ON \
    -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_PNG=ON \
    -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_BROTLI=ON
cmake --build "$WORK_DIR/build-freetype"
cmake --install "$WORK_DIR/build-freetype"

echo "==> Building libzip $LIBZIP_VERSION"
cmake -S "$WORK_DIR/libzip-$LIBZIP_VERSION" -B "$WORK_DIR/build-libzip" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="$PLATFORM" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_SHARED_LIBS=ON \
    -DENABLE_GNUTLS=OFF -DENABLE_MBEDTLS=OFF -DENABLE_OPENSSL=OFF -DENABLE_WINDOWS_CRYPTO=OFF \
    -DENABLE_BZIP2=OFF -DENABLE_LZMA=OFF -DENABLE_ZSTD=OFF \
    -DBUILD_TOOLS=OFF -DBUILD_REGRESS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOC=OFF
cmake --build "$WORK_DIR/build-libzip"
cmake --install "$WORK_DIR/build-libzip"

echo "==> Building poppler $POPPLER_VERSION (Qt6 frontend only)"
# freetype must already be installed at $PREFIX before this configure step
# (poppler's own find_package(Freetype) needs it) -- both CMAKE_PREFIX_PATH
# and CMAKE_FIND_ROOT_PATH point at $PREFIX, since the NDK toolchain file
# restricts find_package/find_library to CMAKE_FIND_ROOT_PATH only.
"$QT_ANDROID_KIT/bin/qt-cmake" -S "$WORK_DIR/poppler-$POPPLER_VERSION" -B "$WORK_DIR/build-poppler" -G Ninja \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" -DCMAKE_FIND_ROOT_PATH="$PREFIX" \
    -DBUILD_SHARED_LIBS=ON \
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
