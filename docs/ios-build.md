# Building Mnemosyne for iOS (planning doc — no working build yet)

This documents the state of iOS support and the path to a first smoke test,
mirroring `docs/android-build.md`'s structure for the equivalent Android work.
**Nothing here has been built or verified yet** — unlike the Android doc, this
is a roadmap, not a confirmed recipe. Root and `src/CMakeLists.txt` now
correctly distinguish `IOS` from `APPLE` (macOS) and from `ANDROID` (so an iOS
cross-compile no longer tries to build the QWidgets desktop UI or
`platform/MacWindowChrome.mm`), and a placeholder `elseif(IOS)` branch exists
in `src/CMakeLists.txt` — but there is no `MnemosyneIOS` app target yet, and no
iOS build of Poppler-Qt6/libzip/freetype exists to link against even if there
were.

## Why a separate dependency build

Same reason as Android: Poppler-Qt6 and libzip (and the freetype they depend
on) have no prebuilt iOS packages, and Homebrew doesn't cross-compile — its
`qt`/`poppler-qt6`/`libzip` formulas only build for the host Mac. They'll need
to be cross-compiled once per iOS target (device `arm64`, simulator — see the
open question below about which simulator architecture) into one prefix,
pointed at via `-DMNEMOSYNE_IOS_DEPS_PREFIX=...` (already wired up in the
top-level `CMakeLists.txt`, parallel to `MNEMOSYNE_ANDROID_DEPS_PREFIX`).

## 1. Toolchain prerequisites

- **Qt-for-iOS kit** — not installed on this machine. Homebrew's `qt` formula
  is host-only. The Android kit was obtained as `qt.qt6.683.android` (see
  `docs/android-build.md`); the iOS equivalent package id would be something
  like `qt.qt6.683.ios`. `aqtinstall` (`brew install aqtinstall`) is the
  Homebrew-available tool for fetching official Qt kits without the full
  Maintenance Tool GUI — matching version 6.8.3 to the Android kit is probably
  right for consistency, though whatever version the desktop Qt is on by the
  time this is picked up should be considered too.
- **Xcode** — already installed on this machine (26.5, iOS SDK 26.5). CMake
  4.4.2 is installed and has native iOS cross-compile support
  (`-DCMAKE_SYSTEM_NAME=iOS`) since 3.14, no separate toolchain file needed
  the way Android needs the NDK's `android.toolchain.cmake`.
- **Open question: which Simulator architecture.** This dev machine is Intel
  (`x86_64`), so testing in Simulator (rather than only targeting a physical
  device) needs an `x86_64` iOS Simulator runtime. Confirm Xcode 26.5 still
  ships one before relying on it — Apple has trimmed Intel Simulator support
  in some recent Xcode versions for other Apple platforms (watchOS/tvOS), and
  it's not yet confirmed here whether iOS Simulator is unaffected.

## 2. Cross-compile the three dependencies (not yet attempted)

Same three libraries, same order, as Android's Stage 2
(`docs/android-build.md`) — freetype 2.13.3, then libzip 1.10.1, then Poppler
24.02.0, freetype first since Poppler depends on it. The NDK's
`android.toolchain.cmake` toolchain file becomes plain
`-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator` (or `iphoneos`
for device) `-DCMAKE_OSX_ARCHITECTURES=x86_64` (or `arm64`), Poppler still
configured through the Qt-for-iOS kit's `qt-cmake` wrapper rather than plain
`cmake`, same as Android.

**Open risk, not yet resolved:** Android's Poppler build auto-selects an
`android` font backend when `CMAKE_SYSTEM_NAME` is `Android` (see
`docs/android-build.md`'s "Poppler 24.02.0" section) — Poppler has no `ios`
equivalent backend. What font backend to configure for an iOS build (likely
`ENABLE_FONTCONFIG=OFF` plus whatever Poppler's fallback is, needs checking
against Poppler's actual CMake options) needs research and iteration, not
assumed. This is the most likely place the Android recipe won't translate
directly.

## 3. Configure and build Mnemosyne's iOS target (not yet possible)

Blocked on both of the above. Once a Qt-for-iOS kit and a cross-compiled
`MNEMOSYNE_IOS_DEPS_PREFIX` exist, the remaining work (see the `elseif(IOS)`
placeholder comment in `src/CMakeLists.txt`) is:

- A real `MnemosyneIOS` executable target in `src/CMakeLists.txt`, reusing
  the same QML file set the Android target uses (`qml/Main.qml`,
  `Theme.qml`, `LibraryScreen.qml`, `PdfReaderScreen.qml`,
  `components/*.qml`, `EpubReaderScreen.qml`, `SettingsScreen.qml`) — all of
  it is portable Qt Quick, confirmed to have no Android-only API.
- A new `quick/main_ios.mm` entry point, structurally mirroring
  `quick/main_android.cpp` (same `QGuiApplication` + `QQmlApplicationEngine`
  + context-property wiring for `BookmarksModel`, `HighlightsModel`,
  `LibraryModel`, `PdfDocumentModel`, `EpubReaderModel`,
  `PdfSelectionController`, `SyncController`, `SmokeTestBridge`) minus
  `AndroidStorageAccess`.
- A new `IOSStorageAccess` bridge (`UIDocumentPickerViewController` + a
  security-scoped bookmark, since iOS apps can't browse arbitrary filesystem
  paths any more than Android's SAF allows) as the document-picker
  replacement.
- A new `Q_OS_IOS` branch in `src/app/GoogleAuth.cpp` (alongside its existing
  `Q_OS_ANDROID` branch) plus a custom URL scheme declared in `Info.plist`
  (`CFBundleURLTypes`) and a small Objective-C++ bridge forwarding the
  redirect into it — mirroring `src/platform/OAuthRedirect_android.{h,cpp}` +
  `android/src/org/mnemosyne/OAuthRedirectActivity.java`. Desktop's loopback
  `QTcpServer` approach (`GoogleAuth.cpp`'s `#else` branch) isn't reused here,
  same as Android didn't reuse it.
  `src/platform/TokenStore_mac.mm`, by contrast, **is** directly reusable
  as-is on iOS — it only calls Keychain Services (`Security/Security.h`),
  the same API on both platforms.
- `qml/EpubReaderScreen.qml` needs real rework, not just a recompile — its
  `QtWebView` import backs onto `WKWebView` on iOS instead of Android's
  `android.webkit.WebView`, and the screen's selection/toolbar logic
  currently assumes Android `WebView`'s native long-press behavior (see the
  comment at the top of that file), which `WKWebView` doesn't replicate the
  same way.
- A CI job (`macos-15-intel` or `macos-latest`), once the above is proven
  locally, building the iOS target and running it in Simulator — parallel to
  the existing `android` job in `.github/workflows/build.yml`.

## Status / next steps

Nothing beyond the `CMakeLists.txt` platform-gating fixes and this planning
doc exists yet. Next concrete step is acquiring the Qt-for-iOS kit and
attempting the freetype → libzip → Poppler cross-compile from section 2,
which is the actual unblocking work — everything in section 3 is downstream
of having something to link against.
