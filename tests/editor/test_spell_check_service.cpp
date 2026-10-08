/// @file test_spell_check_service.cpp
/// @brief Unit tests for SpellCheckService (OpenSpec #00042 Phase 6.4-6.9)

#include <catch2/catch_test_macros.hpp>
#include <QCoreApplication>

#include "kalahari/editor/spell_check_service.h"
#include "kalahari/editor/book_editor.h"

using namespace kalahari::editor;

// ============================================================================
// Construction and Basic State
// ============================================================================

TEST_CASE("SpellCheckService construction", "[editor][spell_check]") {
    SpellCheckService service;

    SECTION("initial state") {
        REQUIRE(service.isEnabled());
        REQUIRE_FALSE(service.isDictionaryLoaded());
        REQUIRE(service.currentLanguage().isEmpty());
        // Note: userDictionaryWords may not be empty if persisted from previous runs
    }
}

TEST_CASE("SpellCheckService enable/disable", "[editor][spell_check]") {
    SpellCheckService service;

    SECTION("disable") {
        service.setEnabled(false);
        REQUIRE_FALSE(service.isEnabled());
    }

    SECTION("enable") {
        service.setEnabled(false);
        service.setEnabled(true);
        REQUIRE(service.isEnabled());
    }
}

// ============================================================================
// User Dictionary Operations (no Hunspell required)
// ============================================================================

TEST_CASE("SpellCheckService user dictionary", "[editor][spell_check]") {
    SpellCheckService service;

    SECTION("add to user dictionary") {
        service.addToUserDictionary("customword");
        REQUIRE(service.isInUserDictionary("customword"));
        REQUIRE(service.userDictionaryWords().contains("customword"));
    }

    SECTION("remove from user dictionary") {
        service.addToUserDictionary("tempword");
        REQUIRE(service.isInUserDictionary("tempword"));
        service.removeFromUserDictionary("tempword");
        REQUIRE_FALSE(service.isInUserDictionary("tempword"));
    }

    SECTION("case sensitivity") {
        service.addToUserDictionary("MixedCase");
        // User dictionary should preserve case
        REQUIRE(service.isInUserDictionary("MixedCase"));
    }

    SECTION("multiple words") {
        // Get initial count (may have persisted words from previous runs)
        int initialCount = service.userDictionaryWords().size();

        service.addToUserDictionary("testword1_unique");
        service.addToUserDictionary("testword2_unique");
        service.addToUserDictionary("testword3_unique");
        auto words = service.userDictionaryWords();
        REQUIRE(words.size() == initialCount + 3);
        REQUIRE(words.contains("testword1_unique"));
        REQUIRE(words.contains("testword2_unique"));
        REQUIRE(words.contains("testword3_unique"));

        // Cleanup
        service.removeFromUserDictionary("testword1_unique");
        service.removeFromUserDictionary("testword2_unique");
        service.removeFromUserDictionary("testword3_unique");
    }

    SECTION("duplicate add is no-op") {
        int initialCount = service.userDictionaryWords().size();
        service.addToUserDictionary("duplicate_unique_test");
        service.addToUserDictionary("duplicate_unique_test");
        REQUIRE(service.userDictionaryWords().size() == initialCount + 1);
        REQUIRE(service.userDictionaryWords().count("duplicate_unique_test") == 1);

        // Cleanup
        service.removeFromUserDictionary("duplicate_unique_test");
    }
}

// ============================================================================
// Ignore Word (Session Only)
// ============================================================================

TEST_CASE("SpellCheckService ignore word", "[editor][spell_check]") {
    SpellCheckService service;

    SECTION("ignore word") {
        // Ignored words are session-only, not in user dictionary
        service.ignoreWord("ignoreme");
        // Note: ignoreWord doesn't add to user dictionary
        REQUIRE_FALSE(service.isInUserDictionary("ignoreme"));
    }
}

// ============================================================================
// BookEditor Integration
// ============================================================================

TEST_CASE("SpellCheckService BookEditor integration", "[editor][spell_check]") {
    // Note: Service must be declared AFTER editor to ensure correct destruction order
    // (editor destroyed after service disconnects from it)

    SECTION("set editor") {
        BookEditor editor;
        SpellCheckService service;
        service.setBookEditor(&editor);
        service.setBookEditor(nullptr);  // Disconnect before editor is destroyed
    }

    SECTION("set null editor") {
        BookEditor editor;
        SpellCheckService service;
        service.setBookEditor(&editor);
        service.setBookEditor(nullptr);
        // Should not crash
    }

    SECTION("change editor") {
        BookEditor editor1;
        BookEditor editor2;
        SpellCheckService service;
        service.setBookEditor(&editor1);
        service.setBookEditor(&editor2);
        service.setBookEditor(nullptr);  // Disconnect before editors are destroyed
    }
}

// ============================================================================
// Dictionary Loading (may fail if no dictionaries installed)
// ============================================================================

TEST_CASE("SpellCheckService dictionary loading", "[editor][spell_check]") {
    SpellCheckService service;

    SECTION("available dictionaries returns list") {
        auto dicts = service.availableDictionaries();
        // List may be empty if no dictionaries installed, but should not crash
        REQUIRE(dicts.size() >= 0);
    }

    SECTION("load nonexistent dictionary returns false") {
        bool result = service.loadDictionary("xx_YY_NONEXISTENT");
        REQUIRE_FALSE(result);
        REQUIRE_FALSE(service.isDictionaryLoaded());
    }
}

// ============================================================================
// Dictionaries shipped with Kalahari (resources/dictionaries)
// ============================================================================

TEST_CASE("SpellCheckService finds the shipped dictionaries", "[editor][spell_check]") {
    SpellCheckService service;
    const QStringList dicts = service.availableDictionaries();
    REQUIRE(dicts.contains("pl_PL"));
    REQUIRE(dicts.contains("en_US"));

    SECTION("a language picks its main dictionary") {
        REQUIRE(service.dictionaryFor("pl") == "pl_PL");
        REQUIRE(service.dictionaryFor("en") == "en_US");
        REQUIRE(service.dictionaryFor("pl-PL") == "pl_PL");
        REQUIRE(service.dictionaryFor("en_US") == "en_US");
    }

    SECTION("a language without a dictionary has none") {
        REQUIRE(service.dictionaryFor("xx").isEmpty());
        REQUIRE(service.dictionaryFor("").isEmpty());
    }
}

TEST_CASE("SpellCheckService checks Polish with the shipped dictionary", "[editor][spell_check]") {
    SpellCheckService service;
    REQUIRE(service.loadDictionary("pl_PL"));

    // Words with Polish letters: the dictionary is UTF-8, like the text
    REQUIRE(service.isCorrect(QStringLiteral("ksi\u0105\u017cka")));             // książka
    REQUIRE(service.isCorrect(QStringLiteral("\u017b\u00f3\u0142w")));          // Żółw
    REQUIRE(service.isCorrect(QStringLiteral("nies\u0142ychanie")));            // niesłychanie
    REQUIRE_FALSE(service.isCorrect(QStringLiteral("ksi\u0105rzka")));          // książka misspelled

    const QStringList suggestions = service.suggestions(QStringLiteral("ksi\u0105rzka"));
    REQUIRE(suggestions.contains(QStringLiteral("ksi\u0105\u017cka")));

    const auto errors = service.checkParagraph(
        QStringLiteral("Ta ksi\u0105\u017cka ma b\u0142\u0105d: ksi\u0105rzka."));
    REQUIRE(errors.size() == 1);
    REQUIRE(errors.first().word == QStringLiteral("ksi\u0105rzka"));
}

TEST_CASE("SpellCheckService checks English with the shipped dictionary", "[editor][spell_check]") {
    SpellCheckService service;
    REQUIRE(service.loadDictionary("en_US"));

    REQUIRE(service.isCorrect("writer"));
    REQUIRE(service.isCorrect("chapter"));
    REQUIRE_FALSE(service.isCorrect("chaptre"));
    REQUIRE(service.suggestions("chaptre").contains("chapter"));
}

// ============================================================================
// Checking without dictionary
// ============================================================================

TEST_CASE("SpellCheckService checking without dictionary", "[editor][spell_check]") {
    SpellCheckService service;
    // No dictionary loaded

    SECTION("isCorrect returns true when no dictionary") {
        // Without dictionary, all words are considered correct
        bool result = service.isCorrect("anyword");
        REQUIRE(result);
    }

    SECTION("suggestions returns empty when no dictionary") {
        auto suggestions = service.suggestions("misspeled");
        REQUIRE(suggestions.isEmpty());
    }

    SECTION("checkParagraph returns empty when no dictionary") {
        auto errors = service.checkParagraph("This is a tset with errrors.");
        REQUIRE(errors.isEmpty());
    }
}

// ============================================================================
// SpellErrorInfo struct
// ============================================================================

TEST_CASE("SpellErrorInfo struct", "[editor][spell_check]") {
    SECTION("default construction") {
        SpellErrorInfo info;
        REQUIRE(info.startPos == 0);
        REQUIRE(info.length == 0);
        REQUIRE(info.word.isEmpty());
        REQUIRE(info.suggestions.isEmpty());
    }

    SECTION("parameterized construction") {
        SpellErrorInfo info(5, 7, "misspel");
        REQUIRE(info.startPos == 5);
        REQUIRE(info.length == 7);
        REQUIRE(info.word == "misspel");
    }

    SECTION("equality comparison") {
        SpellErrorInfo info1(5, 7, "word");
        SpellErrorInfo info2(5, 7, "word");
        SpellErrorInfo info3(6, 7, "word");

        REQUIRE(info1 == info2);
        REQUIRE_FALSE(info1 == info3);
    }
}

