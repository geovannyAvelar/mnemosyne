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
    ui/MainWindow.cpp
    ui/MainWindow.h
    ui/PdfView.h
    ui/PdfView.cpp
    ui/PdfPageCanvas.h
    ui/PdfPageCanvas.cpp
    ui/EpubView.h
    ui/EpubView.cpp
    ui/HtmlView.h
    ui/HtmlView.cpp
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
)

add_library(mnemosynecore STATIC ${MNEMOSYNE_CORE_SOURCES})

target_link_libraries(mnemosynecore PUBLIC
    Qt6::Widgets
    Qt6::WebEngineWidgets
    PkgConfig::POPPLER_QT6
    PkgConfig::LIBZIP
)

target_include_directories(mnemosynecore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(Mnemosyne main.cpp)

target_link_libraries(Mnemosyne PRIVATE mnemosynecore)

if(APPLE)
    set_target_properties(Mnemosyne PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.geovannyavelar.Mnemosyne"
        MACOSX_BUNDLE_BUNDLE_NAME "Mnemosyne"
    )
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    install(TARGETS Mnemosyne RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(FILES ${CMAKE_SOURCE_DIR}/resources/mnemosyne.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()
