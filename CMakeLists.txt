cmake_minimum_required(VERSION 3.21)

# Project definition
project(Kalahari
    VERSION 0.3.0
    DESCRIPTION "Writer's IDE - Advanced writing environment for book authors"
    LANGUAGES CXX
)

# C++20 standard (required)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for IDE support (VSCode, CLion, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug/Release/RelWithDebInfo/MinSizeRel)" FORCE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Platform detection
if(WIN32)
    message(STATUS "Building for Windows")
    add_definitions(-DKALAHARI_PLATFORM_WINDOWS)
elseif(APPLE)
    message(STATUS "Building for macOS")
    add_definitions(-DKALAHARI_PLATFORM_MACOS)
elseif(UNIX)
    message(STATUS "Building for Linux")
    add_definitions(-DKALAHARI_PLATFORM_LINUX)
endif()

# Compiler warnings (strict)
if(MSVC)
    # Microsoft Visual C++
    add_compile_options(/W4 /WX)
else()
    # GCC or Clang
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Sanitizers (Debug builds only, GCC/Clang)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
    option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
    option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

    if(ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
        message(STATUS "AddressSanitizer ENABLED")
    endif()

    if(ENABLE_UBSAN)
        add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=undefined)
        message(STATUS "UndefinedBehaviorSanitizer ENABLED")
    endif()
endif()

# Link-Time Optimization (Release builds)
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)

    if(IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "Link-Time Optimization (LTO/IPO) ENABLED")
    else()
        message(STATUS "Link-Time Optimization NOT supported: ${IPO_ERROR}")
    endif()
endif()

# clang-tidy integration (optional)
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        message(STATUS "clang-tidy ENABLED: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy requested but not found")
    endif()
endif()

# vcpkg integration
# Note: vcpkg toolchain file should be specified via:
#   cmake -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake ..
# Or set CMAKE_TOOLCHAIN_FILE environment variable
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
else()
    message(WARNING "CMAKE_TOOLCHAIN_FILE not set. vcpkg dependencies will not be available.")
    message(WARNING "Set it with: cmake -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake ..")
endif()

# Find dependencies (vcpkg will install these automatically)
# Task #00020: Network for IconDownloader, Xml for SvgConverter, SvgWidgets for preview
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Gui Network Xml SvgWidgets)

# LGPL v3 Compliance Check - Verify Qt is dynamically linked
# First check: triplet name (fast, catches obvious static triplets)
set(LGPL_VIOLATION FALSE)
set(LGPL_REASON "")

if(DEFINED VCPKG_TARGET_TRIPLET)
    # Check if triplet explicitly contains "-static" (but NOT "-static-md" which is dynamic runtime)
    # Static triplets: x64-linux-static, x64-windows-static
    # Dynamic triplets: x64-linux, x64-windows, x64-linux-dynamic, x64-windows-static-md (dynamic CRT)
    string(FIND "${VCPKG_TARGET_TRIPLET}" "-static" STATIC_POS)
    string(FIND "${VCPKG_TARGET_TRIPLET}" "-static-md" STATIC_MD_POS)

    if(STATIC_POS GREATER -1 AND STATIC_MD_POS EQUAL -1)
        # Contains "-static" but NOT "-static-md"
        set(LGPL_VIOLATION TRUE)
        set(LGPL_REASON "Static triplet detected: ${VCPKG_TARGET_TRIPLET}")
    endif()
endif()

# Second check: actual Qt library type (fallback verification)
if(NOT LGPL_VIOLATION)
    get_target_property(QT_CORE_TYPE Qt6::Core TYPE)
    if(QT_CORE_TYPE STREQUAL "STATIC_LIBRARY")
        # Double-check: some platforms report STATIC_LIBRARY for imported shared libs
        # Only fail if triplet also suggests static linking
        if(DEFINED VCPKG_TARGET_TRIPLET)
            string(FIND "${VCPKG_TARGET_TRIPLET}" "-static" STATIC_POS)
            string(FIND "${VCPKG_TARGET_TRIPLET}" "-static-md" STATIC_MD_POS)
            if(STATIC_POS GREATER -1 AND STATIC_MD_POS EQUAL -1)
                set(LGPL_VIOLATION TRUE)
                set(LGPL_REASON "Qt6::Core is STATIC_LIBRARY with static triplet")
            else()
                message(STATUS "Note: Qt6::Core reports STATIC_LIBRARY but triplet '${VCPKG_TARGET_TRIPLET}' is dynamic - proceeding")
            endif()
        else()
            # No triplet info, trust the library type
            set(LGPL_VIOLATION TRUE)
            set(LGPL_REASON "Qt6::Core is STATIC_LIBRARY (no triplet info available)")
        endif()
    endif()
endif()

if(LGPL_VIOLATION)
    message(FATAL_ERROR
        "========================================\n"
        "LGPL v3 VIOLATION DETECTED!\n"
        "========================================\n"
        "Qt6 static linking detected. This violates LGPL v3 license.\n"
        "Reason: ${LGPL_REASON}\n"
        "\n"
        "Solutions:\n"
        "1. Use dynamic Qt triplet (e.g., x64-linux, x64-windows, x64-linux-dynamic)\n"
        "2. Avoid triplets with '-static' suffix (except '-static-md')\n"
        "3. Purchase Commercial Qt License: https://www.qt.io/licensing/\n"
        "\n"
        "Current triplet: ${VCPKG_TARGET_TRIPLET}\n"
        "========================================\n"
    )
endif()
message(STATUS "Qt6 LGPL v3 compliance: Dynamic linking verified (triplet: ${VCPKG_TARGET_TRIPLET})")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)
find_package(Python3 3.11 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Project version accessible in code
configure_file(
    ${CMAKE_SOURCE_DIR}/include/kalahari/version.h.in
    ${CMAKE_BINARY_DIR}/include/kalahari/version.h
    @ONLY
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include  # For generated version.h
)

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(PrecompiledHeaders)

# Subdirectories
add_subdirectory(src)
add_subdirectory(src/bindings)

# Convert ALL icons to use color placeholders (run before copying)
# This ensures SVG icons have {COLOR_PRIMARY} placeholders for theming
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/convert_all_icons.py"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE CONVERT_RESULT
    OUTPUT_VARIABLE CONVERT_OUTPUT
    ERROR_VARIABLE CONVERT_ERROR
)
if(CONVERT_RESULT EQUAL 0)
    message(STATUS "SVG icon conversion completed")
else()
    message(WARNING "SVG icon conversion failed: ${CONVERT_ERROR}")
endif()

# Copy resources directory to build output (icons, themes, etc.)
file(COPY ${CMAKE_SOURCE_DIR}/resources
     DESTINATION ${CMAKE_BINARY_DIR}/bin/)
message(STATUS "Copied resources/ to ${CMAKE_BINARY_DIR}/bin/")

# Enable testing
enable_testing()
add_subdirectory(tests)

# Installation rules (Phase 5)
# install(TARGETS kalahari DESTINATION bin)

# Print configuration summary
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Kalahari ${PROJECT_VERSION} - Build Configuration")
message(STATUS "========================================")
message(STATUS "Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "Source directory: ${CMAKE_SOURCE_DIR}")
message(STATUS "Build directory:  ${CMAKE_BINARY_DIR}")
message(STATUS "Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")
message(STATUS "")
