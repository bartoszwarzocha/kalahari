/// @file reset_singletons.h
/// @brief Test-only helper that returns every process-global singleton to a
///        deterministic baseline between test cases.
///
/// Sub-Project C — WS4.1. Used by the Catch2 event listener in test_main.cpp so
/// test ordering never leaks singleton state.

#pragma once

namespace kalahari {
namespace test {

/// @brief Reset all process-global singletons to a known baseline.
///
/// @note MAIN-THREAD ONLY. Catch2 tests run single-threaded on the QApplication
///       thread created in tests/test_main.cpp, so this is safe to call from the
///       listener's testCaseStarting() hook.
///
/// Order: configuration first, then derived visual state. Logger is
/// intentionally left untouched (append-only, harmless between tests).
void resetSingletons();

} // namespace test
} // namespace kalahari
