cmake_minimum_required(VERSION 3.20)
project(Mnemosyne LANGUAGES CXX VERSION 1.13.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(APPLE AND NOT IOS)
  # Needed to compile platform/MacTouchBar.mm (Touch Bar integration).
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

    # CMAKE_PREFIX_PATH above only helps Qt's CMake-config find_package()
    # lookup below -- pkg_check_modules() (poppler-qt6/libzip) goes through
    # the actual pkg-config/pkgconf binary instead, which only ever
    # consults the PKG_CONFIG_PATH environment variable, never
    # CMAKE_PREFIX_PATH. A keg-only formula's .pc directory (not
    # symlinked into the Homebrew prefix, to avoid clashing with a
    # same-named non-Qt package -- e.g. poppler-qt6 vs. plain poppler)
    # has to be added to PKG_CONFIG_PATH explicitly too, or pkg-config
    # can't find it no matter what CMAKE_PREFIX_PATH says.
    list(APPEND _mnemosyne_pkgconfig_path ${_brew_prefix}/poppler-qt6/lib/pkgconfig)
    list(APPEND _mnemosyne_pkgconfig_path ${_brew_prefix}/libzip/lib/pkgconfig)
  endforeach()
  list(JOIN _mnemosyne_pkgconfig_path ":" _mnemosyne_pkgconfig_path_joined)
  set(ENV{PKG_CONFIG_PATH} "${_mnemosyne_pkgconfig_path_joined}:$ENV{PKG_CONFIG_PATH}")
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

# QuickJS-based plugins (see src/app/PluginHost.*). Desktop-only for now --
# mobile discovery/UI is a separate task, tracked in the plugin system plan,
# even though quickjs-ng itself cross-compiles fine for Android/iOS.
if(ANDROID OR IOS)
  set(MNEMOSYNE_ENABLE_PLUGINS_DEFAULT OFF)
else()
  set(MNEMOSYNE_ENABLE_PLUGINS_DEFAULT ON)
endif()
option(MNEMOSYNE_ENABLE_PLUGINS "Enable the QuickJS-based plugin system" ${MNEMOSYNE_ENABLE_PLUGINS_DEFAULT})

if(MNEMOSYNE_ENABLE_PLUGINS)
  # quickjs-ng has no reliable system package on Windows and none at all for
  # a future Android/iOS port, unlike this project's other dependencies
  # (Poppler/libzip/libmobi) -- FetchContent, pinned to a tagged release,
  # builds uniformly across desktop platforms with no per-platform packaging
  # work. This is the only FetchContent dependency in this project; it's not
  # a precedent to reach for by default -- a system package is still
  # preferred whenever one reliably exists.
  include(FetchContent)
  set(QJS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE) # embedded only, never installed as its own package
  FetchContent_Declare(
    quickjs-ng
    GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
    GIT_TAG v0.16.2
    GIT_SHALLOW TRUE
  )
  # Deliberately not FetchContent_MakeAvailable(): quickjs-ng's CMakeLists.txt
  # unconditionally adds its CLI (qjs_exe), test262 runner, api-test,
  # lre-test, and unicode_gen targets alongside the qjs library -- none of
  # which mnemosynecore needs, and qjs_exe/run-test262 pull in
  # quickjs-libc.c's OS bindings (file/process access) that have no business
  # being built into this app at all, let alone by default on every
  # platform. EXCLUDE_FROM_ALL keeps them out of the default build; nothing
  # here depends on them, so they're simply never built. (CMake >= 3.28 could
  # spell this as FetchContent_Declare(... EXCLUDE_FROM_ALL); this project's
  # floor is 3.20, so it's done the older way.)
  FetchContent_GetProperties(quickjs-ng)
  if(NOT quickjs-ng_POPULATED)
    FetchContent_Populate(quickjs-ng)
    # quickjs-ng is plain C with no UI of its own; this project's global
    # AUTOMOC/AUTOUIC/AUTORCC=ON would otherwise make CMake run (and warn
    # about) moc/uic/rcc against every one of its targets for nothing.
    set(CMAKE_AUTOMOC OFF)
    set(CMAKE_AUTOUIC OFF)
    set(CMAKE_AUTORCC OFF)
    add_subdirectory(${quickjs-ng_SOURCE_DIR} ${quickjs-ng_BINARY_DIR} EXCLUDE_FROM_ALL)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
  endif()
endif()

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
