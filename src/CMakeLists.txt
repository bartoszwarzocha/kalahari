# Kalahari Core Library (shared with Python bindings)

# Core library source files
set(KALAHARI_CORE_SOURCES
    core/logger.cpp
    core/settings_manager.cpp
    core/python_interpreter.cpp
    core/cmd_line_parser.cpp
    core/diagnostic_manager.cpp
    core/plugin_manager.cpp
    core/plugin_manifest.cpp
    core/plugin_archive.cpp
    core/extension_points.cpp
    core/event_bus.cpp
    core/book_element.cpp
    core/part.cpp
    core/book.cpp
    core/document.cpp
    core/document_archive.cpp
)

# Create core library as SHARED for Python module compatibility
# SHARED library ensures Python module can link correctly across all platforms
add_library(kalahari_core SHARED ${KALAHARI_CORE_SOURCES})

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
target_link_libraries(kalahari_core PUBLIC
    Qt6::Core
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    libzip::zip
    Python3::Python
    pybind11::embed
)

# Static library - no post-build copy needed

# Kalahari executable target
# NOTE: GUI layer removed during Qt migration (Step 0.2)
# Will be rebuilt with Qt6 in subsequent phases

# Executable source files
set(KALAHARI_SOURCES
    main.cpp
    gui/main_window.cpp
    gui/settings_dialog.cpp
    gui/panels/editor_panel.cpp
    gui/panels/navigator_panel.cpp
    gui/panels/properties_panel.cpp
    gui/panels/search_panel.cpp
    gui/panels/assistant_panel.cpp
    gui/panels/log_panel.cpp
    # Headers with Q_OBJECT (needed for AUTOMOC to generate moc files)
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/main_window.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/settings_dialog.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/editor_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/navigator_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/properties_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/search_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/assistant_panel.h
    ${CMAKE_SOURCE_DIR}/include/kalahari/gui/panels/log_panel.h
)

# Create executable
add_executable(kalahari ${KALAHARI_SOURCES})

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

# Compile definitions
target_compile_definitions(kalahari PRIVATE
    KALAHARI_VERSION="${PROJECT_VERSION}"
)

# Link libraries (Qt6 Hello World - Task #00001)
target_link_libraries(kalahari PRIVATE
    kalahari_core
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
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
endif()

message(STATUS "Configured executable target: kalahari")
