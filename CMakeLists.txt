cmake_minimum_required(VERSION 3.21)

# Project definition
project(Kalahari
    VERSION 0.0.1
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

# Find dependencies (Day 3 - vcpkg will install these automatically)
find_package(wxWidgets CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

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

# Subdirectories
add_subdirectory(src)

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
