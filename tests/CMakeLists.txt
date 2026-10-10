# Kalahari test suite (Catch2)

# Test source files
set(TEST_SOURCES
    test_main.cpp
    test_support/reset_singletons.cpp  # Sub-Project C WS4.1: singleton reset hook
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
    core/test_text_statistics.cpp
    core/test_text_file.cpp         # Plain text files in their own encoding and line ends
    core/test_standalone_file.cpp   # Chapters and text files opened outside a book project
    core/test_project_database.cpp  # OpenSpec #00041: SQLite Project Database
    core/test_book_type_registry.cpp  # Book type packages and their registry
    core/test_book_project.cpp        # Book project and its .klh file
    core/test_project_manager.cpp
    core/test_logger.cpp
    core/test_trusted_keys.cpp      # Sub-Project C WS4.3: TrustedKeys unit tests
    core/test_art_provider_theme.cpp  # BUG-1: theme-change ordering in the icon stack
    core/test_theme_manager_apply.cpp  # Theme switch with color overrides in one pass
    core/test_plugin_manager.cpp
    core/test_python_interpreter.cpp
    core/test_python_interop.cpp
    # OpenSpec #00042: Custom Text Editor
    editor/test_editor_types.cpp
    editor/test_kml_load.cpp        # KML reading through BookEditor::fromKml
    editor/test_kml_serializer.cpp  # Phase 11.2.5: KmlSerializer tests
    ${CMAKE_SOURCE_DIR}/src/editor/annotation.cpp  # Comments, TODOs and notes
    editor/test_viewport_manager.cpp       # OpenSpec #00043: ViewportManager
    editor/test_book_editor.cpp             # Phase 3.1: BookEditor widget
    editor/test_clipboard_handler.cpp       # Phase 4.13-4.16: Clipboard
    editor/test_view_modes.cpp              # EditorAppearance and BookEditor view modes
    editor/test_statistics_collector.cpp    # Phase 6/Task 7.17: StatisticsCollector tests
    editor/test_word_frequency_analyzer.cpp # Phase 6/Task 7.17: WordFrequencyAnalyzer tests
    editor/test_spell_check_service.cpp     # Phase 6.4-6.9: SpellCheckService tests
    editor/test_grammar_check_service.cpp   # Phase 6.14-6.17: GrammarCheckService tests
    editor/test_book_editor_integration.cpp # Task 7.18: Integration tests
    editor/test_buffer_commands.cpp        # Cursor position helpers
    editor/test_search_engine.cpp          # OpenSpec #00044 Task 9.4: SearchEngine tests
    editor/test_kml_document_model.cpp     # KML reader tests
    editor/test_annotations.cpp            # Comments, TODOs and notes: model, file, editing
    editor/test_style_resolver.cpp         # Sub-Project C WS4.4: StyleResolver unit tests
    editor/test_editor_stage0_kml_roundtrip.cpp  # Editor Stage 0: KML load/save through BookEditor
    editor/test_editor_stage0_layout.cpp         # Editor Stage 0: layout/viewport geometry
    editor/test_editor_stage1.cpp                # Editor Stage 1: regressions for the quick fixes
    editor/test_editor_stage2.cpp                # Editor Stage 2: typography, paste/undo, on-demand layout
    editor/test_editor_stage3.cpp                # Editor Stage 3: drag and drop, find and replace, cursor
    editor/test_editor_stage4.cpp                # Editor Stage 4: page mode, zoom, typewriter scrolling
    editor/test_editor_stage5.cpp                # Editor Stage 5: highlight layer
    benchmarks/test_editor_stage0_benchmark.cpp  # Editor Stage 0: hidden benchmark, tag [benchmark]
    benchmarks/test_document_generator.cpp       # 150k-word KML generator for the benchmark
    gui/test_command_registry.cpp
    gui/test_command_registry_execution.cpp
    gui/test_command_shortcuts.cpp               # Editor Stage 3: one command per shortcut
    gui/test_settings_dialog.cpp                 # Editor Stage 4: settings pages are not squeezed
    gui/test_navigator_expansion.cpp             # Navigator expansion state on reopen
    gui/test_recent_books_menu.cpp               # File > Recent Books follows the list
    gui/test_panel_toggle.cpp                    # Panel toggles follow open panels
    gui/test_setting_toggle.cpp                  # Toggles follow on/off settings
    gui/test_dialogs.cpp                         # The program's own dialogs
    gui/test_message_dialog.cpp                  # Own messages, questions and typed texts
    gui/test_distraction_free_layout.cpp         # The window's layout for Distraction-Free writing
    gui/test_editor_panel.cpp                    # Editor panel between the settings and its editor
    gui/test_views_toolbar.cpp                   # Views toolbar: view modes and switches
    gui/test_editor_settings.cpp                 # Editors follow the editor settings
    gui/test_annotations_panel.cpp               # Annotations panel and its coordinator
    gui/test_dock_layout.cpp                     # The panels on the right: Annotations on top
)

# Windows: Add compatibility shims for Catch2 missing CRT symbols (Task #00035 blocker)
if(WIN32)
    list(APPEND TEST_SOURCES catch2_windows_compat.cpp)
endif()

set(TEST_SOURCES ${TEST_SOURCES}
    # Source files under test (needed for linking)
    ${CMAKE_SOURCE_DIR}/src/core/logger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/settings_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/settings_schema.cpp
    ${CMAKE_SOURCE_DIR}/src/core/cmd_line_parser.cpp
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
    ${CMAKE_SOURCE_DIR}/src/core/text_statistics.cpp
    ${CMAKE_SOURCE_DIR}/src/core/text_file.cpp
    ${CMAKE_SOURCE_DIR}/src/core/standalone_file.cpp
    ${CMAKE_SOURCE_DIR}/src/core/book_type_package.cpp
    ${CMAKE_SOURCE_DIR}/src/core/book_type_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/book_project.cpp
    ${CMAKE_SOURCE_DIR}/src/core/utils/svg_converter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/project_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/recent_books_manager.cpp
    # OpenSpec #00041: SQLite Project Database
    ${CMAKE_SOURCE_DIR}/src/core/project_database.cpp
    ${CMAKE_SOURCE_DIR}/src/core/database_schema_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/project_lock.cpp
    ${CMAKE_SOURCE_DIR}/src/core/backup_manager.cpp
    # OpenSpec #00042: Custom Text Editor
    ${CMAKE_SOURCE_DIR}/src/editor/editor_types.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/kml_serializer.cpp  # OpenSpec #00043 Phase 11.2: KmlSerializer
    ${CMAKE_SOURCE_DIR}/src/editor/kml_format_registry.cpp  # OpenSpec #00043: Shared KML format registry
    ${CMAKE_SOURCE_DIR}/src/editor/annotation.cpp  # Comments, TODOs and notes
    ${CMAKE_SOURCE_DIR}/src/editor/style_resolver.cpp    # Sub-Project C WS4.4: StyleResolver (Q_OBJECT)
    ${CMAKE_SOURCE_DIR}/src/editor/viewport_manager.cpp      # OpenSpec #00043: ViewportManager
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor.cpp            # Phase 3.1: BookEditor widget
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor_annotations.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor_checking.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor_cursor.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor_editing.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor_formatting.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor_input.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor_search.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor_view.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/find_replace_bar.cpp       # Find/Replace bar of the editor
    ${CMAKE_SOURCE_DIR}/src/editor/book_editor_accessible.cpp  # Task 7.16: Accessibility
    ${CMAKE_SOURCE_DIR}/src/editor/buffer_commands.cpp       # Cursor position helpers
    ${CMAKE_SOURCE_DIR}/src/editor/search_engine.cpp        # OpenSpec #00044 Task 9.4: SearchEngine
    ${CMAKE_SOURCE_DIR}/src/editor/kml_document_model.cpp  # KML reader
    ${CMAKE_SOURCE_DIR}/src/editor/kalahari_text_document_layout.cpp  # OpenSpec #00043 Phase 11.11: Custom layout
    ${CMAKE_SOURCE_DIR}/src/editor/text_source_adapter.cpp      # OpenSpec #00043 Phase 12.1: Text source adapters
    ${CMAKE_SOURCE_DIR}/src/editor/editor_render_pipeline.cpp   # OpenSpec #00043 Phase 12.1: Unified render pipeline
    ${CMAKE_SOURCE_DIR}/src/gui/panels/navigator_panel.cpp  # Navigator expansion state tests
    ${CMAKE_SOURCE_DIR}/src/gui/recent_books_menu.cpp       # File > Recent Books submenu
    # GUI dependencies for FindReplaceBar and the GUI tests
    ${CMAKE_SOURCE_DIR}/src/core/art_provider.cpp          # ArtProvider for icons
    ${CMAKE_SOURCE_DIR}/src/gui/command_registry.cpp       # Sub-Project C WS4.1: CommandRegistry for resetSingletons()
    ${CMAKE_SOURCE_DIR}/src/gui/command.cpp                 # Sub-Project C WS4.1: Command/KeyboardShortcut for CommandRegistry
    ${CMAKE_SOURCE_DIR}/src/gui/command_registrar.cpp       # Editor Stage 3: the application's commands, for shortcut checks
    ${CMAKE_SOURCE_DIR}/src/gui/settings_dialog.cpp         # Editor Stage 4: settings dialog layout checks
    ${CMAKE_SOURCE_DIR}/src/gui/settings/settings_page.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/settings/general_pages.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/settings/appearance_pages.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/settings/editor_pages.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/settings/annotations_page.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/color_config_widget.cpp  # ColorConfigWidget for SettingsDialog
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/length_spin_box.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/utils/layout_utils.cpp      # clearLayout for SettingsDialog
    ${CMAKE_SOURCE_DIR}/src/gui/utils/panel_toggle.cpp      # Show/hide toggles of the dock panels
    ${CMAKE_SOURCE_DIR}/src/gui/utils/setting_toggle.cpp    # Toggles that show an on/off setting
    ${CMAKE_SOURCE_DIR}/src/gui/utils/distraction_free_layout.cpp  # The window's layout for Distraction-Free writing
    ${CMAKE_SOURCE_DIR}/src/gui/panels/editor_panel.cpp     # Editor panel: settings to and from its editor
    ${CMAKE_SOURCE_DIR}/src/gui/panels/annotation_entry.cpp  # Annotations panel: filters and orders
    ${CMAKE_SOURCE_DIR}/src/gui/panels/annotation_card.cpp   # Annotations panel: cards
    ${CMAKE_SOURCE_DIR}/src/gui/panels/annotation_colors.cpp # Annotations: readable colors of cards
    ${CMAKE_SOURCE_DIR}/src/gui/panels/annotation_frame.cpp  # Annotations: the frame text is written in
    ${CMAKE_SOURCE_DIR}/src/gui/panels/annotations_panel.cpp # Annotations panel
    ${CMAKE_SOURCE_DIR}/src/gui/annotations_coordinator.cpp  # Annotation commands and the panel
    ${CMAKE_SOURCE_DIR}/src/gui/dock_coordinator.cpp        # The panels of the main window and their layout
    ${CMAKE_SOURCE_DIR}/src/gui/navigator_coordinator.cpp   # The Navigator's requests
    ${CMAKE_SOURCE_DIR}/src/gui/panels/properties_panel.cpp # Panels of DockCoordinator
    ${CMAKE_SOURCE_DIR}/src/gui/panels/dashboard_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/panels/log_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/panels/search_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/panels/assistant_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/standalone_info_bar.cpp
    ${CMAKE_SOURCE_DIR}/src/core/log_panel_sink.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/toolbar_manager.cpp         # Toolbars of the main window
    ${CMAKE_SOURCE_DIR}/src/gui/dialogs/toolbar_manager_dialog.cpp  # Toolbar Manager dialog for ToolbarManager
    ${CMAKE_SOURCE_DIR}/src/gui/dialogs/kalahari_dialog.cpp          # Base of the program's own dialogs
    ${CMAKE_SOURCE_DIR}/src/gui/dialogs/message_dialog.cpp           # Own messages, questions and typed texts
    ${CMAKE_SOURCE_DIR}/src/gui/dialogs/progress_dialog.cpp          # Own window for the progress of a long task
    ${CMAKE_SOURCE_DIR}/src/gui/dialogs/color_dialog.cpp             # Own window for choosing a color
    ${CMAKE_SOURCE_DIR}/src/gui/dialogs/new_element_dialog.cpp       # New element of the book in the Navigator
    ${CMAKE_SOURCE_DIR}/src/gui/dialogs/rename_element_dialog.cpp    # New name of an element in the Navigator
    ${CMAKE_SOURCE_DIR}/src/gui/icon_registrar.cpp          # Icons of the application's commands
    ${CMAKE_SOURCE_DIR}/src/core/icon_registry.cpp         # IconRegistry for ArtProvider
    ${CMAKE_SOURCE_DIR}/src/core/theme.cpp                 # Theme for ArtProvider
    ${CMAKE_SOURCE_DIR}/src/core/theme_manager.cpp         # ThemeManager for ArtProvider
    ${CMAKE_SOURCE_DIR}/src/core/resource_paths.cpp        # ResourcePaths for ThemeManager/IconRegistry
    ${CMAKE_SOURCE_DIR}/src/editor/clipboard_handler.cpp     # Phase 4.13-4.16: Clipboard
    ${CMAKE_SOURCE_DIR}/src/editor/editor_appearance.cpp    # Phase 5: Editor appearance
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
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/search_engine.h   # SearchEngine with Q_OBJECT (OpenSpec #00044 Task 9.4)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/style_resolver.h  # Sub-Project C WS4.4: StyleResolver with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/kalahari_text_document_layout.h  # KalahariTextDocumentLayout with Q_OBJECT (OpenSpec #00043 Phase 11.11)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/editor_render_pipeline.h  # EditorRenderPipeline with Q_OBJECT (OpenSpec #00043 Phase 12.1)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/find_replace_bar.h  # FindReplaceBar with Q_OBJECT (OpenSpec #00044 Task 9.6)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings_dialog.h  # SettingsDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings/settings_page.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings/settings_pages.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/color_config_widget.h  # ColorConfigWidget with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/length_spin_box.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/navigator_panel.h  # NavigatorPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/recent_books_menu.h  # RecentBooksMenu with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/editor_panel.h  # EditorPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/annotation_card.h  # AnnotationCard with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/annotation_frame.h  # AnnotationFrame with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/annotations_panel.h  # AnnotationsPanel with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/annotations_coordinator.h  # AnnotationsCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dock_coordinator.h  # DockCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/navigator_coordinator.h  # NavigatorCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/properties_panel.h  # Panels of DockCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/dashboard_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/log_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/search_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/assistant_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/standalone_info_bar.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/log_panel_sink.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/toolbar_manager_dialog.h  # ToolbarManagerDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/kalahari_dialog.h  # KalahariDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/message_dialog.h  # MessageDialog, TextInputDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/progress_dialog.h  # ProgressDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/color_dialog.h  # ColorDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/new_element_dialog.h  # NewElementDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/rename_element_dialog.h  # RenameElementDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/utils/distraction_free_layout.h  # DistractionFreeLayout with Q_OBJECT
    # GUI dependency headers for MOC
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/art_provider.h    # ArtProvider with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/command_registry.h # Sub-Project C WS4.1: CommandRegistry with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/icon_registry.h   # IconRegistry with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/theme_manager.h   # ThemeManager with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/project_manager.h # ProjectManager with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/recent_books_manager.h
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

# Source tree, for tests that read files of the repository (the example project)
target_compile_definitions(kalahari-tests PRIVATE
    KALAHARI_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
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
    ${CMAKE_DL_LIBS}  # dladdr (Python home detection on Linux)
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

# Register every Catch2 test case as a separate CTest test.
# PRE_TEST discovery runs the binary at ctest time, not at build time, so the
# build does not depend on runtime DLLs (Qt, Python) being on PATH.
include(Catch)
catch_discover_tests(kalahari-tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DISCOVERY_MODE PRE_TEST
)

# Process exit with a plugin still loaded, PluginManager created before the
# Python interpreter: its Python objects must not be released after
# py::finalize_interpreter(). The seed fixes an order that crashed before.
add_test(NAME plugin_manager.exit_after_python_finalize
    COMMAND kalahari-tests
        "Python interop: PluginManager accessible from C++,Plugin Manager - Loading"
        --order rand --rng-seed 1
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Python binding tests: plain scripts that exit with a non-zero code on failure.
# They import the kalahari_api module copied to bin/ by src/bindings.
# Registered only with system packages (Linux CI): vcpkg's Python is a static
# library, so the extension module carries its own interpreter (and Qt) copy and
# aborts when loaded into a separate python process.
if(USE_SYSTEM_PACKAGES)
    foreach(_script test_python_bindings test_event_bus)
        add_test(NAME python.${_script}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${_script}.py
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        set_tests_properties(python.${_script} PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_BINARY_DIR}/bin"
        )
    endforeach()
endif()

message(STATUS "Configured test target: kalahari-tests")
