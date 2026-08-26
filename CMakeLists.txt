cmake_minimum_required(VERSION 3.20)
project(Mnemosyne LANGUAGES CXX VERSION 1.6.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(APPLE AND NOT IOS)
  # Needed to compile platform/MacWindowChrome.mm (native title-bar integration).
  enable_language(OBJCXX)

  # Homebrew keg-only packages aren't on the default prefix path. Cover both
  # the Intel (/usr/local) and Apple Silicon (/opt/homebrew) Homebrew prefixes,
  # since only one will exist on any given machine. Homebrew is host-only —
  # it has no iOS cross-compiled packages, so this is skipped for IOS; see
  # MNEMOSYNE_IOS_DEPS_PREFIX below instead.
  foreach(_brew_prefix /usr/local/opt /opt/homebrew/opt)
    list(APPEND CMAKE_PREFIX_PATH ${_brew_prefix}/qt)
    list(APPEND CMAKE_PREFIX_PATH ${_brew_prefix}/poppler-qt6)
    list(APPEND CMAKE_PREFIX_PATH ${_brew_prefix}/libzip)
  endforeach()
endif()

# QtWebEngine (Chromium) only ships prebuilt binaries for macOS/Linux; on
# Windows it's source-build-only (multi-hour, MSVC-specific), so HTML support
# is compiled out of the Windows build. It has no Android or iOS build at
# all either. PDF and EPUB are unaffected.
if(WIN32 OR ANDROID OR IOS)
  set(MNEMOSYNE_ENABLE_HTML_DEFAULT OFF)
else()
  set(MNEMOSYNE_ENABLE_HTML_DEFAULT ON)
endif()
option(MNEMOSYNE_ENABLE_HTML "Enable Qt WebEngine-based HTML/HTM support" ${MNEMOSYNE_ENABLE_HTML_DEFAULT})

# Google Drive sign-in as a second, optional sync backend alongside the
# local-folder one (see src/app/GoogleDriveSync.*). Defaults on everywhere,
# including Android as of the mobile port's Stage 8 (custom-scheme
# OAuth-redirect Activity + a Keystore-backed TokenStore_android.cpp — see
# platform/OAuthRedirect_android.* and platform/TokenStore_android.cpp).
set(MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC_DEFAULT ON)
option(MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC "Enable signing in with Google Drive as a sync backend" ${MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC_DEFAULT})

if(ANDROID OR IOS)
  find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Concurrent Quick QuickControls2 WebView)
elseif(MNEMOSYNE_ENABLE_HTML)
  # Quick is needed on desktop too, despite the desktop app itself being
  # QWidget-only: mnemosynequick (src/quick/*, see src/CMakeLists.txt) is
  # built on every platform so its logic gets real desktop Qt Test coverage
  # in CI rather than only ever running on Android — see Stage 9 of the
  # mobile-port plan.
  find_package(Qt6 REQUIRED COMPONENTS Widgets Test Network Concurrent WebEngineWidgets Quick)
else()
  find_package(Qt6 REQUIRED COMPONENTS Widgets Test Network Concurrent Quick)
endif()

find_package(PkgConfig REQUIRED)

if(ANDROID)
  # Poppler-Qt6 and libzip have no prebuilt Android packages, so they're
  # cross-compiled separately (see docs/android-build.md) into one prefix.
  # PKG_CONFIG_LIBDIR is overridden (not appended to PKG_CONFIG_PATH) so
  # pkg-config resolves *only* against the vendored Android build products,
  # never against a same-named host poppler-qt6/libzip.pc that may also be
  # installed on this machine.
  set(MNEMOSYNE_ANDROID_DEPS_PREFIX "" CACHE PATH
      "Prefix containing cross-compiled Poppler-Qt6/libzip/Freetype for this Android ABI")
  if(NOT MNEMOSYNE_ANDROID_DEPS_PREFIX)
    message(FATAL_ERROR "Set -DMNEMOSYNE_ANDROID_DEPS_PREFIX=/path/to/android-deps/install-<abi> "
                         "(see docs/android-build.md)")
  endif()
  set(ENV{PKG_CONFIG_LIBDIR} "${MNEMOSYNE_ANDROID_DEPS_PREFIX}/lib/pkgconfig")
  set(ENV{PKG_CONFIG_PATH} "")
  set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
elseif(IOS)
  # Same situation as Android: Poppler-Qt6/libzip/freetype have no iOS
  # build and no Homebrew iOS cross-compile path, so they're cross-compiled
  # separately (see docs/ios-build.md) into one prefix per architecture.
  set(MNEMOSYNE_IOS_DEPS_PREFIX "" CACHE PATH
      "Prefix containing cross-compiled Poppler-Qt6/libzip/Freetype for this iOS target")
  if(NOT MNEMOSYNE_IOS_DEPS_PREFIX)
    message(FATAL_ERROR "Set -DMNEMOSYNE_IOS_DEPS_PREFIX=/path/to/ios-deps/install-<arch> "
                         "(see docs/ios-build.md)")
  endif()
  set(ENV{PKG_CONFIG_LIBDIR} "${MNEMOSYNE_IOS_DEPS_PREFIX}/lib/pkgconfig")
  set(ENV{PKG_CONFIG_PATH} "")
  set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
endif()

pkg_check_modules(POPPLER_QT6 REQUIRED IMPORTED_TARGET poppler-qt6)
pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)

# libmobi has no prebuilt Android or iOS package and isn't part of either
# cross-compiled deps prefix (see docs/android-build.md, docs/ios-build.md)
# -- MOBI/AZW support is desktop-only for now (mobi/MobiDocument.* is
# excluded from mnemosynecore on Android/iOS too, see src/CMakeLists.txt).
if(NOT ANDROID AND NOT IOS)
  pkg_check_modules(LIBMOBI REQUIRED IMPORTED_TARGET libmobi)
endif()

if(MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC AND UNIX AND NOT APPLE AND NOT ANDROID)
  # OAuth refresh tokens need a real OS secret store on every platform: macOS
  # uses Keychain (Security.framework, already linked), Windows uses
  # Credential Manager (wincred.h, no extra dependency); Linux uses the
  # Secret Service API via libsecret.
  pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
endif()

qt_standard_project_setup()

add_subdirectory(src)

if(NOT ANDROID AND NOT IOS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(UNIX AND NOT APPLE)
  # Appended to the .deb's version only (not PROJECT_VERSION, so
  # MNEMOSYNE_VERSION / the About dialog stay untouched) -- lets the
  # unstable-release pipeline (see .github/workflows/unstable.yml) give each
  # rebuild a distinct, monotonically increasing version, e.g.
  # "+unstable.20260824180000.abc1234". Without this, every unstable build
  # would carry the same version as the last tagged release, and apt would
  # never see a newer version to upgrade to.
  set(MNEMOSYNE_PACKAGE_VERSION_SUFFIX "" CACHE STRING
      "Appended to CPACK_PACKAGE_VERSION, e.g. for an unstable/rolling build")

  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_NAME "mnemosyne-pdf")
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}${MNEMOSYNE_PACKAGE_VERSION_SUFFIX}")
  set(CPACK_PACKAGE_VENDOR "Mnemosyne")
  set(CPACK_PACKAGE_CONTACT "geovanny.avelar@gmail.com")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PDF, EPUB, and HTML reader")
  set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/geovannyAvelar/mnemosyne")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

  include(CPack)
endif()

# macOS packaging (.dmg) doesn't go through CPack: see scripts/package-macos.sh,
# which uses `cmake --install` + macdeployqt/dylibbundler + hdiutil directly.
# CPack's own install-time RPATH rewriting conflicts with dylibbundler having
# already rewritten it.
