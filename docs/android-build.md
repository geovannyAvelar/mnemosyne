# Building Mnemosyne for Android (mobile-port smoke test)

This covers the `MnemosyneAndroid` target added in Stage 2 of the mobile
port: a minimal Qt Quick app that opens a bundled sample PDF through the
unmodified `mnemosynebackend` library (real `Poppler-Qt6`, not a stub) and
displays the result, proving the whole Android cross-compile chain works.
It is **not** the reader UI — that lands in later mobile-port milestones.

## Why a separate dependency build

Poppler-Qt6 and libzip have no prebuilt Android packages, unlike every
desktop platform Mnemosyne supports. They must be cross-compiled once per
Android ABI and pointed at via `-DMNEMOSYNE_ANDROID_DEPS_PREFIX=...`.
`libpoppler-qt6.so`/`libpoppler.so`/`libzip.so`/`libfreetype.so` are **not**
committed to this repo (multi-hundred-MB of ABI-specific binaries); build
them locally following the recipe below, or fetch a build script/CI
artifact once Stage 9 sets one up.

## 1. Toolchain prerequisites

- Qt 6.8.3 Android kit for the ABI you're targeting (`android_arm64_v8a`,
  `android_x86_64`, etc.), installed via the Qt Maintenance Tool alongside
  the desktop kit: `qt.qt6.683.android` (and `qt.qt6.683.addons.qtwebview`
  for the later EPUB milestone).
- Android SDK + NDK. This was set up via the command-line tools, not
  Android Studio:
  ```bash
  sdkmanager --sdk_root=$ANDROID_SDK_ROOT \
    "platform-tools" "platforms;android-35" "build-tools;35.0.1" \
    "ndk;27.2.12479018"
  ```
  NDK 27.2.12479018 is one of the two versions Qt 6.8 supports (the other
  is 26.1.10909125).
- `ANDROID_SDK_ROOT` / `ANDROID_NDK_ROOT` env vars pointing at the above.
- JDK 17+ (JDK 21 confirmed working) on `PATH` as `JAVA_HOME`.

An x86_64 emulator (`google_apis` system image) is enough to test an
`android_x86_64` build locally without a physical device; KVM acceleration
needs `/dev/kvm` and membership in the `kvm` group.

## 2. Cross-compile the three dependencies

All three use CMake with the NDK's toolchain file, installed into one
prefix per ABI (e.g. `~/android-deps/install-x86_64`). Run each `cmake -S
... -B ... && cmake --build ... && cmake --install ...` in order —
freetype and libzip have no interdependency, but poppler needs freetype
already installed at the target prefix.

**Freetype 2.13.3** — minimal build (no HarfBuzz/PNG/Bzip2/Brotli; the
NDK sysroot's `libz` covers zlib):
```bash
cmake -S freetype-2.13.3 -B build/freetype-$ABI -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PREFIX \
  -DBUILD_SHARED_LIBS=ON \
  -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_PNG=ON \
  -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_BROTLI=ON
```

**libzip 1.10.1** — AES/encrypted-zip support disabled (Mnemosyne's EPUB
reader doesn't need it), all optional compression backends off:
```bash
cmake -S libzip-1.10.1 -B build/libzip-$ABI -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PREFIX \
  -DBUILD_SHARED_LIBS=ON \
  -DENABLE_GNUTLS=OFF -DENABLE_MBEDTLS=OFF -DENABLE_OPENSSL=OFF -DENABLE_WINDOWS_CRYPTO=OFF \
  -DENABLE_BZIP2=OFF -DENABLE_LZMA=OFF -DENABLE_ZSTD=OFF \
  -DBUILD_TOOLS=OFF -DBUILD_REGRESS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOC=OFF
```

**Poppler 24.02.0** — Qt6 frontend only (no Qt5, glib, cpp wrapper, cairo,
or command-line utils); JPEG/JPX/LCMS/TIFF/NSS/GPGME/curl all disabled to
avoid cross-compiling further dependencies the smoke test doesn't need
(real PDFs using those features will fail to fully render until a later
pass adds them back). Font configuration auto-selects Poppler's own
`android` backend when `CMAKE_SYSTEM_NAME` is `Android` — no fontconfig
needed. Configure with the target ABI's `qt-cmake` wrapper (not plain
`cmake`) so Qt's own Android toolchain defaults line up, and add the
freetype prefix to **both** `CMAKE_PREFIX_PATH` and `CMAKE_FIND_ROOT_PATH`
— the NDK toolchain file restricts `find_package`/`find_library` to
`CMAKE_FIND_ROOT_PATH` only, so `CMAKE_PREFIX_PATH` alone isn't enough:
```bash
$QT_ANDROID_KIT/bin/qt-cmake -S poppler-24.02.0 -B build/poppler-$ABI -G Ninja \
  -DANDROID_SDK_ROOT=$ANDROID_SDK_ROOT -DANDROID_NDK_ROOT=$ANDROID_NDK_ROOT \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PREFIX \
  -DCMAKE_PREFIX_PATH=$PREFIX -DCMAKE_FIND_ROOT_PATH=$PREFIX \
  -DBUILD_SHARED_LIBS=ON \
  -DENABLE_QT5=OFF -DENABLE_QT6=ON \
  -DBUILD_QT6_TESTS=OFF -DBUILD_GTK_TESTS=OFF -DBUILD_CPP_TESTS=OFF -DBUILD_MANUAL_TESTS=OFF \
  -DENABLE_CPP=OFF -DENABLE_GLIB=OFF -DENABLE_GOBJECT_INTROSPECTION=OFF \
  -DENABLE_UTILS=OFF -DENABLE_BOOST=OFF \
  -DENABLE_LIBOPENJPEG=none -DENABLE_DCTDECODER=none -DENABLE_LCMS=OFF \
  -DENABLE_LIBCURL=OFF -DENABLE_LIBTIFF=OFF -DENABLE_NSS3=OFF -DENABLE_GPGME=OFF \
  -DRUN_GPERF_IF_PRESENT=OFF
```

## 3. Configure and build Mnemosyne's Android target

```bash
$QT_ANDROID_KIT/bin/qt-cmake -S . -B build-android-$ABI -G Ninja \
  -DANDROID_SDK_ROOT=$ANDROID_SDK_ROOT -DANDROID_NDK_ROOT=$ANDROID_NDK_ROOT \
  -DQT_ANDROID_BUILD_ALL_ABIS=OFF \
  -DMNEMOSYNE_ANDROID_DEPS_PREFIX=$PREFIX \
  -DCMAKE_BUILD_TYPE=Debug   # Debug auto-signs with the Gradle debug keystore;
                             # Release produces an unsigned APK adb can't install
cmake --build build-android-$ABI
```

The `MnemosyneAndroid` target (`src/CMakeLists.txt`, `if(ANDROID)` block)
links `mnemosynebackend` + `Qt6::Quick`, and declares the four vendored
`.so`s via `QT_ANDROID_EXTRA_LIBS` — without that property androiddeployqt
only bundles Qt's own libraries and the app dies at launch with
`UnsatisfiedLinkError: library "libpoppler-qt6.so" not found`.

Install and run:
```bash
adb install -r build-android-$ABI/src/android-build/build/outputs/apk/debug/android-build-debug.apk
adb shell am start -n org.qtproject.example.MnemosyneAndroid/org.qtproject.qt.android.bindings.QtActivity
```

A successful run shows `OK — <N> pages, title "...", page 0 rendered
WxH`. `FAILED: ...` means `openDocument()` or rendering itself failed —
check `adb logcat`.

## Known gotchas

- **`qt_standard_project_setup()` must run before any `qt_add_qml_module`
  call**, or the QML module's resource prefix falls back to the pre-6.8
  default and the app crashes at runtime with `Module "X" contains no type
  named "Main"` — it builds and installs fine, so this only shows up at
  launch. Mnemosyne's top-level `CMakeLists.txt` already calls it
  unconditionally, and the Android target additionally sets
  `qt_policy(SET QTP0001 NEW)` explicitly before its own
  `qt_add_qml_module` call as a second guard.
- **A cold-launch crash with `FORTIFY: pthread_mutex_lock called on a
  destroyed mutex` in Android's own `hwuiTask0` thread** is a known
  OS/emulator race (Google issuetracker #279250953), not app-caused — if a
  fresh install briefly fails to display, just relaunch.
- **Resource files referenced by absolute path** (e.g. `qt_add_resources`
  pointing at `tests/fixtures/...`) need an explicit `QT_RESOURCE_ALIAS`
  source property, or configure fails with "specified with an absolute
  path and is used in a Qt resource."

## Status / next steps

Built and verified on `android_x86_64` only (this dev machine has no
arm64 device/emulator). `arm64-v8a` — the real target for physical
devices — needs the same three-dependency cross-compile repeated with
`-DANDROID_ABI=arm64-v8a`, not yet done. See the mobile-port plan for the
full staged roadmap beyond this smoke test.
