cmake_minimum_required(VERSION 3.20)
project(Mnemosyne LANGUAGES CXX VERSION 1.5.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(APPLE)
  # Needed to compile platform/MacWindowChrome.mm (native title-bar integration).
  enable_language(OBJCXX)

  # Homebrew keg-only packages aren't on the default prefix path. Cover both
  # the Intel (/usr/local) and Apple Silicon (/opt/homebrew) Homebrew prefixes,
  # since only one will exist on any given machine.
  foreach(_brew_prefix /usr/local/opt /opt/homebrew/opt)
    list(APPEND CMAKE_PREFIX_PATH ${_brew_prefix}/qt)
    list(APPEND CMAKE_PREFIX_PATH ${_brew_prefix}/poppler-qt6)
    list(APPEND CMAKE_PREFIX_PATH ${_brew_prefix}/libzip)
  endforeach()
endif()

# QtWebEngine (Chromium) only ships prebuilt binaries for macOS/Linux; on
# Windows it's source-build-only (multi-hour, MSVC-specific), so HTML support
# is compiled out of the Windows build. PDF and EPUB are unaffected.
if(WIN32)
  set(MNEMOSYNE_ENABLE_HTML_DEFAULT OFF)
else()
  set(MNEMOSYNE_ENABLE_HTML_DEFAULT ON)
endif()
option(MNEMOSYNE_ENABLE_HTML "Enable Qt WebEngine-based HTML/HTM support" ${MNEMOSYNE_ENABLE_HTML_DEFAULT})

# Google Drive sign-in as a second, optional sync backend alongside the
# local-folder one (see src/app/GoogleDriveSync.*). Unlike HTML support,
# there's no platform this can't build on, so it defaults on everywhere.
option(MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC "Enable signing in with Google Drive as a sync backend" ON)

if(MNEMOSYNE_ENABLE_HTML)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Test Network Concurrent WebEngineWidgets)
else()
  find_package(Qt6 REQUIRED COMPONENTS Widgets Test Network Concurrent)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER_QT6 REQUIRED IMPORTED_TARGET poppler-qt6)
pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)
pkg_check_modules(LIBMOBI REQUIRED IMPORTED_TARGET libmobi)

if(MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC AND UNIX AND NOT APPLE)
  # OAuth refresh tokens need a real OS secret store on every platform: macOS
  # uses Keychain (Security.framework, already linked), Windows uses
  # Credential Manager (wincred.h, no extra dependency); Linux uses the
  # Secret Service API via libsecret.
  pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
endif()

qt_standard_project_setup()

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

if(UNIX AND NOT APPLE)
  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_NAME "mnemosyne-pdf")
  set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
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
