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
    editor/test_kml_serializer.cpp  # Phase 11.2.5: KmlSerializer tests
    editor/test_kml_table.cpp  # Phase 1.12: Table elements
    ${CMAKE_SOURCE_DIR}/src/editor/kml_comment.cpp  # Phase 7.8: Comments feature
    editor/test_paragraph_layout.cpp  # Phase 2.1: Paragraph layout
    editor/test_table_layout.cpp  # Phase 2.6: Table layout
    # Phase 11.8: Removed test_text_buffer.cpp, test_lazy_layout_manager.cpp, test_format_layer.cpp, test_kml_converter.cpp
    # Phase 11.9: Removed test_virtual_scroll_manager.cpp, test_layout_manager.cpp
    editor/test_height_tree.cpp            # OpenSpec #00043: HeightTree B-tree
    editor/test_viewport_manager.cpp       # OpenSpec #00043: ViewportManager
    editor/test_render_engine.cpp          # OpenSpec #00043: RenderEngine
    editor/test_book_editor.cpp             # Phase 3.1: BookEditor widget
    editor/test_clipboard_handler.cpp       # Phase 4.13-4.16: Clipboard
    editor/test_view_modes.cpp              # Phase 5: ViewModes and EditorAppearance
    editor/test_statistics_collector.cpp    # Phase 6/Task 7.17: StatisticsCollector tests
    editor/test_word_frequency_analyzer.cpp # Phase 6/Task 7.17: WordFrequencyAnalyzer tests
    editor/test_spell_check_service.cpp     # Phase 6.4-6.9: SpellCheckService tests
    editor/test_grammar_check_service.cpp   # Phase 6.14-6.17: GrammarCheckService tests
    editor/test_book_editor_integration.cpp # Task 7.18: Integration tests
    editor/test_buffer_commands.cpp        # OpenSpec #00043 Phase 9 Task 9.3: Buffer commands tests
    editor/test_search_engine.cpp          # OpenSpec #00044 Task 9.4: SearchEngine tests
    editor/test_search_service.cpp         # OpenSpec #00043 Phase 11.7: SearchService tests
    editor/test_kml_document_model.cpp     # OpenSpec #00043: KmlDocumentModel lazy rendering tests
    # Phase 11.8: Removed test_snapshot_roundtrip.cpp (tested old KmlConverter)
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
    ${CMAKE_SOURCE_DIR}/src/editor/kml_serializer.cpp  # OpenSpec #00043 Phase 11.2: KmlSerializer
    ${CMAKE_SOURCE_DIR}/src/editor/kml_format_registry.cpp  # OpenSpec #00043: Shared KML format registry
    ${CMAKE_SOURCE_DIR}/src/editor/kml_table.cpp  # Phase 1.12: Table elements
    ${CMAKE_SOURCE_DIR}/src/editor/kml_comment.cpp  # Phase 7.8: Comments feature
    ${CMAKE_SOURCE_DIR}/src/editor/paragraph_layout.cpp  # Phase 2.1: Paragraph layout
    ${CMAKE_SOURCE_DIR}/src/editor/format_converter.cpp  # Phase 2.2: Format conversion
    ${CMAKE_SOURCE_DIR}/src/editor/table_layout.cpp      # Phase 2.6: Table layout
    # Phase 11.8: Removed text_buffer.cpp, lazy_layout_manager.cpp, format_layer.cpp, kml_converter.cpp
    # Phase 11.9: Removed virtual_scroll_manager.cpp, layout_manager.cpp, page_layout_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/height_tree.cpp           # OpenSpec #00043: HeightTree B-tree
    ${CMAKE_SOURCE_DIR}/src/editor/viewport_manager.cpp      # OpenSpec #00043: ViewportManager
    ${CMAKE_SOURCE_DIR}/src/editor/render_engine.cpp        # OpenSpec #00043: RenderEngine
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor.cpp            # Phase 3.1: BookEditor widget
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor_accessible.cpp  # Task 7.16: Accessibility
    ${CMAKE_SOURCE_DIR}/src/editor/buffer_commands.cpp       # OpenSpec #00043 Phase 9: Buffer undo commands
    ${CMAKE_SOURCE_DIR}/src/editor/search_engine.cpp        # OpenSpec #00044 Task 9.4: SearchEngine
    ${CMAKE_SOURCE_DIR}/src/editor/search_service.cpp      # OpenSpec #00043 Phase 11.7: SearchService
    ${CMAKE_SOURCE_DIR}/src/editor/kml_document_model.cpp  # OpenSpec #00043: KmlDocumentModel lazy rendering
    ${CMAKE_SOURCE_DIR}/src/gui/find_replace_bar.cpp       # OpenSpec #00044 Task 9.6: FindReplaceBar
    # GUI dependencies for FindReplaceBar
    ${CMAKE_SOURCE_DIR}/src/core/art_provider.cpp          # ArtProvider for icons
    ${CMAKE_SOURCE_DIR}/src/core/icon_registry.cpp         # IconRegistry for ArtProvider
    ${CMAKE_SOURCE_DIR}/src/core/theme.cpp                 # Theme for ArtProvider
    ${CMAKE_SOURCE_DIR}/src/core/theme_manager.cpp         # ThemeManager for ArtProvider
    ${CMAKE_SOURCE_DIR}/src/core/resource_paths.cpp        # ResourcePaths for ThemeManager/IconRegistry
    ${CMAKE_SOURCE_DIR}/src/core/stylesheet.cpp            # StyleSheet for ThemeManager
    ${CMAKE_SOURCE_DIR}/src/editor/clipboard_handler.cpp     # Phase 4.13-4.16: Clipboard
    ${CMAKE_SOURCE_DIR}/src/editor/editor_appearance.cpp    # Phase 5: Editor appearance
    ${CMAKE_SOURCE_DIR}/src/editor/view_modes.cpp           # Phase 5: View modes
    ${CMAKE_SOURCE_DIR}/src/editor/statistics_collector.cpp   # Phase 6/Task 7.17: Statistics
    ${CMAKE_SOURCE_DIR}/src/editor/word_frequency_analyzer.cpp # Phase 6/Task 7.17: Word frequency
    ${CMAKE_SOURCE_DIR}/src/editor/spell_check_service.cpp     # Phase 6.4-6.9: Spell check service
    ${CMAKE_SOURCE_DIR}/src/editor/grammar_check_service.cpp  # Phase 6.14-6.17: Grammar check service
    # MOC headers for Q_OBJECT classes
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/book_editor.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/statistics_collector.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/word_frequency_analyzer.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/spell_check_service.h  # SpellCheckService with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/grammar_check_service.h  # GrammarCheckService with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/viewport_manager.h  # ViewportManager with Q_OBJECT (OpenSpec #00043)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/render_engine.h    # RenderEngine with Q_OBJECT (OpenSpec #00043)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/search_engine.h   # SearchEngine with Q_OBJECT (OpenSpec #00044 Task 9.4)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/search_service.h # SearchSession with Q_OBJECT (OpenSpec #00043 Phase 11.7)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/kml_document_model.h  # KmlDocumentModel with Q_OBJECT (OpenSpec #00043)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/find_replace_bar.h  # FindReplaceBar with Q_OBJECT (OpenSpec #00044 Task 9.6)
    # GUI dependency headers for MOC
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/art_provider.h    # ArtProvider with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/icon_registry.h   # IconRegistry with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/theme_manager.h   # ThemeManager with Q_OBJECT
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
    Qt6::Svg        # For IconRegistry (QSvgRenderer) used by ArtProvider
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    libzip::zip
    Python3::Python
    pybind11::embed
    OpenSSL::SSL      # OpenSpec #00038: Ed25519 signature verification
    OpenSSL::Crypto   # OpenSpec #00038: SHA256 hashing
    ${HUNSPELL_LIBRARIES}  # OpenSpec #00042: Spell check service with Hunspell
    # bwx_gui removed - wxWidgets library no longer used (Qt6 migration)
)

# OpenSpec #00042: Hunspell include/library paths for spell check service
target_include_directories(kalahari-tests PRIVATE ${HUNSPELL_INCLUDE_DIRS})
target_link_directories(kalahari-tests PRIVATE ${HUNSPELL_LIBRARY_DIRS})

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

# OpenSpec #00043: Prototype benchmarks for editor performance rewrite
add_subdirectory(prototypes)

# Phase 11.8: Removed benchmark-new-arch target (used old TextBuffer/LazyLayoutManager architecture)
