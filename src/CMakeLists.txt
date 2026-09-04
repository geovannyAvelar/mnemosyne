# Widgets-free layer: document parsing/rendering, highlights/notes,
# reading-progress + Drive sync, OAuth. Nothing here may depend on
# Qt6::Widgets — this is the boundary a future mobile (Qt Quick/QML) front
# end would link against instead of mnemosynedesktopui.
set(MNEMOSYNE_CORE_SOURCES
    core/Highlight.h
    core/ReadingProgress.h
    core/Document.h
    core/Document.cpp
    core/ReaderView.h
    core/SearchUtil.h
    core/SearchUtil.cpp
    core/CoordinateUtil.h
    core/CoordinateUtil.cpp
    core/TextSelectionUtil.h
    core/TextSelectionUtil.cpp
    core/HtmlAttrUtil.h
    core/HtmlAttrUtil.cpp
    app/RecentFiles.h
    app/RecentFiles.cpp
    app/SingleInstanceGuard.h
    app/SingleInstanceGuard.cpp
    app/BookMetadataClient.h
    app/BookMetadataClient.cpp
    app/HighlightStore.h
    app/HighlightStore.cpp
    app/HighlightExporter.h
    app/HighlightExporter.cpp
    app/DeviceIdentity.h
    app/DeviceIdentity.cpp
    app/FileIdentity.h
    app/FileIdentity.cpp
    app/SyncFolder.h
    app/SyncFolder.cpp
    app/ReadingProgressStore.h
    app/ReadingProgressStore.cpp
    app/ProgressSyncLog.h
    app/ProgressSyncLog.cpp
    app/HighlightSyncLog.h
    app/HighlightSyncLog.cpp
    app/HighlightSync.h
    app/HighlightSync.cpp
    platform/SystemAppearance.h
    platform/SystemAppearance.cpp
    pdf/PopplerPdfDocument.h
    pdf/PopplerPdfDocument.cpp
    epub/ZipArchive.h
    epub/ZipArchive.cpp
    epub/EpubDocument.h
    epub/EpubDocument.cpp
    comic/CbzDocument.h
    comic/CbzDocument.cpp
    markdown/MarkdownDocument.h
    markdown/MarkdownDocument.cpp
    txt/TxtDocument.h
    txt/TxtDocument.cpp
)

if(NOT ANDROID AND NOT IOS)
    # libmobi has no prebuilt Android or iOS package (see top-level
    # CMakeLists.txt); MobiDocument is only ever constructed from
    # ui/MobiView.cpp anyway (desktop-only, see MNEMOSYNE_DESKTOP_UI_SOURCES
    # below), so excluding it here doesn't remove anything the mobile apps
    # could reach.
    list(APPEND MNEMOSYNE_CORE_SOURCES
        mobi/MobiDocument.h
        mobi/MobiDocument.cpp
    )
endif()

if(MNEMOSYNE_ENABLE_PLUGINS)
    list(APPEND MNEMOSYNE_CORE_SOURCES
        app/PluginHost.h
        app/PluginHost.cpp
    )
endif()

if(MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC)
    list(APPEND MNEMOSYNE_CORE_SOURCES
        app/IHttpClient.h
        app/IHttpClient.cpp
        app/GoogleAuth.h
        app/GoogleAuth.cpp
        app/GoogleDriveSync.h
        app/GoogleDriveSync.cpp
        app/GoogleDriveHighlightSync.h
        app/GoogleDriveHighlightSync.cpp
        platform/TokenStore.h
    )

    if(APPLE)
        list(APPEND MNEMOSYNE_CORE_SOURCES platform/TokenStore_mac.mm)
    elseif(WIN32)
        list(APPEND MNEMOSYNE_CORE_SOURCES platform/TokenStore_win.cpp)
    elseif(ANDROID)
        list(APPEND MNEMOSYNE_CORE_SOURCES
            platform/TokenStore_android.cpp
            platform/OAuthRedirect_android.h
            platform/OAuthRedirect_android.cpp
        )
    else()
        list(APPEND MNEMOSYNE_CORE_SOURCES platform/TokenStore_linux.cpp)
    endif()
endif()

add_library(mnemosynecore STATIC ${MNEMOSYNE_CORE_SOURCES})

target_link_libraries(mnemosynecore PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    PkgConfig::POPPLER_QT6
    PkgConfig::LIBZIP
)

if(UNIX AND NOT APPLE AND NOT ANDROID AND NOT IOS)
    # SystemAppearance.cpp falls back to querying the freedesktop desktop
    # portal over D-Bus on Qt < 6.5 (no QStyleHints::colorScheme() yet --
    # e.g. the qt6-base-dev apt package this project's Linux CI installs,
    # see .github/workflows/build.yml). Qt >= 6.5 never takes that code
    # path, but linking DBus is harmless either way.
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    target_link_libraries(mnemosynecore PUBLIC Qt6::DBus)
endif()

if(NOT ANDROID AND NOT IOS)
    target_link_libraries(mnemosynecore PUBLIC PkgConfig::LIBMOBI)
endif()

if(ANDROID)
    # The Android NDK toolchain file restricts find_library() to
    # CMAKE_FIND_ROOT_PATH, so PkgConfig::POPPLER_QT6/LIBZIP's -l flags
    # (from MNEMOSYNE_ANDROID_DEPS_PREFIX's .pc files, see top-level
    # CMakeLists.txt) resolve without a -L unless it's added explicitly here.
    target_link_directories(mnemosynecore PUBLIC ${MNEMOSYNE_ANDROID_DEPS_PREFIX}/lib)
elseif(IOS)
    # Same gotcha as Android above, but for a different reason: the Xcode
    # generator doesn't reliably turn PkgConfig::POPPLER_QT6/LIBZIP's bare
    # "-lpoppler-qt6"/"-lzip" (from MNEMOSYNE_IOS_DEPS_PREFIX's .pc files)
    # into a Library Search Paths entry on its own, so the final app link
    # fails with "library 'poppler-qt6' not found" without this.
    target_link_directories(mnemosynecore PUBLIC ${MNEMOSYNE_IOS_DEPS_PREFIX}/lib)
endif()

if(MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC)
    target_compile_definitions(mnemosynecore PUBLIC MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC)

    if(APPLE)
        target_link_libraries(mnemosynecore PUBLIC "-framework Security")
    elseif(WIN32)
        target_link_libraries(mnemosynecore PUBLIC Crypt32 Advapi32)
    elseif(ANDROID)
        # TokenStore_android.cpp/OAuthRedirect_android.cpp use QJniObject
        # and <jni.h> only -- both already available through Qt6::Gui/Core,
        # no extra library to link (no libsecret equivalent needed here).
    else()
        target_link_libraries(mnemosynecore PUBLIC PkgConfig::LIBSECRET)
    endif()
endif()

if(MNEMOSYNE_ENABLE_PLUGINS)
    target_compile_definitions(mnemosynecore PUBLIC MNEMOSYNE_ENABLE_PLUGINS)
    # qjs is quickjs-ng's static library target, brought in via FetchContent
    # in the top-level CMakeLists.txt; PRIVATE since it's an implementation
    # detail of PluginHost.cpp, not something callers of mnemosynecore need
    # on their own include path.
    target_link_libraries(mnemosynecore PRIVATE qjs)
endif()

target_compile_definitions(mnemosynecore PUBLIC MNEMOSYNE_VERSION="${PROJECT_VERSION}")

target_include_directories(mnemosynecore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# QML-facing bridge classes (src/quick/*) that have no Android-specific
# dependency -- everything except AndroidStorageAccess.{h,cpp} (uses
# QtAndroidPrivate::startActivity for the SAF picker) and main_android.cpp
# (the Android entry point). Built as one library on every platform, not
# just under ANDROID: the Android executable target below links it as-is,
# and tests/CMakeLists.txt links it too, so this logic gets real desktop
# Qt Test coverage instead of only ever being exercised by manual on-device
# testing on Android.
set(MNEMOSYNE_QUICK_SOURCES
    quick/SmokeTestBridge.h
    quick/SmokeTestBridge.cpp
    quick/LibraryModel.h
    quick/LibraryModel.cpp
    quick/ContentUriCache.h
    quick/ContentUriCache.cpp
    quick/PdfPageImageProvider.h
    quick/PdfPageImageProvider.cpp
    quick/PdfDocumentModel.h
    quick/PdfDocumentModel.cpp
    quick/EpubReaderModel.h
    quick/EpubReaderModel.cpp
    quick/HighlightsModel.h
    quick/HighlightsModel.cpp
    quick/PdfSelectionController.h
    quick/PdfSelectionController.cpp
    quick/SyncController.h
    quick/SyncController.cpp
    quick/ThemeSettings.h
    quick/ThemeSettings.cpp
)

add_library(mnemosynequick STATIC ${MNEMOSYNE_QUICK_SOURCES})

target_link_libraries(mnemosynequick PUBLIC mnemosynecore Qt6::Quick)
target_include_directories(mnemosynequick PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Desktop (Qt Widgets) UI, layered on top of mnemosynecore. Everything in
# here is specific to the mouse/keyboard/windowed desktop experience — a
# mobile front end would link mnemosynecore directly and skip this target
# entirely rather than reusing any of it. QWidgets isn't available on
# Android or iOS at all, so this whole block is skipped on both: without
# excluding IOS here too, an iOS cross-compile would try to build this
# (and the AppKit-only platform/MacWindowChrome.mm below, since CMake's
# APPLE is true for iOS as well as macOS) and fail outright.
if(NOT ANDROID AND NOT IOS)

set(MNEMOSYNE_DESKTOP_UI_SOURCES
    ui/MainWindow.cpp
    ui/MainWindow.h
    ui/PdfView.h
    ui/PdfView.cpp
    ui/PdfPageCanvas.h
    ui/PdfPageCanvas.cpp
    ui/PdfPageStackView.h
    ui/PdfPageStackView.cpp
    ui/EpubView.h
    ui/EpubView.cpp
    ui/ComicView.h
    ui/ComicView.cpp
    ui/MarkdownView.h
    ui/MarkdownView.cpp
    ui/MobiView.h
    ui/MobiView.cpp
    ui/TxtView.h
    ui/TxtView.cpp
    ui/TocDock.h
    ui/TocDock.cpp
    ui/NotesDock.h
    ui/NotesDock.cpp
    ui/BookInfoDock.h
    ui/BookInfoDock.cpp
    ui/NoteDialog.h
    ui/NoteDialog.cpp
    ui/LibraryView.h
    ui/LibraryView.cpp
    ui/ThumbnailProvider.h
    ui/ThumbnailProvider.cpp
    ui/SearchDock.h
    ui/SearchDock.cpp
    ui/SyncPromptBar.h
    ui/SyncPromptBar.cpp
    ui/Theme.h
    ui/Theme.cpp
    ui/TopBar.h
    ui/TopBar.cpp
    ui/TrafficLightButton.h
    ui/TrafficLightButton.cpp
)

if(MNEMOSYNE_ENABLE_HTML)
    list(APPEND MNEMOSYNE_DESKTOP_UI_SOURCES
        ui/HtmlView.h
        ui/HtmlView.cpp
    )
endif()

if(APPLE)
    list(APPEND MNEMOSYNE_DESKTOP_UI_SOURCES
        platform/MacWindowChrome.h
        platform/MacWindowChrome.mm
    )
endif()

if(MNEMOSYNE_ENABLE_PLUGINS)
    list(APPEND MNEMOSYNE_DESKTOP_UI_SOURCES
        ui/PluginsDialog.h
        ui/PluginsDialog.cpp
        ui/PluginFormDialog.h
        ui/PluginFormDialog.cpp
    )
endif()

add_library(mnemosynedesktopui STATIC ${MNEMOSYNE_DESKTOP_UI_SOURCES})

target_link_libraries(mnemosynedesktopui PUBLIC
    mnemosynecore
    Qt6::Widgets
    Qt6::Concurrent
)

if(APPLE)
    target_link_libraries(mnemosynedesktopui PUBLIC "-framework Cocoa")
endif()

if(MNEMOSYNE_ENABLE_HTML)
    target_link_libraries(mnemosynedesktopui PUBLIC Qt6::WebEngineWidgets)
    target_compile_definitions(mnemosynedesktopui PUBLIC MNEMOSYNE_ENABLE_HTML)
endif()

target_include_directories(mnemosynedesktopui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(Mnemosyne main.cpp ${CMAKE_SOURCE_DIR}/resources/icons.qrc)

target_link_libraries(Mnemosyne PRIVATE mnemosynedesktopui)

if(APPLE)
    set(MNEMOSYNE_ICNS ${CMAKE_SOURCE_DIR}/resources/icons/mnemosyne.icns)
    target_sources(Mnemosyne PRIVATE ${MNEMOSYNE_ICNS})
    set_source_files_properties(${MNEMOSYNE_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    set_target_properties(Mnemosyne PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.geovannyavelar.Mnemosyne"
        MACOSX_BUNDLE_BUNDLE_NAME "Mnemosyne"
        MACOSX_BUNDLE_ICON_FILE "mnemosyne.icns"
        # Without this, Qt falls back to its own bare-bones Info.plist
        # template, which declares no document types at all -- macOS then has
        # no idea Mnemosyne can open PDF/EPUB/etc., so it never shows up in
        # Finder's "Open With" menu or as a candidate default reader. Our
        # template (a copy of Qt's, plus CFBundleDocumentTypes/UTType
        # declarations) fixes that; see resources/macos/Info.plist.in.
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/macos/Info.plist.in"
        # Homebrew's Qt/Poppler/libzip aren't on end-user machines, so the
        # installed (packaged) copy needs to look for its dependencies next
        # to itself instead of in Homebrew's prefix. macdeployqt/dylibbundler
        # (scripts/package-macos.sh) then copy those dependencies in under
        # Contents/Frameworks. The build-tree copy keeps the Homebrew rpath
        # so `cmake --build` + run still works for local development.
        INSTALL_RPATH "@executable_path/../Frameworks"
    )

    install(TARGETS Mnemosyne BUNDLE DESTINATION .)

    # Drag-and-drop install target shown next to the app in the .dmg.
    install(CODE "
        file(REMOVE \"\${CMAKE_INSTALL_PREFIX}/Applications\")
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink /Applications \"\${CMAKE_INSTALL_PREFIX}/Applications\")
    ")
endif()

if(WIN32)
    target_sources(Mnemosyne PRIVATE ${CMAKE_SOURCE_DIR}/resources/mnemosyne.rc)

    set_target_properties(Mnemosyne PROPERTIES WIN32_EXECUTABLE TRUE)

    # Plain layout for windeployqt/dylib copying to fill in (see
    # scripts/package-windows.sh) — no install(CODE) tricks needed here
    # since there's no rpath to rewrite on Windows.
    install(TARGETS Mnemosyne RUNTIME DESTINATION .)
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    install(TARGETS Mnemosyne RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(FILES ${CMAKE_SOURCE_DIR}/resources/mnemosyne.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

    foreach(_icon_size 16 32 48 64 128 256 512)
        install(FILES ${CMAKE_SOURCE_DIR}/resources/icons/mnemosyne_${_icon_size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_icon_size}x${_icon_size}/apps
            RENAME mnemosyne.png)
    endforeach()

    install(FILES ${CMAKE_SOURCE_DIR}/resources/icons/mnemosyne.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()

endif() # NOT ANDROID AND NOT IOS

if(ANDROID)
    # Stage 2 proved the cross-compiled Poppler-Qt6/libzip/Freetype chain
    # works (SmokeTestBridge, still linked below for later regression
    # checks). Stage 3 adds the real QML app shell: a Library screen backed
    # by the same app/RecentFiles.h store the desktop LibraryView uses, and
    # Android's Storage Access Framework document picker in place of a
    # desktop-style native file dialog (Android apps can't browse arbitrary
    # filesystem paths without it).
    # quick/SmokeTestBridge.{h,cpp} through quick/SyncController.{h,cpp}
    # (everything but the SAF picker and the entry point below) live in
    # mnemosynequick now, shared with desktop's Qt Test coverage — see its
    # definition above for why.
    qt_add_executable(MnemosyneAndroid
        quick/main_android.cpp
        quick/AndroidStorageAccess.h
        quick/AndroidStorageAccess.cpp
    )

    set_source_files_properties(${CMAKE_SOURCE_DIR}/tests/fixtures/test_multipage.pdf
        PROPERTIES QT_RESOURCE_ALIAS test_multipage.pdf)

    qt_add_resources(MnemosyneAndroid smoke_test_fixtures
        PREFIX /
        FILES ${CMAKE_SOURCE_DIR}/tests/fixtures/test_multipage.pdf
    )

    set(MNEMOSYNE_ANDROID_QML_FILES
        ${CMAKE_SOURCE_DIR}/qml/Main.qml
        ${CMAKE_SOURCE_DIR}/qml/Theme.qml
        ${CMAKE_SOURCE_DIR}/qml/LibraryScreen.qml
        ${CMAKE_SOURCE_DIR}/qml/PdfReaderScreen.qml
        ${CMAKE_SOURCE_DIR}/qml/components/PdfContinuousPageItem.qml
        ${CMAKE_SOURCE_DIR}/qml/components/SelectionToolbar.qml
        ${CMAKE_SOURCE_DIR}/qml/components/SyncPromptBar.qml
        ${CMAKE_SOURCE_DIR}/qml/EpubReaderScreen.qml
        ${CMAKE_SOURCE_DIR}/qml/SettingsScreen.qml
    )
    foreach(_qml_file ${MNEMOSYNE_ANDROID_QML_FILES})
        # Relative to qml/, not just the basename, so
        # components/PdfContinuousPageItem.qml keeps its subdirectory —
        # PdfReaderScreen.qml imports it as
        # `import "components"`, which needs that structure preserved in
        # the module's resource tree.
        file(RELATIVE_PATH _qml_alias ${CMAKE_SOURCE_DIR}/qml ${_qml_file})
        set_source_files_properties(${_qml_file} PROPERTIES QT_RESOURCE_ALIAS ${_qml_alias})
    endforeach()
    set_source_files_properties(${CMAKE_SOURCE_DIR}/qml/Theme.qml
        PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

    # Without this, the QML module's resource prefix silently falls back to
    # the pre-6.8 default and the app crashes at runtime with "Module
    # 'MnemosyneAndroid' contains no type named Main" — see the Stage 0
    # note in the mobile-port plan for how long that took to diagnose.
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0002 NEW)
    qt_policy(SET QTP0004 NEW)

    qt_add_qml_module(MnemosyneAndroid
        URI MnemosyneAndroid
        VERSION 1.0
        QML_FILES ${MNEMOSYNE_ANDROID_QML_FILES}
    )

    target_link_libraries(MnemosyneAndroid PRIVATE
        mnemosynequick
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::WebView
        Qt6::CorePrivate # AndroidStorageAccess uses QtAndroidPrivate::startActivity (qandroidextras_p.h)
    )

    # Poppler-Qt6/libzip/Freetype are linked but not Qt libraries, so
    # androiddeployqt doesn't know to bundle them into the APK on its own —
    # QT_ANDROID_EXTRA_LIBS is how a target tells it to.
    set_target_properties(MnemosyneAndroid PROPERTIES
        QT_ANDROID_EXTRA_LIBS
            "${MNEMOSYNE_ANDROID_DEPS_PREFIX}/lib/libpoppler-qt6.so;${MNEMOSYNE_ANDROID_DEPS_PREFIX}/lib/libpoppler.so;${MNEMOSYNE_ANDROID_DEPS_PREFIX}/lib/libzip.so;${MNEMOSYNE_ANDROID_DEPS_PREFIX}/lib/libfreetype.so"
    )

    if(MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC)
        # Stage 8 needs a real manifest: OAuthRedirectActivity's
        # mnemosyne://oauth2redirect intent-filter has no equivalent in
        # androiddeployqt's auto-generated default. QT_ANDROID_PACKAGE_NAME
        # must match both this manifest's package="org.mnemosyne" and the
        # JNI method names in OAuthRedirect_android.cpp/TokenStoreBridge.java
        # (Java_org_mnemosyne_..., "org/mnemosyne/..." class names) —
        # androiddeployqt patches the manifest's package attribute from this
        # property regardless of what's literally written in the file.
        set_target_properties(MnemosyneAndroid PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/android
            QT_ANDROID_PACKAGE_NAME "org.mnemosyne"
        )
    endif()
elseif(IOS)
    # Mirrors the ANDROID branch above: same mnemosynequick bridge classes,
    # same QML file set, an entry point (quick/main_ios.mm) structurally
    # identical to quick/main_android.cpp, and IOSStorageAccess in place of
    # AndroidStorageAccess as the document-picker bridge (both are kept out
    # of mnemosynequick and linked directly into their platform's
    # executable, same as AndroidStorageAccess always has been). See
    # docs/ios-build.md for the dependency cross-compile this links against.
    # Ghostscript/URW Base35 substitute fonts for Poppler's "generic" font
    # backend (see PopplerFontSetup.cpp and scripts/build-ios-deps.sh) --
    # MACOSX_PACKAGE_LOCATION copies these directly into the app bundle
    # root (iOS bundles are flat, unlike macOS's Contents/Resources), which
    # is what QCoreApplication::applicationDirPath() resolves to on iOS.
    file(GLOB MNEMOSYNE_IOS_BASE14_FONTS ${CMAKE_SOURCE_DIR}/resources/pdf-base14-fonts/*.pfb)
    set_source_files_properties(${MNEMOSYNE_IOS_BASE14_FONTS}
        PROPERTIES MACOSX_PACKAGE_LOCATION "pdf-base14-fonts")

    # App icon (ios/Assets.xcassets/AppIcon.appiconset) -- a single 1024x1024
    # source image, opaque (no alpha, required for App Store icons). Xcode's
    # asset catalog compiler (actool) recognizes the .xcassets extension and
    # generates every actually-needed scaled variant from it at build time,
    # so unlike the older per-size-file icon sets this needs no other
    # declarations beyond XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME
    # below pointing at the appiconset's name.
    set(MNEMOSYNE_IOS_ASSET_CATALOG ${CMAKE_SOURCE_DIR}/ios/Assets.xcassets)
    set_source_files_properties(${MNEMOSYNE_IOS_ASSET_CATALOG}
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    qt_add_executable(MnemosyneIOS
        quick/main_ios.mm
        quick/IOSStorageAccess.h
        quick/IOSStorageAccess.mm
        quick/PopplerFontSetup.h
        quick/PopplerFontSetup.cpp
        ${MNEMOSYNE_IOS_BASE14_FONTS}
        ${MNEMOSYNE_IOS_ASSET_CATALOG}
    )

    set_source_files_properties(${CMAKE_SOURCE_DIR}/tests/fixtures/test_multipage.pdf
        PROPERTIES QT_RESOURCE_ALIAS test_multipage.pdf)

    qt_add_resources(MnemosyneIOS smoke_test_fixtures
        PREFIX /
        FILES ${CMAKE_SOURCE_DIR}/tests/fixtures/test_multipage.pdf
    )

    set(MNEMOSYNE_IOS_QML_FILES
        ${CMAKE_SOURCE_DIR}/qml/Main.qml
        ${CMAKE_SOURCE_DIR}/qml/Theme.qml
        ${CMAKE_SOURCE_DIR}/qml/LibraryScreen.qml
        ${CMAKE_SOURCE_DIR}/qml/PdfReaderScreen.qml
        ${CMAKE_SOURCE_DIR}/qml/components/PdfContinuousPageItem.qml
        ${CMAKE_SOURCE_DIR}/qml/components/SelectionToolbar.qml
        ${CMAKE_SOURCE_DIR}/qml/components/SyncPromptBar.qml
        ${CMAKE_SOURCE_DIR}/qml/EpubReaderScreen.qml
        ${CMAKE_SOURCE_DIR}/qml/SettingsScreen.qml
    )
    foreach(_qml_file ${MNEMOSYNE_IOS_QML_FILES})
        file(RELATIVE_PATH _qml_alias ${CMAKE_SOURCE_DIR}/qml ${_qml_file})
        set_source_files_properties(${_qml_file} PROPERTIES QT_RESOURCE_ALIAS ${_qml_alias})
    endforeach()
    set_source_files_properties(${CMAKE_SOURCE_DIR}/qml/Theme.qml
        PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

    # See the identical guard in the ANDROID branch above for why this is
    # needed before qt_add_qml_module.
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0002 NEW)
    qt_policy(SET QTP0004 NEW)

    qt_add_qml_module(MnemosyneIOS
        URI MnemosyneIOS
        VERSION 1.0
        QML_FILES ${MNEMOSYNE_IOS_QML_FILES}
    )

    target_link_libraries(MnemosyneIOS PRIVATE
        mnemosynequick
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::WebView
        "-framework UIKit"
        "-framework UniformTypeIdentifiers"
    )

    # PopplerFontSetup.cpp includes <GlobalParams.h> -- poppler-qt6's own
    # public headers (already reachable via mnemosynecore's PkgConfig::
    # POPPLER_QT6 Cflags) only cover include/poppler/qt6, not this
    # top-level, ENABLE_UNSTABLE_API_ABI_HEADERS-only header.
    target_include_directories(MnemosyneIOS PRIVATE ${MNEMOSYNE_IOS_DEPS_PREFIX}/include/poppler)

    # Poppler-Qt6/libzip/Freetype were cross-compiled as static libraries
    # (scripts/build-ios-deps.sh) rather than shared like Android's .so's --
    # an iOS app bundle can embed dynamic frameworks, but that needs its
    # own codesigning and an "Embed Frameworks" build phase per library, so
    # static sidesteps all of that. Being static, they just need linking,
    # not an Android-style QT_ANDROID_EXTRA_LIBS bundling step.
    # z is poppler.a's own zlib dependency -- a static lib doesn't carry
    # its dependency graph the way Android's .so's do, so the final
    # executable link needs it spelled out explicitly (iOS's system
    # libz.tbd, same one poppler's own configure step found).
    target_link_libraries(MnemosyneIOS PRIVATE
        ${MNEMOSYNE_IOS_DEPS_PREFIX}/lib/libpoppler-qt6.a
        ${MNEMOSYNE_IOS_DEPS_PREFIX}/lib/libpoppler.a
        ${MNEMOSYNE_IOS_DEPS_PREFIX}/lib/libzip.a
        ${MNEMOSYNE_IOS_DEPS_PREFIX}/lib/libfreetype.a
        z
    )

    # MACOSX_BUNDLE_GUI_IDENTIFIER is CMake's Apple-wide property name for
    # both macOS .app bundles and iOS ones -- reusing the desktop target's
    # identifier string here is harmless since Apple registers macOS and
    # iOS App IDs separately even when the string matches (see the
    # Mnemosyne desktop target above for the macOS one). Leaving
    # DEVELOPMENT_TEAM/CODE_SIGN_STYLE unset lets Qt's Xcode-generator
    # helper (QtPublicAppleHelpers.cmake) auto-detect the sole
    # "Apple Development" identity on this machine and default to
    # Automatic signing, the same as a fresh Xcode project would.
    set_target_properties(MnemosyneIOS PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.geovannyavelar.Mnemosyne"
        MACOSX_BUNDLE_BUNDLE_NAME "Mnemosyne"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        # MACOSX_BUNDLE_BUNDLE_NAME alone only sets CFBundleName -- Qt's
        # Info.plist template sets CFBundleDisplayName (what actually
        # shows under the home screen icon) from Xcode's own PRODUCT_NAME
        # build setting, which defaults to the CMake target name
        # ("MnemosyneIOS") rather than this. Setting it explicitly is what
        # makes the home screen label read "Mnemosyne" instead.
        XCODE_ATTRIBUTE_PRODUCT_NAME "Mnemosyne"
    )
endif()
