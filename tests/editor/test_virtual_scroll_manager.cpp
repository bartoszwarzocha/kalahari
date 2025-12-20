/// @file test_virtual_scroll_manager.cpp
/// @brief Unit tests for VirtualScrollManager (OpenSpec #00042 Phase 2.8-2.9)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/virtual_scroll_manager.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>

using namespace kalahari::editor;

// =============================================================================
// Helper Functions
// =============================================================================

/// Create a document with specified number of paragraphs
std::unique_ptr<KmlDocument> createDocument(int paragraphCount) {
    auto doc = std::make_unique<KmlDocument>();
    for (int i = 0; i < paragraphCount; ++i) {
        auto para = std::make_unique<KmlParagraph>(
            QString("Paragraph %1 with some text content.").arg(i));
        doc->addParagraph(std::move(para));
    }
    return doc;
}

// =============================================================================
// Constructor Tests
// =============================================================================

TEST_CASE("VirtualScrollManager default constructor", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;

    SECTION("Initial document is null") {
        REQUIRE(manager.document() == nullptr);
    }

    SECTION("Initial viewport is zero") {
        REQUIRE(manager.viewportTop() == 0.0);
        REQUIRE(manager.viewportHeight() == 0.0);
    }

    SECTION("Default buffer size is BUFFER_PARAGRAPHS") {
        REQUIRE(manager.bufferParagraphs() == BUFFER_PARAGRAPHS);
        REQUIRE(manager.bufferParagraphs() == 10);
    }

    SECTION("Visible range is invalid without document") {
        auto [first, last] = manager.visibleRange();
        REQUIRE(first == -1);
        REQUIRE(last == -1);
    }
}

// =============================================================================
// Document Management Tests
// =============================================================================

TEST_CASE("VirtualScrollManager setDocument", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    auto doc = createDocument(20);

    SECTION("Set document") {
        manager.setDocument(doc.get());
        REQUIRE(manager.document() == doc.get());
    }

    SECTION("Set null document") {
        manager.setDocument(doc.get());
        manager.setDocument(nullptr);
        REQUIRE(manager.document() == nullptr);
    }

    SECTION("Change document") {
        auto doc2 = createDocument(10);
        manager.setDocument(doc.get());
        manager.setDocument(doc2.get());
        REQUIRE(manager.document() == doc2.get());
    }
}

// =============================================================================
// Viewport Management Tests
// =============================================================================

TEST_CASE("VirtualScrollManager setViewport", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;

    SECTION("Set viewport top and height") {
        manager.setViewport(100.0, 600.0);
        REQUIRE(manager.viewportTop() == 100.0);
        REQUIRE(manager.viewportHeight() == 600.0);
    }

    SECTION("Negative top is clamped to 0") {
        manager.setViewport(-100.0, 600.0);
        REQUIRE(manager.viewportTop() == 0.0);
        REQUIRE(manager.viewportHeight() == 600.0);
    }

    SECTION("Negative height is clamped to 0") {
        manager.setViewport(100.0, -600.0);
        REQUIRE(manager.viewportTop() == 100.0);
        REQUIRE(manager.viewportHeight() == 0.0);
    }

    SECTION("Zero viewport") {
        manager.setViewport(0.0, 0.0);
        REQUIRE(manager.viewportTop() == 0.0);
        REQUIRE(manager.viewportHeight() == 0.0);
    }
}

TEST_CASE("VirtualScrollManager individual viewport setters", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;

    SECTION("setViewportTop") {
        manager.setViewportTop(250.0);
        REQUIRE(manager.viewportTop() == 250.0);
    }

    SECTION("setViewportTop negative clamped") {
        manager.setViewportTop(-50.0);
        REQUIRE(manager.viewportTop() == 0.0);
    }

    SECTION("setViewportHeight") {
        manager.setViewportHeight(800.0);
        REQUIRE(manager.viewportHeight() == 800.0);
    }

    SECTION("setViewportHeight negative clamped") {
        manager.setViewportHeight(-100.0);
        REQUIRE(manager.viewportHeight() == 0.0);
    }
}

// =============================================================================
// Visible Range Tests (Core Functionality)
// =============================================================================

TEST_CASE("VirtualScrollManager visibleRange with no document", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    manager.setViewport(0.0, 600.0);

    auto [first, last] = manager.visibleRange();
    REQUIRE(first == -1);
    REQUIRE(last == -1);
}

TEST_CASE("VirtualScrollManager visibleRange with empty document", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    KmlDocument emptyDoc;
    manager.setDocument(&emptyDoc);
    manager.setViewport(0.0, 600.0);

    auto [first, last] = manager.visibleRange();
    REQUIRE(first == -1);
    REQUIRE(last == -1);
}

TEST_CASE("VirtualScrollManager visibleRange with zero viewport height", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    auto doc = createDocument(20);
    manager.setDocument(doc.get());
    manager.setViewport(0.0, 0.0);  // Zero height

    auto [first, last] = manager.visibleRange();
    REQUIRE(first == -1);
    REQUIRE(last == -1);
}

TEST_CASE("VirtualScrollManager visibleRange at document start", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    auto doc = createDocument(100);  // 100 paragraphs
    manager.setDocument(doc.get());
    manager.setViewport(0.0, 600.0);  // Viewport at top

    SECTION("Returns valid range") {
        auto [first, last] = manager.visibleRange();
        REQUIRE(first >= 0);
        REQUIRE(last >= 0);
        REQUIRE(first <= last);
        REQUIRE(last < doc->paragraphCount());
    }

    SECTION("First paragraph is visible") {
        auto [first, last] = manager.visibleRange();
        REQUIRE(first == 0);  // At top, first visible should be 0
    }

    SECTION("Exact range starts at 0") {
        auto [first, last] = manager.exactVisibleRange();
        REQUIRE(first == 0);
    }
}

TEST_CASE("VirtualScrollManager visibleRange with scrolling", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    auto doc = createDocument(100);
    manager.setDocument(doc.get());

    SECTION("Scrolling down changes visible range") {
        manager.setViewport(0.0, 600.0);
        auto [first1, last1] = manager.visibleRange();

        manager.setViewport(500.0, 600.0);  // Scroll down
        auto [first2, last2] = manager.visibleRange();

        // First visible paragraph should be different after scrolling
        REQUIRE(first2 > first1);
    }

    SECTION("Visible range always within document bounds") {
        // Test various scroll positions
        std::vector<qreal> scrollPositions = {0.0, 100.0, 500.0, 1000.0, 5000.0, 10000.0};

        for (qreal pos : scrollPositions) {
            manager.setViewport(pos, 600.0);
            auto [first, last] = manager.visibleRange();

            if (first >= 0) {  // Valid range
                REQUIRE(first >= 0);
                REQUIRE(first < doc->paragraphCount());
                REQUIRE(last >= 0);
                REQUIRE(last < doc->paragraphCount());
                REQUIRE(first <= last);
            }
        }
    }
}

TEST_CASE("VirtualScrollManager visibleRange includes buffer", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    auto doc = createDocument(100);
    manager.setDocument(doc.get());
    manager.setViewport(500.0, 300.0);  // Scroll to middle

    SECTION("Buffer extends range") {
        auto [exactFirst, exactLast] = manager.exactVisibleRange();
        auto [bufferedFirst, bufferedLast] = manager.visibleRange();

        // Buffered range should be wider (or equal if at boundaries)
        REQUIRE(bufferedFirst <= exactFirst);
        REQUIRE(bufferedLast >= exactLast);
    }

    SECTION("Buffer does not exceed document bounds") {
        // At start of document
        manager.setViewport(0.0, 300.0);
        auto [first, last] = manager.visibleRange();
        REQUIRE(first == 0);  // Cannot go below 0

        // At end of document (scroll very far)
        manager.setViewport(10000.0, 300.0);
        auto [first2, last2] = manager.visibleRange();
        REQUIRE(last2 < doc->paragraphCount());  // Cannot exceed document
    }
}

// =============================================================================
// Exact Visible Range Tests
// =============================================================================

TEST_CASE("VirtualScrollManager exactVisibleRange", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    auto doc = createDocument(50);
    manager.setDocument(doc.get());
    manager.setViewport(0.0, 600.0);

    SECTION("Returns valid range") {
        auto [first, last] = manager.exactVisibleRange();
        REQUIRE(first >= 0);
        REQUIRE(last >= 0);
        REQUIRE(first <= last);
    }

    SECTION("Exact range is subset of buffered range") {
        auto [exactFirst, exactLast] = manager.exactVisibleRange();
        auto [bufferedFirst, bufferedLast] = manager.visibleRange();

        REQUIRE(exactFirst >= bufferedFirst);
        REQUIRE(exactLast <= bufferedLast);
    }
}

// =============================================================================
// isParagraphVisible Tests
// =============================================================================

TEST_CASE("VirtualScrollManager isParagraphVisible", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    auto doc = createDocument(100);
    manager.setDocument(doc.get());
    manager.setViewport(500.0, 300.0);

    SECTION("Visible paragraphs return true") {
        auto [first, last] = manager.visibleRange();
        for (int i = first; i <= last; ++i) {
            REQUIRE(manager.isParagraphVisible(i));
        }
    }

    SECTION("Non-visible paragraphs return false") {
        auto [first, last] = manager.visibleRange();
        if (first > 0) {
            REQUIRE_FALSE(manager.isParagraphVisible(first - 1));
        }
        if (last < doc->paragraphCount() - 1) {
            REQUIRE_FALSE(manager.isParagraphVisible(last + 1));
        }
    }

    SECTION("Negative index returns false") {
        REQUIRE_FALSE(manager.isParagraphVisible(-1));
    }

    SECTION("Out of bounds index returns false") {
        REQUIRE_FALSE(manager.isParagraphVisible(doc->paragraphCount()));
        REQUIRE_FALSE(manager.isParagraphVisible(1000));
    }
}

TEST_CASE("VirtualScrollManager isParagraphExactlyVisible", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    auto doc = createDocument(100);
    manager.setDocument(doc.get());
    manager.setViewport(500.0, 300.0);

    SECTION("Exactly visible paragraphs return true") {
        auto [first, last] = manager.exactVisibleRange();
        for (int i = first; i <= last; ++i) {
            REQUIRE(manager.isParagraphExactlyVisible(i));
        }
    }

    SECTION("Buffer paragraphs may not be exactly visible") {
        auto [exactFirst, exactLast] = manager.exactVisibleRange();
        auto [bufferedFirst, bufferedLast] = manager.visibleRange();

        // If there's a difference, buffer paragraphs should not be exactly visible
        if (bufferedFirst < exactFirst) {
            REQUIRE_FALSE(manager.isParagraphExactlyVisible(bufferedFirst));
        }
        if (bufferedLast > exactLast) {
            REQUIRE_FALSE(manager.isParagraphExactlyVisible(bufferedLast));
        }
    }
}

// =============================================================================
// Buffer Configuration Tests
// =============================================================================

TEST_CASE("VirtualScrollManager buffer configuration", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;

    SECTION("Default buffer size") {
        REQUIRE(manager.bufferParagraphs() == BUFFER_PARAGRAPHS);
    }

    SECTION("Set buffer size") {
        manager.setBufferParagraphs(5);
        REQUIRE(manager.bufferParagraphs() == 5);

        manager.setBufferParagraphs(20);
        REQUIRE(manager.bufferParagraphs() == 20);
    }

    SECTION("Negative buffer clamped to 0") {
        manager.setBufferParagraphs(-10);
        REQUIRE(manager.bufferParagraphs() >= 0);
    }

    SECTION("Zero buffer is valid") {
        manager.setBufferParagraphs(0);
        REQUIRE(manager.bufferParagraphs() == 0);
    }
}

TEST_CASE("VirtualScrollManager buffer affects visible range", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    auto doc = createDocument(100);
    manager.setDocument(doc.get());
    manager.setViewport(500.0, 300.0);

    SECTION("Larger buffer extends range") {
        manager.setBufferParagraphs(5);
        auto [first5, last5] = manager.visibleRange();

        manager.setBufferParagraphs(20);
        auto [first20, last20] = manager.visibleRange();

        // Larger buffer should result in wider range
        REQUIRE(first20 <= first5);
        REQUIRE(last20 >= last5);
    }

    SECTION("Zero buffer equals exact range") {
        manager.setBufferParagraphs(0);
        auto [buffered, bufferedLast] = manager.visibleRange();
        auto [exact, exactLast] = manager.exactVisibleRange();

        REQUIRE(buffered == exact);
        REQUIRE(bufferedLast == exactLast);
    }
}

// =============================================================================
// Copy/Move Tests
// =============================================================================

TEST_CASE("VirtualScrollManager copy constructor", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager original;
    auto doc = createDocument(50);
    original.setDocument(doc.get());
    original.setViewport(200.0, 400.0);
    original.setBufferParagraphs(15);

    VirtualScrollManager copy(original);

    SECTION("Copy has same document pointer") {
        REQUIRE(copy.document() == doc.get());
    }

    SECTION("Copy has same viewport") {
        REQUIRE(copy.viewportTop() == 200.0);
        REQUIRE(copy.viewportHeight() == 400.0);
    }

    SECTION("Copy has same buffer size") {
        REQUIRE(copy.bufferParagraphs() == 15);
    }
}

TEST_CASE("VirtualScrollManager move constructor", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager original;
    auto doc = createDocument(50);
    original.setDocument(doc.get());
    original.setViewport(200.0, 400.0);

    VirtualScrollManager moved(std::move(original));

    SECTION("Moved has original data") {
        REQUIRE(moved.document() == doc.get());
        REQUIRE(moved.viewportTop() == 200.0);
        REQUIRE(moved.viewportHeight() == 400.0);
    }

    SECTION("Original is reset") {
        REQUIRE(original.document() == nullptr);
        REQUIRE(original.viewportTop() == 0.0);
        REQUIRE(original.viewportHeight() == 0.0);
    }
}

TEST_CASE("VirtualScrollManager copy assignment", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager original;
    auto doc = createDocument(50);
    original.setDocument(doc.get());
    original.setViewport(200.0, 400.0);

    VirtualScrollManager target;
    target = original;

    SECTION("Target has source data") {
        REQUIRE(target.document() == doc.get());
        REQUIRE(target.viewportTop() == 200.0);
        REQUIRE(target.viewportHeight() == 400.0);
    }

    SECTION("Self-assignment is safe") {
        target = target;
        REQUIRE(target.document() == doc.get());
    }
}

TEST_CASE("VirtualScrollManager move assignment", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager original;
    auto doc = createDocument(50);
    original.setDocument(doc.get());
    original.setViewport(200.0, 400.0);

    VirtualScrollManager target;
    target = std::move(original);

    SECTION("Target has moved data") {
        REQUIRE(target.document() == doc.get());
        REQUIRE(target.viewportTop() == 200.0);
    }

    SECTION("Original is reset") {
        REQUIRE(original.document() == nullptr);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("VirtualScrollManager edge cases", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;

    SECTION("Single paragraph document") {
        auto doc = createDocument(1);
        manager.setDocument(doc.get());
        manager.setViewport(0.0, 600.0);

        auto [first, last] = manager.visibleRange();
        REQUIRE(first == 0);
        REQUIRE(last == 0);
    }

    SECTION("Very large scroll position") {
        auto doc = createDocument(100);
        manager.setDocument(doc.get());
        manager.setViewport(1000000.0, 600.0);

        auto [first, last] = manager.visibleRange();
        // Should be clamped to valid range
        REQUIRE(first >= 0);
        REQUIRE(last < doc->paragraphCount());
    }

    SECTION("Very large viewport height") {
        auto doc = createDocument(10);
        manager.setDocument(doc.get());
        manager.setViewport(0.0, 100000.0);

        auto [first, last] = manager.visibleRange();
        // Should include all paragraphs
        REQUIRE(first == 0);
        REQUIRE(last == doc->paragraphCount() - 1);
    }

    SECTION("Very small viewport height") {
        auto doc = createDocument(100);
        manager.setDocument(doc.get());
        manager.setViewport(0.0, 1.0);

        auto [first, last] = manager.visibleRange();
        // Should still return valid range
        REQUIRE(first >= 0);
        REQUIRE(last >= 0);
    }
}

// =============================================================================
// BUFFER_PARAGRAPHS Constant Tests
// =============================================================================

TEST_CASE("BUFFER_PARAGRAPHS constant", "[editor][virtual_scroll_manager]") {
    SECTION("Value is 10") {
        REQUIRE(BUFFER_PARAGRAPHS == 10);
    }

    SECTION("Value is reasonable") {
        REQUIRE(BUFFER_PARAGRAPHS > 0);
        REQUIRE(BUFFER_PARAGRAPHS <= 100);
    }
}

// =============================================================================
// Document Change Handling
// =============================================================================

TEST_CASE("VirtualScrollManager after document changes", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    auto doc = createDocument(50);
    manager.setDocument(doc.get());
    manager.setViewport(0.0, 600.0);

    SECTION("Range still valid after paragraph added") {
        auto para = std::make_unique<KmlParagraph>("New paragraph");
        doc->addParagraph(std::move(para));

        auto [first, last] = manager.visibleRange();
        REQUIRE(first >= 0);
        REQUIRE(last < doc->paragraphCount());
    }

    SECTION("Range still valid after paragraph removed") {
        doc->removeParagraph(0);

        auto [first, last] = manager.visibleRange();
        REQUIRE(first >= 0);
        REQUIRE(last < doc->paragraphCount());
    }

    SECTION("Range still valid after document cleared") {
        doc->clear();

        auto [first, last] = manager.visibleRange();
        // Empty document returns invalid range
        REQUIRE(first == -1);
        REQUIRE(last == -1);
    }
}

// =============================================================================
// Visible Range Calculation Properties
// =============================================================================

TEST_CASE("VirtualScrollManager visible range properties", "[editor][virtual_scroll_manager]") {
    VirtualScrollManager manager;
    auto doc = createDocument(100);
    manager.setDocument(doc.get());
    manager.setViewport(500.0, 600.0);

    SECTION("First is less than or equal to last") {
        auto [first, last] = manager.visibleRange();
        REQUIRE(first <= last);
    }

    SECTION("Exact first is less than or equal to exact last") {
        auto [first, last] = manager.exactVisibleRange();
        REQUIRE(first <= last);
    }

    SECTION("Range is consistent across multiple calls") {
        auto [first1, last1] = manager.visibleRange();
        auto [first2, last2] = manager.visibleRange();
        REQUIRE(first1 == first2);
        REQUIRE(last1 == last2);
    }

    SECTION("Changing viewport changes range") {
        auto [first1, last1] = manager.visibleRange();
        manager.setViewportTop(manager.viewportTop() + 100.0);
        auto [first2, last2] = manager.visibleRange();

        // Range should be different after scrolling
        REQUIRE((first1 != first2 || last1 != last2));
    }
}

// =============================================================================
// ParagraphInfo Tests (Phase 2.9)
// =============================================================================

TEST_CASE("ParagraphInfo struct", "[editor][virtual_scroll_manager][height]") {
    SECTION("Default constructor") {
        ParagraphInfo info;
        REQUIRE(info.y == 0.0);
        REQUIRE(info.height == ESTIMATED_LINE_HEIGHT);
        REQUIRE(info.heightKnown == false);
    }

    SECTION("Parameterized constructor") {
        ParagraphInfo info(100.0, 30.0, true);
        REQUIRE(info.y == 100.0);
        REQUIRE(info.height == 30.0);
        REQUIRE(info.heightKnown == true);
    }

    SECTION("Equality comparison") {
        ParagraphInfo a(100.0, 30.0, true);
        ParagraphInfo b(100.0, 30.0, true);
        ParagraphInfo c(100.0, 30.0, false);
        ParagraphInfo d(100.0, 25.0, true);

        REQUIRE(a == b);
        REQUIRE(a != c);
        REQUIRE(a != d);
    }
}

TEST_CASE("ESTIMATED_LINE_HEIGHT constant", "[editor][virtual_scroll_manager][height]") {
    SECTION("Value is 20.0") {
        REQUIRE(ESTIMATED_LINE_HEIGHT == 20.0);
    }

    SECTION("Value is reasonable for text") {
        REQUIRE(ESTIMATED_LINE_HEIGHT > 10.0);  // Not too small
        REQUIRE(ESTIMATED_LINE_HEIGHT < 50.0);  // Not too large
    }
}

// =============================================================================
// Height Management Tests (Phase 2.9)
// =============================================================================

TEST_CASE("VirtualScrollManager updateParagraphHeight", "[editor][virtual_scroll_manager][height]") {
    VirtualScrollManager manager;
    auto doc = createDocument(10);
    manager.setDocument(doc.get());

    SECTION("Update height marks as known") {
        REQUIRE_FALSE(manager.isHeightKnown(0));
        manager.updateParagraphHeight(0, 30.0);
        REQUIRE(manager.isHeightKnown(0));
    }

    SECTION("Update height changes value") {
        manager.updateParagraphHeight(0, 45.0);
        ParagraphInfo info = manager.paragraphInfo(0);
        REQUIRE(info.height == 45.0);
    }

    SECTION("Out of range index is ignored") {
        manager.updateParagraphHeight(-1, 30.0);
        manager.updateParagraphHeight(100, 30.0);
        // Should not crash, and nothing should change
        REQUIRE(manager.knownHeightCount() == 0);
    }

    SECTION("Negative height is clamped to 1.0") {
        manager.updateParagraphHeight(0, -10.0);
        ParagraphInfo info = manager.paragraphInfo(0);
        REQUIRE(info.height >= 1.0);
    }

    SECTION("Zero height is clamped to 1.0") {
        manager.updateParagraphHeight(0, 0.0);
        ParagraphInfo info = manager.paragraphInfo(0);
        REQUIRE(info.height >= 1.0);
    }
}

TEST_CASE("VirtualScrollManager totalHeight", "[editor][virtual_scroll_manager][height]") {
    VirtualScrollManager manager;

    SECTION("No document returns 0") {
        REQUIRE(manager.totalHeight() == 0.0);
    }

    SECTION("Empty document returns 0") {
        KmlDocument emptyDoc;
        manager.setDocument(&emptyDoc);
        REQUIRE(manager.totalHeight() == 0.0);
    }

    SECTION("Uses estimated height for unknown paragraphs") {
        auto doc = createDocument(5);
        manager.setDocument(doc.get());

        qreal expectedHeight = 5 * ESTIMATED_LINE_HEIGHT;
        REQUIRE(manager.totalHeight() == expectedHeight);
    }

    SECTION("Uses actual height for known paragraphs") {
        auto doc = createDocument(3);
        manager.setDocument(doc.get());

        manager.updateParagraphHeight(0, 30.0);
        manager.updateParagraphHeight(1, 40.0);
        // Paragraph 2 still uses estimated height

        qreal expectedHeight = 30.0 + 40.0 + ESTIMATED_LINE_HEIGHT;
        REQUIRE(manager.totalHeight() == expectedHeight);
    }

    SECTION("All known heights") {
        auto doc = createDocument(3);
        manager.setDocument(doc.get());

        manager.updateParagraphHeight(0, 25.0);
        manager.updateParagraphHeight(1, 35.0);
        manager.updateParagraphHeight(2, 45.0);

        REQUIRE(manager.totalHeight() == 105.0);
    }
}

TEST_CASE("VirtualScrollManager paragraphY", "[editor][virtual_scroll_manager][height]") {
    VirtualScrollManager manager;
    auto doc = createDocument(5);
    manager.setDocument(doc.get());

    SECTION("First paragraph at Y=0") {
        REQUIRE(manager.paragraphY(0) == 0.0);
    }

    SECTION("Subsequent paragraphs accumulate height") {
        // All estimated heights initially
        REQUIRE(manager.paragraphY(0) == 0.0);
        REQUIRE(manager.paragraphY(1) == ESTIMATED_LINE_HEIGHT);
        REQUIRE(manager.paragraphY(2) == 2 * ESTIMATED_LINE_HEIGHT);
        REQUIRE(manager.paragraphY(3) == 3 * ESTIMATED_LINE_HEIGHT);
        REQUIRE(manager.paragraphY(4) == 4 * ESTIMATED_LINE_HEIGHT);
    }

    SECTION("Updates reflect in Y positions") {
        manager.updateParagraphHeight(0, 30.0);
        manager.updateParagraphHeight(1, 25.0);

        REQUIRE(manager.paragraphY(0) == 0.0);
        REQUIRE(manager.paragraphY(1) == 30.0);
        REQUIRE(manager.paragraphY(2) == 55.0);  // 30 + 25
        REQUIRE(manager.paragraphY(3) == 55.0 + ESTIMATED_LINE_HEIGHT);
    }

    SECTION("Invalid index returns 0") {
        REQUIRE(manager.paragraphY(-1) == 0.0);
        REQUIRE(manager.paragraphY(100) == 0.0);
    }
}

TEST_CASE("VirtualScrollManager paragraphInfo", "[editor][virtual_scroll_manager][height]") {
    VirtualScrollManager manager;
    auto doc = createDocument(5);
    manager.setDocument(doc.get());

    SECTION("Returns correct info for valid index") {
        manager.updateParagraphHeight(2, 50.0);

        ParagraphInfo info = manager.paragraphInfo(2);
        REQUIRE(info.y == 2 * ESTIMATED_LINE_HEIGHT);
        REQUIRE(info.height == 50.0);
        REQUIRE(info.heightKnown == true);
    }

    SECTION("Returns default for invalid index") {
        ParagraphInfo info = manager.paragraphInfo(-1);
        REQUIRE(info.y == 0.0);
        REQUIRE(info.height == ESTIMATED_LINE_HEIGHT);
        REQUIRE(info.heightKnown == false);

        info = manager.paragraphInfo(100);
        REQUIRE(info.y == 0.0);
        REQUIRE(info.height == ESTIMATED_LINE_HEIGHT);
        REQUIRE(info.heightKnown == false);
    }
}

TEST_CASE("VirtualScrollManager isHeightKnown", "[editor][virtual_scroll_manager][height]") {
    VirtualScrollManager manager;
    auto doc = createDocument(5);
    manager.setDocument(doc.get());

    SECTION("Initially all unknown") {
        for (int i = 0; i < 5; ++i) {
            REQUIRE_FALSE(manager.isHeightKnown(i));
        }
    }

    SECTION("Known after update") {
        manager.updateParagraphHeight(2, 30.0);
        REQUIRE(manager.isHeightKnown(2));
        REQUIRE_FALSE(manager.isHeightKnown(0));
        REQUIRE_FALSE(manager.isHeightKnown(4));
    }

    SECTION("Invalid index returns false") {
        REQUIRE_FALSE(manager.isHeightKnown(-1));
        REQUIRE_FALSE(manager.isHeightKnown(100));
    }
}

TEST_CASE("VirtualScrollManager knownHeightCount", "[editor][virtual_scroll_manager][height]") {
    VirtualScrollManager manager;
    auto doc = createDocument(10);
    manager.setDocument(doc.get());

    SECTION("Initially zero") {
        REQUIRE(manager.knownHeightCount() == 0);
    }

    SECTION("Increases with updates") {
        manager.updateParagraphHeight(0, 30.0);
        REQUIRE(manager.knownHeightCount() == 1);

        manager.updateParagraphHeight(3, 40.0);
        REQUIRE(manager.knownHeightCount() == 2);

        manager.updateParagraphHeight(7, 50.0);
        REQUIRE(manager.knownHeightCount() == 3);
    }

    SECTION("Same index update doesn't increase count") {
        manager.updateParagraphHeight(0, 30.0);
        manager.updateParagraphHeight(0, 35.0);
        REQUIRE(manager.knownHeightCount() == 1);
    }

    SECTION("No document returns zero") {
        VirtualScrollManager emptyManager;
        REQUIRE(emptyManager.knownHeightCount() == 0);
    }
}

TEST_CASE("VirtualScrollManager resetHeights", "[editor][virtual_scroll_manager][height]") {
    VirtualScrollManager manager;
    auto doc = createDocument(5);
    manager.setDocument(doc.get());

    // Update some heights
    manager.updateParagraphHeight(0, 30.0);
    manager.updateParagraphHeight(2, 40.0);
    manager.updateParagraphHeight(4, 50.0);
    REQUIRE(manager.knownHeightCount() == 3);

    SECTION("Resets all heights to estimated") {
        manager.resetHeights();

        REQUIRE(manager.knownHeightCount() == 0);
        REQUIRE(manager.totalHeight() == 5 * ESTIMATED_LINE_HEIGHT);
    }

    SECTION("Resets Y positions") {
        manager.resetHeights();

        for (int i = 0; i < 5; ++i) {
            REQUIRE(manager.paragraphY(i) == i * ESTIMATED_LINE_HEIGHT);
        }
    }

    SECTION("Heights can be updated again after reset") {
        manager.resetHeights();
        manager.updateParagraphHeight(0, 25.0);
        REQUIRE(manager.isHeightKnown(0));
        REQUIRE(manager.paragraphInfo(0).height == 25.0);
    }
}

// =============================================================================
// Height-Based Visible Range Tests (Phase 2.9)
// =============================================================================

TEST_CASE("VirtualScrollManager visible range with known heights", "[editor][virtual_scroll_manager][height]") {
    VirtualScrollManager manager;
    auto doc = createDocument(20);
    manager.setDocument(doc.get());
    manager.setBufferParagraphs(0);  // Disable buffer for precise testing

    SECTION("Uses known heights for range calculation") {
        // Set first 5 paragraphs to 100px each
        for (int i = 0; i < 5; ++i) {
            manager.updateParagraphHeight(i, 100.0);
        }

        // Viewport at 0-200 should show paragraphs 0-1 (100px each)
        manager.setViewport(0.0, 200.0);
        auto [first, last] = manager.exactVisibleRange();

        REQUIRE(first == 0);
        REQUIRE(last == 1);  // 0-99 and 100-199 are visible in 0-200
    }

    SECTION("Mixed known and estimated heights") {
        manager.updateParagraphHeight(0, 50.0);
        // Rest use estimated height (20px)

        manager.setViewport(0.0, 100.0);
        auto [first, last] = manager.exactVisibleRange();

        REQUIRE(first == 0);
        // Paragraph 0: 0-50, Paragraph 1: 50-70, Paragraph 2: 70-90, Paragraph 3: 90-110
        // So paragraphs 0-4 should be visible in 0-100
        REQUIRE(last >= 2);  // At least paragraphs 0, 1, 2 visible
    }

    SECTION("Scrolled position with known heights") {
        // 10 paragraphs of 50px each
        for (int i = 0; i < 10; ++i) {
            manager.updateParagraphHeight(i, 50.0);
        }

        // Scroll to Y=150, viewport 100px
        // Paragraphs at: 0@0, 1@50, 2@100, 3@150, 4@200, 5@250...
        // Visible: paragraphs 3 (150-200) and maybe 4 (200-250, starts at 200)
        manager.setViewport(150.0, 100.0);
        auto [first, last] = manager.exactVisibleRange();

        REQUIRE(first == 3);  // First paragraph that overlaps with 150-250
        // last should be 4 (starts at 200, which is < 250)
    }
}

TEST_CASE("VirtualScrollManager document changes update heights", "[editor][virtual_scroll_manager][height]") {
    VirtualScrollManager manager;
    auto doc = createDocument(5);
    manager.setDocument(doc.get());

    manager.updateParagraphHeight(0, 30.0);
    manager.updateParagraphHeight(1, 40.0);

    SECTION("Adding paragraph syncs info") {
        auto para = std::make_unique<KmlParagraph>("New paragraph");
        doc->addParagraph(std::move(para));

        // Should have 6 paragraphs now with proper info
        REQUIRE(manager.paragraphY(5) > 0.0);
        REQUIRE_FALSE(manager.isHeightKnown(5));
    }

    SECTION("Removing paragraph syncs info") {
        doc->removeParagraph(0);

        // After removal, should have 4 paragraphs
        // The known height for paragraph 1 (now 0) should still be there
        qreal totalBefore = manager.totalHeight();
        REQUIRE(totalBefore > 0.0);
    }

    SECTION("Changing document resets heights") {
        auto newDoc = createDocument(3);
        manager.setDocument(newDoc.get());

        REQUIRE(manager.knownHeightCount() == 0);
        REQUIRE(manager.totalHeight() == 3 * ESTIMATED_LINE_HEIGHT);
    }
}

TEST_CASE("VirtualScrollManager copy preserves heights", "[editor][virtual_scroll_manager][height]") {
    VirtualScrollManager original;
    auto doc = createDocument(5);
    original.setDocument(doc.get());
    original.updateParagraphHeight(0, 30.0);
    original.updateParagraphHeight(2, 50.0);

    SECTION("Copy constructor preserves heights") {
        VirtualScrollManager copy(original);

        REQUIRE(copy.knownHeightCount() == 2);
        REQUIRE(copy.isHeightKnown(0));
        REQUIRE(copy.isHeightKnown(2));
        REQUIRE(copy.paragraphInfo(0).height == 30.0);
        REQUIRE(copy.paragraphInfo(2).height == 50.0);
    }

    SECTION("Copy assignment preserves heights") {
        VirtualScrollManager copy;
        copy = original;

        REQUIRE(copy.knownHeightCount() == 2);
        REQUIRE(copy.paragraphInfo(0).height == 30.0);
    }
}

TEST_CASE("VirtualScrollManager move preserves heights", "[editor][virtual_scroll_manager][height]") {
    VirtualScrollManager original;
    auto doc = createDocument(5);
    original.setDocument(doc.get());
    original.updateParagraphHeight(0, 30.0);
    original.updateParagraphHeight(2, 50.0);

    SECTION("Move constructor transfers heights") {
        VirtualScrollManager moved(std::move(original));

        REQUIRE(moved.knownHeightCount() == 2);
        REQUIRE(moved.isHeightKnown(0));
        REQUIRE(moved.paragraphInfo(0).height == 30.0);
    }

    SECTION("Move assignment transfers heights") {
        VirtualScrollManager moved;
        moved = std::move(original);

        REQUIRE(moved.knownHeightCount() == 2);
    }
}

// =============================================================================
// Scrolling Support Tests (Phase 2.10)
// =============================================================================

TEST_CASE("VirtualScrollManager scrollOffset getter/setter", "[editor][virtual_scroll_manager][scrolling]") {
    VirtualScrollManager manager;
    auto doc = createDocument(50);
    manager.setDocument(doc.get());
    manager.setViewportHeight(200.0);

    SECTION("Initial scroll offset is 0") {
        REQUIRE(manager.scrollOffset() == 0.0);
    }

    SECTION("scrollOffset returns same as viewportTop") {
        manager.setViewportTop(100.0);
        REQUIRE(manager.scrollOffset() == 100.0);
        REQUIRE(manager.scrollOffset() == manager.viewportTop());
    }

    SECTION("setScrollOffset updates viewportTop") {
        manager.setScrollOffset(150.0);
        REQUIRE(manager.viewportTop() == 150.0);
    }

    SECTION("setScrollOffset clamps negative to 0") {
        manager.setScrollOffset(-100.0);
        REQUIRE(manager.scrollOffset() == 0.0);
    }

    SECTION("setScrollOffset clamps to max scroll") {
        // Try to scroll way past the end
        manager.setScrollOffset(1000000.0);
        // Should be clamped to maxScrollOffset
        REQUIRE(manager.scrollOffset() <= manager.maxScrollOffset());
    }
}

TEST_CASE("VirtualScrollManager maxScrollOffset", "[editor][virtual_scroll_manager][scrolling]") {
    VirtualScrollManager manager;

    SECTION("No document returns 0") {
        manager.setViewportHeight(200.0);
        REQUIRE(manager.maxScrollOffset() == 0.0);
    }

    SECTION("Zero viewport returns 0") {
        auto doc = createDocument(10);
        manager.setDocument(doc.get());
        manager.setViewportHeight(0.0);
        REQUIRE(manager.maxScrollOffset() == 0.0);
    }

    SECTION("Content smaller than viewport returns 0") {
        auto doc = createDocument(3);  // 3 * 20 = 60px
        manager.setDocument(doc.get());
        manager.setViewportHeight(200.0);  // Larger than content
        REQUIRE(manager.maxScrollOffset() == 0.0);
    }

    SECTION("Content larger than viewport returns positive value") {
        auto doc = createDocument(50);  // 50 * 20 = 1000px
        manager.setDocument(doc.get());
        manager.setViewportHeight(200.0);

        qreal maxScroll = manager.maxScrollOffset();
        REQUIRE(maxScroll > 0.0);
        REQUIRE(maxScroll == manager.totalHeight() - manager.viewportHeight());
    }

    SECTION("Accounts for known heights") {
        auto doc = createDocument(10);
        manager.setDocument(doc.get());
        manager.setViewportHeight(100.0);

        // Update some heights to larger values
        for (int i = 0; i < 10; ++i) {
            manager.updateParagraphHeight(i, 50.0);  // 10 * 50 = 500px
        }

        REQUIRE(manager.maxScrollOffset() == 400.0);  // 500 - 100
    }
}

TEST_CASE("VirtualScrollManager paragraphAtY", "[editor][virtual_scroll_manager][scrolling]") {
    VirtualScrollManager manager;
    auto doc = createDocument(10);
    manager.setDocument(doc.get());

    SECTION("No document returns -1") {
        VirtualScrollManager emptyManager;
        REQUIRE(emptyManager.paragraphAtY(50.0) == -1);
    }

    SECTION("Empty document returns -1") {
        KmlDocument emptyDoc;
        manager.setDocument(&emptyDoc);
        REQUIRE(manager.paragraphAtY(50.0) == -1);
    }

    SECTION("Negative Y returns 0") {
        REQUIRE(manager.paragraphAtY(-100.0) == 0);
    }

    SECTION("Y past end returns last paragraph") {
        REQUIRE(manager.paragraphAtY(10000.0) == 9);  // Last index
    }

    SECTION("Y at 0 returns 0") {
        REQUIRE(manager.paragraphAtY(0.0) == 0);
    }

    SECTION("Y within first paragraph returns 0") {
        // First paragraph is at Y=0, height=20
        REQUIRE(manager.paragraphAtY(10.0) == 0);
        REQUIRE(manager.paragraphAtY(19.0) == 0);
    }

    SECTION("Y at paragraph boundary returns next paragraph") {
        // First paragraph ends at Y=20, second starts there
        REQUIRE(manager.paragraphAtY(20.0) == 1);
    }

    SECTION("Y within middle paragraph") {
        // Paragraph 5 is at Y=100 (5*20), ends at Y=120
        REQUIRE(manager.paragraphAtY(100.0) == 5);
        REQUIRE(manager.paragraphAtY(110.0) == 5);
        REQUIRE(manager.paragraphAtY(119.0) == 5);
    }

    SECTION("Works with known heights") {
        // Set varying heights
        manager.updateParagraphHeight(0, 50.0);  // 0-50
        manager.updateParagraphHeight(1, 30.0);  // 50-80
        manager.updateParagraphHeight(2, 40.0);  // 80-120

        REQUIRE(manager.paragraphAtY(25.0) == 0);
        REQUIRE(manager.paragraphAtY(55.0) == 1);
        REQUIRE(manager.paragraphAtY(85.0) == 2);
    }
}

TEST_CASE("VirtualScrollManager ensureParagraphVisible", "[editor][virtual_scroll_manager][scrolling]") {
    VirtualScrollManager manager;
    auto doc = createDocument(50);
    manager.setDocument(doc.get());
    manager.setViewportHeight(100.0);

    // All paragraphs are 20px each by default

    SECTION("No document returns current offset") {
        VirtualScrollManager emptyManager;
        emptyManager.setViewportHeight(100.0);
        emptyManager.setViewportTop(50.0);
        REQUIRE(emptyManager.ensureParagraphVisible(5) == 50.0);
    }

    SECTION("Paragraph already visible - no change") {
        manager.setScrollOffset(0.0);
        // Paragraphs 0-4 are visible in viewport (0-100px, 5 paragraphs of 20px each)
        qreal result = manager.ensureParagraphVisible(2);
        REQUIRE(result == 0.0);  // No change
    }

    SECTION("Paragraph above viewport - scroll up") {
        manager.setScrollOffset(200.0);  // Viewing paragraphs 10-14
        // Request paragraph 5 (Y=100-120) which is above viewport
        qreal result = manager.ensureParagraphVisible(5);
        REQUIRE(result == 100.0);  // Should scroll to show paragraph 5 at top
    }

    SECTION("Paragraph below viewport - scroll down") {
        manager.setScrollOffset(0.0);  // Viewing paragraphs 0-4
        // Request paragraph 10 (Y=200-220) which is below viewport
        qreal result = manager.ensureParagraphVisible(10);
        // Should scroll so paragraph 10's bottom (220) aligns with viewport bottom
        // New offset = 220 - 100 = 120
        REQUIRE(result == 120.0);
    }

    SECTION("Large paragraph - shows top") {
        manager.updateParagraphHeight(5, 150.0);  // Larger than viewport
        manager.setScrollOffset(0.0);

        qreal result = manager.ensureParagraphVisible(5);
        // Should show top of paragraph 5, which is at Y=100 (5*20)
        REQUIRE(result == manager.paragraphY(5));
    }

    SECTION("Clamps negative index to 0") {
        manager.setScrollOffset(200.0);
        qreal result = manager.ensureParagraphVisible(-5);
        REQUIRE(result == 0.0);  // Should show paragraph 0
    }

    SECTION("Clamps out-of-bounds index") {
        manager.setScrollOffset(0.0);
        qreal result = manager.ensureParagraphVisible(1000);
        // Should scroll to last paragraph
        int lastIdx = doc->paragraphCount() - 1;
        REQUIRE(result >= 0.0);
        // After scrolling, last paragraph should be visible
        REQUIRE(manager.isParagraphVisible(lastIdx));
    }

    SECTION("First paragraph - scroll to 0") {
        manager.setScrollOffset(500.0);
        qreal result = manager.ensureParagraphVisible(0);
        REQUIRE(result == 0.0);
    }

    SECTION("Last paragraph - scrolls to show") {
        manager.setScrollOffset(0.0);
        int lastIdx = doc->paragraphCount() - 1;
        manager.ensureParagraphVisible(lastIdx);
        REQUIRE(manager.isParagraphVisible(lastIdx));
    }
}

TEST_CASE("VirtualScrollManager ensurePositionVisible", "[editor][virtual_scroll_manager][scrolling]") {
    VirtualScrollManager manager;
    auto doc = createDocument(50);
    manager.setDocument(doc.get());
    manager.setViewportHeight(100.0);

    SECTION("Uses paragraph from CursorPosition") {
        manager.setScrollOffset(200.0);
        CursorPosition pos{5, 10};  // Paragraph 5, offset 10

        qreal result = manager.ensurePositionVisible(pos);

        // Should be same as ensureParagraphVisible(5)
        manager.setScrollOffset(200.0);
        qreal expected = manager.ensureParagraphVisible(5);
        REQUIRE(result == expected);
    }

    SECTION("Works with paragraph 0") {
        manager.setScrollOffset(300.0);
        CursorPosition pos{0, 0};

        qreal result = manager.ensurePositionVisible(pos);
        REQUIRE(result == 0.0);
    }

    SECTION("Works with various cursor offsets") {
        manager.setScrollOffset(0.0);
        CursorPosition pos1{20, 0};
        CursorPosition pos2{20, 50};
        CursorPosition pos3{20, 100};

        // All should scroll to paragraph 20
        qreal result1 = manager.ensurePositionVisible(pos1);
        manager.setScrollOffset(0.0);
        qreal result2 = manager.ensurePositionVisible(pos2);
        manager.setScrollOffset(0.0);
        qreal result3 = manager.ensurePositionVisible(pos3);

        REQUIRE(result1 == result2);
        REQUIRE(result2 == result3);
    }
}

TEST_CASE("VirtualScrollManager scrolling integration", "[editor][virtual_scroll_manager][scrolling]") {
    VirtualScrollManager manager;
    auto doc = createDocument(100);
    manager.setDocument(doc.get());
    manager.setViewportHeight(200.0);

    SECTION("Scroll and find paragraph round-trip") {
        // Scroll to a known position
        manager.setScrollOffset(500.0);

        // Find which paragraph is at the top
        int topParagraph = manager.paragraphAtY(manager.scrollOffset());

        // That paragraph should be visible
        REQUIRE(manager.isParagraphVisible(topParagraph));
    }

    SECTION("Ensure visible updates scroll correctly") {
        // Start at top
        manager.setScrollOffset(0.0);

        // Navigate to paragraph 50
        manager.ensureParagraphVisible(50);

        // Paragraph 50 should now be visible
        REQUIRE(manager.isParagraphVisible(50));

        // Navigate back to paragraph 10
        manager.ensureParagraphVisible(10);

        // Paragraph 10 should now be visible
        REQUIRE(manager.isParagraphVisible(10));
    }

    SECTION("Scroll through document with known heights") {
        // Set all paragraphs to 25px
        for (int i = 0; i < 100; ++i) {
            manager.updateParagraphHeight(i, 25.0);
        }

        // Total height = 2500px
        REQUIRE(manager.totalHeight() == 2500.0);

        // Max scroll = 2500 - 200 = 2300
        REQUIRE(manager.maxScrollOffset() == 2300.0);

        // Scroll to middle
        manager.setScrollOffset(1000.0);

        // Paragraph at Y=1000 is paragraph 40 (1000/25 = 40)
        int para = manager.paragraphAtY(1000.0);
        REQUIRE(para == 40);
    }

    SECTION("Scroll clipping at document end") {
        // Scroll beyond end
        manager.setScrollOffset(100000.0);

        // Should be clamped
        REQUIRE(manager.scrollOffset() <= manager.maxScrollOffset());

        // Last paragraph should still be visible
        REQUIRE(manager.isParagraphVisible(doc->paragraphCount() - 1));
    }
}
