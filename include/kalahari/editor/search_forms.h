// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file search_forms.h
/// @brief What the search finds with the option of all forms ("Odm"): the forms of a name of
///        the book's dictionary, or of a word of the spelling dictionary

#ifndef KALAHARI_EDITOR_SEARCH_FORMS_H
#define KALAHARI_EDITOR_SEARCH_FORMS_H

#include <QString>
#include <QStringList>

namespace kalahari::core {
class BookDictionary;
}  // namespace kalahari::core

namespace kalahari::editor {

class SpellCheckService;

/// @brief The forms the option of all forms (SearchOptions::allForms) finds for @p text
///
/// For a name or another name of the book's dictionary, or a form of one: every form of it
/// ("Anna" -> "Anna", "Anny", "Annie"...), but not those of its other names ("Ania"). Else,
/// for one word: every form of the words of the spelling dictionary it is a form of
/// ("powiedział" -> "powiedziała", "powiedzieli"...), the word itself with them.
/// @param dictionary The book's dictionary; none: no names
/// @param spelling The spelling dictionary of the text; none: no words
/// @return The forms, each once; empty when it is neither (the text itself is found)
[[nodiscard]] QStringList formsToFind(const QString& text, const core::BookDictionary* dictionary,
                                      const SpellCheckService* spelling);

}  // namespace kalahari::editor

#endif  // KALAHARI_EDITOR_SEARCH_FORMS_H
