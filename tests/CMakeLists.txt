# Kalahari test suite (Catch2)

# Test source files
set(TEST_SOURCES
    test_main.cpp
    core/test_settings_manager.cpp
    core/test_cmd_line_parser.cpp
    core/test_svg_converter.cpp
    core/test_extension_points.cpp
    core/test_event_bus.cpp
    core/test_plugin_loading.cpp
    core/test_book_element.cpp
    core/test_part.cpp
    core/test_book.cpp
    core/test_document.cpp
    core/test_document_archive.cpp
    core/test_chapter_document.cpp
    core/test_project_database.cpp  # OpenSpec #00041: SQLite Project Database
    # OpenSpec #00042: Custom Text Editor
    editor/test_editor_types.cpp
    editor/test_kml_element.cpp
    editor/test_kml_text_run.cpp
    editor/test_kml_inline_elements.cpp
    editor/test_kml_paragraph.cpp
    editor/test_kml_document.cpp
    editor/test_kml_parser.cpp
    editor/test_kml_table.cpp  # Phase 1.12: Table elements
    editor/test_paragraph_layout.cpp  # Phase 2.1: Paragraph layout
    editor/test_table_layout.cpp  # Phase 2.6: Table layout
    editor/test_virtual_scroll_manager.cpp  # Phase 2.8: Virtual scroll manager
    editor/test_layout_manager.cpp          # Phase 2.11: Layout manager
    # GUI tests removed (wxWidgets → Qt6 migration - CommandRegistry/ShortcutManager not yet ported)
)

# Windows: Add compatibility shims for Catch2 missing CRT symbols (Task #00035 blocker)
if(WIN32)
    list(APPEND TEST_SOURCES catch2_windows_compat.cpp)
endif()

set(TEST_SOURCES ${TEST_SOURCES}
    # GUI tests REMOVED - all require GTK/X11 display connection (incompatible with console-only CI/CD)
    # - test_bwx_text_editor.cpp: All 9 test cases created wxFrame windows
    # - test_bwx_text_document.cpp: Uses wxMemoryDC (requires display on Linux/macOS)
    # - test_bwx_text_renderer.cpp: Uses wxMemoryDC (requires display on Linux/macOS)
    # These tests will be moved to manual/integration test suite in Phase 1
    # Source files under test (needed for linking)
    ${CMAKE_SOURCE_DIR}/src/core/logger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/settings_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/cmd_line_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/diagnostic_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/extension_points.cpp
    ${CMAKE_SOURCE_DIR}/src/core/event_bus.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_manifest.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_archive.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_signature.cpp
    ${CMAKE_SOURCE_DIR}/src/core/trusted_keys.cpp
    ${CMAKE_SOURCE_DIR}/src/core/python_interpreter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/book_element.cpp
    ${CMAKE_SOURCE_DIR}/src/core/part.cpp
    ${CMAKE_SOURCE_DIR}/src/core/book.cpp
    ${CMAKE_SOURCE_DIR}/src/core/document.cpp
    ${CMAKE_SOURCE_DIR}/src/core/document_archive.cpp
    ${CMAKE_SOURCE_DIR}/src/core/chapter_document.cpp
    ${CMAKE_SOURCE_DIR}/src/core/utils/svg_converter.cpp
    # OpenSpec #00041: SQLite Project Database
    ${CMAKE_SOURCE_DIR}/src/core/project_database.cpp
    ${CMAKE_SOURCE_DIR}/src/core/database_schema_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/project_lock.cpp
    ${CMAKE_SOURCE_DIR}/src/core/backup_manager.cpp
    # OpenSpec #00042: Custom Text Editor
    ${CMAKE_SOURCE_DIR}/src/editor/editor_types.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/kml_element.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/kml_text_run.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/kml_inline_elements.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/kml_paragraph.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/kml_document.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/kml_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/kml_table.cpp  # Phase 1.12: Table elements
    ${CMAKE_SOURCE_DIR}/src/editor/paragraph_layout.cpp  # Phase 2.1: Paragraph layout
    ${CMAKE_SOURCE_DIR}/src/editor/format_converter.cpp  # Phase 2.2: Format conversion
    ${CMAKE_SOURCE_DIR}/src/editor/table_layout.cpp      # Phase 2.6: Table layout
    ${CMAKE_SOURCE_DIR}/src/editor/virtual_scroll_manager.cpp  # Phase 2.8: Virtual scroll manager
    ${CMAKE_SOURCE_DIR}/src/editor/layout_manager.cpp          # Phase 2.11: Layout manager
    # NOTE: IconDownloader moved to kalahari executable, not needed in tests
    # GUI source files removed (wxWidgets → Qt6 migration in progress)
    # More test files will be added as we implement features
)

# Create test executable
add_executable(kalahari-tests ${TEST_SOURCES})

# Target properties
set_target_properties(kalahari-tests PROPERTIES
    OUTPUT_NAME "kalahari-tests"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(kalahari-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

# Link libraries
target_link_libraries(kalahari-tests PRIVATE
    Catch2::Catch2  # Changed from Catch2WithMain - we have custom main()
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Network
    Qt6::Xml
    Qt6::Sql        # OpenSpec #00041: SQLite Project Database
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    libzip::zip
    Python3::Python
    pybind11::embed
    OpenSSL::SSL      # OpenSpec #00038: Ed25519 signature verification
    OpenSSL::Crypto   # OpenSpec #00038: SHA256 hashing
    # bwx_gui removed - wxWidgets library no longer used (Qt6 migration)
)

# Windows: Workaround for Catch2d.lib link errors with VS 2022/2026
# Root cause: vcpkg Catch2 built with incompatible CRT, missing __std_find_* symbols
# Solution: Provide fallback implementations in catch2_windows_compat.cpp
# (already added to TEST_SOURCES above)
if(WIN32)
    message(STATUS "Using Catch2 Windows compatibility shims for missing CRT symbols")
endif()

# Copy plugins directory to build directory for plugin loading tests
# This ensures test_plugin_loading.cpp can find hello_plugin.kplugin
add_custom_command(TARGET kalahari-tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/plugins
        ${CMAKE_BINARY_DIR}/plugins
    COMMENT "Copying plugins/ to build directory for tests"
)

# OpenSpec #00041: Copy Qt SQL drivers for database tests
if(WIN32)
    add_custom_command(TARGET kalahari-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/Qt6/plugins/sqldrivers"
            "$<TARGET_FILE_DIR:kalahari-tests>/sqldrivers"
        COMMENT "Copying Qt SQL drivers for tests"
    )
endif()

# Register tests with CTest (commented out until Day 4 - when we have real tests)
# include(CTest)
# include(Catch)
# catch_discover_tests(kalahari-tests)

# For now: manual test execution
add_test(NAME kalahari-basic-tests
         COMMAND kalahari-tests
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

message(STATUS "Configured test target: kalahari-tests")
