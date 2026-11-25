// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file build_info.h
/// @brief What the program was built from: the commit, its branch, when and how
///
/// cmake/BuildInfo.cmake writes it at the build (from cmake/build_info.cpp.in) when the code is
/// another than at the build before. The About dialog shows it and copies it, to tell which
/// program a test or a report of a fault is about.

#pragma once

namespace kalahari::build {

/// @brief The short hash of the commit the program was built from; empty without git
[[nodiscard]] const char* commit();

/// @brief The branch of that commit; empty when no branch points at it
[[nodiscard]] const char* branch();

/// @brief Whether the code had changes not yet committed
[[nodiscard]] bool modified();

/// @brief When the program was built: ISO 8601 in UTC, e.g. "2026-11-22T13:05:00Z"
[[nodiscard]] const char* builtAt();

/// @brief The configuration of the build: Debug, Release, RelWithDebInfo or MinSizeRel
[[nodiscard]] const char* configuration();

}  // namespace kalahari::build
