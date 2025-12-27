/// @file test_viewport_manager.cpp
/// @brief Unit tests for ViewportManager (OpenSpec #00043 Phase 4)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <kalahari/editor/viewport_manager.h>
#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/lazy_layout_manager.h>

using namespace kalahari::editor;

// =============================================================================
// Helper: Create buffer with test paragraphs
// =============================================================================

static TextBuffer createTestBuffer(size_t paragraphCount, double height = 20.0) {
    TextBuffer buffer;
    QString text;
    for (size_t i = 0; i < paragraphCount; ++i) {
        if (i > 0) text += "\n";
        text += QString("Paragraph %1").arg(i + 1);
    }
    buffer.setPlainText(text);

    // Set consistent heights for testing
    for (size_t i = 0; i < paragraphCount; ++i) {
        buffer.setParagraphHeight(i, height);
    }

    return buffer;
}

// =============================================================================
// Constructor / Destructor Tests
// =============================================================================

TEST_CASE("ViewportManager - Construction", "[viewport_manager]") {
    SECTION("Default construction") {
        ViewportManager viewport;

        REQUIRE(viewport.buffer() == nullptr);
        REQUIRE(viewport.layoutManager() == nullptr);
        REQUIRE(viewport.viewportSize() == QSize(0, 0));
        REQUIRE(viewport.scrollPosition() == 0.0);
        REQUIRE(viewport.firstVisibleParagraph() == 0);
        REQUIRE(viewport.lastVisibleParagraph() == 0);
    }

    SECTION("Construction with parent") {
        QObject parent;
        ViewportManager viewport(&parent);

        REQUIRE(viewport.parent() == &parent);
    }
}

// =============================================================================
// Component Integration Tests
// =============================================================================

TEST_CASE("ViewportManager - Buffer Integration", "[viewport_manager]") {
    TextBuffer buffer = createTestBuffer(100);
    ViewportManager viewport;

    SECTION("Set buffer") {
        viewport.setBuffer(&buffer);

        REQUIRE(viewport.buffer() == &buffer);
    }

    SECTION("Buffer total height") {
        viewport.setBuffer(&buffer);

        // 100 paragraphs * 20px each = 2000px
        REQUIRE(viewport.totalDocumentHeight() == 2000.0);
    }

    SECTION("Clear buffer") {
        viewport.setBuffer(&buffer);
        viewport.setBuffer(nullptr);

        REQUIRE(viewport.buffer() == nullptr);
        REQUIRE(viewport.totalDocumentHeight() == 0.0);
    }
}

TEST_CASE("ViewportManager - Layout Manager Integration", "[viewport_manager]") {
    TextBuffer buffer = createTestBuffer(100);
    LazyLayoutManager layoutManager(&buffer);
    ViewportManager viewport;

    viewport.setBuffer(&buffer);
    viewport.setLayoutManager(&layoutManager);

    SECTION("Set layout manager") {
        REQUIRE(viewport.layoutManager() == &layoutManager);
    }
}

// =============================================================================
// Viewport Configuration Tests
// =============================================================================

TEST_CASE("ViewportManager - Viewport Size", "[viewport_manager]") {
    TextBuffer buffer = createTestBuffer(100);
    ViewportManager viewport;
    viewport.setBuffer(&buffer);

    SECTION("Set viewport size") {
        viewport.setViewportSize(QSize(800, 600));

        REQUIRE(viewport.viewportSize() == QSize(800, 600));
        REQUIRE(viewport.viewportWidth() == 800);
        REQUIRE(viewport.viewportHeight() == 600);
    }

    SECTION("Viewport size change emits signal") {
        int count = 0;
        QObject::connect(&viewport, &ViewportManager::viewportChanged, [&count]() {
            ++count;
        });

        viewport.setViewportSize(QSize(800, 600));

        REQUIRE(count == 1);
    }

    SECTION("Same size doesn't emit signal") {
        viewport.setViewportSize(QSize(800, 600));

        int count = 0;
        QObject::connect(&viewport, &ViewportManager::viewportChanged, [&count]() {
            ++count;
        });

        viewport.setViewportSize(QSize(800, 600));

        REQUIRE(count == 0);
    }
}

TEST_CASE("ViewportManager - Buffer Size Configuration", "[viewport_manager]") {
    ViewportManager viewport;

    SECTION("Default buffer size") {
        REQUIRE(viewport.bufferSize() == DEFAULT_BUFFER_SIZE);
    }

    SECTION("Set buffer size") {
        viewport.setBufferSize(100);
        REQUIRE(viewport.bufferSize() == 100);
    }
}

// =============================================================================
// Scroll Position Tests
// =============================================================================

TEST_CASE("ViewportManager - Scroll Position", "[viewport_manager]") {
    TextBuffer buffer = createTestBuffer(100);
    ViewportManager viewport;
    viewport.setBuffer(&buffer);
    viewport.setViewportSize(QSize(800, 600));

    SECTION("Initial scroll position") {
        REQUIRE(viewport.scrollPosition() == 0.0);
    }

    SECTION("Set scroll position") {
        viewport.setScrollPosition(500.0);
        REQUIRE(viewport.scrollPosition() == 500.0);
    }

    SECTION("Scroll position clamped to zero") {
        viewport.setScrollPosition(-100.0);
        REQUIRE(viewport.scrollPosition() == 0.0);
    }

    SECTION("Scroll position clamped to max") {
        // Total height = 2000, viewport = 600, max scroll = 1400
        viewport.setScrollPosition(5000.0);
        REQUIRE(viewport.scrollPosition() == 1400.0);
    }

    SECTION("Scroll by delta") {
        viewport.setScrollPosition(100.0);
        viewport.scrollBy(50.0);
        REQUIRE(viewport.scrollPosition() == 150.0);

        viewport.scrollBy(-75.0);
        REQUIRE(viewport.scrollPosition() == 75.0);
    }

    SECTION("Max scroll position") {
        // Total = 2000, viewport = 600
        REQUIRE(viewport.maxScrollPosition() == 1400.0);
    }

    SECTION("Max scroll when content fits") {
        viewport.setViewportSize(QSize(800, 3000));  // Larger than content
        REQUIRE(viewport.maxScrollPosition() == 0.0);
    }

    SECTION("Scroll position emits signal") {
        int count = 0;
        double lastValue = 0.0;
        QObject::connect(&viewport, &ViewportManager::scrollPositionChanged,
                         [&count, &lastValue](double pos) {
            ++count;
            lastValue = pos;
        });

        viewport.setScrollPosition(500.0);

        REQUIRE(count == 1);
        REQUIRE(lastValue == 500.0);
    }
}

TEST_CASE("ViewportManager - Scroll To Paragraph", "[viewport_manager]") {
    TextBuffer buffer = createTestBuffer(100);  // 100 * 20 = 2000px
    ViewportManager viewport;
    viewport.setBuffer(&buffer);
    viewport.setViewportSize(QSize(800, 600));

    SECTION("Scroll to paragraph in viewport - no change") {
        // First visible paragraphs (0-30) should be visible at scroll=0
        double newY = viewport.scrollToMakeParagraphVisible(10);
        REQUIRE(newY == 0.0);  // Already visible
    }

    SECTION("Scroll to paragraph below viewport") {
        // Paragraph 50 is at Y=1000, need to scroll down
        double newY = viewport.scrollToMakeParagraphVisible(50);

        // Should scroll so paragraph 50 is at bottom of viewport
        // Para Y = 1000, height = 20, viewport = 600
        // newScrollY = 1000 + 20 - 600 = 420
        REQUIRE(newY == 420.0);
    }

    SECTION("Scroll to paragraph above viewport") {
        viewport.setScrollPosition(1000.0);

        // Paragraph 10 is at Y=200
        double newY = viewport.scrollToMakeParagraphVisible(10);
        REQUIRE(newY == 200.0);  // Scroll to paragraph top
    }
}

// =============================================================================
// Visible Range Tests
// =============================================================================

TEST_CASE("ViewportManager - Visible Range", "[viewport_manager]") {
    TextBuffer buffer = createTestBuffer(100);  // 100 * 20px = 2000px
    ViewportManager viewport;
    viewport.setBuffer(&buffer);
    viewport.setViewportSize(QSize(800, 600));  // Shows ~30 paragraphs

    SECTION("Initial visible range") {
        auto [first, last] = viewport.visibleRange();

        REQUIRE(first == 0);
        // Viewport 600px, paragraphs 20px each
        // Paragraph 30 starts at Y=600 (on viewport bottom edge)
        // Implementation includes paragraph at bottom edge, so last=30
        REQUIRE(last == 30);
    }

    SECTION("Visible range after scroll") {
        viewport.setScrollPosition(500.0);  // Y=500 is paragraph 25

        auto [first, last] = viewport.visibleRange();

        REQUIRE(first == 25);
        // 500 + 600 = 1100, paragraph at 1100 is 55 (55*20=1100)
        // Paragraph 55 starts exactly at bottom edge, so included
        REQUIRE(last == 55);
    }

    SECTION("Is paragraph visible") {
        REQUIRE(viewport.isParagraphVisible(0));
        REQUIRE(viewport.isParagraphVisible(15));
        REQUIRE(viewport.isParagraphVisible(30));  // At bottom edge - included
        REQUIRE_FALSE(viewport.isParagraphVisible(31));
        REQUIRE_FALSE(viewport.isParagraphVisible(50));
    }

    SECTION("Visible range change emits signal") {
        int count = 0;
        QObject::connect(&viewport, &ViewportManager::visibleRangeChanged,
                         [&count](size_t, size_t) { ++count; });

        viewport.setScrollPosition(500.0);

        REQUIRE(count >= 1);
    }
}

// =============================================================================
// Buffer Range Tests
// =============================================================================

TEST_CASE("ViewportManager - Buffer Range", "[viewport_manager]") {
    TextBuffer buffer = createTestBuffer(100);
    ViewportManager viewport;
    viewport.setBuffer(&buffer);
    viewport.setViewportSize(QSize(800, 600));
    viewport.setBufferSize(10);  // 10 paragraphs buffer

    SECTION("Buffer range at start") {
        // Visible: 0-30 (see "Initial visible range" test), buffer size: 10
        // Buffer start: max(0, 0-10) = 0
        // Buffer end: min(99, 30+10) = 40
        REQUIRE(viewport.bufferStart() == 0);
        REQUIRE(viewport.bufferEnd() == 40);
    }

    SECTION("Buffer range in middle") {
        viewport.setScrollPosition(500.0);  // Visible: 25-55

        // Buffer start: 25-10 = 15
        // Buffer end: 55+10 = 65
        REQUIRE(viewport.bufferStart() == 15);
        REQUIRE(viewport.bufferEnd() == 65);
    }

    SECTION("Buffer range at end") {
        viewport.setScrollPosition(1400.0);  // Scroll to max

        // Visible range at end
        auto [first, last] = viewport.visibleRange();

        // Buffer end should be clamped to document end
        REQUIRE(viewport.bufferEnd() <= 99);
    }

    SECTION("Is paragraph in buffer") {
        // Buffer: 0-40
        REQUIRE(viewport.isParagraphInBuffer(0));
        REQUIRE(viewport.isParagraphInBuffer(35));
        REQUIRE(viewport.isParagraphInBuffer(40));
        REQUIRE_FALSE(viewport.isParagraphInBuffer(50));
    }

    SECTION("Buffered range") {
        auto [start, end] = viewport.bufferedRange();
        REQUIRE(start == viewport.bufferStart());
        REQUIRE(end == viewport.bufferEnd());
    }
}

// =============================================================================
// Scrollbar Tests
// =============================================================================

TEST_CASE("ViewportManager - Scrollbar", "[viewport_manager]") {
    TextBuffer buffer = createTestBuffer(100);  // 2000px
    ViewportManager viewport;
    viewport.setBuffer(&buffer);
    viewport.setViewportSize(QSize(800, 600));

    SECTION("Scrollbar position at start") {
        REQUIRE(viewport.scrollbarPosition() == 0.0);
    }

    SECTION("Scrollbar position at end") {
        viewport.setScrollPosition(1400.0);
        REQUIRE(viewport.scrollbarPosition() == 1.0);
    }

    SECTION("Scrollbar position in middle") {
        viewport.setScrollPosition(700.0);  // Half of max (1400)
        REQUIRE(viewport.scrollbarPosition() == 0.5);
    }

    SECTION("Scrollbar thumb size") {
        // Viewport = 600, Total = 2000
        // Thumb size = 600 / 2000 = 0.3
        REQUIRE(viewport.scrollbarThumbSize() == Catch::Approx(0.3).epsilon(0.01));
    }

    SECTION("Scrollbar thumb size minimum") {
        // Create large document (200 paragraphs * 100px = 20000px)
        // Thumb would be 600/20000 = 0.03, but minimum is 0.05
        TextBuffer largeBuffer = createTestBuffer(200, 100.0);
        viewport.setBuffer(&largeBuffer);

        // Thumb size should be at least 5%
        REQUIRE(viewport.scrollbarThumbSize() >= 0.05);

        // Reset to avoid dangling pointer when largeBuffer is destroyed
        viewport.setBuffer(nullptr);
    }

    SECTION("Set scrollbar position") {
        viewport.setScrollbarPosition(0.5);

        // Position 0.5 * max(1400) = 700
        REQUIRE(viewport.scrollPosition() == 700.0);
    }

    SECTION("Is scrollbar needed") {
        REQUIRE(viewport.isScrollbarNeeded());
    }

    SECTION("Scrollbar not needed for small content") {
        TextBuffer smallBuffer = createTestBuffer(10);  // 200px
        viewport.setBuffer(&smallBuffer);

        REQUIRE_FALSE(viewport.isScrollbarNeeded());

        // Reset to avoid dangling pointer when smallBuffer is destroyed
        viewport.setBuffer(nullptr);
    }
}

// =============================================================================
// Geometry Query Tests
// =============================================================================

TEST_CASE("ViewportManager - Geometry Queries", "[viewport_manager]") {
    TextBuffer buffer = createTestBuffer(100);
    ViewportManager viewport;
    viewport.setBuffer(&buffer);
    viewport.setViewportSize(QSize(800, 600));

    SECTION("Viewport rect") {
        viewport.setScrollPosition(500.0);

        QRectF rect = viewport.viewportRect();

        REQUIRE(rect.x() == 0.0);
        REQUIRE(rect.y() == 500.0);
        REQUIRE(rect.width() == 800.0);
        REQUIRE(rect.height() == 600.0);
    }

    SECTION("Paragraph at Y") {
        REQUIRE(viewport.paragraphAtY(0.0) == 0);
        REQUIRE(viewport.paragraphAtY(15.0) == 0);
        REQUIRE(viewport.paragraphAtY(20.0) == 1);
        REQUIRE(viewport.paragraphAtY(500.0) == 25);
    }

    SECTION("Paragraph Y position") {
        REQUIRE(viewport.paragraphY(0) == 0.0);
        REQUIRE(viewport.paragraphY(1) == 20.0);
        REQUIRE(viewport.paragraphY(25) == 500.0);
    }

    SECTION("Paragraph height") {
        REQUIRE(viewport.paragraphHeight(0) == 20.0);
        REQUIRE(viewport.paragraphHeight(50) == 20.0);
    }
}

// =============================================================================
// Observer Callback Tests
// =============================================================================

TEST_CASE("ViewportManager - Observer Callbacks", "[viewport_manager]") {
    TextBuffer buffer = createTestBuffer(100);
    ViewportManager viewport;
    viewport.setBuffer(&buffer);
    viewport.setViewportSize(QSize(800, 600));

    SECTION("Text changed updates height") {
        int count = 0;
        QObject::connect(&viewport, &ViewportManager::documentHeightChanged,
                         [&count](double) { ++count; });

        // Simulate text change by inserting text
        buffer.insert(50, "New paragraph\n");

        // Height should update
        REQUIRE(count >= 1);
    }

    SECTION("Paragraph height change") {
        int count = 0;
        QObject::connect(&viewport, &ViewportManager::documentHeightChanged,
                         [&count](double) { ++count; });

        buffer.setParagraphHeight(10, 40.0);  // Double height

        REQUIRE(count == 1);
    }
}

// =============================================================================
// Layout Coordination Tests
// =============================================================================

TEST_CASE("ViewportManager - Layout Coordination", "[viewport_manager]") {
    TextBuffer buffer = createTestBuffer(100);
    LazyLayoutManager layoutManager(&buffer);
    ViewportManager viewport;
    viewport.setBuffer(&buffer);
    viewport.setLayoutManager(&layoutManager);
    viewport.setViewportSize(QSize(800, 600));

    SECTION("Request layout") {
        // Should not crash
        viewport.requestLayout();
    }

    SECTION("Sync layout manager viewport") {
        viewport.setScrollPosition(500.0);
        viewport.syncLayoutManagerViewport();

        // Layout manager should have updated viewport
        // (We can't easily verify internal state, but should not crash)
    }

    SECTION("Layout requested signal") {
        int count = 0;
        QObject::connect(&viewport, &ViewportManager::layoutRequested,
                         [&count](size_t, size_t) { ++count; });

        viewport.setScrollPosition(500.0);

        REQUIRE(count >= 1);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("ViewportManager - Edge Cases", "[viewport_manager]") {
    SECTION("Operations with no buffer") {
        ViewportManager viewport;
        viewport.setViewportSize(QSize(800, 600));

        REQUIRE(viewport.totalDocumentHeight() == 0.0);
        REQUIRE(viewport.maxScrollPosition() == 0.0);
        REQUIRE(viewport.paragraphAtY(100.0) == 0);
        REQUIRE(viewport.paragraphY(10) == 0.0);
        REQUIRE(viewport.paragraphHeight(10) == 0.0);
    }

    SECTION("Empty buffer") {
        TextBuffer buffer;
        ViewportManager viewport;
        viewport.setBuffer(&buffer);
        viewport.setViewportSize(QSize(800, 600));

        REQUIRE(viewport.totalDocumentHeight() == 0.0);
        REQUIRE(viewport.firstVisibleParagraph() == 0);
        REQUIRE(viewport.lastVisibleParagraph() == 0);
    }

    SECTION("Single paragraph") {
        TextBuffer buffer;
        buffer.setPlainText("Single paragraph");
        buffer.setParagraphHeight(0, 30.0);

        ViewportManager viewport;
        viewport.setBuffer(&buffer);
        viewport.setViewportSize(QSize(800, 600));

        REQUIRE(viewport.totalDocumentHeight() == 30.0);
        REQUIRE(viewport.firstVisibleParagraph() == 0);
        REQUIRE(viewport.lastVisibleParagraph() == 0);
        REQUIRE_FALSE(viewport.isScrollbarNeeded());
    }

    SECTION("Scroll to invalid paragraph") {
        TextBuffer buffer = createTestBuffer(10);
        ViewportManager viewport;
        viewport.setBuffer(&buffer);
        viewport.setViewportSize(QSize(800, 600));

        // Invalid paragraph index
        double result = viewport.scrollToMakeParagraphVisible(100);
        REQUIRE(result == viewport.scrollPosition());  // No change
    }
}
