// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_card_names.cpp
/// @brief The names of the cards of the Workshop in the book's dictionary: what a name of a
/// card is, the name a card takes over or gets, a new title, the forms and the other names a
/// card is found by, and the names a removed card leaves in the dictionary

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_dictionary.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/card_names.h>
#include <kalahari/core/name_declension.h>
#include <kalahari/core/project_manager.h>

#include "test_support/window_parts.h"

#include <QObject>
#include <QStringList>
#include <QTemporaryDir>

using kalahari::core::BookDictionary;
using kalahari::core::BookPlace;
using kalahari::core::DictionaryName;
using kalahari::core::GrammaticalCase;
using kalahari::core::KindRef;
using kalahari::core::makeName;
using kalahari::core::NameDeclension;
using kalahari::core::NameKind;
using kalahari::core::NameVariant;
using kalahari::core::ProjectManager;

namespace {

/// @p utf8 as a QString
QString text(const char* utf8) {
    return QString::fromUtf8(utf8);
}

/// A new book in @p dir open in @p pm, with the built-in book types
void openBook(ProjectManager& pm, const QTemporaryDir& dir) {
    ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.bookTypes().problems().isEmpty());
    REQUIRE(pm.createProject(dir.path(), "Names", "Anna", "pl", true, "kalahari.novel"));
}

/// The kind @p kindId of the Base
KindRef baseKind(const ProjectManager& pm, const char* kindId) {
    const KindRef kind =
        pm.bookTypes().findKind(QStringLiteral("kalahari.base"), QString::fromLatin1(kindId));
    REQUIRE(kind);
    return kind;
}

/// The texts of the variants of @p name, the name itself first
QStringList textsOf(const DictionaryName& name) {
    QStringList texts;
    for (const NameVariant& variant : name.variants) {
        texts << variant.text;
    }
    return texts;
}

}  // namespace

TEST_CASE("Card names: what a name of a card is", "[core][cards][dictionary]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    openBook(pm, dir);
    CHECK(kalahari::core::nameKindOf(*baseKind(pm, "character").kind) == NameKind::Character);
    CHECK(kalahari::core::nameKindOf(*baseKind(pm, "location").kind) == NameKind::Place);
    CHECK(kalahari::core::nameKindOf(*baseKind(pm, "item").kind) == NameKind::Item);
    CHECK(kalahari::core::nameKindOf(*baseKind(pm, "work_note").kind) == NameKind::Other);
}

TEST_CASE("Card names: the name a card takes over or gets", "[core][cards][dictionary]") {
    const NameDeclension polish(QStringLiteral("pl"));
    BookDictionary dictionary;

    SECTION("a card without a name gets a new one, with its other names") {
        const DictionaryName name = kalahari::core::nameOfCard(
            dictionary, QStringLiteral("c1"), QStringLiteral(" Anna  Wolska "),
            NameKind::Character, polish);
        CHECK(name.card == QStringLiteral("c1"));
        CHECK(textsOf(name) == QStringList{QStringLiteral("Anna Wolska"), QStringLiteral("Anna"),
                                           QStringLiteral("Wolska")});
        CHECK(kalahari::core::formOfName(name, GrammaticalCase::Genitive) ==
              text("Anny Wolskiej"));
    }

    SECTION("a name without a card is taken over with the forms the author wrote") {
        DictionaryName free = makeName(QStringLiteral("Ruth"), NameKind::Character, polish);
        free.variants[0].forms.cell(GrammaticalCase::Vocative, false) =
            QStringList{QStringLiteral("Ruth!")};
        free.variants[0].edited = QStringList{QStringLiteral("vocative")};
        const QString freeId = dictionary.putName(free);
        REQUIRE_FALSE(freeId.isEmpty());

        const DictionaryName name = kalahari::core::nameOfCard(
            dictionary, QStringLiteral("c2"), QStringLiteral("ruth"), NameKind::Character, polish);
        CHECK(name.id == freeId);
        CHECK(name.card == QStringLiteral("c2"));
        CHECK(name.variants.at(0).forms.cell(GrammaticalCase::Vocative, false) ==
              QStringList{QStringLiteral("Ruth!")});

        // A place of that name gives the other cells the forms of a place
        const DictionaryName place = kalahari::core::nameOfCard(
            dictionary, QStringLiteral("c3"), QStringLiteral("Ruth"), NameKind::Place, polish);
        CHECK(place.id == freeId);
        CHECK(place.kind == NameKind::Place);
        CHECK(place.variants.at(0).forms.cell(GrammaticalCase::Vocative, false) ==
              QStringList{QStringLiteral("Ruth!")});
    }

    SECTION("a card keeps its own name, and a name of another card is not taken over") {
        DictionaryName own = makeName(QStringLiteral("Jan"), NameKind::Character, polish);
        own.card = QStringLiteral("c4");
        const QString ownId = dictionary.putName(own);
        CHECK(kalahari::core::nameOfCard(dictionary, QStringLiteral("c4"),
                                         QStringLiteral("Jan Wolski"), NameKind::Character,
                                         polish)
                  .id == ownId);
        const DictionaryName other = kalahari::core::nameOfCard(
            dictionary, QStringLiteral("c5"), QStringLiteral("Jan"), NameKind::Character, polish);
        CHECK(other.id != ownId);
        CHECK(other.variants.size() == 1);
        CHECK(other.card == QStringLiteral("c5"));
    }
}

TEST_CASE("Card names: a new title of a card", "[core][cards][dictionary]") {
    const NameDeclension polish(QStringLiteral("pl"));
    DictionaryName name = makeName(QStringLiteral("Anna Wolska"), NameKind::Character, polish);
    NameVariant ania;
    ania.text = QStringLiteral("Ania");
    name.variants.append(ania);
    NameVariant wolska;  // the author's, of the same text as one the program adds
    wolska.text = QStringLiteral("wolska");
    name.variants.append(wolska);

    // The program's other names go with the old title, the author's stay
    REQUIRE(kalahari::core::retitleName(name, QStringLiteral(" Anna  Kowalska "), polish));
    CHECK(textsOf(name) == QStringList{QStringLiteral("Anna Kowalska"), QStringLiteral("Anna"),
                                       QStringLiteral("Kowalska"), QStringLiteral("Ania"),
                                       QStringLiteral("wolska")});
    CHECK(kalahari::core::formOfName(name, GrammaticalCase::Genitive) == text("Anny Kowalskiej"));

    // Not a new title: nothing changes
    CHECK_FALSE(kalahari::core::retitleName(name, QStringLiteral("Anna Kowalska"), polish));
    CHECK_FALSE(kalahari::core::retitleName(name, QStringLiteral("  "), polish));
    CHECK(name.variants.size() == 5);
}

TEST_CASE("Card names: the forms and the other names a card is found by",
          "[core][cards][dictionary]") {
    const NameDeclension polish(QStringLiteral("pl"));
    DictionaryName jan = makeName(QStringLiteral("Jan Wolski"), NameKind::Character, polish);
    NameVariant jasiek;
    jasiek.text = QStringLiteral("Jasiek");
    jan.variants.append(jasiek);

    CHECK(kalahari::core::nameHas(jan, QStringLiteral("janem")));
    CHECK(kalahari::core::nameHas(jan, QStringLiteral(" WOLSKIEGO ")));
    CHECK(kalahari::core::nameHas(jan, QStringLiteral("jasi")));
    CHECK(kalahari::core::nameHas(jan, QStringLiteral("  ")));
    CHECK_FALSE(kalahari::core::nameHas(jan, QStringLiteral("Anna")));

    const QStringList forms = kalahari::core::cardForms(jan);
    CHECK(forms.contains(QStringLiteral("Jan Wolski")));
    CHECK(forms.contains(QStringLiteral("Jana Wolskiego")));
    CHECK(forms.contains(QStringLiteral("Wolskim")));
    CHECK(forms.contains(QStringLiteral("Jasiek")));
    QStringList once = forms;
    CHECK(once.removeDuplicates() == 0);

    CHECK(kalahari::core::formOfName(DictionaryName{}, GrammaticalCase::Genitive).isEmpty());
    CHECK(kalahari::core::cardForms(DictionaryName{}).isEmpty());
}

TEST_CASE("Card names: a new card is said, a removed one leaves its name without a card",
          "[core][cards][dictionary][project_manager]") {
    const QTemporaryDir dir;
    auto& pm = kalahari::test::book();
    openBook(pm, dir);
    QStringList added;
    const QObject listener;
    QObject::connect(&pm, &ProjectManager::cardAdded, &listener,
                     [&added](const QString& elementId) { added << elementId; });

    const QString anna = pm.addElement(baseKind(pm, "character"), QStringLiteral("Anna Wolska"),
                                       BookPlace::Workshop);
    const QString note =
        pm.addElement(baseKind(pm, "work_note"), QStringLiteral("Anna"), BookPlace::Workshop);
    REQUIRE_FALSE(anna.isEmpty());
    REQUIRE_FALSE(note.isEmpty());
    CHECK(added == QStringList{anna});

    const NameDeclension polish(QStringLiteral("pl"));
    const DictionaryName name = kalahari::core::nameOfCard(
        pm.bookDictionary(), anna, QStringLiteral("Anna Wolska"), NameKind::Character, polish);
    REQUIRE(pm.changeBookDictionary(
        [&name](BookDictionary& dictionary) { dictionary.putName(name); }));
    const DictionaryName* named = pm.bookDictionary().findNameOfCard(anna);
    REQUIRE(named != nullptr);
    const QString nameId = named->id;

    int changes = 0;
    QObject::connect(&pm, &ProjectManager::bookDictionaryChanged, &listener,
                     [&changes]() { ++changes; });
    REQUIRE(pm.removeElement(note));
    CHECK(changes == 0);  // the note has no name
    REQUIRE(pm.removeElement(anna));
    CHECK(changes == 1);
    CHECK(pm.bookDictionary().findNameOfCard(anna) == nullptr);
    const DictionaryName* left = pm.bookDictionary().findNameWithText(QStringLiteral("anna wolska"));
    REQUIRE(left != nullptr);
    CHECK(left->id == nameId);
    CHECK(left->card.isEmpty());
}
