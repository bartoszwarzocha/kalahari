// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file name_labels.h
/// @brief The words the program shows for the names of the book's dictionary, in the
///        program's language: the cases of their forms and their kinds

#ifndef KALAHARI_GUI_NAME_LABELS_H
#define KALAHARI_GUI_NAME_LABELS_H

#include <kalahari/core/name_declension.h>

#include <QString>

namespace kalahari::gui {

/// @brief The name of a case: "genitive"; in the plural, "genitive plural"
[[nodiscard]] QString caseLabel(core::GrammaticalCase grammaticalCase, bool inPlural);

/// @brief The name of a number: "singular", "plural" (the forms of a language without cases)
[[nodiscard]] QString numberLabel(bool inPlural);

/// @brief What a name of the kind is: "character", "location", "item", "name"
[[nodiscard]] QString nameKindLabel(core::NameKind kind);

/// @brief A kind as the windows offer it: "Character", "Location", "Item", "Other name"
[[nodiscard]] QString nameKindChoice(core::NameKind kind);

/// @brief The questions of a case, as the grammars ask them: "of whom? of what?" for the
///        genitive
[[nodiscard]] QString caseQuestion(core::GrammaticalCase grammaticalCase);

/// @brief A model of declension (NameDeclension::models()) as the windows offer it: its
///        sample word and the words that decline like it ("Anna – a feminine noun ending in
///        -a"), "Does not change" for NameDeclension::unchanged(); an unknown one as its id
[[nodiscard]] QString modelLabel(const QString& model);

/// @brief The heading of the names of the kind in a list: "Characters", "Locations", "Items",
///        "Other Names" (those of the kinds of the cards of the Workshop for the first three)
[[nodiscard]] QString nameKindGroup(core::NameKind kind);

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_NAME_LABELS_H
