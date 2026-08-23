# Widgets-free layer: document parsing/rendering, bookmarks/highlights,
# reading-progress + Drive sync, OAuth. Nothing here may depend on
# Qt6::Widgets — this is the boundary a future mobile (Qt Quick/QML) front
# end would link against instead of mnemosynedesktopui.
set(MNEMOSYNE_CORE_SOURCES
    core/Bookmark.h
    core/Highlight.h
    core/ReadingProgress.h
    core/Document.h
    core/Document.cpp
    core/ReaderView.h
    core/SearchUtil.h
    core/SearchUtil.cpp
    core/CoordinateUtil.h
    core/CoordinateUtil.cpp
    core/HtmlAttrUtil.h
    core/HtmlAttrUtil.cpp
    app/RecentFiles.h
    app/RecentFiles.cpp
    app/BookmarkStore.h
    app/BookmarkStore.cpp
    app/HighlightStore.h
    app/HighlightStore.cpp
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

if(NOT ANDROID)
    # libmobi has no prebuilt Android package (see top-level CMakeLists.txt);
    # MobiDocument is only ever constructed from ui/MobiView.cpp anyway
    # (desktop-only, see MNEMOSYNE_DESKTOP_UI_SOURCES below), so excluding it
    # here doesn't remove anything the Android app could reach.
    list(APPEND MNEMOSYNE_CORE_SOURCES
        mobi/MobiDocument.h
        mobi/MobiDocument.cpp
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
        platform/TokenStore.h
    )

    if(APPLE)
        list(APPEND MNEMOSYNE_CORE_SOURCES platform/TokenStore_mac.mm)
    elseif(WIN32)
        list(APPEND MNEMOSYNE_CORE_SOURCES platform/TokenStore_win.cpp)
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

if(NOT ANDROID)
    target_link_libraries(mnemosynecore PUBLIC PkgConfig::LIBMOBI)
endif()

if(ANDROID)
    # The Android NDK toolchain file restricts find_library() to
    # CMAKE_FIND_ROOT_PATH, so PkgConfig::POPPLER_QT6/LIBZIP's -l flags
    # (from MNEMOSYNE_ANDROID_DEPS_PREFIX's .pc files, see top-level
    # CMakeLists.txt) resolve without a -L unless it's added explicitly here.
    target_link_directories(mnemosynecore PUBLIC ${MNEMOSYNE_ANDROID_DEPS_PREFIX}/lib)
endif()

if(MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC)
    target_compile_definitions(mnemosynecore PUBLIC MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC)

    if(APPLE)
        target_link_libraries(mnemosynecore PUBLIC "-framework Security")
    elseif(WIN32)
        target_link_libraries(mnemosynecore PUBLIC Crypt32 Advapi32)
    else()
        target_link_libraries(mnemosynecore PUBLIC PkgConfig::LIBSECRET)
    endif()
endif()

target_compile_definitions(mnemosynecore PUBLIC MNEMOSYNE_VERSION="${PROJECT_VERSION}")

target_include_directories(mnemosynecore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Desktop (Qt Widgets) UI, layered on top of mnemosynecore. Everything in
# here is specific to the mouse/keyboard/windowed desktop experience — a
# mobile front end would link mnemosynecore directly and skip this target
# entirely rather than reusing any of it.
if(NOT ANDROID)

set(MNEMOSYNE_DESKTOP_UI_SOURCES
    ui/MainWindow.cpp
    ui/MainWindow.h
    ui/PdfView.h
    ui/PdfView.cpp
    ui/PdfPageCanvas.h
    ui/PdfPageCanvas.cpp
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
    ui/BookmarksDock.h
    ui/BookmarksDock.cpp
    ui/LibraryView.h
    ui/LibraryView.cpp
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

endif() # NOT ANDROID

if(ANDROID)
    # Stage 2 proved the cross-compiled Poppler-Qt6/libzip/Freetype chain
    # works (SmokeTestBridge, still linked below for later regression
    # checks). Stage 3 adds the real QML app shell: a Library screen backed
    # by the same app/RecentFiles.h store the desktop LibraryView uses, and
    # Android's Storage Access Framework document picker in place of a
    # desktop-style native file dialog (Android apps can't browse arbitrary
    # filesystem paths without it).
    qt_add_executable(MnemosyneAndroid
        quick/main_android.cpp
        quick/SmokeTestBridge.h
        quick/SmokeTestBridge.cpp
        quick/AndroidStorageAccess.h
        quick/AndroidStorageAccess.cpp
        quick/LibraryModel.h
        quick/LibraryModel.cpp
        quick/ContentUriCache.h
        quick/ContentUriCache.cpp
        quick/PdfDocumentModel.h
        quick/PdfDocumentModel.cpp
        quick/PdfPageImageProvider.h
        quick/PdfPageImageProvider.cpp
        quick/EpubReaderModel.h
        quick/EpubReaderModel.cpp
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
        ${CMAKE_SOURCE_DIR}/qml/components/PdfPageItem.qml
        ${CMAKE_SOURCE_DIR}/qml/EpubReaderScreen.qml
    )
    foreach(_qml_file ${MNEMOSYNE_ANDROID_QML_FILES})
        # Relative to qml/, not just the basename, so components/PdfPageItem.qml
        # keeps its subdirectory — PdfReaderScreen.qml imports it as
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
        mnemosynecore
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

    # No QT_ANDROID_PACKAGE_SOURCE_DIR yet: androiddeployqt generates a
    # default AndroidManifest.xml. A real android/ source dir (package id,
    # permissions) lands with app packaging polish in a later stage — the
    # ACTION_OPEN_DOCUMENT picker itself needs no manifest declaration.
endif()
