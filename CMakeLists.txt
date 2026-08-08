cmake_minimum_required(VERSION 3.20)
project(Mnemosyne LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if(APPLE)
  # Homebrew keg-only packages aren't on the default prefix path.
  list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/qt)
  list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/poppler-qt6)
  list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/libzip)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Test WebEngineWidgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER_QT6 REQUIRED IMPORTED_TARGET poppler-qt6)
pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)

qt_standard_project_setup()

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

if(UNIX AND NOT APPLE)
  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_NAME "mnemosyne")
  set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
  set(CPACK_PACKAGE_VENDOR "Mnemosyne")
  set(CPACK_PACKAGE_CONTACT "geovanny.avelar@gmail.com")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PDF, EPUB, and HTML reader")
  set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/geovannyavelar/mnemosyne")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

  include(CPack)
endif()
