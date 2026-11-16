// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_card_name_index.cpp
/// @brief The names of the cards in a text: which card a name at a place of a paragraph is

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_dictionary.h>
#include <kalahari/core/card_name_index.h>
#include <kalahari/core/card_names.h>
#include <kalahari/core/name_declension.h>

#include <QString>

using kalahari::core::BookDictionary;
using kalahari::core::CardNameIndex;
using kalahari::core::CardNameMatch;
using kalahari::core::NameDeclension;
using kalahari::core::NameKind;

namespace {

/// @p utf8 as a QString
QString text(const char* utf8) {
    return QString::fromUtf8(utf8);
}

/// Give @p dictionary the name of card @p card, @p title, with its forms in Polish
void addCard(BookDictionary& dictionary, const char* card, const char* title,
             NameKind kind = NameKind::Character) {
    const NameDeclension polish(QStringLiteral("pl"));
    const QString id = dictionary.putName(kalahari::core::nameOfCard(
        dictionary, QString::fromLatin1(card), text(title), kind, polish));
    REQUIRE_FALSE(id.isEmpty());
}

/// What is found in @p paragraph at the place of the first @p word in it, @p into it
CardNameMatch cardAt(const CardNameIndex& index, const QString& paragraph, const char* word,
                     qsizetype into = 1) {
    const qsizetype at = paragraph.indexOf(text(word));
    REQUIRE(at >= 0);
    return index.cardAt(paragraph, at + into);
}

/// The text of @p match in @p paragraph
QString foundText(const QString& paragraph, const CardNameMatch& match) {
    return paragraph.mid(match.start, match.length);
}

}  // namespace

TEST_CASE("Card name index: no names find nothing", "[core][cards][names]") {
    const CardNameIndex none;
    CHECK(none.isEmpty());
    CHECK_FALSE(none.cardAt(QStringLiteral("Anna Wolska"), 2).isFound());

    BookDictionary dictionary;
    // A name of the dictionary that is not a card's
    const NameDeclension polish(QStringLiteral("pl"));
    REQUIRE_FALSE(
        dictionary.putName(kalahari::core::makeName(QStringLiteral("Ruth"), NameKind::Character,
                                                    polish))
            .isEmpty());
    const CardNameIndex index(dictionary);
    CHECK(index.isEmpty());
    CHECK_FALSE(index.cardAt(QStringLiteral("Ruth"), 1).isFound());
}

TEST_CASE("Card name index: the name of a card in any form", "[core][cards][names]") {
    BookDictionary dictionary;
    addCard(dictionary, "anna", "Anna Wolska");
    addCard(dictionary, "eldrion", "Eldrion");
    addCard(dictionary, "forest", "Czarny Las", NameKind::Place);
    const CardNameIndex index(dictionary);
    REQUIRE_FALSE(index.isEmpty());

    SECTION("the whole name, from any of its words, and the word that ends at the cursor") {
        const QString paragraph = text("Spotkał Annę Wolską nad rzeką.");
        for (const char* word : {"Annę", "Wolską"}) {
            const CardNameMatch match = cardAt(index, paragraph, word);
            CHECK(match.card == QStringLiteral("anna"));
            CHECK(foundText(paragraph, match) == text("Annę Wolską"));
        }
        CHECK(cardAt(index, paragraph, "Wolską", 6).card == QStringLiteral("anna"));
        CHECK(cardAt(index, paragraph, "Annę", 0).card == QStringLiteral("anna"));
        CHECK_FALSE(cardAt(index, paragraph, "rzeką").isFound());
        CHECK_FALSE(cardAt(index, paragraph, "Spotkał").isFound());
    }

    SECTION("a form of a name of one word and of a place") {
        const QString paragraph = text("Bez Eldriona weszli do Czarnego Lasu.");
        CHECK(cardAt(index, paragraph, "Eldriona").card == QStringLiteral("eldrion"));
        const CardNameMatch forest = cardAt(index, paragraph, "Lasu");
        CHECK(forest.card == QStringLiteral("forest"));
        CHECK(foundText(paragraph, forest) == text("Czarnego Lasu"));
    }

    SECTION("the first name alone, the program's other name") {
        const QString paragraph = text("Anna, Wolska i Ania.");
        const CardNameMatch first = cardAt(index, paragraph, "Anna");
        CHECK(first.card == QStringLiteral("anna"));
        CHECK(foundText(paragraph, first) == QStringLiteral("Anna"));
        CHECK(cardAt(index, paragraph, "Wolska").card == QStringLiteral("anna"));
        CHECK_FALSE(cardAt(index, paragraph, "Ania").isFound());
    }

    SECTION("any spaces between the words, an apostrophe after the name") {
        const QString spaces = text("Anna  Wolska");
        const CardNameMatch match = index.cardAt(spaces, 1);
        CHECK(match.card == QStringLiteral("anna"));
        CHECK(match.length == spaces.size());
        CHECK(cardAt(index, text("Anna's eyes"), "Anna").card == QStringLiteral("anna"));
    }

    SECTION("not in another word, nor in other letters") {
        CHECK_FALSE(cardAt(index, text("Wolska-Nowak"), "Wolska").isFound());
        CHECK_FALSE(cardAt(index, text("Annasz"), "Annasz").isFound());
        CHECK_FALSE(cardAt(index, text("anna wolska"), "anna").isFound());
        CHECK_FALSE(index.cardAt(text("Anna  Wolska"), 5).isFound());  // Between the spaces
        CHECK_FALSE(index.cardAt(QString(), 0).isFound());
    }
}

TEST_CASE("Card name index: a form of two cards", "[core][cards][names]") {
    BookDictionary dictionary;
    addCard(dictionary, "anna", "Anna Wolska");
    addCard(dictionary, "other", "Anna Nowak");
    addCard(dictionary, "nowak", "Nowak");
    const CardNameIndex index(dictionary);

    // The first name alone of both cards: neither of them
    CHECK_FALSE(cardAt(index, QStringLiteral("Anna"), "Anna").isFound());
    // The whole name is one card's
    CHECK(cardAt(index, QStringLiteral("Anna Nowak"), "Anna").card == QStringLiteral("other"));
    // The name of a card before the surname alone of another
    CHECK(cardAt(index, QStringLiteral("Nowak"), "Nowak").card == QStringLiteral("nowak"));
}
