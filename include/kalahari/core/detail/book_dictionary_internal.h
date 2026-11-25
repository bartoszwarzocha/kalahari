// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_dictionary_internal.h
/// @brief What the source files of BookDictionary share (not part of its API): the words of a
///        text as the dictionary reads them, the places that go against the rules, new ids

#ifndef KALAHARI_CORE_DETAIL_BOOK_DICTIONARY_INTERNAL_H
#define KALAHARI_CORE_DETAIL_BOOK_DICTIONARY_INTERNAL_H

#include <kalahari/core/book_dictionary.h>

#include <QChar>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace kalahari::core::dictionary {

/// @brief A word of a text: where it starts and how long it is
struct TextWord {
    qsizetype start = 0;   ///< Where it starts
    qsizetype length = 0;  ///< How long it is
};

/// @brief Whether @p character belongs to a word: a letter or an accent
[[nodiscard]] bool isWordCharacter(QChar character);

/// @brief The words of @p text: letters, also joined by apostrophes and hyphens
[[nodiscard]] QList<TextWord> wordsOf(const QString& text);

/// @brief The words of a form or a spelling (split as wordsOf() splits a text)
[[nodiscard]] QStringList wordTexts(const QString& text);

/// @brief @p text with a capital first letter
[[nodiscard]] QString capitalFirst(const QString& text);

/// @brief @p text with a small first letter
[[nodiscard]] QString smallFirst(const QString& text);

/// @brief Whether @p text is in lower case
[[nodiscard]] bool isLowerCase(const QString& text);

/// @brief The word before an apostrophe in it (Anna's -> Anna), or empty
[[nodiscard]] QString beforeApostrophe(const QString& word);

/// @brief The words of @p text (a paragraph) that go against the rules, in their order
/// @param wrong The wrong spellings of the rules, by their first word
[[nodiscard]] QList<RuleDeviation> findDeviations(
    const QString& text, const QHash<QString, QList<WrongSpelling>>& wrong);

/// @brief A new id of a name or a rule
[[nodiscard]] QString newId();

}  // namespace kalahari::core::dictionary

#endif  // KALAHARI_CORE_DETAIL_BOOK_DICTIONARY_INTERNAL_H
