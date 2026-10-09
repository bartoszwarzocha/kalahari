/// @file test_buffer_commands.cpp
/// @brief Unit tests for the cursor position helpers of buffer_commands.h

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/editor/buffer_commands.h>
#include <QTextDocument>
#include <QTextCursor>

using namespace kalahari::editor;

// =============================================================================
// Helper Functions Tests
// =============================================================================

TEST_CASE("Buffer command helper functions", "[editor][buffer_commands][helpers]") {
    QTextDocument document;
    document.setPlainText(QStringLiteral("Hello\nWorld\nTest"));

    // Document structure:
    // Block 0: "Hello" (positions 0-5, then \n at 5)
    // Block 1: "World" (positions 6-11, then \n at 11)
    // Block 2: "Test" (positions 12-15)

    SECTION("calculateAbsolutePosition - first block") {
        REQUIRE(calculateAbsolutePosition(&document, 0, 0) == 0);
        REQUIRE(calculateAbsolutePosition(&document, 0, 5) == 5);
    }

    SECTION("calculateAbsolutePosition - second block") {
        // Block 1 starts at position 6 (after "Hello\n")
        int pos10 = calculateAbsolutePosition(&document, 1, 0);
        int pos15 = calculateAbsolutePosition(&document, 1, 5);
        REQUIRE(pos10 == 6);
        REQUIRE(pos15 == 11);
    }

    SECTION("calculateAbsolutePosition - third block") {
        // Block 2 starts at position 12 (after "Hello\nWorld\n")
        int pos20 = calculateAbsolutePosition(&document, 2, 0);
        int pos24 = calculateAbsolutePosition(&document, 2, 4);
        REQUIRE(pos20 == 12);
        REQUIRE(pos24 == 16);
    }

    SECTION("calculateAbsolutePosition from CursorPosition") {
        CursorPosition pos{1, 3};
        REQUIRE(calculateAbsolutePosition(&document, pos) == 9);  // 6 + 3
    }

    SECTION("absoluteToCursorPosition - first block") {
        CursorPosition pos = absoluteToCursorPosition(&document, 0);
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 0);

        pos = absoluteToCursorPosition(&document, 3);
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 3);
    }

    SECTION("absoluteToCursorPosition - block boundary") {
        // Position 5 is at "o" in "Hello"
        CursorPosition pos = absoluteToCursorPosition(&document, 5);
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 5);

        // Position 6 is at "W" in "World" (start of block 1)
        pos = absoluteToCursorPosition(&document, 6);
        REQUIRE(pos.paragraph == 1);
        REQUIRE(pos.offset == 0);
    }

    SECTION("absoluteToCursorPosition - second block middle") {
        // Position 9 = 6 (start of block 1) + 3 = offset 3 in block 1
        CursorPosition pos = absoluteToCursorPosition(&document, 9);
        REQUIRE(pos.paragraph == 1);
        REQUIRE(pos.offset == 3);
    }

    SECTION("createCursor - single position") {
        CursorPosition pos{1, 2};
        QTextCursor cursor = createCursor(&document, pos);

        REQUIRE(cursor.position() == 8);  // 6 + 2
        REQUIRE(!cursor.hasSelection());
    }

    SECTION("createCursor - selection") {
        CursorPosition start{0, 0};
        CursorPosition end{0, 5};
        QTextCursor cursor = createCursor(&document, start, end);

        REQUIRE(cursor.hasSelection());
        REQUIRE(cursor.selectedText() == QStringLiteral("Hello"));
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("Buffer commands edge cases", "[editor][buffer_commands][edge]") {
    SECTION("Operations on empty document") {
        QTextDocument document;
        document.setPlainText(QString());

        CHECK(calculateAbsolutePosition(&document, 0, 0) == 0);
        CHECK(absoluteToCursorPosition(&document, 0).paragraph == 0);
    }

    SECTION("Null document handling") {
        REQUIRE(calculateAbsolutePosition(nullptr, 0, 0) == 0);
        REQUIRE(absoluteToCursorPosition(nullptr, 0).paragraph == 0);
        REQUIRE(!createCursor(nullptr, CursorPosition{0, 0}).isNull() == false);
    }

    SECTION("Position beyond document bounds") {
        QTextDocument document;
        document.setPlainText(QStringLiteral("Short"));

        CursorPosition pos = absoluteToCursorPosition(&document, 1000);
        // Should clamp to end of document
        REQUIRE(pos.paragraph == 0);
    }
}
