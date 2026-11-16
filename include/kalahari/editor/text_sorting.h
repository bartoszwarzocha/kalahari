// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file text_sorting.h
/// @brief Sorting the rows of a table and paragraphs (Table > Sort): by text in the order of
///        a language, by numbers and by dates

#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

class QLocale;

namespace kalahari::editor {

/// @brief What the values of a key are read as
enum class SortType {
    Text,     ///< Alphabetically, in the order of the language
    Numbers,  ///< The first number of the text (2 before 10)
    Dates     ///< The first date of the text ("3.11.2026", "3 listopada 2026", "2026-11-03")
};

/// @brief A key of sorting: a column (of a row; 0 for paragraphs), how it is read, which way
struct SortKey {
    int column = 0;
    SortType type = SortType::Text;
    bool descending = false;
};

/// @brief How rows are sorted: by the first key, rows of the same value by the next one
struct SortOptions {
    std::vector<SortKey> keys;
    bool caseSensitive = false;
    QString language;  ///< ISO 639 code of the text's language, with or without a country
};

/// @brief The first number in the text, with a decimal point or comma; nothing without one
std::optional<double> firstNumberIn(const QString& text);

/// @brief The first date in the text, its numbers in the order of the language or its month
///        named in it (or in English); nothing without one
std::optional<QDate> firstDateIn(const QString& text, const QLocale& locale);

/// @brief The order of the rows sorted: the indexes of the rows, the first first
///
/// Each row is its texts by column. Rows of the same values keep their order; a row without
/// a number or a date of a key's column goes after the others, either way.
std::vector<int> sortedOrder(const std::vector<QStringList>& rows, const SortOptions& options);

}  // namespace kalahari::editor
