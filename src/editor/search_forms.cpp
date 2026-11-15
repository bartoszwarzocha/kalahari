// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file search_forms.cpp
/// @brief What the search finds with the option of all forms

#include <kalahari/editor/search_forms.h>

#include <kalahari/core/book_dictionary.h>
#include <kalahari/editor/spell_check_service.h>

#include <algorithm>

namespace kalahari::editor {

namespace {

/// @brief Whether @p text is one word: letters, also joined by hyphens and apostrophes
bool isOneWord(const QString& text) {
    return !text.isEmpty() && text.front().isLetter() && std::ranges::all_of(text, [](QChar c) {
               return c.isLetter() || c == QLatin1Char('-') || c == QLatin1Char('\'') ||
                      c == QChar(0x2019);  // ’
           });
}

/// @brief The forms of the words of the spelling dictionary that @p word is a form of, as
///        written and in lower case ("Smoka" at the start of a sentence), each once
QStringList wordForms(const QString& word, const SpellCheckService& spelling) {
    QStringList forms = spelling.relatedForms(word);
    if (const QString lower = word.toLower(); lower != word) {
        for (const QString& form : spelling.relatedForms(lower)) {
            if (!forms.contains(form)) {
                forms.append(form);
            }
        }
    }
    if (!forms.isEmpty() && !forms.contains(word, Qt::CaseInsensitive)) {
        forms.prepend(word);
    }
    return forms;
}

}  // namespace

QStringList formsToFind(const QString& text, const core::BookDictionary* dictionary,
                        const SpellCheckService* spelling) {
    const QString wanted = text.simplified();
    if (wanted.isEmpty()) {
        return {};
    }
    if (dictionary != nullptr) {
        if (QStringList names = dictionary->formsOf(wanted); !names.isEmpty()) {
            return names;
        }
    }
    if (spelling != nullptr && isOneWord(wanted)) {
        return wordForms(wanted, *spelling);
    }
    return {};
}

}  // namespace kalahari::editor
