/// @file test_editor_types.cpp
/// @brief Unit tests for editor basic types (OpenSpec #00042)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/editor_types.h>

using namespace kalahari::editor;

// =============================================================================
// CursorPosition Tests
// =============================================================================

TEST_CASE("CursorPosition comparison operators", "[editor][cursor]") {
    SECTION("Equality") {
        CursorPosition a{0, 0};
        CursorPosition b{0, 0};
        CursorPosition c{1, 0};
        CursorPosition d{0, 1};

        REQUIRE(a == b);
        REQUIRE_FALSE(a == c);
        REQUIRE_FALSE(a == d);
        REQUIRE(a != c);
        REQUIRE(a != d);
    }

    SECTION("Less than - same paragraph") {
        CursorPosition a{0, 5};
        CursorPosition b{0, 10};

        REQUIRE(a < b);
        REQUIRE_FALSE(b < a);
        REQUIRE_FALSE(a < a);
    }

    SECTION("Less than - different paragraphs") {
        CursorPosition a{0, 100};  // End of paragraph 0
        CursorPosition b{1, 0};    // Start of paragraph 1

        REQUIRE(a < b);
        REQUIRE_FALSE(b < a);
    }

    SECTION("Less than or equal") {
        CursorPosition a{0, 5};
        CursorPosition b{0, 5};
        CursorPosition c{0, 10};

        REQUIRE(a <= b);
        REQUIRE(a <= c);
        REQUIRE_FALSE(c <= a);
    }

    SECTION("Greater than") {
        CursorPosition a{1, 5};
        CursorPosition b{0, 100};

        REQUIRE(a > b);
        REQUIRE_FALSE(b > a);
    }

    SECTION("Greater than or equal") {
        CursorPosition a{1, 5};
        CursorPosition b{1, 5};
        CursorPosition c{0, 100};

        REQUIRE(a >= b);
        REQUIRE(a >= c);
        REQUIRE_FALSE(c >= a);
    }
}

// =============================================================================
// SelectionRange Tests
// =============================================================================

TEST_CASE("SelectionRange operations", "[editor][selection]") {
    SECTION("isEmpty - empty selection") {
        SelectionRange range{{0, 5}, {0, 5}};
        REQUIRE(range.isEmpty());
    }

    SECTION("isEmpty - non-empty selection") {
        SelectionRange range{{0, 5}, {0, 10}};
        REQUIRE_FALSE(range.isEmpty());
    }

    SECTION("isMultiParagraph - single paragraph") {
        SelectionRange range{{0, 0}, {0, 100}};
        REQUIRE_FALSE(range.isMultiParagraph());
    }

    SECTION("isMultiParagraph - multiple paragraphs") {
        SelectionRange range{{0, 50}, {2, 10}};
        REQUIRE(range.isMultiParagraph());
    }

    SECTION("normalized - already normalized") {
        SelectionRange range{{0, 5}, {0, 10}};
        SelectionRange norm = range.normalized();

        REQUIRE(norm.start.offset == 5);
        REQUIRE(norm.end.offset == 10);
    }

    SECTION("normalized - reversed selection") {
        SelectionRange range{{0, 10}, {0, 5}};
        SelectionRange norm = range.normalized();

        REQUIRE(norm.start.offset == 5);
        REQUIRE(norm.end.offset == 10);
    }

    SECTION("normalized - reversed multi-paragraph") {
        SelectionRange range{{2, 10}, {0, 50}};
        SelectionRange norm = range.normalized();

        REQUIRE(norm.start.paragraph == 0);
        REQUIRE(norm.start.offset == 50);
        REQUIRE(norm.end.paragraph == 2);
        REQUIRE(norm.end.offset == 10);
    }
}
