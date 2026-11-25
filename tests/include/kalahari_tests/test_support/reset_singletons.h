// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file reset_singletons.h
/// @brief Test-only helper that returns every process-global singleton to a
///        deterministic baseline between test cases.
///
/// Sub-Project C — WS4.1. Used by the Catch2 event listener in test_main.cpp so
/// test ordering never leaks singleton state.

#ifndef KALAHARI_TESTS_TEST_SUPPORT_RESET_SINGLETONS_H
#define KALAHARI_TESTS_TEST_SUPPORT_RESET_SINGLETONS_H

namespace kalahari::test {

/// @brief Reset all process-global singletons to a known baseline.
///
/// @note MAIN-THREAD ONLY. Catch2 tests run single-threaded on the QApplication
///       thread created in tests/test_main.cpp, so this is safe to call from the
///       listener's testCaseStarting() hook.
///
/// Order: configuration first, then derived visual state. Logger is
/// intentionally left untouched (append-only, harmless between tests).
void resetSingletons();

} // namespace kalahari::test

#endif  // KALAHARI_TESTS_TEST_SUPPORT_RESET_SINGLETONS_H
