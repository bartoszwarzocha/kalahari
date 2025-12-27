# OpenSpec #00043 - Phase 1: Research & Spike
# Prototype benchmarks for editor performance rewrite

# Benchmark executable
add_executable(benchmark_prototypes
    benchmark_main.cpp
    piece_table_prototype.h
)

target_include_directories(benchmark_prototypes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(benchmark_prototypes PRIVATE
    Qt6::Core
    Qt6::Gui
)

set_target_properties(benchmark_prototypes PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

message(STATUS "Configured prototype benchmark: benchmark_prototypes")
