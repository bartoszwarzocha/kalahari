# Kalahari test suite (Catch2)

# Test source files
set(TEST_SOURCES
    test_main.cpp
    core/test_settings_manager.cpp
    core/test_cmd_line_parser.cpp
    core/test_extension_points.cpp
    core/test_event_bus.cpp
    core/test_plugin_loading.cpp
    core/test_book_element.cpp
    core/test_part.cpp
    core/test_book.cpp
    core/test_document.cpp
    core/test_document_archive.cpp
    gui/test_command_registry.cpp  # Task #00025 - CommandRegistry unit tests (no display required)
    gui/test_command_registry_execution.cpp  # Task #00026 - CommandRegistry execution API tests
    gui/test_shortcut_manager.cpp     # Task #00027 - ShortcutManager unit tests (no display required)
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
    ${CMAKE_SOURCE_DIR}/src/core/python_interpreter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/book_element.cpp
    ${CMAKE_SOURCE_DIR}/src/core/part.cpp
    ${CMAKE_SOURCE_DIR}/src/core/book.cpp
    ${CMAKE_SOURCE_DIR}/src/core/document.cpp
    ${CMAKE_SOURCE_DIR}/src/core/document_archive.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/command.cpp  # Task #00024 - Command structures
    ${CMAKE_SOURCE_DIR}/src/gui/command_registry.cpp  # Task #00025 - CommandRegistry singleton
    ${CMAKE_SOURCE_DIR}/src/gui/shortcut_manager.cpp  # Task #00027 - ShortcutManager singleton
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
    wx::core
    wx::base
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    libzip::zip
    Python3::Python
    pybind11::embed
    bwx_gui  # For bwxTextDocument tests (Task #00019)
)

# Copy plugins directory to build directory for plugin loading tests
# This ensures test_plugin_loading.cpp can find hello_plugin.kplugin
add_custom_command(TARGET kalahari-tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/plugins
        ${CMAKE_BINARY_DIR}/plugins
    COMMENT "Copying plugins/ to build directory for tests"
)

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
