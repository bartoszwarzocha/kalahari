/// @file test_search_engine.cpp
/// @brief Unit tests for SearchEngine (OpenSpec #00044 Task 9.4)
///
/// Comprehensive tests for search functionality:
/// - Basic find operations
/// - Case sensitivity
/// - Whole word matching
/// - Regex search
/// - Navigation (next/previous)
/// - Wrap around

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <kalahari/editor/search_engine.h>
#include <QAbstractTextDocumentLayout>
#include <QTextCursor>
#include <QTextDocument>
#include <QUndoStack>
#include <algorithm>
#include <random>

using namespace kalahari::editor;

// =============================================================================
// SearchMatch Tests
// =============================================================================

TEST_CASE("SearchMatch basic properties", "[editor][search_engine][match]") {
    SECTION("Default match is invalid") {
        SearchMatch match;
        REQUIRE_FALSE(match.isValid());
        REQUIRE(match.start == 0);
        REQUIRE(match.length == 0);
        REQUIRE(match.end() == 0);
    }

    SECTION("Match with length is valid") {
        SearchMatch match;
        match.start = 10;
        match.length = 5;
        REQUIRE(match.isValid());
        REQUIRE(match.end() == 15);
    }

    SECTION("Match comparison") {
        SearchMatch m1, m2;
        m1.start = 5;
        m1.length = 3;
        m2.start = 10;
        m2.length = 3;

        REQUIRE(m1 < m2);
        REQUIRE_FALSE(m2 < m1);
    }

    SECTION("Match equality") {
        SearchMatch m1, m2;
        m1.start = 5;
        m1.length = 3;
        m2.start = 5;
        m2.length = 3;

        REQUIRE(m1 == m2);

        m2.start = 6;
        REQUIRE_FALSE(m1 == m2);
    }
}

// =============================================================================
// SearchOptions Tests
// =============================================================================

TEST_CASE("SearchOptions defaults", "[editor][search_engine][options]") {
    SearchOptions options;
    REQUIRE_FALSE(options.caseSensitive);
    REQUIRE_FALSE(options.wholeWord);
    REQUIRE_FALSE(options.useRegex);
    REQUIRE_FALSE(options.searchBackward);
    REQUIRE(options.wrapAround);
}

// =============================================================================
// SearchEngine Basic Tests
// =============================================================================

TEST_CASE("SearchEngine initialization", "[editor][search_engine]") {
    SearchEngine engine;

    REQUIRE(engine.document() == nullptr);
    REQUIRE(engine.searchText().isEmpty());
    REQUIRE(engine.replaceText().isEmpty());
    REQUIRE(engine.currentMatchIndex() == -1);
    REQUIRE(engine.totalMatchCount() == 0);
    REQUIRE_FALSE(engine.isActive());
}

TEST_CASE("SearchEngine configuration", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("Test content");

    SECTION("Set document") {
        engine.setDocument(&doc);
        REQUIRE(engine.document() == &doc);
    }

    SECTION("Set search text") {
        engine.setSearchText("test");
        REQUIRE(engine.searchText() == "test");
        REQUIRE(engine.isActive());
    }

    SECTION("Set replace text") {
        engine.setReplaceText("replacement");
        REQUIRE(engine.replaceText() == "replacement");
    }

    SECTION("Set options") {
        SearchOptions opts;
        opts.caseSensitive = true;
        opts.wholeWord = true;
        engine.setOptions(opts);

        SearchOptions result = engine.options();
        REQUIRE(result.caseSensitive);
        REQUIRE(result.wholeWord);
    }
}

// =============================================================================
// Basic Find Tests
// =============================================================================

TEST_CASE("SearchEngine basic find", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("Hello World Hello");

    engine.setDocument(&doc);
    engine.setSearchText("Hello");

    SECTION("Find all returns correct matches") {
        auto matches = engine.findAll();
        REQUIRE(matches.size() == 2);

        REQUIRE(matches[0].start == 0);
        REQUIRE(matches[0].length == 5);
        REQUIRE(matches[0].matchedText == "Hello");

        REQUIRE(matches[1].start == 12);
        REQUIRE(matches[1].length == 5);
        REQUIRE(matches[1].matchedText == "Hello");
    }

    SECTION("Total match count") {
        REQUIRE(engine.totalMatchCount() == 2);
    }

    SECTION("Find next from position 0") {
        auto match = engine.findNext(0);
        REQUIRE(match.isValid());
        REQUIRE(match.start == 0);
    }

    SECTION("Find next from position 1") {
        auto match = engine.findNext(1);
        REQUIRE(match.isValid());
        REQUIRE(match.start == 12);  // Second "Hello"
    }

    SECTION("Find previous from end") {
        auto match = engine.findPrevious(17);
        REQUIRE(match.isValid());
        REQUIRE(match.start == 12);  // Second "Hello"
    }

    SECTION("Find previous from position 10") {
        auto match = engine.findPrevious(10);
        REQUIRE(match.isValid());
        REQUIRE(match.start == 0);  // First "Hello"
    }
}

TEST_CASE("SearchEngine no matches", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("Hello World");

    engine.setDocument(&doc);
    engine.setSearchText("xyz");

    SECTION("Find all returns empty") {
        auto matches = engine.findAll();
        REQUIRE(matches.empty());
    }

    SECTION("Total match count is zero") {
        REQUIRE(engine.totalMatchCount() == 0);
    }

    SECTION("Find next returns invalid match") {
        auto match = engine.findNext(0);
        REQUIRE_FALSE(match.isValid());
    }

    SECTION("Navigation returns invalid match") {
        auto match = engine.nextMatch();
        REQUIRE_FALSE(match.isValid());

        match = engine.previousMatch();
        REQUIRE_FALSE(match.isValid());
    }
}

// =============================================================================
// Case Sensitivity Tests
// =============================================================================

TEST_CASE("SearchEngine case sensitivity", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("Hello HELLO hello");

    engine.setDocument(&doc);
    engine.setSearchText("Hello");

    SECTION("Case insensitive finds all variants") {
        SearchOptions opts;
        opts.caseSensitive = false;
        engine.setOptions(opts);

        auto matches = engine.findAll();
        REQUIRE(matches.size() == 3);
    }

    SECTION("Case sensitive finds exact match only") {
        SearchOptions opts;
        opts.caseSensitive = true;
        engine.setOptions(opts);

        auto matches = engine.findAll();
        REQUIRE(matches.size() == 1);
        REQUIRE(matches[0].start == 0);
        REQUIRE(matches[0].matchedText == "Hello");
    }
}

// =============================================================================
// Whole Word Tests
// =============================================================================

TEST_CASE("SearchEngine whole word matching", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("Hello HelloWorld WorldHello");

    engine.setDocument(&doc);
    engine.setSearchText("Hello");

    SECTION("Without whole word matches partial words") {
        SearchOptions opts;
        opts.wholeWord = false;
        engine.setOptions(opts);

        auto matches = engine.findAll();
        REQUIRE(matches.size() == 3);
    }

    SECTION("With whole word only matches complete words") {
        SearchOptions opts;
        opts.wholeWord = true;
        engine.setOptions(opts);

        auto matches = engine.findAll();
        REQUIRE(matches.size() == 1);
        REQUIRE(matches[0].start == 0);
    }
}

// =============================================================================
// Navigation Tests
// =============================================================================

TEST_CASE("SearchEngine navigation", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("A B A C A");

    engine.setDocument(&doc);
    engine.setSearchText("A");

    SECTION("Navigate through matches with nextMatch") {
        REQUIRE(engine.currentMatchIndex() == -1);

        auto m1 = engine.nextMatch();
        REQUIRE(m1.isValid());
        REQUIRE(m1.start == 0);
        REQUIRE(engine.currentMatchIndex() == 0);

        auto m2 = engine.nextMatch();
        REQUIRE(m2.isValid());
        REQUIRE(m2.start == 4);
        REQUIRE(engine.currentMatchIndex() == 1);

        auto m3 = engine.nextMatch();
        REQUIRE(m3.isValid());
        REQUIRE(m3.start == 8);
        REQUIRE(engine.currentMatchIndex() == 2);
    }

    SECTION("Navigate backwards with previousMatch") {
        // First go to last match
        engine.setCurrentMatchIndex(2);
        REQUIRE(engine.currentMatchIndex() == 2);

        auto m1 = engine.previousMatch();
        REQUIRE(m1.isValid());
        REQUIRE(m1.start == 4);
        REQUIRE(engine.currentMatchIndex() == 1);

        auto m2 = engine.previousMatch();
        REQUIRE(m2.isValid());
        REQUIRE(m2.start == 0);
        REQUIRE(engine.currentMatchIndex() == 0);
    }

    SECTION("Set current match index") {
        REQUIRE(engine.setCurrentMatchIndex(1));
        REQUIRE(engine.currentMatchIndex() == 1);
        REQUIRE(engine.currentMatch().start == 4);

        REQUIRE_FALSE(engine.setCurrentMatchIndex(100));
        REQUIRE_FALSE(engine.setCurrentMatchIndex(-1));
    }

    SECTION("Current match") {
        // Initially no current match
        auto match = engine.currentMatch();
        REQUIRE_FALSE(match.isValid());

        // After navigation
        engine.nextMatch();
        match = engine.currentMatch();
        REQUIRE(match.isValid());
        REQUIRE(match.start == 0);
    }
}

// =============================================================================
// Wrap Around Tests
// =============================================================================

TEST_CASE("SearchEngine wrap around", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("A B A");

    engine.setDocument(&doc);
    engine.setSearchText("A");

    SECTION("Wrap around enabled - next wraps to first") {
        SearchOptions opts;
        opts.wrapAround = true;
        engine.setOptions(opts);

        engine.setCurrentMatchIndex(1);  // Last match
        auto match = engine.nextMatch();
        REQUIRE(match.isValid());
        REQUIRE(engine.currentMatchIndex() == 0);  // Wrapped to first
    }

    SECTION("Wrap around enabled - previous wraps to last") {
        SearchOptions opts;
        opts.wrapAround = true;
        engine.setOptions(opts);

        engine.setCurrentMatchIndex(0);  // First match
        auto match = engine.previousMatch();
        REQUIRE(match.isValid());
        REQUIRE(engine.currentMatchIndex() == 1);  // Wrapped to last
    }

    SECTION("Wrap around disabled - next returns invalid at end") {
        SearchOptions opts;
        opts.wrapAround = false;
        engine.setOptions(opts);

        engine.setCurrentMatchIndex(1);  // Last match
        auto match = engine.nextMatch();
        REQUIRE_FALSE(match.isValid());
        REQUIRE(engine.currentMatchIndex() == 1);  // Stays at last
    }

    SECTION("Wrap around disabled - previous returns invalid at start") {
        SearchOptions opts;
        opts.wrapAround = false;
        engine.setOptions(opts);

        engine.setCurrentMatchIndex(0);  // First match
        auto match = engine.previousMatch();
        REQUIRE_FALSE(match.isValid());
        REQUIRE(engine.currentMatchIndex() == 0);  // Stays at first
    }
}

// =============================================================================
// Find from Position with Wrap Tests
// =============================================================================

TEST_CASE("SearchEngine findNext/findPrevious with wrap", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("A B A");

    engine.setDocument(&doc);
    engine.setSearchText("A");

    SECTION("findNext wraps when no match after position") {
        SearchOptions opts;
        opts.wrapAround = true;
        engine.setOptions(opts);

        // Find from position after last match
        auto match = engine.findNext(10);
        REQUIRE(match.isValid());
        REQUIRE(match.start == 0);  // Wrapped to first
    }

    SECTION("findPrevious wraps when no match before position") {
        SearchOptions opts;
        opts.wrapAround = true;
        engine.setOptions(opts);

        // Find from position before first match
        auto match = engine.findPrevious(0);
        REQUIRE(match.isValid());
        REQUIRE(match.start == 4);  // Wrapped to last
    }
}

// =============================================================================
// Regex Tests
// =============================================================================

TEST_CASE("SearchEngine regex search", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("cat bat rat hat");

    engine.setDocument(&doc);

    SECTION("Simple regex pattern") {
        engine.setSearchText("[cbr]at");
        SearchOptions opts;
        opts.useRegex = true;
        engine.setOptions(opts);

        auto matches = engine.findAll();
        REQUIRE(matches.size() == 3);
        REQUIRE(matches[0].matchedText == "cat");
        REQUIRE(matches[1].matchedText == "bat");
        REQUIRE(matches[2].matchedText == "rat");
    }

    SECTION("Regex with case insensitivity") {
        doc.setPlainText("Cat CAT cat");
        engine.setSearchText("cat");
        SearchOptions opts;
        opts.useRegex = true;
        opts.caseSensitive = false;
        engine.setOptions(opts);

        auto matches = engine.findAll();
        REQUIRE(matches.size() == 3);
    }

    SECTION("Invalid regex returns no matches") {
        engine.setSearchText("[invalid");
        SearchOptions opts;
        opts.useRegex = true;
        engine.setOptions(opts);

        auto matches = engine.findAll();
        REQUIRE(matches.empty());
    }
}

// =============================================================================
// Multi-paragraph Tests
// =============================================================================

TEST_CASE("SearchEngine multi-paragraph search", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("Hello\nWorld\nHello Again");

    engine.setDocument(&doc);
    engine.setSearchText("Hello");

    SECTION("Finds matches across paragraphs") {
        auto matches = engine.findAll();
        REQUIRE(matches.size() == 2);

        // First "Hello" in paragraph 0
        REQUIRE(matches[0].paragraph == 0);
        REQUIRE(matches[0].paragraphOffset == 0);

        // Second "Hello" in paragraph 2
        REQUIRE(matches[1].paragraph == 2);
        REQUIRE(matches[1].paragraphOffset == 0);
    }
}

// =============================================================================
// Clear and State Tests
// =============================================================================

TEST_CASE("SearchEngine clear", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("Hello World");

    engine.setDocument(&doc);
    engine.setSearchText("Hello");
    engine.setReplaceText("Hi");
    engine.findAll();
    engine.nextMatch();

    REQUIRE(engine.isActive());
    REQUIRE(engine.totalMatchCount() == 1);
    REQUIRE(engine.currentMatchIndex() == 0);

    engine.clear();

    REQUIRE_FALSE(engine.isActive());
    REQUIRE(engine.searchText().isEmpty());
    REQUIRE(engine.replaceText().isEmpty());
    REQUIRE(engine.matches().empty());
    REQUIRE(engine.currentMatchIndex() == -1);
}

// =============================================================================
// Replace Functionality Tests
// =============================================================================

TEST_CASE("SearchEngine replace functionality", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    QUndoStack undoStack;

    doc.setPlainText("Hello World Hello");
    engine.setDocument(&doc);
    engine.setSearchText("Hello");
    engine.setReplaceText("Hi");
    engine.nextMatch();

    SECTION("replaceCurrent replaces single match") {
        REQUIRE(engine.totalMatchCount() == 2);

        // Phase 11.8: Removed FormatLayer parameter
        bool result = engine.replaceCurrent(&undoStack);
        REQUIRE(result);
        REQUIRE(doc.toPlainText() == "Hi World Hello");

        // Undo should restore original
        undoStack.undo();
        REQUIRE(doc.toPlainText() == "Hello World Hello");

        // Redo should re-apply
        undoStack.redo();
        REQUIRE(doc.toPlainText() == "Hi World Hello");
    }

    SECTION("replaceAll replaces all matches") {
        REQUIRE(engine.totalMatchCount() == 2);

        // Phase 11.8: Removed FormatLayer parameter
        int count = engine.replaceAll(&undoStack);
        REQUIRE(count == 2);
        REQUIRE(doc.toPlainText() == "Hi World Hi");

        // Undo should restore all
        undoStack.undo();
        REQUIRE(doc.toPlainText() == "Hello World Hello");

        // Redo should re-apply all
        undoStack.redo();
        REQUIRE(doc.toPlainText() == "Hi World Hi");
    }

    SECTION("replaceCurrent with no current match returns false") {
        SearchEngine emptyEngine;
        QTextDocument emptyDoc;
        emptyDoc.setPlainText("test");
        emptyEngine.setDocument(&emptyDoc);
        emptyEngine.setSearchText("notfound");
        emptyEngine.setReplaceText("x");

        // Phase 11.8: Removed FormatLayer parameter
        bool result = emptyEngine.replaceCurrent(&undoStack);
        REQUIRE_FALSE(result);
    }

    SECTION("replaceAll with no matches returns 0") {
        SearchEngine emptyEngine;
        QTextDocument emptyDoc;
        emptyDoc.setPlainText("test");
        emptyEngine.setDocument(&emptyDoc);
        emptyEngine.setSearchText("notfound");
        emptyEngine.setReplaceText("x");

        // Phase 11.8: Removed FormatLayer parameter
        int count = emptyEngine.replaceAll(&undoStack);
        REQUIRE(count == 0);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("SearchEngine edge cases", "[editor][search_engine][edge]") {
    SearchEngine engine;

    SECTION("Search without document") {
        engine.setSearchText("test");
        auto matches = engine.findAll();
        REQUIRE(matches.empty());
    }

    SECTION("Search with empty search text") {
        QTextDocument doc;
        doc.setPlainText("Hello World");
        engine.setDocument(&doc);
        engine.setSearchText("");

        auto matches = engine.findAll();
        REQUIRE(matches.empty());
        REQUIRE_FALSE(engine.isActive());
    }

    SECTION("Search in empty document") {
        QTextDocument doc;
        doc.setPlainText("");
        engine.setDocument(&doc);
        engine.setSearchText("test");

        auto matches = engine.findAll();
        REQUIRE(matches.empty());
    }

    SECTION("Single character search") {
        QTextDocument doc;
        doc.setPlainText("abcabc");
        engine.setDocument(&doc);
        engine.setSearchText("a");

        auto matches = engine.findAll();
        REQUIRE(matches.size() == 2);
        REQUIRE(matches[0].length == 1);
    }

    SECTION("Search text equals entire document") {
        QTextDocument doc;
        doc.setPlainText("Hello");
        engine.setDocument(&doc);
        engine.setSearchText("Hello");

        auto matches = engine.findAll();
        REQUIRE(matches.size() == 1);
        REQUIRE(matches[0].start == 0);
        REQUIRE(matches[0].length == 5);
    }
}

// =============================================================================
// Match Position Tests
// =============================================================================

TEST_CASE("SearchEngine match positions", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("Line1\nLine2\nLine3");

    engine.setDocument(&doc);
    engine.setSearchText("Line");

    auto matches = engine.findAll();
    REQUIRE(matches.size() == 3);

    SECTION("Verify absolute positions") {
        // "Line1\n" = 6 chars, "Line2\n" = 6 chars
        REQUIRE(matches[0].start == 0);   // "Line" in "Line1"
        REQUIRE(matches[1].start == 6);   // "Line" in "Line2" (after "Line1\n")
        REQUIRE(matches[2].start == 12);  // "Line" in "Line3" (after "Line1\nLine2\n")
    }

    SECTION("Verify paragraph positions") {
        REQUIRE(matches[0].paragraph == 0);
        REQUIRE(matches[0].paragraphOffset == 0);

        REQUIRE(matches[1].paragraph == 1);
        REQUIRE(matches[1].paragraphOffset == 0);

        REQUIRE(matches[2].paragraph == 2);
        REQUIRE(matches[2].paragraphOffset == 0);
    }
}

// =============================================================================
// Signal Tests
// =============================================================================

TEST_CASE("SearchEngine signals", "[editor][search_engine]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("A B A");

    engine.setDocument(&doc);

    bool searchTextChangedEmitted = false;
    bool matchesChangedEmitted = false;
    bool currentMatchChangedEmitted = false;

    QObject::connect(&engine, &SearchEngine::searchTextChanged,
                     [&](const QString&) { searchTextChangedEmitted = true; });
    QObject::connect(&engine, &SearchEngine::matchesChanged,
                     [&]() { matchesChangedEmitted = true; });
    QObject::connect(&engine, &SearchEngine::currentMatchChanged,
                     [&](const SearchMatch&) { currentMatchChangedEmitted = true; });

    SECTION("searchTextChanged emitted on setSearchText") {
        engine.setSearchText("A");
        REQUIRE(searchTextChangedEmitted);
    }

    SECTION("matchesChanged emitted on findAll") {
        engine.setSearchText("A");
        searchTextChangedEmitted = false;
        engine.findAll();
        REQUIRE(matchesChangedEmitted);
    }

    SECTION("currentMatchChanged emitted on navigation") {
        engine.setSearchText("A");
        engine.findAll();
        currentMatchChangedEmitted = false;
        engine.nextMatch();
        REQUIRE(currentMatchChangedEmitted);
    }
}

// =============================================================================
// Editor Stage 3: matching, matches that follow edits, searching on from a cursor
// =============================================================================

TEST_CASE("SearchEngine regex that can match nothing finds only text it matches",
          "[editor][search_engine][stage3]") {
    // Regression: an expression matching an empty string (b*, ^) never got past it
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("abbc b\nc");
    engine.setDocument(&doc);
    SearchOptions opts;
    opts.useRegex = true;
    engine.setOptions(opts);

    engine.setSearchText("b*");
    const auto matches = engine.findAll();
    REQUIRE(matches.size() == 2);
    CHECK(matches[0].matchedText == "bb");
    CHECK(matches[1].matchedText == "b");

    engine.setSearchText("^");
    CHECK(engine.findAll().empty());
}

TEST_CASE("SearchEngine regex words and letter case beyond ASCII",
          "[editor][search_engine][stage3]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("Żółw i żółwie. ŻÓŁW!"));
    engine.setDocument(&doc);
    SearchOptions opts;
    opts.useRegex = true;
    engine.setOptions(opts);

    engine.setSearchText(QStringLiteral("\\bżółw\\b"));
    const auto matches = engine.findAll();
    REQUIRE(matches.size() == 2);
    CHECK(matches[0].start == 0);
    CHECK(matches[1].matchedText == QStringLiteral("ŻÓŁW"));
}

TEST_CASE("SearchEngine a space matches a non-breaking space", "[editor][search_engine][stage3]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("w domu i w domu"));
    engine.setDocument(&doc);

    engine.setSearchText(QStringLiteral("w domu"));
    auto matches = engine.findAll();
    REQUIRE(matches.size() == 2);
    CHECK(matches[1].matchedText == QStringLiteral("w domu"));

    engine.setSearchText(QStringLiteral("w domu"));
    CHECK(engine.findAll().size() == 2);
}

TEST_CASE("SearchEngine goes on from the origin", "[editor][search_engine][stage3]") {
    SearchEngine engine;
    QTextDocument doc;
    doc.setPlainText("ab x ab x ab");  // matches at 0, 5, 10
    engine.setDocument(&doc);
    engine.setSearchText("ab");
    REQUIRE(engine.totalMatchCount() == 3);

    SECTION("from a cursor") {
        engine.setOrigin(3, 3);
        CHECK(engine.nextMatch().start == 5);
        engine.setOrigin(3, 3);
        CHECK(engine.previousMatch().start == 0);
        engine.setOrigin(11, 11);  // inside the last match
        CHECK(engine.nextMatch().start == 0);  // from the top
    }

    SECTION("a selected match is the current one") {
        engine.setOrigin(5, 7);
        CHECK(engine.currentMatchIndex() == 1);
        CHECK(engine.nextMatch().start == 10);
        engine.setOrigin(4, 7);  // more than the match
        CHECK(engine.currentMatchIndex() == -1);
        CHECK(engine.nextMatch().start == 10);
    }

    SECTION("the origin moves with edits") {
        engine.setOrigin(6, 6);  // within the second match
        QTextCursor cursor(&doc);
        cursor.insertText("ab ");  // "ab ab x ab x ab": the origin is now at 9
        CHECK(engine.nextMatch().start == 13);
    }

    SECTION("a match selected before the search becomes the current one") {
        engine.setSearchText(QString());
        engine.setOrigin(10, 12);
        engine.setSearchText("ab");
        CHECK(engine.totalMatchCount() == 3);
        CHECK(engine.currentMatchIndex() == 2);
    }
}

TEST_CASE("SearchEngine matches follow edits of the document", "[editor][search_engine][stage3]") {
    struct Variant {
        const char* name;
        QString text;
        SearchOptions options;
    };
    SearchOptions caseAndWord;
    caseAndWord.caseSensitive = true;
    caseAndWord.wholeWord = true;
    SearchOptions regex;
    regex.useRegex = true;
    const Variant variant = GENERATE_COPY(values<Variant>({
        {"plain", QStringLiteral("ab"), SearchOptions{}},
        {"case sensitive, whole words", QStringLiteral("ab"), caseAndWord},
        {"with a space", QStringLiteral("a b"), SearchOptions{}},
        {"regex", QStringLiteral("a+b"), regex},
    }));
    INFO(variant.name);

    QTextDocument doc;
    doc.documentLayout();  // QTextDocument reports edited ranges only with a layout
    doc.setPlainText(QStringLiteral("ab x AB ab\nxab a b\n\nab ab"));
    SearchEngine engine;
    engine.setDocument(&doc);
    engine.setOptions(variant.options);
    engine.setSearchText(variant.text);
    REQUIRE(engine.totalMatchCount() > 0);

    int reported = 0;
    int followed = 0;
    QObject::connect(&doc, &QTextDocument::contentsChange, [&reported]() { ++reported; });
    QObject::connect(&engine, &SearchEngine::matchesChanged, [&followed]() { ++followed; });

    std::mt19937 random(20261004);
    const auto number = [&random](int from, int to) {
        return std::uniform_int_distribution<int>(from, to)(random);
    };
    const auto randomText = [&]() {
        static const QString letters = QStringLiteral("abAB  x \n");
        QString text;
        for (int i = number(1, 8); i > 0; --i) {
            text += letters.at(number(0, static_cast<int>(letters.size()) - 1));
        }
        return text;
    };
    const auto randomRange = [&](QTextCursor& cursor) {
        const int last = doc.characterCount() - 1;
        const int from = number(0, last);
        cursor.setPosition(from);
        cursor.setPosition(std::min(last, from + number(0, 20)), QTextCursor::KeepAnchor);
    };

    int missed = 0;
    for (int step = 0; step < 300; ++step) {
        const int reportedBefore = reported;
        const int followedBefore = followed;
        QTextCursor cursor(&doc);
        switch (number(0, 6)) {
        case 0:  // typing
            cursor.setPosition(number(0, doc.characterCount() - 1));
            cursor.insertText(randomText());
            break;
        case 1:  // deleting, also across paragraphs
            randomRange(cursor);
            cursor.removeSelectedText();
            break;
        case 2:  // typing over a selection
            randomRange(cursor);
            cursor.insertText(randomText());
            break;
        case 3:  // several edits in one undo step
            cursor.beginEditBlock();
            for (int i = number(2, 3); i > 0; --i) {
                randomRange(cursor);
                cursor.insertText(i % 2 ? randomText() : QString());
            }
            cursor.endEditBlock();
            break;
        case 4: {  // formatting only
            randomRange(cursor);
            QTextCharFormat bold;
            bold.setFontWeight(QFont::Bold);
            cursor.mergeCharFormat(bold);
            break;
        }
        case 5:
            doc.undo();
            break;
        default:
            doc.redo();
            break;
        }

        // Followed at once, from the edited paragraphs, when the document reported the edit
        if (reported > reportedBefore && followed == followedBefore) {
            ++missed;
        }

        SearchEngine fresh;
        fresh.setDocument(&doc);
        fresh.setOptions(variant.options);
        fresh.setSearchText(variant.text);
        const std::vector<SearchMatch>& expected = fresh.matches();
        const std::vector<SearchMatch>& actual = engine.matches();
        INFO("step " << step << ", text: " << doc.toPlainText().toStdString());
        REQUIRE(actual.size() == expected.size());
        for (size_t i = 0; i < actual.size(); ++i) {
            INFO("match " << i);
            REQUIRE(actual[i].start == expected[i].start);
            REQUIRE(actual[i].length == expected[i].length);
            REQUIRE(actual[i].paragraph == expected[i].paragraph);
            REQUIRE(actual[i].paragraphOffset == expected[i].paragraphOffset);
            REQUIRE(actual[i].matchedText == expected[i].matchedText);
        }
    }
    CHECK(reported > 150);
    CHECK(missed == 0);
}
