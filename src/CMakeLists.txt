# Kalahari executable target

# Source files
set(KALAHARI_SOURCES
    main.cpp
    core/logger.cpp
    gui/kalahari_app.cpp
    gui/main_window.cpp
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

# Link libraries
target_link_libraries(kalahari PRIVATE
    wx::core
    wx::base
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    libzip::zip
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
