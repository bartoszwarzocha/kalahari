// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_names.h
/// @brief The names of the cards of the Workshop in the book's dictionary
///
/// The forms of the name of a card of a character, a location or an item and its other names
/// are those of a name of the book's dictionary (DictionaryName) that has the id of the card
/// (DictionaryName::card): they are kept there once, and the card shows and changes them there.
/// A new card takes over a name of the dictionary without a card whose name is its title, with
/// its forms; else it gets a new one with the forms the program gives it.

#ifndef KALAHARI_CORE_CARD_NAMES_H
#define KALAHARI_CORE_CARD_NAMES_H

#include <kalahari/core/book_dictionary.h>
#include <kalahari/core/name_declension.h>

#include <QString>
#include <QStringList>

namespace kalahari::core {

struct ElementKind;

/// @brief What a name of a card of the kind is in the book's dictionary: a character, a place
///        for a location, an item; another name for the other kinds of cards
[[nodiscard]] NameKind nameKindOf(const ElementKind& kind);

/// @brief The name of a card in the dictionary: its own, else one without a card whose name
///        is @p title, in any case (the card takes it over, with its forms), else a new one
///        with the forms the program gives it; the name has the card's id and kind
/// @param declension The models of the language of the book
/// @param forms The forms the spelling dictionary knows (none: the models alone)
[[nodiscard]] DictionaryName nameOfCard(const BookDictionary& dictionary, const QString& cardId,
                                        const QString& title, NameKind kind,
                                        const NameDeclension& declension,
                                        const NameDeclension::DictionaryForms& forms = {});

/// @brief Give the name of a card the card's new title: the name itself gets its models and
///        its forms anew, the first name and the surname alone that the program adds follow it,
///        and the other names the author added stay
/// @return Whether the name changed (a title written as the name is changes nothing)
bool retitleName(DictionaryName& name, const QString& title, const NameDeclension& declension,
                 const NameDeclension::DictionaryForms& forms = {});

/// @brief The form of the name itself in a case of the singular ("Jana Wolskiego" in the
///        genitive); empty when the name has none there (a language without cases)
[[nodiscard]] QString formOfName(const DictionaryName& name, GrammaticalCase grammaticalCase);

/// @brief Whether a form of the name or of one of its other names has @p text in it, in any
///        letters ("janem" in "Janem Wolskim"); an empty text: true
[[nodiscard]] bool nameHas(const DictionaryName& name, const QString& text);

/// @brief What Find in Book of a card finds: the name and its other names, each in every
///        form, each once ("Jan Wolski", "Jana Wolskiego"... "Jasiek", "Jaśka"...)
[[nodiscard]] QStringList cardForms(const DictionaryName& name);

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_CARD_NAMES_H
