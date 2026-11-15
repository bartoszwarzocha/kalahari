// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file name_labels.cpp
/// @brief The words the program shows for the names of the book's dictionary

#include "kalahari/gui/name_labels.h"

#include <QCoreApplication>

#include <array>
#include <cstddef>

namespace kalahari::gui {

/// @brief The context of the translations of the words for names
struct NameLabels {
    Q_DECLARE_TR_FUNCTIONS(NameLabels)
};

namespace {

/// @brief Where a case or a kind is in the tables below
template <typename Enum>
constexpr std::size_t indexOf(Enum value) {
    return static_cast<std::size_t>(value);
}

}  // namespace

QString caseLabel(core::GrammaticalCase grammaticalCase, bool inPlural) {
    // In the order of core::GrammaticalCase
    static const std::array<const char*, core::CASE_COUNT> cases = {
        //: A grammatical case of a name; in Polish: mianownik
        QT_TRANSLATE_NOOP("NameLabels", "nominative"),
        //: In Polish: dopełniacz
        QT_TRANSLATE_NOOP("NameLabels", "genitive"),
        //: In Polish: celownik
        QT_TRANSLATE_NOOP("NameLabels", "dative"),
        //: In Polish: biernik
        QT_TRANSLATE_NOOP("NameLabels", "accusative"),
        //: In Polish: narzędnik
        QT_TRANSLATE_NOOP("NameLabels", "instrumental"),
        //: In Polish: miejscownik
        QT_TRANSLATE_NOOP("NameLabels", "locative"),
        //: In Polish: wołacz
        QT_TRANSLATE_NOOP("NameLabels", "vocative")};
    const QString name = NameLabels::tr(cases.at(indexOf(grammaticalCase)));
    //: A grammatical case (%1, "genitive") in the plural; in Polish: %1 lm.
    return inPlural ? NameLabels::tr("%1 plural").arg(name) : name;
}

QString numberLabel(bool inPlural) {
    //: The number of a form of a name, in a language without cases
    return inPlural ? NameLabels::tr("plural") : NameLabels::tr("singular");
}

QString nameKindLabel(core::NameKind kind) {
    // In the order of core::NameKind
    static const std::array<const char*, 4> kinds = {
        //: What a name of the book's dictionary names: a person or another being of the story
        QT_TRANSLATE_NOOP("NameLabels", "character"),
        //: What a name of the book's dictionary names: a place
        QT_TRANSLATE_NOOP("NameLabels", "location"),
        //: What a name of the book's dictionary names: a thing
        QT_TRANSLATE_NOOP("NameLabels", "item"),
        //: What a name of the book's dictionary names: a people, an order, an event
        QT_TRANSLATE_NOOP("NameLabels", "name")};
    return NameLabels::tr(kinds.at(indexOf(kind)));
}

QString nameKindGroup(core::NameKind kind) {
    // In the order of core::NameKind
    static const std::array<const char*, 4> groups = {
        //: The heading of the characters of the book in a list
        QT_TRANSLATE_NOOP("NameLabels", "Characters"),
        //: The heading of the places of the book in a list
        QT_TRANSLATE_NOOP("NameLabels", "Locations"),
        //: The heading of the things of the book in a list
        QT_TRANSLATE_NOOP("NameLabels", "Items"),
        //: The heading of the other proper names of the book in a list
        QT_TRANSLATE_NOOP("NameLabels", "Other Names")};
    return NameLabels::tr(groups.at(indexOf(kind)));
}

}  // namespace kalahari::gui
