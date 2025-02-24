cmake_minimum_required(VERSION 3.21)

include(cmake/setup.cmake)
include(cmake/load_functions.cmake)
include(cmake/util_functions.cmake)

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

# General settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(VCPKG_KEEP_BUILT_PACKAGES ON CACHE BOOL "Keep vcpkg built packages" FORCE)

# Collect source files automatically
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/include/*.hpp" "${CMAKE_SOURCE_DIR}/include/*.h")

# Create the executable
add_executable(${PROJECT_NAME} ${SRC_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE ON)

# Correct local vcpkg installation directory (including triplet)
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    message(FATAL_ERROR "VCPKG_TARGET_TRIPLET is not set. Please provide it via -DVCPKG_TARGET_TRIPLET=<triplet>.")
endif()
set(VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")

#------------------------------------------------------------------------------
load_wxwidgets()
load_sqlite3()
load_hunspell()

#------------------------------------------------------------------------------
set_output_directories(${PROJECT_NAME})

message(STATUS "Project configured successfully.")
