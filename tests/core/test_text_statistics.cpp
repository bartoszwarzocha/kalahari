/// @file test_text_statistics.cpp
/// @brief Unit tests for countText() - the application's single word count definition

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/text_statistics.h>
#include <QString>

using namespace kalahari::core;

namespace {

int words(const QString& text) {
    return countText(text, WordCountRules{}).words;
}

/// @brief Words counted with the dialogue dashes as words
int wordsWithDashes(const QString& text) {
    WordCountRules rules;
    rules.dashesAreWords = true;
    return countText(text, rules).words;
}

int characters(const QString& text) {
    return countText(text, WordCountRules{}).characters;
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
        CHECK(words(QStringLiteral("w domu")) == 2);
        CHECK(words(QStringLiteral("koniec Nowy")) == 2);
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

TEST_CASE("countText counts standalone dashes as words when the rules say so",
          "[core][text_statistics]") {
    SECTION("The dialogue dashes of a line of dialogue") {
        // The option of the settings: Microsoft Word counts them, LibreOffice does not
        CHECK(words(QStringLiteral("– Tak – powiedział.")) == 2);
        CHECK(wordsWithDashes(QStringLiteral("– Tak – powiedział.")) == 4);
        CHECK(words(QStringLiteral("— Yes — she said.")) == 3);
        CHECK(wordsWithDashes(QStringLiteral("— Yes — she said.")) == 5);
    }

    SECTION("Any run of dashes alone: hyphen, en dash, em dash, horizontal bar") {
        CHECK(wordsWithDashes(QStringLiteral("a - b")) == 3);
        CHECK(wordsWithDashes(QStringLiteral("a -- b")) == 3);
        CHECK(wordsWithDashes(QStringLiteral("a ――― b")) == 3);
        CHECK(wordsWithDashes(QStringLiteral("–")) == 1);
        CHECK(wordsWithDashes(QStringLiteral("koniec – Nowy")) == 3);
    }

    SECTION("A dash with other characters is no word of its own") {
        CHECK(wordsWithDashes(QStringLiteral("–Tak")) == 1);
        CHECK(wordsWithDashes(QStringLiteral("2010–2020")) == 1);
        CHECK(wordsWithDashes(QStringLiteral("e-mail")) == 1);
        CHECK(wordsWithDashes(QStringLiteral("–, a")) == 1);
        CHECK(wordsWithDashes(QStringLiteral("*** …")) == 0);
    }

    SECTION("The rules change the words only, not the characters") {
        WordCountRules rules;
        rules.dashesAreWords = true;
        const TextCounts with = countText(QStringLiteral("– Tak –"), rules);
        const TextCounts without = countText(QStringLiteral("– Tak –"), WordCountRules{});
        CHECK(with.characters == without.characters);
        CHECK(with.nonSpaceCharacters == without.nonSpaceCharacters);
    }
}

TEST_CASE("countText counts non-space characters", "[core][text_statistics]") {
    CHECK(countText(QString(), WordCountRules{}).nonSpaceCharacters == 0);
    CHECK(countText(QStringLiteral("abc def"), WordCountRules{}).nonSpaceCharacters == 6);
    CHECK(countText(QStringLiteral("– Tak !"), WordCountRules{}).nonSpaceCharacters == 5);
    // A surrogate pair is one character
    CHECK(countText(QString::fromUcs4(U"a\U0001F600"), WordCountRules{}).nonSpaceCharacters == 2);
}

TEST_CASE("countText counts characters with spaces, without line and paragraph breaks",
          "[core][text_statistics]") {
    CHECK(characters(QString()) == 0);
    CHECK(characters(QStringLiteral("abc def")) == 7);
    CHECK(characters(QStringLiteral("– Tak !")) == 7);
    CHECK(characters(QStringLiteral("a\tb")) == 3);
    // The breaks are not characters of the text, as in Word and LibreOffice
    CHECK(characters(QStringLiteral("ab\ncd")) == 4);
    CHECK(characters(QStringLiteral("ab\r\ncd")) == 4);
    CHECK(characters(QStringLiteral("ab cd ef")) == 6);
    // A surrogate pair is one character
    CHECK(characters(QString::fromUcs4(U"a \U0001F600")) == 3);
}

TEST_CASE("wordCountRules follow the setting", "[core][text_statistics]") {
    auto& settings = SettingsManager::getInstance();
    settings.set<bool>("editor.wordCount.dashesAsWords", true);
    CHECK(wordCountRules().dashesAreWords);
    settings.set<bool>("editor.wordCount.dashesAsWords", false);
    CHECK_FALSE(wordCountRules().dashesAreWords);
    CHECK(wordCountRules() == WordCountRules{});
}
