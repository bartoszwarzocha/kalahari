// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file name_labels.cpp
/// @brief The words the program shows for the names of the book's dictionary

#include "kalahari/gui/name_labels.h"

#include "kalahari/core/name_declension.h"

#include <QCoreApplication>
#include <QLatin1String>
#include <QString>

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

QString nameKindChoice(core::NameKind kind) {
    // In the order of core::NameKind
    static const std::array<const char*, 4> kinds = {
        //: A kind of name of the book's dictionary in its window; in Polish: Postać
        QT_TRANSLATE_NOOP("NameLabels", "Character"),
        //: In Polish: Miejsce
        QT_TRANSLATE_NOOP("NameLabels", "Location"),
        //: In Polish: Przedmiot
        QT_TRANSLATE_NOOP("NameLabels", "Item"),
        //: A people, an order, an event; in Polish: Inna nazwa
        QT_TRANSLATE_NOOP("NameLabels", "Other name")};
    return NameLabels::tr(kinds.at(indexOf(kind)));
}

QString caseQuestion(core::GrammaticalCase grammaticalCase) {
    // In the order of core::GrammaticalCase
    static const std::array<const char*, core::CASE_COUNT> questions = {
        //: The questions of the nominative; in Polish: kto? co?
        QT_TRANSLATE_NOOP("NameLabels", "who? what?"),
        //: Of the genitive; in Polish: kogo? czego?
        QT_TRANSLATE_NOOP("NameLabels", "of whom? of what?"),
        //: Of the dative; in Polish: komu? czemu?
        QT_TRANSLATE_NOOP("NameLabels", "to whom? to what?"),
        //: Of the accusative; in Polish: kogo? co?
        QT_TRANSLATE_NOOP("NameLabels", "whom? what?"),
        //: Of the instrumental; in Polish: z kim? z czym?
        QT_TRANSLATE_NOOP("NameLabels", "with whom? with what?"),
        //: Of the locative; in Polish: o kim? o czym?
        QT_TRANSLATE_NOOP("NameLabels", "about whom? about what?"),
        //: Of the vocative; in Polish: o!
        QT_TRANSLATE_NOOP("NameLabels", "O!")};
    return NameLabels::tr(questions.at(indexOf(grammaticalCase)));
}

QString modelLabel(const QString& model) {
    // By the ids of core::NameDeclension::models()
    struct Model {
        const char* id;
        const char* label;
    };
    static const std::array<Model, 13> models = {{
        //: A model of declension: its sample word, then the words that decline like it. In
        //: Polish: Jan – męskie na spółgłoskę
        {.id = "jan",
         .label = QT_TRANSLATE_NOOP("NameLabels", "Jan – a man's name ending in a consonant")},
        //: In Polish: Marek – męskie z opadającym e (Marka)
        {.id = "marek",
         .label = QT_TRANSLATE_NOOP("NameLabels", "Marek – a man's name whose e drops (Marka)")},
        //: In Polish: Kuba – męskie na -a lub -o
        {.id = "kuba",
         .label = QT_TRANSLATE_NOOP("NameLabels", "Kuba – a man's name ending in -a or -o")},
        //: In Polish: Kowalski – przymiotnikowe męskie (-ski, -cki, -y)
        {.id = "kowalski",
         .label = QT_TRANSLATE_NOOP("NameLabels",
                                    "Kowalski – a masculine adjective (-ski, -cki, -y)")},
        //: In Polish: Anna – żeńskie na -a
        {.id = "anna",
         .label = QT_TRANSLATE_NOOP("NameLabels", "Anna – a feminine noun ending in -a")},
        //: In Polish: Kowalska – przymiotnikowe żeńskie (-ska, -cka, -owa)
        {.id = "kowalska",
         .label = QT_TRANSLATE_NOOP("NameLabels",
                                    "Kowalska – a feminine adjective (-ska, -cka, -owa)")},
        //: In Polish: Gdańsk – męskie nieżywotne, dopełniacz na -a
        {.id = "gdansk",
         .label = QT_TRANSLATE_NOOP("NameLabels", "Gdańsk – a masculine thing, genitive -a")},
        //: In Polish: Londyn – męskie nieżywotne, dopełniacz na -u
        {.id = "londyn",
         .label = QT_TRANSLATE_NOOP("NameLabels", "Londyn – a masculine thing, genitive -u")},
        //: In Polish: Gniezno – nijakie na -o
        {.id = "gniezno",
         .label = QT_TRANSLATE_NOOP("NameLabels", "Gniezno – a neuter noun ending in -o")},
        //: In Polish: Opole – nijakie na -e
        {.id = "opole",
         .label = QT_TRANSLATE_NOOP("NameLabels", "Opole – a neuter noun ending in -e")},
        //: In Polish: Zakopane – przymiotnikowe nijakie
        {.id = "zakopane",
         .label = QT_TRANSLATE_NOOP("NameLabels", "Zakopane – a neuter adjective")},
        //: An English name: its plural with -s. In Polish: Regularna – liczba mnoga z -s
        {.id = "regular",
         .label = QT_TRANSLATE_NOOP("NameLabels", "Regular – the plural with -s")},
        //: A word of a name that has the same form in every case. In Polish: Nie odmienia się
        {.id = "none",
         .label = QT_TRANSLATE_NOOP("NameLabels", "Does not change")},
    }};
    for (const Model& entry : models) {
        if (model == QLatin1String(entry.id)) {
            return NameLabels::tr(entry.label);
        }
    }
    return model;
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
