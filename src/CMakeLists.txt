# Kalahari Core Library (shared with Python bindings)

# Core library source files
set(KALAHARI_CORE_SOURCES
    core/logger.cpp
    core/settings_manager.cpp
    core/settings_schema.cpp         # Default values of all settings
    # OpenSpec #00042: Custom Text Editor
    editor/editor_types.cpp
    editor/kml_serializer.cpp  # Phase 11.2: QTextDocument to KML serializer
    editor/kml_format_registry.cpp  # OpenSpec #00043: Shared KML format registry
    editor/annotation.cpp  # Comments, TODOs and notes anchored to the text
    editor/clipboard_handler.cpp       # Phase 4.13-4.16: Clipboard operations
    editor/editor_appearance.cpp       # Phase 5: Editor appearance configuration
    core/database_schema_manager.cpp
    core/project_lock.cpp
    core/backup_manager.cpp
    core/project_database.cpp
    core/python_interpreter.cpp
    core/cmd_line_parser.cpp
    core/plugin_manager.cpp
    core/plugin_manifest.cpp
    core/plugin_archive.cpp
    core/plugin_signature.cpp   # OpenSpec #00038: Plugin signature verification (Ed25519)
    core/trusted_keys.cpp       # OpenSpec #00038: Trusted publisher key management
    core/extension_points.cpp
    core/event_bus.cpp
    core/book_element.cpp
    core/part.cpp
    core/book.cpp
    core/document.cpp
    core/document_archive.cpp
    core/chapter_document.cpp
    core/text_statistics.cpp    # Word/character counting shared by the whole application
    core/text_file.cpp          # Plain text files in their own encoding and line ends
    core/standalone_file.cpp    # Chapters and text files opened outside a book project
    core/book_type_package.cpp  # Book type packages: kinds, structure and styles as data
    core/book_type_registry.cpp # Book type packages loaded from folders
    core/book_project.cpp       # Book project as its .klh file saves it
    core/utils/svg_converter.cpp
    core/theme.cpp
    # NOTE: icon_downloader.cpp, theme_manager.cpp, icon_registry.cpp moved to kalahari executable
    # (avoid Q_OBJECT DLL export issues with MOC-generated symbols)
)

# Create core library as SHARED for Python module compatibility
# SHARED library ensures Python module can link correctly across all platforms
add_library(kalahari_core SHARED ${KALAHARI_CORE_SOURCES})

# Precompiled headers — activate the function defined in cmake/PrecompiledHeaders.cmake.
# Covers stdlib + spdlog + nlohmann_json. Qt6 headers are intentionally left out of the
# PCH set in Sub-Project D1 (they carry higher risk of MOC macro collisions and will be
# tuned in a later phase). Guarded with if(COMMAND ...) so the build still works if
# PrecompiledHeaders.cmake is ever removed or renamed.
if(COMMAND kalahari_add_pch)
    kalahari_add_pch(kalahari_core)
endif()

# Core library properties
set_target_properties(kalahari_core PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
)

# MSVC Release: Disable LTCG (Link-Time Code Generation) to avoid corrupted .obj files
# Issue: vcpkg Python3 + LTCG produces "unrecognized file format" errors in Release builds
if(MSVC)
    target_compile_options(kalahari_core PRIVATE
        $<$<CONFIG:Release>:/GL->  # Disable whole program optimization
    )
    target_link_options(kalahari_core PRIVATE
        $<$<CONFIG:Release>:/LTCG:OFF>  # Disable link-time code generation
    )
endif()

# On Windows with multi-config generators (Visual Studio), explicitly set output dirs
# This is needed because CMAKE_<CONFIG>_POSTFIX doesn't affect import library placement
if(MSVC AND CMAKE_CONFIGURATION_TYPES)
    set_target_properties(kalahari_core PROPERTIES
        PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
    # For multi-config, we need to handle each config
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER ${config} config_upper)
        set_target_properties(kalahari_core PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/lib"
            RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/lib"
            ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/lib"
        )
    endforeach()
endif()

# Core library include directories
target_include_directories(kalahari_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

# Core library dependencies (includes Qt6::Core for QSize/QPoint in SettingsManager)
# Task #00020: Qt6::Network for IconDownloader, Qt6::Xml for SvgConverter
# Task #00021: Qt6::Svg for IconRegistry (QSvgRenderer), Qt6::Gui for QPixmap/QIcon
# OpenSpec #00041: Qt6::Sql for ProjectDatabase (SQLite)
target_link_libraries(kalahari_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Svg
    Qt6::Network
    Qt6::Xml
    Qt6::Sql
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    libzip::zip
    Python3::Python
    pybind11::embed
    ${CMAKE_DL_LIBS}  # dladdr (Python home detection on Linux)
    OpenSSL::SSL      # OpenSpec #00038: Ed25519 signature verification
    OpenSSL::Crypto   # OpenSpec #00038: SHA256 hashing
)

# Kalahari executable target

# Executable source files
set(KALAHARI_SOURCES
    main.cpp
    core/utils/icon_downloader.cpp  # IconDownloader moved to executable (avoid moc DLL export issues)
    core/theme_manager.cpp  # ThemeManager moved to executable (avoid moc DLL export issues)
    core/icon_registry.cpp  # IconRegistry moved to executable (Task #00025: Q_OBJECT for theme signals)
    core/art_provider.cpp   # ArtProvider - Central Visual Resource Manager (OpenSpec #00026)
    core/log_panel_sink.cpp # LogPanelSink - spdlog Qt sink (OpenSpec #00024)
    core/resource_paths.cpp # ResourcePaths - Platform-aware resource locator (OpenSpec #00029)
    core/recent_books_manager.cpp # RecentBooksManager - Recent files management (OpenSpec #00030)
    core/project_manager.cpp # ProjectManager - Project lifecycle management (OpenSpec #00033)
    gui/kalahari_style.cpp  # KalahariStyle - Dynamic icon sizing (OpenSpec #00026)
    gui/command.cpp
    gui/command_registry.cpp
    gui/dialogs/about_dialog.cpp
    gui/dialogs/icon_downloader_dialog.cpp
    gui/dialogs/item_templates.cpp  # TemplateRegistry - Project/file templates (OpenSpec #00033)
    gui/dialogs/new_item_dialog.cpp  # NewItemDialog - New project/file dialog (OpenSpec #00033)
    gui/dialogs/add_to_project_dialog.cpp  # AddToProjectDialog - Add standalone file to project (OpenSpec #00033)
    gui/dialogs/toolbar_manager_dialog.cpp  # ToolbarManagerDialog - Visual Studio-style toolbar customization (OpenSpec #00031)
    gui/dialogs/kalahari_dialog.cpp  # KalahariDialog - base of the program's own dialogs
    gui/dialogs/message_dialog.cpp  # MessageDialog, TextInputDialog - own messages, questions, typed texts
    gui/dialogs/new_element_dialog.cpp  # NewElementDialog - new chapter, part or item in the Navigator
    gui/dialogs/rename_element_dialog.cpp  # RenameElementDialog - new name of an element in the Navigator
    gui/kalahari_syntax_highlighter.cpp
    gui/icon_registrar.cpp  # IconRegistrar - Icon registration extracted from MainWindow (OpenSpec #00038)
    gui/command_registrar.cpp  # CommandRegistrar - Command registration extracted from MainWindow (OpenSpec #00038)
    gui/diagnostic_controller.cpp  # DiagnosticController - Diagnostic/dev mode extracted from MainWindow (OpenSpec #00038)
    gui/dock_coordinator.cpp  # DockCoordinator - Dock/panel management extracted from MainWindow (OpenSpec #00038 Phase 4)
    gui/settings_coordinator.cpp  # SettingsCoordinator - Settings management extracted from MainWindow (OpenSpec #00038 Phase 5)
    gui/navigator_coordinator.cpp  # NavigatorCoordinator - Navigator handlers extracted from MainWindow (OpenSpec #00038 Phase 6)
    gui/document_coordinator.cpp  # DocumentCoordinator - Document operations extracted from MainWindow (OpenSpec #00038 Phase 7)
    gui/annotations_coordinator.cpp  # AnnotationsCoordinator - annotation commands and the Annotations panel
    gui/main_window.cpp
    gui/menu_builder.cpp
    gui/recent_books_menu.cpp
    gui/settings_dialog.cpp
    gui/settings/settings_page.cpp
    gui/settings/general_pages.cpp
    gui/settings/appearance_pages.cpp
    gui/settings/editor_pages.cpp
    gui/settings/annotations_page.cpp
    gui/settings/shortcuts_page.cpp  # Settings > Keyboard Shortcuts
    gui/shortcut_rules.cpp  # Which keys a command can have
    gui/shortcut_settings.cpp  # The user's keys in the settings and in the files
    gui/widget_keys.cpp  # Qt's widgets have the keys of Windows, also on Linux
    gui/widgets/color_config_widget.cpp
    gui/widgets/path_label.cpp  # A file path that wraps also inside a long name
    gui/widgets/length_spin_box.cpp
    gui/widgets/wrapping_label.cpp  # A label that wraps only where its line does not fit
    gui/widgets/shortcut_recorder.cpp  # A field that records key presses
    gui/widgets/standalone_info_bar.cpp  # StandaloneInfoBar widget (OpenSpec #00033 Phase F)
    gui/toolbar_manager.cpp
    gui/panels/annotation_card.cpp
    gui/panels/annotation_colors.cpp
    gui/panels/annotation_entry.cpp
    gui/panels/annotation_frame.cpp
    gui/panels/annotations_panel.cpp
    gui/panels/assistant_panel.cpp
    gui/panels/dashboard_panel.cpp
    gui/panels/dashboard_layouts.cpp  # The Dashboard on a narrow or a low panel
    gui/panels/editor_panel.cpp
    gui/panels/log_panel.cpp
    gui/panels/navigator_panel.cpp
    gui/panels/properties_panel.cpp
    gui/panels/search_panel.cpp
    gui/panels/tags_panel.cpp       # TagsPanel - Tags panel (OpenSpec #00042 Task 7.10)
    gui/utils/layout_utils.cpp  # OpenSpec #00038: Layout utilities (clearLayout)
    gui/utils/panel_toggle.cpp  # Show/hide toggles of the dock panels
    gui/utils/setting_toggle.cpp  # Toggles that show an on/off setting
    gui/utils/distraction_free_layout.cpp  # The main window's layout for Distraction-Free writing
    gui/utils/qt_standard_texts.cpp  # Qt's own texts (standard buttons) for the translations
    # OpenSpec #00042: BookEditor (Q_OBJECT requires executable, not DLL)
    editor/book_editor.cpp      # Phase 3.1: BookEditor widget
    editor/book_editor_annotations.cpp
    editor/book_editor_checking.cpp
    editor/book_editor_cursor.cpp
    editor/book_editor_editing.cpp
    editor/book_editor_formatting.cpp
    editor/book_editor_input.cpp
    editor/book_editor_search.cpp
    editor/book_editor_view.cpp
    editor/find_replace_bar.cpp  # Find/Replace bar of the editor
    editor/book_editor_accessible.cpp  # Task 7.16: Accessibility interface
    editor/buffer_commands.cpp  # Cursor position helpers
    editor/text_keys.cpp  # The keys of the text on each system
    editor/split_editor_panel.cpp  # Phase 5.8: Split view panel
    editor/statistics_collector.cpp  # Phase 6.10-6.13: Statistics collector
    editor/style_resolver.cpp        # Phase 6.1-6.3: Style resolver with inheritance
    editor/spell_check_service.cpp   # Phase 6.4-6.9: Spell check service with Hunspell
    editor/grammar_check_service.cpp # Phase 6.14-6.17: Grammar check service with LanguageTool
    editor/word_frequency_analyzer.cpp  # Phase 6.18-6.20: Word frequency analyzer
    editor/text_to_speech_service.cpp   # Phase 6.21-6.24: Text-to-speech service (optional Qt Speech)
    editor/tag_detector.cpp              # Task 7.10: Tag detector service
    editor/quick_insert_handler.cpp      # Tasks 7.11-7.12: Quick insert @ and # triggers
    editor/quick_insert_popup.cpp        # Tasks 7.11-7.12: Quick insert autocomplete popup
    editor/snapshot_manager.cpp          # Task 7.13: Snapshot (restore points) manager
    editor/viewport_manager.cpp          # OpenSpec #00043: Viewport manager for Word/Writer-style scrolling
    editor/search_engine.cpp             # OpenSpec #00044 Task 9.4: Search engine for Find/Replace
    editor/kml_document_model.cpp        # KML reader
    editor/kalahari_text_document_layout.cpp  # OpenSpec #00043 Phase 11.11: Custom layout without Qt leading gaps
    editor/text_source_adapter.cpp            # OpenSpec #00043 Phase 12.1: Text source adapters
    editor/editor_render_pipeline.cpp         # OpenSpec #00043 Phase 12.1: Unified render pipeline
    editor/editor_benchmark.cpp               # OpenSpec #00043 Phase 11.11: Performance benchmarks
    # Headers with Q_OBJECT (needed for AUTOMOC to generate moc files)
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/utils/icon_downloader.h  # IconDownloader with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/theme_manager.h  # ThemeManager with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/icon_registry.h  # IconRegistry with Q_OBJECT (Task #00025)
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/art_provider.h   # ArtProvider with Q_OBJECT (OpenSpec #00026)
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/recent_books_manager.h  # RecentBooksManager with Q_OBJECT (OpenSpec #00030)
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/project_manager.h  # ProjectManager with Q_OBJECT (OpenSpec #00033)
    ${CMAKE_SOURCE_DIR}/include/kalahari/core/log_panel_sink.h # LogPanelSink with Q_OBJECT (OpenSpec #00024)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/kalahari_style.h  # KalahariStyle with Q_OBJECT (OpenSpec #00026)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/recent_books_menu.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dock_coordinator.h  # DockCoordinator with Q_OBJECT (OpenSpec #00038 Phase 4)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/about_dialog.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/icon_downloader_dialog.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/toolbar_manager_dialog.h  # ToolbarManagerDialog with Q_OBJECT (OpenSpec #00031)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/new_item_dialog.h  # NewItemDialog with Q_OBJECT (OpenSpec #00033)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/add_to_project_dialog.h  # AddToProjectDialog with Q_OBJECT (OpenSpec #00033)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/kalahari_dialog.h  # KalahariDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/message_dialog.h  # MessageDialog, TextInputDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/new_element_dialog.h  # NewElementDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/dialogs/rename_element_dialog.h  # RenameElementDialog with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/kalahari_syntax_highlighter.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/main_window.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/diagnostic_controller.h  # DiagnosticController with Q_OBJECT (OpenSpec #00038)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings_coordinator.h  # SettingsCoordinator with Q_OBJECT (OpenSpec #00038 Phase 5)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/navigator_coordinator.h  # NavigatorCoordinator with Q_OBJECT (OpenSpec #00038 Phase 6)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/document_coordinator.h  # DocumentCoordinator with Q_OBJECT (OpenSpec #00038 Phase 7)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/annotations_coordinator.h  # AnnotationsCoordinator with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings_dialog.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings/settings_page.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings/settings_pages.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings/shortcuts_page.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/color_config_widget.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/length_spin_box.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/shortcut_recorder.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widgets/standalone_info_bar.h  # StandaloneInfoBar with Q_OBJECT (OpenSpec #00033)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/widget_keys.h  # WidgetKeys with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/utils/distraction_free_layout.h  # DistractionFreeLayout with Q_OBJECT
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/find_replace_bar.h  # FindReplaceBar with Q_OBJECT (OpenSpec #00044 Task 9.6)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/annotation_card.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/annotation_frame.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/annotations_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/assistant_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/dashboard_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/editor_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/log_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/navigator_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/properties_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/search_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/tags_panel.h     # TagsPanel with Q_OBJECT (OpenSpec #00042 Task 7.10)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/command_registry.h  # CommandRegistry with Q_OBJECT (OpenSpec #00040)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/book_editor.h  # BookEditor with Q_OBJECT (OpenSpec #00042 Phase 3.1)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/split_editor_panel.h  # SplitEditorPanel with Q_OBJECT (OpenSpec #00042 Phase 5.8)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/statistics_collector.h  # StatisticsCollector with Q_OBJECT (OpenSpec #00042 Phase 6)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/style_resolver.h  # StyleResolver with Q_OBJECT (OpenSpec #00042 Phase 6.1-6.3)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/spell_check_service.h  # SpellCheckService with Q_OBJECT (OpenSpec #00042 Phase 6.4-6.9)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/grammar_check_service.h  # GrammarCheckService with Q_OBJECT (OpenSpec #00042 Phase 6.14-6.17)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/word_frequency_analyzer.h  # WordFrequencyAnalyzer with Q_OBJECT (OpenSpec #00042 Phase 6.18-6.20)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/text_to_speech_service.h  # TextToSpeechService with Q_OBJECT (OpenSpec #00042 Phase 6.21-6.24)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/tag_detector.h  # TagDetector with Q_OBJECT (OpenSpec #00042 Task 7.10)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/quick_insert_handler.h  # QuickInsertHandler with Q_OBJECT (OpenSpec #00042 Tasks 7.11-7.12)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/quick_insert_popup.h    # QuickInsertPopup with Q_OBJECT (OpenSpec #00042 Tasks 7.11-7.12)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/snapshot_manager.h      # SnapshotManager with Q_OBJECT (OpenSpec #00042 Task 7.13)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/viewport_manager.h     # ViewportManager with Q_OBJECT (OpenSpec #00043 Phase 4)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/search_engine.h       # SearchEngine with Q_OBJECT (OpenSpec #00044 Task 9.4)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/kalahari_text_document_layout.h  # KalahariTextDocumentLayout with Q_OBJECT (OpenSpec #00043 Phase 11.11)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/editor_render_pipeline.h       # EditorRenderPipeline with Q_OBJECT (OpenSpec #00043 Phase 12.1)
    ${CMAKE_SOURCE_DIR}/include/kalahari/editor/editor_benchmark.h            # EditorBenchmark with Q_OBJECT (OpenSpec #00043 Phase 11.11)
)

# Create executable
add_executable(kalahari ${KALAHARI_SOURCES})

# Precompiled headers for the executable target (same rationale as kalahari_core above).
if(COMMAND kalahari_add_pch)
    kalahari_add_pch(kalahari)
endif()

# UI translations: lrelease compiles the .ts files into .qm files embedded under :/i18n.
# After changing UI strings, refresh the .ts files with: cmake --build <dir> --target update_translations
qt_add_translations(kalahari
    TS_FILES ${CMAKE_SOURCE_DIR}/translations/kalahari_pl.ts
    LUPDATE_OPTIONS -no-obsolete -locations none
)

# Target properties
set_target_properties(kalahari PROPERTIES
    OUTPUT_NAME "kalahari"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories (for this target)
target_include_directories(kalahari PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

# Link libraries (Qt6 Hello World - Task #00001)
# Task #00020: Added SvgWidgets for IconDownloaderDialog preview
# OpenSpec #00042: Added Hunspell for spell checking (via pkg-config)
target_link_libraries(kalahari PRIVATE
    kalahari_core
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::SvgWidgets
    ${HUNSPELL_LIBRARIES}
)
target_include_directories(kalahari PRIVATE ${HUNSPELL_INCLUDE_DIRS})
target_link_directories(kalahari PRIVATE ${HUNSPELL_LIBRARY_DIRS})

# OpenSpec #00042: Text-to-Speech (optional Qt Speech module)
if(QT_TEXTTOSPEECH_AVAILABLE)
    target_link_libraries(kalahari PRIVATE Qt6::TextToSpeech)
    target_compile_definitions(kalahari PRIVATE QT_TEXTTOSPEECH_LIB)
    message(STATUS "Text-to-Speech: Enabled (linked Qt6::TextToSpeech)")
else()
    message(STATUS "Text-to-Speech: Disabled (graceful degradation)")
endif()

# Copy resources to output directory after each build (OpenSpec #00029)
# This ensures resources are always up-to-date, unlike file(COPY) which only runs at configure time
add_custom_command(TARGET kalahari POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:kalahari>/resources"
    COMMENT "Copying resources to output directory"
)

# Platform-specific settings
if(WIN32)
    # Windows: GUI application (not console)
    set_target_properties(kalahari PROPERTIES WIN32_EXECUTABLE TRUE)

    # Unicode support
    target_compile_definitions(kalahari PRIVATE UNICODE _UNICODE)
elseif(APPLE)
    # macOS: Bundle structure
    set_target_properties(kalahari PROPERTIES MACOSX_BUNDLE TRUE)

    # macOS: Copy resources to bundle (OpenSpec #00029)
    add_custom_command(TARGET kalahari POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources"
            "$<TARGET_BUNDLE_CONTENT_DIR:kalahari>/Resources/resources"
        COMMENT "Copying resources to macOS bundle"
    )
endif()

# Copy Qt plugins after build (Windows/Linux/macOS)
# This fixes "no Qt platform plugin could be initialized" error
if(WIN32)
    # Windows: Copy Debug plugins from vcpkg_installed
    add_custom_command(TARGET kalahari POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/Qt6/plugins/platforms"
            "$<TARGET_FILE_DIR:kalahari>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/Qt6/plugins/styles"
            "$<TARGET_FILE_DIR:kalahari>/styles"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/Qt6/plugins/imageformats"
            "$<TARGET_FILE_DIR:kalahari>/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/Qt6/plugins/tls"
            "$<TARGET_FILE_DIR:kalahari>/tls"
        # OpenSpec #00041: SQLite database support
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/Qt6/plugins/sqldrivers"
            "$<TARGET_FILE_DIR:kalahari>/sqldrivers"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/bin/libcrypto-3-x64.dll"
            "$<TARGET_FILE_DIR:kalahari>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/$<IF:$<CONFIG:Debug>,debug,>/bin/libssl-3-x64.dll"
            "$<TARGET_FILE_DIR:kalahari>/"
        COMMENT "Copying Qt plugins and OpenSSL DLLs to output directory"
    )
elseif(UNIX AND NOT APPLE)
    # Linux: Find Qt plugins directory dynamically
    # Check if using system Qt6 (from apt) - skip plugin copying in that case
    # System Qt6 knows where its plugins are at runtime via built-in paths
    if(USE_SYSTEM_PACKAGES)
        message(STATUS "Using system Qt6 - plugins handled by system at runtime (skipping copy)")
    else()
        # vcpkg on Linux places plugins in different locations depending on triplet:
        # - x64-linux-dynamic: lib/qt6/plugins/
        # - system Qt: lib/qt6/plugins/ or lib/x86_64-linux-gnu/qt6/plugins/
        #
        # Qt6_DIR points to: <prefix>/share/Qt6 (cmake config location)
        # Need to find: <prefix>/lib/qt6/plugins/ or <prefix>/plugins/

        # Get vcpkg installed directory (Qt6_DIR is <prefix>/share/Qt6)
        get_filename_component(_qt6_share_dir "${Qt6_DIR}" DIRECTORY)  # <prefix>/share
        get_filename_component(_qt6_prefix "${_qt6_share_dir}" DIRECTORY)  # <prefix>

        message(STATUS "Qt6_DIR: ${Qt6_DIR}")
        message(STATUS "Qt6 prefix (calculated): ${_qt6_prefix}")

        # Try common plugin locations (ordered by likelihood)
        set(_qt6_plugin_candidates
            "${_qt6_prefix}/Qt6/plugins"           # vcpkg x64-linux-dynamic actual location
            "${_qt6_prefix}/lib/qt6/plugins"       # vcpkg x64-linux-dynamic alternative
            "${_qt6_prefix}/plugins"               # some vcpkg configurations
            "${_qt6_prefix}/lib/plugins"           # fallback
            "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/qt6/plugins"
            "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/plugins"
            # System Qt6 from apt (Ubuntu/Debian)
            "/usr/lib/x86_64-linux-gnu/qt6/plugins"
            "/usr/lib/qt6/plugins"
            "/usr/lib64/qt6/plugins"
        )

    set(_qt6_plugins_dir "")
    foreach(_candidate IN LISTS _qt6_plugin_candidates)
        message(STATUS "Checking Qt6 plugins at: ${_candidate}")
        if(EXISTS "${_candidate}/platforms")
            set(_qt6_plugins_dir "${_candidate}")
            message(STATUS "  -> Found platforms/ subdirectory!")
            break()
        endif()
    endforeach()

    if(_qt6_plugins_dir)
        message(STATUS "Using Qt6 plugins directory: ${_qt6_plugins_dir}")

        # REQUIRED: platforms plugin (application cannot start without it)
        if(EXISTS "${_qt6_plugins_dir}/platforms")
            add_custom_command(TARGET kalahari POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qt6_plugins_dir}/platforms"
                    "$<TARGET_FILE_DIR:kalahari>/platforms"
                COMMENT "Copying Qt6 platforms plugins"
            )
        else()
            message(FATAL_ERROR "Qt6 platforms plugin not found at ${_qt6_plugins_dir}/platforms - required for application to start")
        endif()

        # OPTIONAL: styles plugin (application works without it, just uses default style)
        if(EXISTS "${_qt6_plugins_dir}/styles")
            add_custom_command(TARGET kalahari POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qt6_plugins_dir}/styles"
                    "$<TARGET_FILE_DIR:kalahari>/styles"
                COMMENT "Copying Qt6 styles plugins"
            )
        else()
            message(STATUS "Qt6 styles plugin not found at ${_qt6_plugins_dir}/styles (optional, skipping)")
        endif()

        # OPTIONAL: imageformats plugin (application works without it, just limited image format support)
        if(EXISTS "${_qt6_plugins_dir}/imageformats")
            add_custom_command(TARGET kalahari POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${_qt6_plugins_dir}/imageformats"
                    "$<TARGET_FILE_DIR:kalahari>/imageformats"
                COMMENT "Copying Qt6 imageformats plugins"
            )
        else()
            message(STATUS "Qt6 imageformats plugin not found at ${_qt6_plugins_dir}/imageformats (optional, skipping)")
        endif()
    else()
        message(WARNING "Could not find Qt6 plugins directory!")
        message(WARNING "Searched locations:")
        foreach(_candidate IN LISTS _qt6_plugin_candidates)
            message(WARNING "  - ${_candidate}")
        endforeach()
        message(WARNING "Qt6_DIR: ${Qt6_DIR}")
        message(WARNING "VCPKG_TARGET_TRIPLET: ${VCPKG_TARGET_TRIPLET}")
        message(FATAL_ERROR "Qt6 plugins not found. Cannot build without platform plugin.")
    endif()
    endif()  # Close else() for USE_SYSTEM_PACKAGES
elseif(APPLE)
    # macOS: Plugins are embedded in .app bundle automatically by Qt
    # No manual copying needed
endif()

message(STATUS "Configured executable target: kalahari")
