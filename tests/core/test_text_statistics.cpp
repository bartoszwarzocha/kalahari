/// @file test_text_statistics.cpp
/// @brief Unit tests for countText() - the application's single word count definition

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/text_statistics.h>
#include <QString>

using namespace kalahari::core;

namespace {

int words(const QString& text) {
    return countText(text).words;
}

}  // anonymous namespace

TEST_CASE("countText counts words", "[core][text_statistics]") {
    SECTION("Empty and whitespace-only text has no words") {
        CHECK(words(QString()) == 0);
        CHECK(words(QStringLiteral(" \t\n ")) == 0);
    }

    SECTION("Words are separated by any whitespace") {
        CHECK(words(QStringLiteral("One two three")) == 3);
        CHECK(words(QStringLiteral("  One\ttwo\nthree  ")) == 3);
        CHECK(words(QStringLiteral("One  two")) == 2);
    }

    SECTION("No-break space and paragraph separator separate words") {
        CHECK(words(QStringLiteral("w\u00A0domu")) == 2);
        CHECK(words(QStringLiteral("koniec\u2029Nowy")) == 2);
    }

    SECTION("Punctuation-only runs are not words") {
        CHECK(words(QStringLiteral("– Tak – powiedział.")) == 2);
        CHECK(words(QStringLiteral("***")) == 0);
        CHECK(words(QStringLiteral("Koniec …")) == 1);
    }

    SECTION("Runs with letters or digits are single words") {
        CHECK(words(QStringLiteral("don't e-mail 2026 „cytat”")) == 4);
        CHECK(words(QStringLiteral("Tom & Jerry")) == 2);
        CHECK(words(QStringLiteral("2010–2020")) == 1);
    }

    SECTION("Polish and other scripts") {
        CHECK(words(QStringLiteral("Zażółć gęślą jaźń")) == 3);
        CHECK(words(QStringLiteral("привет αβγ")) == 2);
    }

    SECTION("A letter outside the Basic Multilingual Plane makes a word") {
        // U+1D400 MATHEMATICAL BOLD CAPITAL A (a letter, stored as a surrogate pair)
        CHECK(words(QString::fromUcs4(U"\U0001D400")) == 1);
        // U+1F600 GRINNING FACE is not a letter
        CHECK(words(QString::fromUcs4(U"\U0001F600")) == 0);
    }
}

TEST_CASE("countText counts non-space characters", "[core][text_statistics]") {
    CHECK(countText(QString()).nonSpaceCharacters == 0);
    CHECK(countText(QStringLiteral("abc def")).nonSpaceCharacters == 6);
    CHECK(countText(QStringLiteral("– Tak\u00A0!")).nonSpaceCharacters == 5);
    // A surrogate pair is one character
    CHECK(countText(QString::fromUcs4(U"a\U0001F600")).nonSpaceCharacters == 2);
}
