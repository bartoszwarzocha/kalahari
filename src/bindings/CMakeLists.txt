# pybind11 Python bindings for Kalahari core API

# Find pybind11 (should already be available from vcpkg)
find_package(pybind11 CONFIG REQUIRED)

# Create Python extension module
pybind11_add_module(kalahari_api
    python_bindings.cpp
)

# Set target properties
set_target_properties(kalahari_api PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(kalahari_api PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

# Link against Kalahari core library
target_link_libraries(kalahari_api PRIVATE
    kalahari_core
)

# Set output directory for Python module
# This allows tests to find kalahari_api module
set_target_properties(kalahari_api PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/python
)

# Also copy to bin for easier access during testing
add_custom_command(TARGET kalahari_api POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:kalahari_api>
        ${CMAKE_BINARY_DIR}/bin/
    COMMENT "Copying kalahari_api module to bin/"
)

message(STATUS "Configured pybind11 module: kalahari_api")
