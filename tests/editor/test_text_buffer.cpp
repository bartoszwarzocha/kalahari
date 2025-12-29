/// @file test_text_buffer.cpp
/// @brief Unit tests for TextBuffer (OpenSpec #00043 Phase 2)

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/editor/text_buffer.h>

using namespace kalahari::editor;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Test Observer
// =============================================================================

class TestObserver : public ITextBufferObserver {
public:
    int textChangedCount = 0;
    int paragraphInsertedCount = 0;
    int paragraphRemovedCount = 0;
    int paragraphChangedCount = 0;
    int heightChangedCount = 0;
    size_t lastInsertedIndex = 0;
    size_t lastRemovedIndex = 0;
    size_t lastChangedIndex = 0;

    void onTextChanged() override { textChangedCount++; }
    void onParagraphInserted(size_t index) override {
        paragraphInsertedCount++;
        lastInsertedIndex = index;
    }
    void onParagraphRemoved(size_t index) override {
        paragraphRemovedCount++;
        lastRemovedIndex = index;
    }
    void onParagraphChanged(size_t index) override {
        paragraphChangedCount++;
        lastChangedIndex = index;
    }
    void onHeightChanged(size_t /*index*/, double /*oldHeight*/, double /*newHeight*/) override {
        heightChangedCount++;
    }
};

// =============================================================================
// HeightTree Tests
// =============================================================================

TEST_CASE("HeightTree basic operations", "[editor][text_buffer][height_tree]") {
    HeightTree tree(5);

    SECTION("Initial size") {
        REQUIRE(tree.size() == 5);
    }

    SECTION("Set and get heights") {
        tree.setHeight(0, 20.0);
        tree.setHeight(1, 30.0);
        tree.setHeight(2, 25.0);

        REQUIRE_THAT(tree.get(0), WithinAbs(20.0, 0.001));
        REQUIRE_THAT(tree.get(1), WithinAbs(30.0, 0.001));
        REQUIRE_THAT(tree.get(2), WithinAbs(25.0, 0.001));
    }

    SECTION("Prefix sum") {
        tree.setHeight(0, 20.0);
        tree.setHeight(1, 30.0);
        tree.setHeight(2, 25.0);
        tree.setHeight(3, 15.0);
        tree.setHeight(4, 10.0);

        REQUIRE_THAT(tree.prefixSum(0), WithinAbs(20.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(1), WithinAbs(50.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(2), WithinAbs(75.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(3), WithinAbs(90.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(4), WithinAbs(100.0, 0.001));
    }

    SECTION("Total height") {
        tree.setHeight(0, 20.0);
        tree.setHeight(1, 30.0);
        tree.setHeight(2, 25.0);
        tree.setHeight(3, 15.0);
        tree.setHeight(4, 10.0);

        REQUIRE_THAT(tree.totalHeight(), WithinAbs(100.0, 0.001));
    }

    SECTION("Y position of paragraph") {
        tree.setHeight(0, 20.0);
        tree.setHeight(1, 30.0);
        tree.setHeight(2, 25.0);

        REQUIRE_THAT(tree.getYPosition(0), WithinAbs(0.0, 0.001));
        REQUIRE_THAT(tree.getYPosition(1), WithinAbs(20.0, 0.001));
        REQUIRE_THAT(tree.getYPosition(2), WithinAbs(50.0, 0.001));
    }
}

TEST_CASE("HeightTree findParagraphAtY", "[editor][text_buffer][height_tree]") {
    HeightTree tree(5);
    tree.setHeight(0, 20.0);  // 0-20
    tree.setHeight(1, 30.0);  // 20-50
    tree.setHeight(2, 25.0);  // 50-75
    tree.setHeight(3, 15.0);  // 75-90
    tree.setHeight(4, 10.0);  // 90-100

    SECTION("Find paragraph at beginning") {
        REQUIRE(tree.findParagraphAtY(0.0) == 0);
        REQUIRE(tree.findParagraphAtY(10.0) == 0);
        REQUIRE(tree.findParagraphAtY(19.0) == 0);
    }

    SECTION("Find paragraph at boundaries") {
        REQUIRE(tree.findParagraphAtY(20.0) == 1);
        REQUIRE(tree.findParagraphAtY(50.0) == 2);
        REQUIRE(tree.findParagraphAtY(75.0) == 3);
        REQUIRE(tree.findParagraphAtY(90.0) == 4);
    }

    SECTION("Find paragraph in middle") {
        REQUIRE(tree.findParagraphAtY(35.0) == 1);
        REQUIRE(tree.findParagraphAtY(60.0) == 2);
        REQUIRE(tree.findParagraphAtY(85.0) == 3);
    }

    SECTION("Find paragraph beyond end") {
        REQUIRE(tree.findParagraphAtY(150.0) == 4);
    }
}

TEST_CASE("HeightTree insert and remove", "[editor][text_buffer][height_tree]") {
    HeightTree tree(3);
    tree.setHeight(0, 10.0);
    tree.setHeight(1, 20.0);
    tree.setHeight(2, 30.0);

    SECTION("Insert at beginning") {
        tree.insert(0, 15.0);
        REQUIRE(tree.size() == 4);
        REQUIRE_THAT(tree.get(0), WithinAbs(15.0, 0.001));
        REQUIRE_THAT(tree.get(1), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(75.0, 0.001));
    }

    SECTION("Insert in middle") {
        tree.insert(1, 15.0);
        REQUIRE(tree.size() == 4);
        REQUIRE_THAT(tree.get(0), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.get(1), WithinAbs(15.0, 0.001));
        REQUIRE_THAT(tree.get(2), WithinAbs(20.0, 0.001));
    }

    SECTION("Insert at end") {
        tree.insert(3, 15.0);
        REQUIRE(tree.size() == 4);
        REQUIRE_THAT(tree.get(3), WithinAbs(15.0, 0.001));
    }

    SECTION("Remove from beginning") {
        tree.remove(0);
        REQUIRE(tree.size() == 2);
        REQUIRE_THAT(tree.get(0), WithinAbs(20.0, 0.001));
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(50.0, 0.001));
    }

    SECTION("Remove from middle") {
        tree.remove(1);
        REQUIRE(tree.size() == 2);
        REQUIRE_THAT(tree.get(0), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.get(1), WithinAbs(30.0, 0.001));
    }
}

// =============================================================================
// TextBuffer Construction Tests
// =============================================================================

TEST_CASE("TextBuffer default constructor", "[editor][text_buffer]") {
    TextBuffer buffer;

    SECTION("Is empty initially") {
        REQUIRE(buffer.isEmpty());
    }

    SECTION("Has one paragraph (empty document)") {
        REQUIRE(buffer.paragraphCount() == 1);
    }

    SECTION("Document is valid") {
        REQUIRE(buffer.document() != nullptr);
    }
}

// =============================================================================
// TextBuffer Text Content Tests
// =============================================================================

TEST_CASE("TextBuffer setPlainText", "[editor][text_buffer]") {
    TextBuffer buffer;

    SECTION("Set simple text") {
        buffer.setPlainText("Hello World");
        REQUIRE(buffer.plainText() == "Hello World");
        REQUIRE(buffer.paragraphCount() == 1);
    }

    SECTION("Set multi-paragraph text") {
        buffer.setPlainText("Line 1\nLine 2\nLine 3");
        REQUIRE(buffer.paragraphCount() == 3);
        REQUIRE(buffer.paragraphText(0) == "Line 1");
        REQUIRE(buffer.paragraphText(1) == "Line 2");
        REQUIRE(buffer.paragraphText(2) == "Line 3");
    }

    SECTION("Plain text is cached") {
        buffer.setPlainText("Test");
        REQUIRE(buffer.isPlainTextCached());

        // Second call should use cache
        QString text = buffer.plainText();
        REQUIRE(text == "Test");
    }

    SECTION("Cache invalidation") {
        buffer.setPlainText("Test");
        buffer.invalidatePlainTextCache();
        REQUIRE_FALSE(buffer.isPlainTextCached());

        // Should rebuild cache
        QString text = buffer.plainText();
        REQUIRE(text == "Test");
        REQUIRE(buffer.isPlainTextCached());
    }
}

// =============================================================================
// TextBuffer Paragraph Access Tests
// =============================================================================

TEST_CASE("TextBuffer paragraph access", "[editor][text_buffer]") {
    TextBuffer buffer;
    buffer.setPlainText("First\nSecond paragraph with more text\nThird");

    SECTION("Paragraph count") {
        REQUIRE(buffer.paragraphCount() == 3);
    }

    SECTION("Paragraph text") {
        REQUIRE(buffer.paragraphText(0) == "First");
        REQUIRE(buffer.paragraphText(1) == "Second paragraph with more text");
        REQUIRE(buffer.paragraphText(2) == "Third");
    }

    SECTION("Invalid paragraph index returns empty") {
        REQUIRE(buffer.paragraphText(100).isEmpty());
    }

    SECTION("Paragraph length") {
        // paragraphLength returns text length without trailing separator
        REQUIRE(buffer.paragraphLength(0) == 5);  // "First" without separator
        REQUIRE(buffer.paragraphLength(2) == 5);  // "Third" without separator
    }

    SECTION("Block access") {
        QTextBlock blk = buffer.block(1);
        REQUIRE(blk.isValid());
        REQUIRE(blk.text() == "Second paragraph with more text");
    }
}

// =============================================================================
// TextBuffer Modification Tests
// =============================================================================

TEST_CASE("TextBuffer text modification", "[editor][text_buffer]") {
    TextBuffer buffer;
    buffer.setPlainText("Hello World");

    SECTION("Insert text") {
        buffer.insert(5, " Beautiful");
        REQUIRE(buffer.plainText() == "Hello Beautiful World");
    }

    SECTION("Remove text") {
        buffer.remove(5, 6);  // Remove " World"
        REQUIRE(buffer.plainText() == "Hello");
    }

    SECTION("Replace text") {
        buffer.replace(6, 5, "Universe");
        REQUIRE(buffer.plainText() == "Hello Universe");
    }

    SECTION("Modification invalidates cache") {
        buffer.insert(0, "Hi ");
        // Cache should be invalidated
        REQUIRE_FALSE(buffer.isPlainTextCached());
    }
}

TEST_CASE("TextBuffer paragraph modification", "[editor][text_buffer]") {
    TextBuffer buffer;
    buffer.setPlainText("One\nTwo\nThree");

    SECTION("Set paragraph text") {
        buffer.setParagraphText(1, "Modified");
        REQUIRE(buffer.paragraphText(1) == "Modified");
    }

    SECTION("Insert paragraph at beginning") {
        buffer.insertParagraph(0, "Zero");
        REQUIRE(buffer.paragraphCount() == 4);
        REQUIRE(buffer.paragraphText(0) == "Zero");
        REQUIRE(buffer.paragraphText(1) == "One");
    }

    SECTION("Insert paragraph in middle") {
        buffer.insertParagraph(1, "OneHalf");
        REQUIRE(buffer.paragraphCount() == 4);
        REQUIRE(buffer.paragraphText(1) == "OneHalf");
        REQUIRE(buffer.paragraphText(2) == "Two");
    }

    SECTION("Insert paragraph at end") {
        buffer.insertParagraph(3, "Four");
        REQUIRE(buffer.paragraphCount() == 4);
        REQUIRE(buffer.paragraphText(3) == "Four");
    }

    SECTION("Remove paragraph") {
        buffer.removeParagraph(1);
        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "One");
        REQUIRE(buffer.paragraphText(1) == "Three");
    }
}

// =============================================================================
// TextBuffer Height Management Tests
// =============================================================================

TEST_CASE("TextBuffer height management", "[editor][text_buffer]") {
    TextBuffer buffer;
    buffer.setEstimatedLineHeight(20.0);
    buffer.setEstimatedCharsPerLine(80);
    buffer.setPlainText("Line 1\nLine 2\nLine 3");

    SECTION("Initial heights are estimated") {
        REQUIRE(buffer.getHeightState(0) == HeightState::Estimated);
        REQUIRE(buffer.getHeightState(1) == HeightState::Estimated);
        REQUIRE(buffer.getHeightState(2) == HeightState::Estimated);
    }

    SECTION("Set paragraph height marks as calculated") {
        buffer.setParagraphHeight(0, 25.0);
        REQUIRE(buffer.getHeightState(0) == HeightState::Calculated);
        REQUIRE_THAT(buffer.getParagraphHeight(0), WithinAbs(25.0, 0.001));
    }

    SECTION("Calculated count updates") {
        REQUIRE(buffer.calculatedParagraphCount() == 0);
        buffer.setParagraphHeight(0, 25.0);
        REQUIRE(buffer.calculatedParagraphCount() == 1);
        buffer.setParagraphHeight(1, 30.0);
        REQUIRE(buffer.calculatedParagraphCount() == 2);
    }

    SECTION("Invalidate paragraph height") {
        buffer.setParagraphHeight(0, 25.0);
        buffer.invalidateParagraphHeight(0);
        REQUIRE(buffer.getHeightState(0) == HeightState::Invalid);
        REQUIRE(buffer.calculatedParagraphCount() == 0);
    }

    SECTION("Y position calculation") {
        buffer.setParagraphHeight(0, 20.0);
        buffer.setParagraphHeight(1, 30.0);
        buffer.setParagraphHeight(2, 25.0);

        REQUIRE_THAT(buffer.getParagraphY(0), WithinAbs(0.0, 0.001));
        REQUIRE_THAT(buffer.getParagraphY(1), WithinAbs(20.0, 0.001));
        REQUIRE_THAT(buffer.getParagraphY(2), WithinAbs(50.0, 0.001));
    }

    SECTION("Find paragraph at Y") {
        buffer.setParagraphHeight(0, 20.0);
        buffer.setParagraphHeight(1, 30.0);
        buffer.setParagraphHeight(2, 25.0);

        REQUIRE(buffer.getParagraphAtY(10.0) == 0);
        REQUIRE(buffer.getParagraphAtY(35.0) == 1);
        REQUIRE(buffer.getParagraphAtY(60.0) == 2);
    }

    SECTION("Total height") {
        buffer.setParagraphHeight(0, 20.0);
        buffer.setParagraphHeight(1, 30.0);
        buffer.setParagraphHeight(2, 25.0);

        REQUIRE_THAT(buffer.totalHeight(), WithinAbs(75.0, 0.001));
    }
}

// =============================================================================
// TextBuffer Observer Tests
// =============================================================================

TEST_CASE("TextBuffer observer notifications", "[editor][text_buffer]") {
    TextBuffer buffer;
    TestObserver observer;
    buffer.addObserver(&observer);
    buffer.setPlainText("Line 1\nLine 2");

    SECTION("Text changed notification on setPlainText") {
        observer.textChangedCount = 0;
        buffer.setPlainText("New text");
        REQUIRE(observer.textChangedCount == 1);
    }

    SECTION("Paragraph inserted notification") {
        buffer.insertParagraph(1, "Inserted");
        REQUIRE(observer.paragraphInsertedCount == 1);
        REQUIRE(observer.lastInsertedIndex == 1);
    }

    SECTION("Paragraph removed notification") {
        buffer.removeParagraph(0);
        REQUIRE(observer.paragraphRemovedCount == 1);
        REQUIRE(observer.lastRemovedIndex == 0);
    }

    SECTION("Paragraph changed notification") {
        buffer.setParagraphText(0, "Changed");
        REQUIRE(observer.paragraphChangedCount == 1);
        REQUIRE(observer.lastChangedIndex == 0);
    }

    SECTION("Height changed notification") {
        // Verify heights are initialized
        REQUIRE(buffer.paragraphCount() == 2);
        double initialHeight = buffer.getParagraphHeight(0);
        REQUIRE(initialHeight > 0.0);  // Should be ~20 for one line

        // Set to a significantly different value (should trigger notification)
        double newHeight = initialHeight * 3.0;  // Triple the height

        // Reset counter to ensure we're counting only from this point
        observer.heightChangedCount = 0;

        buffer.setParagraphHeight(0, newHeight);

        // Verify height was actually changed
        REQUIRE_THAT(buffer.getParagraphHeight(0), WithinAbs(newHeight, 0.001));

        // Check if the difference is sufficient
        double diff = std::abs(newHeight - initialHeight);
        REQUIRE(diff > 0.001);  // Must be > threshold

        // Verify notification was sent
        REQUIRE(observer.heightChangedCount >= 1);
    }

    SECTION("Remove observer") {
        buffer.removeObserver(&observer);
        observer.textChangedCount = 0;
        buffer.setPlainText("Test");
        REQUIRE(observer.textChangedCount == 0);
    }
}

// =============================================================================
// TextBuffer Large Document Tests
// =============================================================================

TEST_CASE("TextBuffer large document performance", "[editor][text_buffer][.benchmark]") {
    TextBuffer buffer;
    buffer.setEstimatedLineHeight(20.0);
    buffer.setEstimatedCharsPerLine(80);

    // Create a large document
    QString largeText;
    for (int i = 0; i < 1000; ++i) {
        largeText += QString("Paragraph %1 with some sample text for testing.\n").arg(i);
    }
    buffer.setPlainText(largeText);

    SECTION("Paragraph count") {
        REQUIRE(buffer.paragraphCount() == 1001);  // 1000 + empty last
    }

    SECTION("Height tree operations are efficient") {
        // Set heights for all paragraphs
        for (size_t i = 0; i < 1000; ++i) {
            buffer.setParagraphHeight(i, 20.0 + (i % 5) * 5.0);
        }

        // Query operations should be fast
        double y = buffer.getParagraphY(500);
        REQUIRE(y > 0.0);

        size_t para = buffer.getParagraphAtY(5000.0);
        REQUIRE(para > 0);
        REQUIRE(para < 1000);
    }
}
