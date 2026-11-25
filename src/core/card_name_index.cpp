// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file card_name_index.cpp
/// @brief The names of the cards in a text: which card a name at a place of a paragraph is

#include "kalahari/core/card_name_index.h"

#include "kalahari/core/book_dictionary.h"

#include <QList>

#include <algorithm>
#include <iterator>
#include <utility>

namespace kalahari::core {

namespace {

/// A word of a text: where it starts and where it ends (after its last letter)
using Word = std::pair<qsizetype, qsizetype>;

/// Whether @p c is a letter of a word: a letter, a digit or a mark over a letter
bool isLetter(QChar c) {
    return c.isLetterOrNumber() || c.isMark();
}

/// Whether @p c joins two parts of one word: a hyphen ("Jean-Luc"). An apostrophe does not:
/// "Anna's" is the name "Anna", and "O'Brien" two words of one name.
bool isJoiner(QChar c) {
    return c == u'-' || c == u'‐' || c == u'‑';
}

/// Whether the character at @p index of @p text is in a word, @p inWord when the one before
/// it is: a letter, or a hyphen between two letters
bool isInWord(const QString& text, qsizetype index, bool inWord) {
    const QChar c = text.at(index);
    if (isLetter(c)) {
        return true;
    }
    return inWord && isJoiner(c) && index + 1 < text.size() && isLetter(text.at(index + 1));
}

/// The words of @p text, in its order
QList<Word> wordsOf(const QString& text) {
    QList<Word> words;
    qsizetype start = -1;
    for (qsizetype index = 0; index < text.size(); ++index) {
        const bool inWord = isInWord(text, index, start >= 0);
        if (inWord && start < 0) {
            start = index;
        } else if (!inWord && start >= 0) {
            words << Word{start, index};
            start = -1;
        }
    }
    if (start >= 0) {
        words << Word{start, text.size()};
    }
    return words;
}

}  // namespace

CardNameIndex::CardNameIndex(const BookDictionary& dictionary) {
    for (const DictionaryName& name : dictionary.names()) {
        if (name.card.isEmpty()) {
            continue;
        }
        for (const NameVariant& variant : name.variants) {
            add(variant.text, name.card, variant.automatic);
            for (const QString& form : variant.forms.allForms(name.plural)) {
                add(form, name.card, variant.automatic);
            }
        }
    }
}

void CardNameIndex::add(const QString& form, const QString& card, bool automatic) {
    const QString key = form.simplified();
    if (key.isEmpty()) {
        return;
    }
    m_words = std::max(m_words, wordsOf(key).size());
    const auto found = m_forms.find(key);
    if (found == m_forms.end()) {
        m_forms.insert(key, Entry{card, automatic});
    } else if (found->card == card) {
        found->automatic = found->automatic && automatic;
    } else if (found->automatic == automatic) {
        found->card.clear();  // Two cards alike: none
    } else if (found->automatic) {
        *found = Entry{card, automatic};
    }
}

CardNameMatch CardNameIndex::cardAt(const QString& text, qsizetype position) const {
    if (m_forms.isEmpty()) {
        return {};
    }
    const QList<Word> words = wordsOf(text);
    const auto at = std::find_if(words.cbegin(), words.cend(), [position](const Word& word) {
        return word.first <= position && position <= word.second;
    });
    if (at == words.cend()) {
        return {};
    }
    const qsizetype cursor = std::distance(words.cbegin(), at);
    // The longest name first: "Anna Wolska" before "Anna"
    for (qsizetype count = std::min(m_words, words.size()); count > 0; --count) {
        const qsizetype last = std::min(cursor, words.size() - count);
        for (qsizetype first = std::max<qsizetype>(0, cursor - count + 1); first <= last;
             ++first) {
            const qsizetype start = words.at(first).first;
            const qsizetype length = words.at(first + count - 1).second - start;
            const auto found = m_forms.constFind(text.mid(start, length).simplified());
            if (found != m_forms.cend()) {
                return found->card.isEmpty() ? CardNameMatch{}
                                             : CardNameMatch{found->card, start, length};
            }
        }
    }
    return {};
}

}  // namespace kalahari::core
