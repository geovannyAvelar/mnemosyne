qt_add_executable(epub_document_test EpubDocumentTest.cpp)

target_link_libraries(epub_document_test PRIVATE mnemosynecore Qt6::Test)
target_compile_definitions(epub_document_test PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME EpubDocumentTest COMMAND epub_document_test)

qt_add_executable(markdown_document_test MarkdownDocumentTest.cpp)

target_link_libraries(markdown_document_test PRIVATE mnemosynecore Qt6::Test)

add_test(NAME MarkdownDocumentTest COMMAND markdown_document_test)

qt_add_executable(cbz_document_test CbzDocumentTest.cpp)

target_link_libraries(cbz_document_test PRIVATE mnemosynecore Qt6::Test)
target_compile_definitions(cbz_document_test PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME CbzDocumentTest COMMAND cbz_document_test)

qt_add_executable(mobi_document_test MobiDocumentTest.cpp)

target_link_libraries(mobi_document_test PRIVATE mnemosynecore Qt6::Test)
target_compile_definitions(mobi_document_test PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME MobiDocumentTest COMMAND mobi_document_test)

qt_add_executable(app_persistence_test AppPersistenceTest.cpp)

target_link_libraries(app_persistence_test PRIVATE mnemosynecore Qt6::Test)

add_test(NAME AppPersistenceTest COMMAND app_persistence_test)

qt_add_executable(search_util_test SearchUtilTest.cpp)

target_link_libraries(search_util_test PRIVATE mnemosynecore Qt6::Test)

add_test(NAME SearchUtilTest COMMAND search_util_test)

qt_add_executable(view_search_test ViewSearchTest.cpp)

target_link_libraries(view_search_test PRIVATE mnemosynedesktopui Qt6::Test)
target_compile_definitions(view_search_test PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME ViewSearchTest COMMAND view_search_test)

qt_add_executable(coordinate_util_test CoordinateUtilTest.cpp)

target_link_libraries(coordinate_util_test PRIVATE mnemosynecore Qt6::Test)

add_test(NAME CoordinateUtilTest COMMAND coordinate_util_test)

qt_add_executable(epub_zoom_test EpubZoomTest.cpp)

target_link_libraries(epub_zoom_test PRIVATE mnemosynedesktopui Qt6::Test)
target_compile_definitions(epub_zoom_test PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME EpubZoomTest COMMAND epub_zoom_test)

qt_add_executable(pdf_text_selection_test PdfTextSelectionTest.cpp)

target_link_libraries(pdf_text_selection_test PRIVATE mnemosynedesktopui Qt6::Test)
target_compile_definitions(pdf_text_selection_test PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME PdfTextSelectionTest COMMAND pdf_text_selection_test)

if(MNEMOSYNE_ENABLE_HTML)
    qt_add_executable(html_view_test HtmlViewTest.cpp)

    target_link_libraries(html_view_test PRIVATE mnemosynedesktopui Qt6::Test)
    target_compile_definitions(html_view_test PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

    add_test(NAME HtmlViewTest COMMAND html_view_test)
endif()

qt_add_executable(sync_test SyncTest.cpp)

target_link_libraries(sync_test PRIVATE mnemosynecore Qt6::Test)

add_test(NAME SyncTest COMMAND sync_test)

qt_add_executable(progress_sync_log_directory_test ProgressSyncLogDirectoryTest.cpp)

target_link_libraries(progress_sync_log_directory_test PRIVATE mnemosynecore Qt6::Test)

add_test(NAME ProgressSyncLogDirectoryTest COMMAND progress_sync_log_directory_test)

if(MNEMOSYNE_ENABLE_GOOGLE_DRIVE_SYNC)
    qt_add_executable(google_auth_test GoogleAuthTest.cpp)

    target_link_libraries(google_auth_test PRIVATE mnemosynecore Qt6::Test)

    add_test(NAME GoogleAuthTest COMMAND google_auth_test)

    qt_add_executable(google_drive_sync_test GoogleDriveSyncTest.cpp)

    target_link_libraries(google_drive_sync_test PRIVATE mnemosynecore Qt6::Test)

    add_test(NAME GoogleDriveSyncTest COMMAND google_drive_sync_test)
endif()

qt_add_executable(progress_sync_integration_test ProgressSyncIntegrationTest.cpp)

target_link_libraries(progress_sync_integration_test PRIVATE mnemosynedesktopui Qt6::Test)
target_compile_definitions(progress_sync_integration_test PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME ProgressSyncIntegrationTest COMMAND progress_sync_integration_test)

qt_add_executable(scroll_page_turn_test ScrollPageTurnTest.cpp)

target_link_libraries(scroll_page_turn_test PRIVATE mnemosynedesktopui Qt6::Test)
target_compile_definitions(scroll_page_turn_test PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_test(NAME ScrollPageTurnTest COMMAND scroll_page_turn_test)
