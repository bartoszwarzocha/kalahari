// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file card_name_index.h
/// @brief The names of the cards in a text: which card a name at a place of a paragraph is

#ifndef KALAHARI_CORE_CARD_NAME_INDEX_H
#define KALAHARI_CORE_CARD_NAME_INDEX_H

#include <QHash>
#include <QString>

namespace kalahari::core {

class BookDictionary;

/// @brief The name of a card found in a text
struct CardNameMatch {
    QString card;          ///< The id of the card; empty: no name of a card there
    qsizetype start = 0;   ///< Where the name starts in the text
    qsizetype length = 0;  ///< How long it is there

    /// @brief Whether a name of a card was found
    [[nodiscard]] bool isFound() const { return !card.isEmpty(); }
};

/// @brief Every form of the names of the cards of the book's dictionary, and the card of each
///
/// The names are those of the dictionary that are names of cards (DictionaryName::card): the
/// name itself and its other names, each in every form (cardForms()), as they are written,
/// big and small letters too. A name of more words is found with any spaces between them
/// (no-break ones too), and not across a comma or another sign. A form that names two cards
/// names the one where it is not the first name or the surname alone that the program added
/// (NameVariant::automatic); when it names both alike, it names none.
class CardNameIndex {
public:
    /// @brief No names: nothing is found
    CardNameIndex() = default;

    /// @brief The names of the cards of @p dictionary, as it has them now
    explicit CardNameIndex(const BookDictionary& dictionary);

    /// @brief Whether it has no names of cards
    [[nodiscard]] bool isEmpty() const { return m_forms.isEmpty(); }

    /// @brief The name of a card at @p position of @p text, a paragraph: the longest one with
    /// the word at @p position in it, or the word that ends there
    /// @return Not found (CardNameMatch::isFound()) when no name of a card is there
    [[nodiscard]] CardNameMatch cardAt(const QString& text, qsizetype position) const;

private:
    /// @brief The card a form names
    struct Entry {
        QString card;            ///< Its id; empty: the form names two cards alike
        bool automatic = false;  ///< The form is of a name the program added
    };

    /// @brief Put form @p form of a name of card @p card in, @p automatic when the program
    /// added the name
    void add(const QString& form, const QString& card, bool automatic);

    QHash<QString, Entry> m_forms;  ///< A form, its spaces simplified -> its card
    qsizetype m_words = 0;          ///< The most words a form has
};

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_CARD_NAME_INDEX_H
