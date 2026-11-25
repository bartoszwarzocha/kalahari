// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file reading_time.h
/// @brief The reading time of a text as the program shows it

#ifndef KALAHARI_GUI_UTILS_READING_TIME_H
#define KALAHARI_GUI_UTILS_READING_TIME_H

#include <QString>

namespace kalahari::gui::utils {

/// @brief The reading time of @p words words (core::readingMinutes()) as the writer reads it
///
/// "18 min", from an hour on "12 h 31 min" (or "2 h" for whole hours), with the numbers in
/// the system's way. The status bar and the Properties panel show it.
/// @param words The words of the text
[[nodiscard]] QString readingTimeText(int words);

} // namespace kalahari::gui::utils

#endif // KALAHARI_GUI_UTILS_READING_TIME_H
