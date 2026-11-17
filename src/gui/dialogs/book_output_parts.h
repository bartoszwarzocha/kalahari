// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_output_parts.h
/// @brief Parts of the windows of the export and of the print of the book: the names of the
///        choices in the settings (the widths in letters and the remarks under the options are
///        those of dialog_parts.h)

#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <utility>

namespace kalahari::gui::dialogs::book_output {

/// @brief The value of @p name in @p names; @p fallback for a name the list does not have
template <typename Value, std::size_t Size>
Value valueOfName(const std::array<std::pair<Value, const char*>, Size>& names,
                  const std::string& name, Value fallback) {
    for (const auto& [value, each] : names) {
        if (name == each) {
            return value;
        }
    }
    return fallback;
}

/// @brief The name of @p value in @p names
template <typename Value, std::size_t Size>
std::string nameOfValue(const std::array<std::pair<Value, const char*>, Size>& names,
                        Value value) {
    for (const auto& [each, name] : names) {
        if (each == value) {
            return name;
        }
    }
    return names.front().second;
}

}  // namespace kalahari::gui::dialogs::book_output
