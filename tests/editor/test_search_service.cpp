/// @file test_search_service.cpp
/// @brief Unit tests for SearchService API (OpenSpec #00043 Phase 11.7)
///
/// Comprehensive tests for QTextDocument-based search:
/// - DocSearchMatch basic properties
/// - SearchService findAll/findNext/findPrevious
/// - SearchService replace/replaceAll
/// - SearchSession navigation and state management
/// - Edge cases and error handling

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/search_service.h>
#include <QTextDocument>

using namespace kalahari::editor;

// =============================================================================
// DocSearchMatch Tests
// =============================================================================

TEST_CASE("DocSearchMatch basic properties", "[editor][search_service][match]") {
    SECTION("Default match is invalid") {
        DocSearchMatch match;
        REQUIRE_FALSE(match.isValid());
        REQUIRE(match.position == 0);
        REQUIRE(match.length == 0);
        REQUIRE(match.end() == 0);
    }

    SECTION("Match with length is valid") {
        DocSearchMatch match;
        match.position = 10;
        match.length = 5;
        REQUIRE(match.isValid());
        REQUIRE(match.end() == 15);
    }

    SECTION("Match comparison by position") {
        DocSearchMatch m1, m2;
        m1.position = 5;
        m1.length = 3;
        m2.position = 10;
        m2.length = 3;

        REQUIRE(m1 < m2);
        REQUIRE_FALSE(m2 < m1);
    }

    SECTION("Match equality") {
        DocSearchMatch m1, m2;
        m1.position = 5;
        m1.length = 3;
        m2.position = 5;
        m2.length = 3;

        REQUIRE(m1 == m2);

        m2.position = 6;
        REQUIRE_FALSE(m1 == m2);
    }
}

// =============================================================================
// DocSearchOptions Tests
// =============================================================================

TEST_CASE("DocSearchOptions defaults", "[editor][search_service][options]") {
    DocSearchOptions options;
    REQUIRE_FALSE(options.caseSensitive);
    REQUIRE_FALSE(options.wholeWord);
    REQUIRE_FALSE(options.useRegex);
    REQUIRE(options.wrapAround);
}

// =============================================================================
// SearchService findAll Tests
// =============================================================================

TEST_CASE("SearchService findAll basic", "[editor][search_service][findAll]") {
    SearchService service;
    QTextDocument doc;
    doc.setPlainText("Hello World Hello");

    SECTION("Find all returns correct matches") {
        auto matches = service.findAll(&doc, "Hello");
        REQUIRE(matches.size() == 2);

        REQUIRE(matches[0].position == 0);
        REQUIRE(matches[0].length == 5);
        REQUIRE(matches[0].matchedText == "Hello");

        REQUIRE(matches[1].position == 12);
        REQUIRE(matches[1].length == 5);
        REQUIRE(matches[1].matchedText == "Hello");
    }

    SECTION("Find with no matches") {
        auto matches = service.findAll(&doc, "xyz");
        REQUIRE(matches.empty());
    }

    SECTION("Find empty query") {
        auto matches = service.findAll(&doc, "");
        REQUIRE(matches.empty());
    }

    SECTION("Find with null document") {
        auto matches = service.findAll(nullptr, "Hello");
        REQUIRE(matches.empty());
    }
}

TEST_CASE("SearchService findAll case sensitivity", "[editor][search_service][findAll]") {
    SearchService service;
    QTextDocument doc;
    doc.setPlainText("Hello HELLO hello");

    SECTION("Case insensitive finds all variants") {
        DocSearchOptions opts;
        opts.caseSensitive = false;
        auto matches = service.findAll(&doc, "Hello", opts);
        REQUIRE(matches.size() == 3);
    }

    SECTION("Case sensitive finds exact match only") {
        DocSearchOptions opts;
        opts.caseSensitive = true;
        auto matches = service.findAll(&doc, "Hello", opts);
        REQUIRE(matches.size() == 1);
        REQUIRE(matches[0].position == 0);
        REQUIRE(matches[0].matchedText == "Hello");
    }
}

TEST_CASE("SearchService findAll whole word", "[editor][search_service][findAll]") {
    SearchService service;
    QTextDocument doc;
    doc.setPlainText("Hello HelloWorld WorldHello");

    SECTION("Without whole word matches partial words") {
        DocSearchOptions opts;
        opts.wholeWord = false;
        auto matches = service.findAll(&doc, "Hello", opts);
        REQUIRE(matches.size() == 3);
    }

    SECTION("With whole word only matches complete words") {
        DocSearchOptions opts;
        opts.wholeWord = true;
        auto matches = service.findAll(&doc, "Hello", opts);
        REQUIRE(matches.size() == 1);
        REQUIRE(matches[0].position == 0);
    }
}

TEST_CASE("SearchService findAll regex", "[editor][search_service][findAll]") {
    SearchService service;
    QTextDocument doc;
    doc.setPlainText("cat bat rat hat");

    SECTION("Simple regex pattern") {
        DocSearchOptions opts;
        opts.useRegex = true;
        auto matches = service.findAll(&doc, "[cbr]at", opts);
        REQUIRE(matches.size() == 3);
        REQUIRE(matches[0].matchedText == "cat");
        REQUIRE(matches[1].matchedText == "bat");
        REQUIRE(matches[2].matchedText == "rat");
    }

    SECTION("Regex with case insensitivity") {
        doc.setPlainText("Cat CAT cat");
        DocSearchOptions opts;
        opts.useRegex = true;
        opts.caseSensitive = false;
        auto matches = service.findAll(&doc, "cat", opts);
        REQUIRE(matches.size() == 3);
    }

    SECTION("Invalid regex returns no matches") {
        DocSearchOptions opts;
        opts.useRegex = true;
        auto matches = service.findAll(&doc, "[invalid", opts);
        REQUIRE(matches.empty());
    }
}

TEST_CASE("SearchService findAll multi-line", "[editor][search_service][findAll]") {
    SearchService service;
    QTextDocument doc;
    doc.setPlainText("Hello\nWorld\nHello Again");

    SECTION("Finds matches across blocks") {
        auto matches = service.findAll(&doc, "Hello");
        REQUIRE(matches.size() == 2);

        // First "Hello" in block 0
        REQUIRE(matches[0].blockNumber == 0);
        REQUIRE(matches[0].positionInBlock == 0);

        // Second "Hello" in block 2
        REQUIRE(matches[1].blockNumber == 2);
        REQUIRE(matches[1].positionInBlock == 0);
    }
}

// =============================================================================
// SearchService findNext/findPrevious Tests
// =============================================================================

TEST_CASE("SearchService findNext", "[editor][search_service][findNext]") {
    SearchService service;
    QTextDocument doc;
    doc.setPlainText("Hello World Hello");

    SECTION("Find next from position 0") {
        auto match = service.findNext(&doc, "Hello", 0);
        REQUIRE(match.isValid());
        REQUIRE(match.position == 0);
    }

    SECTION("Find next from position 1") {
        auto match = service.findNext(&doc, "Hello", 1);
        REQUIRE(match.isValid());
        REQUIRE(match.position == 12);  // Second "Hello"
    }

    SECTION("Find next wraps around") {
        DocSearchOptions opts;
        opts.wrapAround = true;
        auto match = service.findNext(&doc, "Hello", 15, opts);
        REQUIRE(match.isValid());
        REQUIRE(match.position == 0);  // Wrapped to first
    }

    SECTION("Find next without wrap returns invalid") {
        DocSearchOptions opts;
        opts.wrapAround = false;
        auto match = service.findNext(&doc, "Hello", 15, opts);
        REQUIRE_FALSE(match.isValid());
    }
}

TEST_CASE("SearchService findPrevious", "[editor][search_service][findPrevious]") {
    SearchService service;
    QTextDocument doc;
    doc.setPlainText("Hello World Hello");

    SECTION("Find previous from end") {
        auto match = service.findPrevious(&doc, "Hello", 17);
        REQUIRE(match.isValid());
        REQUIRE(match.position == 12);  // Second "Hello"
    }

    SECTION("Find previous from position 10") {
        auto match = service.findPrevious(&doc, "Hello", 10);
        REQUIRE(match.isValid());
        REQUIRE(match.position == 0);  // First "Hello"
    }

    SECTION("Find previous wraps around") {
        DocSearchOptions opts;
        opts.wrapAround = true;
        auto match = service.findPrevious(&doc, "Hello", 0, opts);
        REQUIRE(match.isValid());
        REQUIRE(match.position == 12);  // Wrapped to last
    }

    SECTION("Find previous without wrap returns invalid") {
        DocSearchOptions opts;
        opts.wrapAround = false;
        auto match = service.findPrevious(&doc, "Hello", 0, opts);
        REQUIRE_FALSE(match.isValid());
    }
}

// =============================================================================
// SearchService replace Tests
// =============================================================================

TEST_CASE("SearchService replace", "[editor][search_service][replace]") {
    SearchService service;
    QTextDocument doc;
    doc.setPlainText("Hello World");

    SECTION("Replace single match") {
        DocSearchMatch match;
        match.position = 0;
        match.length = 5;
        match.matchedText = "Hello";

        bool result = service.replace(&doc, match, "Hi");
        REQUIRE(result);
        REQUIRE(doc.toPlainText() == "Hi World");
    }

    SECTION("Replace with invalid match returns false") {
        DocSearchMatch match;  // Invalid (length == 0)
        bool result = service.replace(&doc, match, "Hi");
        REQUIRE_FALSE(result);
    }

    SECTION("Replace with null document returns false") {
        DocSearchMatch match;
        match.position = 0;
        match.length = 5;
        bool result = service.replace(nullptr, match, "Hi");
        REQUIRE_FALSE(result);
    }

    SECTION("Replace with empty replacement") {
        DocSearchMatch match;
        match.position = 0;
        match.length = 6;  // "Hello "
        bool result = service.replace(&doc, match, "");
        REQUIRE(result);
        REQUIRE(doc.toPlainText() == "World");
    }
}

// =============================================================================
// SearchService replaceAll Tests
// =============================================================================

TEST_CASE("SearchService replaceAll", "[editor][search_service][replaceAll]") {
    SearchService service;
    QTextDocument doc;
    doc.setPlainText("Hello World Hello");

    SECTION("Replace all matches") {
        int count = service.replaceAll(&doc, "Hello", "Hi");
        REQUIRE(count == 2);
        REQUIRE(doc.toPlainText() == "Hi World Hi");
    }

    SECTION("Replace all with no matches returns 0") {
        int count = service.replaceAll(&doc, "xyz", "abc");
        REQUIRE(count == 0);
        REQUIRE(doc.toPlainText() == "Hello World Hello");  // Unchanged
    }

    SECTION("Replace all with empty query returns 0") {
        int count = service.replaceAll(&doc, "", "abc");
        REQUIRE(count == 0);
    }

    SECTION("Replace all with null document returns 0") {
        int count = service.replaceAll(nullptr, "Hello", "Hi");
        REQUIRE(count == 0);
    }

    SECTION("Replace all supports undo as single operation") {
        int count = service.replaceAll(&doc, "Hello", "Hi");
        REQUIRE(count == 2);

        // Undo should restore both replacements
        doc.undo();
        REQUIRE(doc.toPlainText() == "Hello World Hello");

        // Redo should re-apply both
        doc.redo();
        REQUIRE(doc.toPlainText() == "Hi World Hi");
    }
}

// =============================================================================
// SearchSession Tests
// =============================================================================

TEST_CASE("SearchSession initialization", "[editor][search_service][session]") {
    SearchSession session;

    REQUIRE(session.document() == nullptr);
    REQUIRE(session.searchText().isEmpty());
    REQUIRE(session.replaceText().isEmpty());
    REQUIRE(session.currentMatchIndex() == -1);
    REQUIRE(session.totalMatchCount() == 0);
    REQUIRE_FALSE(session.isActive());
}

TEST_CASE("SearchSession configuration", "[editor][search_service][session]") {
    SearchSession session;
    QTextDocument doc;
    doc.setPlainText("Test content");

    SECTION("Set document") {
        session.setDocument(&doc);
        REQUIRE(session.document() == &doc);
    }

    SECTION("Set search text") {
        session.setDocument(&doc);
        session.setSearchText("test");
        REQUIRE(session.searchText() == "test");
        REQUIRE(session.isActive());
    }

    SECTION("Set replace text") {
        session.setReplaceText("replacement");
        REQUIRE(session.replaceText() == "replacement");
    }

    SECTION("Set options") {
        DocSearchOptions opts;
        opts.caseSensitive = true;
        opts.wholeWord = true;
        session.setOptions(opts);

        DocSearchOptions result = session.options();
        REQUIRE(result.caseSensitive);
        REQUIRE(result.wholeWord);
    }
}

TEST_CASE("SearchSession navigation", "[editor][search_service][session]") {
    SearchSession session;
    QTextDocument doc;
    doc.setPlainText("A B A C A");

    session.setDocument(&doc);
    session.setSearchText("A");

    SECTION("Total match count") {
        REQUIRE(session.totalMatchCount() == 3);
    }

    SECTION("Navigate with nextMatch") {
        // Initial state: currentMatchIndex is 0 after search
        REQUIRE(session.currentMatchIndex() == 0);

        auto m1 = session.nextMatch();
        REQUIRE(m1.isValid());
        REQUIRE(m1.position == 4);
        REQUIRE(session.currentMatchIndex() == 1);

        auto m2 = session.nextMatch();
        REQUIRE(m2.isValid());
        REQUIRE(m2.position == 8);
        REQUIRE(session.currentMatchIndex() == 2);

        // Wrap around to first
        auto m3 = session.nextMatch();
        REQUIRE(m3.isValid());
        REQUIRE(m3.position == 0);
        REQUIRE(session.currentMatchIndex() == 0);
    }

    SECTION("Navigate with previousMatch") {
        // Start at first match (index 0)
        REQUIRE(session.currentMatchIndex() == 0);

        // Previous wraps to last
        auto m1 = session.previousMatch();
        REQUIRE(m1.isValid());
        REQUIRE(m1.position == 8);
        REQUIRE(session.currentMatchIndex() == 2);

        auto m2 = session.previousMatch();
        REQUIRE(m2.isValid());
        REQUIRE(m2.position == 4);
        REQUIRE(session.currentMatchIndex() == 1);
    }

    SECTION("Set current match index") {
        REQUIRE(session.setCurrentMatchIndex(1));
        REQUIRE(session.currentMatchIndex() == 1);
        REQUIRE(session.currentMatch().position == 4);

        REQUIRE_FALSE(session.setCurrentMatchIndex(100));
        REQUIRE_FALSE(session.setCurrentMatchIndex(-1));
    }

    SECTION("Current match") {
        auto match = session.currentMatch();
        REQUIRE(match.isValid());
        REQUIRE(match.position == 0);
    }
}

TEST_CASE("SearchSession replace operations", "[editor][search_service][session]") {
    SearchSession session;
    QTextDocument doc;
    doc.setPlainText("Hello World Hello");

    session.setDocument(&doc);
    session.setSearchText("Hello");
    session.setReplaceText("Hi");

    SECTION("replaceCurrent replaces single match") {
        REQUIRE(session.totalMatchCount() == 2);

        bool result = session.replaceCurrent();
        REQUIRE(result);
        REQUIRE(doc.toPlainText() == "Hi World Hello");

        // After replacement, matches are rebuilt
        REQUIRE(session.totalMatchCount() == 1);
    }

    SECTION("replaceAll replaces all matches") {
        REQUIRE(session.totalMatchCount() == 2);

        int count = session.replaceAll();
        REQUIRE(count == 2);
        REQUIRE(doc.toPlainText() == "Hi World Hi");

        // After replacement, no more matches
        REQUIRE(session.totalMatchCount() == 0);
    }

    SECTION("replaceCurrent with no matches returns false") {
        session.setSearchText("xyz");
        REQUIRE(session.totalMatchCount() == 0);

        bool result = session.replaceCurrent();
        REQUIRE_FALSE(result);
    }

    SECTION("replaceAll with no matches returns 0") {
        session.setSearchText("xyz");
        REQUIRE(session.totalMatchCount() == 0);

        int count = session.replaceAll();
        REQUIRE(count == 0);
    }
}

TEST_CASE("SearchSession clear", "[editor][search_service][session]") {
    SearchSession session;
    QTextDocument doc;
    doc.setPlainText("Hello World");

    session.setDocument(&doc);
    session.setSearchText("Hello");
    session.setReplaceText("Hi");

    REQUIRE(session.isActive());
    REQUIRE(session.totalMatchCount() == 1);
    REQUIRE(session.currentMatchIndex() == 0);

    session.clear();

    REQUIRE_FALSE(session.isActive());
    REQUIRE(session.searchText().isEmpty());
    REQUIRE(session.replaceText().isEmpty());
    REQUIRE(session.matches().empty());
    REQUIRE(session.currentMatchIndex() == -1);
}

TEST_CASE("SearchSession signals", "[editor][search_service][session]") {
    SearchSession session;
    QTextDocument doc;
    doc.setPlainText("A B A");

    session.setDocument(&doc);

    bool searchTextChangedEmitted = false;
    bool matchesChangedEmitted = false;
    bool currentMatchChangedEmitted = false;

    QObject::connect(&session, &SearchSession::searchTextChanged,
                     [&](const QString&) { searchTextChangedEmitted = true; });
    QObject::connect(&session, &SearchSession::matchesChanged,
                     [&]() { matchesChangedEmitted = true; });
    QObject::connect(&session, &SearchSession::currentMatchChanged,
                     [&](const DocSearchMatch&) { currentMatchChangedEmitted = true; });

    SECTION("searchTextChanged emitted on setSearchText") {
        session.setSearchText("A");
        REQUIRE(searchTextChangedEmitted);
    }

    SECTION("matchesChanged emitted on search") {
        session.setSearchText("A");
        REQUIRE(matchesChangedEmitted);
    }

    SECTION("currentMatchChanged emitted on navigation") {
        session.setSearchText("A");
        currentMatchChangedEmitted = false;
        session.nextMatch();
        REQUIRE(currentMatchChangedEmitted);
    }

    SECTION("No signal when setting same search text") {
        session.setSearchText("A");
        searchTextChangedEmitted = false;
        session.setSearchText("A");  // Same text
        REQUIRE_FALSE(searchTextChangedEmitted);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("SearchService edge cases", "[editor][search_service][edge]") {
    SearchService service;
    QTextDocument doc;

    SECTION("Search in empty document") {
        doc.setPlainText("");
        auto matches = service.findAll(&doc, "test");
        REQUIRE(matches.empty());
    }

    SECTION("Single character search") {
        doc.setPlainText("abcabc");
        auto matches = service.findAll(&doc, "a");
        REQUIRE(matches.size() == 2);
        REQUIRE(matches[0].length == 1);
    }

    SECTION("Search text equals entire document") {
        doc.setPlainText("Hello");
        auto matches = service.findAll(&doc, "Hello");
        REQUIRE(matches.size() == 1);
        REQUIRE(matches[0].position == 0);
        REQUIRE(matches[0].length == 5);
    }

    SECTION("Overlapping matches not found (QTextDocument behavior)") {
        // QTextDocument::find doesn't find overlapping matches
        doc.setPlainText("aaa");
        auto matches = service.findAll(&doc, "aa");
        REQUIRE(matches.size() == 1);  // Only first "aa" found
    }

    SECTION("Replace all with longer replacement") {
        doc.setPlainText("a a a");
        int count = service.replaceAll(&doc, "a", "abc");
        REQUIRE(count == 3);
        REQUIRE(doc.toPlainText() == "abc abc abc");
    }

    SECTION("Replace all with shorter replacement") {
        doc.setPlainText("Hello Hello Hello");
        int count = service.replaceAll(&doc, "Hello", "Hi");
        REQUIRE(count == 3);
        REQUIRE(doc.toPlainText() == "Hi Hi Hi");
    }
}

TEST_CASE("SearchSession edge cases", "[editor][search_service][session][edge]") {
    SearchSession session;

    SECTION("Operations without document") {
        session.setSearchText("test");
        REQUIRE_FALSE(session.isActive());
        REQUIRE(session.totalMatchCount() == 0);

        auto match = session.nextMatch();
        REQUIRE_FALSE(match.isValid());

        bool replaced = session.replaceCurrent();
        REQUIRE_FALSE(replaced);

        int count = session.replaceAll();
        REQUIRE(count == 0);
    }

    SECTION("Navigation with no matches") {
        QTextDocument doc;
        doc.setPlainText("Hello World");
        session.setDocument(&doc);
        session.setSearchText("xyz");

        REQUIRE(session.totalMatchCount() == 0);

        auto m1 = session.nextMatch();
        REQUIRE_FALSE(m1.isValid());

        auto m2 = session.previousMatch();
        REQUIRE_FALSE(m2.isValid());
    }

    SECTION("Single match navigation") {
        QTextDocument doc;
        doc.setPlainText("Hello World");
        session.setDocument(&doc);
        session.setSearchText("Hello");

        REQUIRE(session.totalMatchCount() == 1);
        REQUIRE(session.currentMatchIndex() == 0);

        // Next wraps to same match
        auto m1 = session.nextMatch();
        REQUIRE(m1.isValid());
        REQUIRE(session.currentMatchIndex() == 0);

        // Previous wraps to same match
        auto m2 = session.previousMatch();
        REQUIRE(m2.isValid());
        REQUIRE(session.currentMatchIndex() == 0);
    }
}

// =============================================================================
// Position and Block Tests
// =============================================================================

TEST_CASE("SearchService match positions", "[editor][search_service]") {
    SearchService service;
    QTextDocument doc;
    doc.setPlainText("Line1\nLine2\nLine3");

    auto matches = service.findAll(&doc, "Line");
    REQUIRE(matches.size() == 3);

    SECTION("Verify absolute positions") {
        // "Line1\n" = 6 chars, "Line2\n" = 6 chars
        REQUIRE(matches[0].position == 0);
        REQUIRE(matches[1].position == 6);
        REQUIRE(matches[2].position == 12);
    }

    SECTION("Verify block positions") {
        REQUIRE(matches[0].blockNumber == 0);
        REQUIRE(matches[0].positionInBlock == 0);

        REQUIRE(matches[1].blockNumber == 1);
        REQUIRE(matches[1].positionInBlock == 0);

        REQUIRE(matches[2].blockNumber == 2);
        REQUIRE(matches[2].positionInBlock == 0);
    }
}

// =============================================================================
// Custom Service Injection Tests
// =============================================================================

/// @brief Mock search service for testing
class MockSearchService : public ISearchService {
public:
    int findAllCalls = 0;
    int findNextCalls = 0;
    int findPreviousCalls = 0;
    int replaceCalls = 0;
    int replaceAllCalls = 0;

    QVector<DocSearchMatch> findAll(QTextDocument*, const QString&, const DocSearchOptions&) override {
        ++findAllCalls;
        DocSearchMatch m;
        m.position = 0;
        m.length = 5;
        m.matchedText = "Mock";
        return {m};
    }

    DocSearchMatch findNext(QTextDocument*, const QString&, int, const DocSearchOptions&) override {
        ++findNextCalls;
        return {};
    }

    DocSearchMatch findPrevious(QTextDocument*, const QString&, int, const DocSearchOptions&) override {
        ++findPreviousCalls;
        return {};
    }

    bool replace(QTextDocument*, const DocSearchMatch&, const QString&) override {
        ++replaceCalls;
        return true;
    }

    int replaceAll(QTextDocument*, const QString&, const QString&, const DocSearchOptions&) override {
        ++replaceAllCalls;
        return 1;
    }
};

TEST_CASE("SearchSession with custom service", "[editor][search_service][session]") {
    SearchSession session;
    MockSearchService mockService;
    QTextDocument doc;
    doc.setPlainText("Test");

    session.setSearchService(&mockService);
    session.setDocument(&doc);

    SECTION("Uses custom service for search") {
        session.setSearchText("test");
        REQUIRE(mockService.findAllCalls == 1);
        REQUIRE(session.totalMatchCount() == 1);
    }

    SECTION("Uses custom service for replace") {
        session.setSearchText("test");
        session.setReplaceText("new");
        session.replaceCurrent();
        REQUIRE(mockService.replaceCalls == 1);
    }

    SECTION("Uses custom service for replaceAll") {
        session.setSearchText("test");
        session.setReplaceText("new");
        session.replaceAll();
        REQUIRE(mockService.replaceAllCalls == 1);
    }
}
