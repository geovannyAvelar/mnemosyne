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
    markdown/MarkdownDocument.h
    markdown/MarkdownDocument.cpp
)

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
set(MNEMOSYNE_DESKTOP_UI_SOURCES
    ui/MainWindow.cpp
    ui/MainWindow.h
    ui/PdfView.h
    ui/PdfView.cpp
    ui/PdfPageCanvas.h
    ui/PdfPageCanvas.cpp
    ui/EpubView.h
    ui/EpubView.cpp
    ui/MarkdownView.h
    ui/MarkdownView.cpp
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
