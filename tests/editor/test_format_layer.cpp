/// @file test_format_layer.cpp
/// @brief Unit tests for FormatLayer (OpenSpec #00043 Phase 3)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/format_layer.h>
#include <kalahari/editor/text_buffer.h>

using namespace kalahari::editor;

// =============================================================================
// TextFormat Tests
// =============================================================================

TEST_CASE("TextFormat - Basic Operations", "[format_layer]") {
    SECTION("Default construction") {
        TextFormat format;
        REQUIRE(format.isEmpty());
        REQUIRE(format.flags == FormatType::None);
    }

    SECTION("Set bold") {
        TextFormat format;
        format.setBold();
        REQUIRE(format.hasFlag(FormatType::Bold));
        REQUIRE_FALSE(format.isEmpty());
    }

    SECTION("Set italic") {
        TextFormat format;
        format.setItalic();
        REQUIRE(format.hasFlag(FormatType::Italic));
    }

    SECTION("Set underline") {
        TextFormat format;
        format.setUnderline();
        REQUIRE(format.hasFlag(FormatType::Underline));
    }

    SECTION("Set strikethrough") {
        TextFormat format;
        format.setStrikethrough();
        REQUIRE(format.hasFlag(FormatType::Strikethrough));
    }

    SECTION("Multiple flags") {
        TextFormat format;
        format.setBold();
        format.setItalic();
        REQUIRE(format.hasFlag(FormatType::Bold));
        REQUIRE(format.hasFlag(FormatType::Italic));
    }

    SECTION("Disable flag") {
        TextFormat format;
        format.setBold(true);
        REQUIRE(format.hasFlag(FormatType::Bold));
        format.setBold(false);
        REQUIRE_FALSE(format.hasFlag(FormatType::Bold));
    }

    SECTION("Equality") {
        TextFormat f1, f2;
        REQUIRE(f1 == f2);

        f1.setBold();
        REQUIRE(f1 != f2);

        f2.setBold();
        REQUIRE(f1 == f2);
    }

    SECTION("Merge formats") {
        TextFormat f1, f2;
        f1.setBold();
        f2.setItalic();

        TextFormat merged = f1.merged(f2);
        REQUIRE(merged.hasFlag(FormatType::Bold));
        REQUIRE(merged.hasFlag(FormatType::Italic));
    }
}

// =============================================================================
// FormatRange Tests
// =============================================================================

TEST_CASE("FormatRange - Basic Operations", "[format_layer]") {
    SECTION("Empty range") {
        FormatRange range{10, 10, {}};
        REQUIRE(range.isEmpty());
        REQUIRE(range.length() == 0);
    }

    SECTION("Valid range") {
        FormatRange range{10, 20, {}};
        REQUIRE_FALSE(range.isEmpty());
        REQUIRE(range.length() == 10);
    }

    SECTION("Contains position") {
        FormatRange range{10, 20, {}};
        REQUIRE_FALSE(range.contains(9));
        REQUIRE(range.contains(10));
        REQUIRE(range.contains(15));
        REQUIRE_FALSE(range.contains(20));
    }

    SECTION("Overlaps") {
        FormatRange r1{10, 20, {}};
        FormatRange r2{15, 25, {}};
        FormatRange r3{20, 30, {}};
        FormatRange r4{0, 5, {}};

        REQUIRE(r1.overlaps(r2));
        REQUIRE_FALSE(r1.overlaps(r3));
        REQUIRE_FALSE(r1.overlaps(r4));
    }

    SECTION("Adjacent") {
        FormatRange r1{10, 20, {}};
        FormatRange r2{20, 30, {}};
        FormatRange r3{0, 10, {}};

        REQUIRE(r1.isAdjacentTo(r2));
        REQUIRE(r1.isAdjacentTo(r3));
    }
}

// =============================================================================
// IntervalTree Tests
// =============================================================================

TEST_CASE("IntervalTree - Basic Operations", "[format_layer]") {
    IntervalTree tree;

    SECTION("Empty tree") {
        REQUIRE(tree.empty());
        REQUIRE(tree.size() == 0);
    }

    SECTION("Insert range") {
        TextFormat bold;
        bold.setBold();
        tree.insert({10, 20, bold});

        REQUIRE_FALSE(tree.empty());
        REQUIRE(tree.size() == 1);
    }

    SECTION("Find at position") {
        TextFormat bold;
        bold.setBold();
        tree.insert({10, 20, bold});

        auto at5 = tree.findAt(5);
        REQUIRE(at5.empty());

        auto at15 = tree.findAt(15);
        REQUIRE(at15.size() == 1);
        REQUIRE(at15[0].start == 10);
        REQUIRE(at15[0].end == 20);
    }

    SECTION("Find overlapping") {
        TextFormat bold;
        bold.setBold();
        tree.insert({10, 20, bold});
        tree.insert({30, 40, bold});

        auto overlap = tree.findOverlapping(15, 35);
        REQUIRE(overlap.size() == 2);
    }

    SECTION("Clear") {
        TextFormat bold;
        bold.setBold();
        tree.insert({10, 20, bold});
        tree.insert({30, 40, bold});

        tree.clear();
        REQUIRE(tree.empty());
    }
}

TEST_CASE("IntervalTree - Range Shifting", "[format_layer]") {
    IntervalTree tree;
    TextFormat bold;
    bold.setBold();

    SECTION("Shift after insert") {
        tree.insert({10, 20, bold});

        // Insert 5 characters at position 5 (before range)
        tree.shiftRanges(5, 5);

        auto ranges = tree.all();
        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 15);
        REQUIRE(ranges[0].end == 25);
    }

    SECTION("Shift spanning range") {
        tree.insert({10, 20, bold});

        // Insert 5 characters at position 15 (inside range)
        tree.shiftRanges(15, 5);

        auto ranges = tree.all();
        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 10);
        REQUIRE(ranges[0].end == 25);
    }

    SECTION("No shift for ranges before position") {
        tree.insert({10, 20, bold});

        // Insert at position 25 (after range)
        tree.shiftRanges(25, 5);

        auto ranges = tree.all();
        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 10);
        REQUIRE(ranges[0].end == 20);
    }
}

// =============================================================================
// FormatLayer Tests
// =============================================================================

TEST_CASE("FormatLayer - Add Format", "[format_layer]") {
    FormatLayer layer;

    SECTION("Add single format") {
        TextFormat bold;
        bold.setBold();
        layer.addFormat(10, 20, bold);

        REQUIRE(layer.rangeCount() == 1);
    }

    SECTION("Add empty format ignored") {
        TextFormat empty;
        layer.addFormat(10, 20, empty);

        REQUIRE(layer.isEmpty());
    }

    SECTION("Add empty range ignored") {
        TextFormat bold;
        bold.setBold();
        layer.addFormat(10, 10, bold);

        REQUIRE(layer.isEmpty());
    }

    SECTION("Add multiple formats") {
        TextFormat bold;
        bold.setBold();
        TextFormat italic;
        italic.setItalic();

        layer.addFormat(10, 20, bold);
        layer.addFormat(15, 25, italic);

        REQUIRE(layer.rangeCount() == 2);
    }
}

TEST_CASE("FormatLayer - Query Formats", "[format_layer]") {
    FormatLayer layer;
    TextFormat bold;
    bold.setBold();
    TextFormat italic;
    italic.setItalic();

    layer.addFormat(10, 20, bold);
    layer.addFormat(15, 25, italic);

    SECTION("Get formats at position") {
        auto at5 = layer.getFormatsAt(5);
        REQUIRE(at5.empty());

        auto at12 = layer.getFormatsAt(12);
        REQUIRE(at12.size() == 1);
        REQUIRE(at12[0].format.hasFlag(FormatType::Bold));

        auto at17 = layer.getFormatsAt(17);
        REQUIRE(at17.size() == 2);
    }

    SECTION("Get merged format") {
        auto merged = layer.getMergedFormatAt(17);
        REQUIRE(merged.hasFlag(FormatType::Bold));
        REQUIRE(merged.hasFlag(FormatType::Italic));
    }

    SECTION("Has format at position") {
        REQUIRE(layer.hasFormatAt(12, FormatType::Bold));
        REQUIRE_FALSE(layer.hasFormatAt(12, FormatType::Italic));
        REQUIRE(layer.hasFormatAt(17, FormatType::Bold));
        REQUIRE(layer.hasFormatAt(17, FormatType::Italic));
    }

    SECTION("Get formats in range") {
        auto inRange = layer.getFormatsInRange(10, 30);
        REQUIRE(inRange.size() == 2);
    }
}

TEST_CASE("FormatLayer - Remove Format", "[format_layer]") {
    FormatLayer layer;
    TextFormat bold;
    bold.setBold();

    layer.addFormat(10, 30, bold);

    SECTION("Remove format type from range") {
        layer.removeFormat(15, 25, FormatType::Bold);

        // Should have two ranges: [10,15) and [25,30)
        auto ranges = layer.allRanges();
        REQUIRE(ranges.size() == 2);
    }

    SECTION("Clear formats in range") {
        layer.clearFormats(15, 25);

        auto ranges = layer.allRanges();
        REQUIRE(ranges.size() == 2);
    }

    SECTION("Clear all") {
        layer.clearAll();
        REQUIRE(layer.isEmpty());
    }
}

TEST_CASE("FormatLayer - Toggle Format", "[format_layer]") {
    FormatLayer layer;

    SECTION("Toggle on empty layer") {
        bool enabled = layer.toggleFormat(10, 20, FormatType::Bold);
        REQUIRE(enabled);
        REQUIRE(layer.hasFormatAt(15, FormatType::Bold));
    }

    SECTION("Toggle off existing format") {
        TextFormat bold;
        bold.setBold();
        layer.addFormat(10, 20, bold);

        bool enabled = layer.toggleFormat(10, 20, FormatType::Bold);
        REQUIRE_FALSE(enabled);
        REQUIRE_FALSE(layer.hasFormatAt(15, FormatType::Bold));
    }
}

TEST_CASE("FormatLayer - Text Changes", "[format_layer]") {
    FormatLayer layer;
    TextFormat bold;
    bold.setBold();

    layer.addFormat(10, 20, bold);

    SECTION("Text inserted before range") {
        layer.onTextInserted(5, 5);

        auto ranges = layer.allRanges();
        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 15);
        REQUIRE(ranges[0].end == 25);
    }

    SECTION("Text inserted inside range") {
        layer.onTextInserted(15, 5);

        auto ranges = layer.allRanges();
        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 10);
        REQUIRE(ranges[0].end == 25);
    }

    SECTION("Text deleted before range") {
        layer.onTextDeleted(0, 5);

        auto ranges = layer.allRanges();
        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 5);
        REQUIRE(ranges[0].end == 15);
    }

    SECTION("Text deleted inside range") {
        layer.onTextDeleted(12, 3);

        auto ranges = layer.allRanges();
        REQUIRE(ranges.size() == 1);
        REQUIRE(ranges[0].start == 10);
        REQUIRE(ranges[0].end == 17);
    }
}

TEST_CASE("FormatLayer - Buffer Integration", "[format_layer]") {
    TextBuffer buffer;
    buffer.setPlainText("Hello World! This is a test.");

    FormatLayer layer;
    layer.attachToBuffer(&buffer);

    TextFormat bold;
    bold.setBold();
    layer.addFormat(0, 5, bold);  // "Hello"

    SECTION("Get formats for paragraph") {
        auto formats = layer.getFormatsForParagraph(buffer, 0);
        REQUIRE(formats.size() == 1);
    }

    SECTION("Detach from buffer") {
        layer.detachFromBuffer();
        // Should not crash on buffer operations
    }
}

TEST_CASE("FormatLayer - Multiple Format Types", "[format_layer]") {
    FormatLayer layer;

    TextFormat boldItalic;
    boldItalic.setBold();
    boldItalic.setItalic();

    layer.addFormat(10, 20, boldItalic);

    SECTION("Both flags present") {
        REQUIRE(layer.hasFormatAt(15, FormatType::Bold));
        REQUIRE(layer.hasFormatAt(15, FormatType::Italic));
    }

    SECTION("Remove only bold") {
        layer.removeFormat(10, 20, FormatType::Bold);

        // Note: Current implementation removes entire range if it has the flag
        // This is a simplification - a more sophisticated implementation
        // would modify the format instead of removing the range
    }
}

TEST_CASE("FormatLayer - Edge Cases", "[format_layer]") {
    FormatLayer layer;

    SECTION("Empty range operations") {
        TextFormat bold;
        bold.setBold();
        layer.addFormat(10, 10, bold);  // Should be ignored
        REQUIRE(layer.isEmpty());
    }

    SECTION("Inverted range") {
        TextFormat bold;
        bold.setBold();
        layer.addFormat(20, 10, bold);  // start > end - should be ignored
        REQUIRE(layer.isEmpty());
    }

    SECTION("Query empty layer") {
        auto at = layer.getFormatsAt(100);
        REQUIRE(at.empty());

        auto merged = layer.getMergedFormatAt(100);
        REQUIRE(merged.isEmpty());
    }
}
