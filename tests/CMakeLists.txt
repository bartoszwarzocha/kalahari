# Kalahari test suite (Catch2)

# Test source files
set(TEST_SOURCES
    test_main.cpp
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

# Link libraries (commented out until Day 3 - Catch2 from vcpkg)
# target_link_libraries(kalahari-tests PRIVATE
#     Catch2::Catch2WithMain
# )

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
