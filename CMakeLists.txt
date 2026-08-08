cmake_minimum_required(VERSION 3.20)
project(Mnemosyne LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Homebrew keg-only packages aren't on the default prefix path.
list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/qt)
list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/poppler-qt6)
list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/libzip)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test WebEngineWidgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER_QT6 REQUIRED IMPORTED_TARGET poppler-qt6)
pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)

qt_standard_project_setup()

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
