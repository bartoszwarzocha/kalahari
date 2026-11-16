// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_output_parts.h
/// @brief Parts of the windows of the export and of the print of the book: widths in letters,
///        remarks under the options, and the names of the choices in the settings

#pragma once

#include <QString>

#include <array>
#include <cstddef>
#include <string>
#include <utility>

class QCheckBox;
class QFontMetrics;
class QLabel;
class QWidget;

namespace kalahari::gui::dialogs::book_output {

/// @brief The width of @p letters letters of a text in the font of @p metrics. Not
///        averageCharWidth(): fonts give it differently (DejaVu Sans 6 pixels, Inter 8, for text
///        that takes 5.8 and 5.5), and the window would wrap its sentences into more lines in
///        one font than in another
int widthOfLetters(const QFontMetrics& metrics, int letters);

/// @brief A remark in grey that wraps
QLabel* newRemark(QWidget* parent, const QString& text);

/// @brief Check box @p box with @p remark right under it, in line with its text; @p beside,
///        when given, on the line of the box after it (an option of its own, which saves a line)
QWidget* withRemark(QCheckBox* box, QLabel* remark, QWidget* beside = nullptr);

/// @brief @p text in the label of a button: an '&' shows as itself, not as an access key
QString literal(QString text);

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
