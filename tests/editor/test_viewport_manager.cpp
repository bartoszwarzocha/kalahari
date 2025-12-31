/// @file test_viewport_manager.cpp
/// @brief Unit tests for ViewportManager (OpenSpec #00043 Phase 11.8)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <kalahari/editor/viewport_manager.h>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <memory>
#include <algorithm>

using namespace kalahari::editor;

// =============================================================================
// Helper: Create document with test paragraphs
// =============================================================================

/// @brief Create a test document with fixed block heights
/// @note QTextDocument uses actual font metrics for height. In tests without
/// a QApplication/QGuiApplication, heights will use the estimated fallback (20.0).
static std::unique_ptr<QTextDocument> createTestDocument(size_t paragraphCount) {
    auto doc = std::make_unique<QTextDocument>();
    QStringList paragraphs;
    for (size_t i = 0; i < paragraphCount; ++i) {
        paragraphs << QString("Paragraph %1").arg(i + 1);
    }
    doc->setPlainText(paragraphs.join("\n"));
    return doc;
}

/// @brief Get actual block height from document (for test expectations)
/// @note Returns estimated height (20.0) when layout not available
static double getBlockHeight(QTextDocument* doc, int blockIndex) {
    if (!doc) return 20.0;

    QTextBlock block = doc->findBlockByNumber(blockIndex);
    if (!block.isValid()) return 20.0;

    // Try document layout
    if (auto* layout = doc->documentLayout()) {
        QRectF rect = layout->blockBoundingRect(block);
        if (rect.height() > 0) {
            return rect.height();
        }
    }

    // Fallback to estimated height
    return 20.0;
}

/// @brief Get total document height (for test expectations)
static double getTotalHeight(QTextDocument* doc) {
    if (!doc) return 0.0;

    if (auto* layout = doc->documentLayout()) {
        return layout->documentSize().height();
    }

    // Calculate from blocks
    double total = 0.0;
    QTextBlock block = doc->begin();
    while (block.isValid()) {
        total += getBlockHeight(doc, block.blockNumber());
        block = block.next();
    }
    return total;
}

// =============================================================================
// Constructor / Destructor Tests
// =============================================================================

TEST_CASE("ViewportManager - Construction", "[viewport_manager]") {
    SECTION("Default construction") {
        ViewportManager viewport;

        REQUIRE(viewport.document() == nullptr);
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

TEST_CASE("ViewportManager - Document Integration", "[viewport_manager]") {
    auto doc = createTestDocument(100);
    ViewportManager viewport;

    SECTION("Set document") {
        viewport.setDocument(doc.get());

        REQUIRE(viewport.document() == doc.get());
    }

    SECTION("Document total height") {
        viewport.setDocument(doc.get());

        // 100 paragraphs * estimated height each
        double expectedHeight = getTotalHeight(doc.get());
        REQUIRE(viewport.totalDocumentHeight() == Catch::Approx(expectedHeight).epsilon(0.01));
    }

    SECTION("Clear document") {
        viewport.setDocument(doc.get());
        viewport.setDocument(nullptr);

        REQUIRE(viewport.document() == nullptr);
        REQUIRE(viewport.totalDocumentHeight() == 0.0);
    }
}

// =============================================================================
// Viewport Configuration Tests
// =============================================================================

TEST_CASE("ViewportManager - Viewport Size", "[viewport_manager]") {
    auto doc = createTestDocument(100);
    ViewportManager viewport;
    viewport.setDocument(doc.get());

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
    auto doc = createTestDocument(100);
    ViewportManager viewport;
    viewport.setDocument(doc.get());
    viewport.setViewportSize(QSize(800, 600));

    double totalHeight = getTotalHeight(doc.get());
    double maxScroll = std::max(0.0, totalHeight - 600.0);

    SECTION("Initial scroll position") {
        REQUIRE(viewport.scrollPosition() == 0.0);
    }

    SECTION("Set scroll position") {
        double scrollPos = std::min(500.0, maxScroll);
        viewport.setScrollPosition(scrollPos);
        REQUIRE(viewport.scrollPosition() == Catch::Approx(scrollPos).epsilon(0.01));
    }

    SECTION("Scroll position clamped to zero") {
        viewport.setScrollPosition(-100.0);
        REQUIRE(viewport.scrollPosition() == 0.0);
    }

    SECTION("Scroll position clamped to max") {
        viewport.setScrollPosition(totalHeight * 2);  // Way beyond max
        REQUIRE(viewport.scrollPosition() == Catch::Approx(maxScroll).epsilon(0.01));
    }

    SECTION("Scroll by delta") {
        viewport.setScrollPosition(100.0);
        viewport.scrollBy(50.0);
        REQUIRE(viewport.scrollPosition() == Catch::Approx(150.0).epsilon(0.01));

        viewport.scrollBy(-75.0);
        REQUIRE(viewport.scrollPosition() == Catch::Approx(75.0).epsilon(0.01));
    }

    SECTION("Max scroll position") {
        REQUIRE(viewport.maxScrollPosition() == Catch::Approx(maxScroll).epsilon(0.01));
    }

    SECTION("Max scroll when content fits") {
        viewport.setViewportSize(QSize(800, static_cast<int>(totalHeight + 1000)));  // Larger than content
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

        double scrollPos = std::min(500.0, maxScroll);
        viewport.setScrollPosition(scrollPos);

        REQUIRE(count == 1);
        REQUIRE(lastValue == Catch::Approx(scrollPos).epsilon(0.01));
    }
}

TEST_CASE("ViewportManager - Scroll To Paragraph", "[viewport_manager]") {
    auto doc = createTestDocument(100);
    ViewportManager viewport;
    viewport.setDocument(doc.get());
    viewport.setViewportSize(QSize(800, 600));

    SECTION("Scroll to paragraph in viewport - no change") {
        // First visible paragraphs should be visible at scroll=0
        double newY = viewport.scrollToMakeParagraphVisible(5);
        REQUIRE(newY == 0.0);  // Already visible
    }

    SECTION("Scroll to paragraph below viewport") {
        // Scroll to a paragraph that's definitely below viewport
        double newY = viewport.scrollToMakeParagraphVisible(80);

        // Should have scrolled down
        REQUIRE(newY > 0.0);

        // The paragraph should now be visible
        REQUIRE(viewport.isParagraphVisible(80));
    }

    SECTION("Scroll to paragraph above viewport") {
        // First scroll far down
        viewport.setScrollPosition(viewport.maxScrollPosition());

        // Then scroll to an early paragraph
        double newY = viewport.scrollToMakeParagraphVisible(5);

        // Should be at or near the top of paragraph 5
        REQUIRE(newY == Catch::Approx(viewport.paragraphY(5)).epsilon(1.0));
    }
}

// =============================================================================
// Visible Range Tests
// =============================================================================

TEST_CASE("ViewportManager - Visible Range", "[viewport_manager]") {
    auto doc = createTestDocument(100);
    ViewportManager viewport;
    viewport.setDocument(doc.get());
    viewport.setViewportSize(QSize(800, 600));

    SECTION("Initial visible range") {
        auto [first, last] = viewport.visibleRange();

        REQUIRE(first == 0);
        // Should have multiple paragraphs visible
        REQUIRE(last > 0);
        REQUIRE(last < 100);
    }

    SECTION("Visible range after scroll") {
        double scrollPos = std::min(500.0, viewport.maxScrollPosition());
        viewport.setScrollPosition(scrollPos);

        auto [first, last] = viewport.visibleRange();

        // After scrolling, first visible should be greater than 0
        if (scrollPos > 0) {
            REQUIRE(first > 0);
        }
        REQUIRE(last >= first);
        REQUIRE(last < 100);
    }

    SECTION("Is paragraph visible") {
        REQUIRE(viewport.isParagraphVisible(0));
        // Some middle paragraph should be visible
        auto [first, last] = viewport.visibleRange();
        size_t middle = (first + last) / 2;
        REQUIRE(viewport.isParagraphVisible(middle));
        // A paragraph well beyond viewport should not be visible
        REQUIRE_FALSE(viewport.isParagraphVisible(99));
    }

    SECTION("Visible range change emits signal") {
        int count = 0;
        QObject::connect(&viewport, &ViewportManager::visibleRangeChanged,
                         [&count](size_t, size_t) { ++count; });

        double scrollPos = std::min(500.0, viewport.maxScrollPosition());
        if (scrollPos > 0) {
            viewport.setScrollPosition(scrollPos);
            REQUIRE(count >= 1);
        }
    }
}

// =============================================================================
// Buffer Range Tests
// =============================================================================

TEST_CASE("ViewportManager - Buffer Range", "[viewport_manager]") {
    auto doc = createTestDocument(100);
    ViewportManager viewport;
    viewport.setDocument(doc.get());
    viewport.setViewportSize(QSize(800, 600));
    viewport.setBufferSize(10);  // 10 paragraphs buffer

    SECTION("Buffer range at start") {
        // Buffer start should be 0 at scroll position 0
        REQUIRE(viewport.bufferStart() == 0);
        // Buffer end should be lastVisible + bufferSize
        auto [first, last] = viewport.visibleRange();
        REQUIRE(viewport.bufferEnd() == std::min(last + 10, static_cast<size_t>(99)));
    }

    SECTION("Buffer range in middle") {
        double scrollPos = std::min(500.0, viewport.maxScrollPosition());
        if (scrollPos > 0) {
            viewport.setScrollPosition(scrollPos);

            auto [first, last] = viewport.visibleRange();
            // Buffer start: first - 10 (or 0 if first < 10)
            size_t expectedStart = first > 10 ? first - 10 : 0;
            REQUIRE(viewport.bufferStart() == expectedStart);
            // Buffer end: last + 10 (clamped to 99)
            REQUIRE(viewport.bufferEnd() == std::min(last + 10, static_cast<size_t>(99)));
        }
    }

    SECTION("Buffer range at end") {
        viewport.setScrollPosition(viewport.maxScrollPosition());

        // Buffer end should be clamped to document end
        REQUIRE(viewport.bufferEnd() <= 99);
    }

    SECTION("Is paragraph in buffer") {
        auto bufStart = viewport.bufferStart();
        auto bufEnd = viewport.bufferEnd();

        REQUIRE(viewport.isParagraphInBuffer(bufStart));
        REQUIRE(viewport.isParagraphInBuffer(bufEnd));
        REQUIRE(viewport.isParagraphInBuffer((bufStart + bufEnd) / 2));
        // A paragraph well outside buffer should not be in buffer
        if (bufEnd < 99) {
            REQUIRE_FALSE(viewport.isParagraphInBuffer(99));
        }
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
    auto doc = createTestDocument(100);
    ViewportManager viewport;
    viewport.setDocument(doc.get());
    viewport.setViewportSize(QSize(800, 600));

    double totalHeight = getTotalHeight(doc.get());
    double maxScroll = std::max(0.0, totalHeight - 600.0);

    SECTION("Scrollbar position at start") {
        REQUIRE(viewport.scrollbarPosition() == 0.0);
    }

    SECTION("Scrollbar position at end") {
        viewport.setScrollPosition(maxScroll);
        REQUIRE(viewport.scrollbarPosition() == Catch::Approx(1.0).epsilon(0.01));
    }

    SECTION("Scrollbar position in middle") {
        double halfMax = maxScroll / 2.0;
        viewport.setScrollPosition(halfMax);
        REQUIRE(viewport.scrollbarPosition() == Catch::Approx(0.5).epsilon(0.01));
    }

    SECTION("Scrollbar thumb size") {
        // Thumb size = viewport / total
        double expectedThumb = 600.0 / totalHeight;
        expectedThumb = std::min(1.0, std::max(0.05, expectedThumb));
        REQUIRE(viewport.scrollbarThumbSize() == Catch::Approx(expectedThumb).epsilon(0.05));
    }

    SECTION("Scrollbar thumb size minimum") {
        // Create large document (1000 paragraphs)
        auto largeDoc = createTestDocument(1000);
        viewport.setDocument(largeDoc.get());

        // Thumb size should be at least 5%
        REQUIRE(viewport.scrollbarThumbSize() >= 0.05);

        // Reset
        viewport.setDocument(nullptr);
    }

    SECTION("Set scrollbar position") {
        viewport.setScrollbarPosition(0.5);

        // Position 0.5 * max
        REQUIRE(viewport.scrollPosition() == Catch::Approx(maxScroll * 0.5).epsilon(1.0));
    }

    SECTION("Is scrollbar needed") {
        if (totalHeight > 600) {
            REQUIRE(viewport.isScrollbarNeeded());
        }
    }

    SECTION("Scrollbar not needed for small content") {
        auto smallDoc = createTestDocument(5);  // Very few paragraphs
        viewport.setDocument(smallDoc.get());

        double smallHeight = getTotalHeight(smallDoc.get());
        if (smallHeight <= 600) {
            REQUIRE_FALSE(viewport.isScrollbarNeeded());
        }

        viewport.setDocument(nullptr);
    }
}

// =============================================================================
// Geometry Query Tests
// =============================================================================

TEST_CASE("ViewportManager - Geometry Queries", "[viewport_manager]") {
    auto doc = createTestDocument(100);
    ViewportManager viewport;
    viewport.setDocument(doc.get());
    viewport.setViewportSize(QSize(800, 600));

    SECTION("Viewport rect") {
        double scrollPos = std::min(500.0, viewport.maxScrollPosition());
        viewport.setScrollPosition(scrollPos);

        QRectF rect = viewport.viewportRect();

        REQUIRE(rect.x() == 0.0);
        REQUIRE(rect.y() == Catch::Approx(scrollPos).epsilon(0.01));
        REQUIRE(rect.width() == 800.0);
        REQUIRE(rect.height() == 600.0);
    }

    SECTION("Paragraph at Y") {
        // Paragraph 0 should be at Y=0
        REQUIRE(viewport.paragraphAtY(0.0) == 0);
        // Paragraph at some Y position should be consistent with paragraphY
        size_t para10 = 10;
        double y10 = viewport.paragraphY(para10);
        REQUIRE(viewport.paragraphAtY(y10 + 1.0) == para10);
    }

    SECTION("Paragraph Y position") {
        // First paragraph at or near Y=0 (may have small margin from layout)
        REQUIRE(viewport.paragraphY(0) >= 0.0);
        REQUIRE(viewport.paragraphY(0) < 10.0);  // Should be at most a small margin
        // Subsequent paragraphs at increasing Y
        REQUIRE(viewport.paragraphY(1) > viewport.paragraphY(0));
        REQUIRE(viewport.paragraphY(10) > viewport.paragraphY(5));
    }

    SECTION("Paragraph height") {
        // Heights should be positive
        REQUIRE(viewport.paragraphHeight(0) > 0.0);
        REQUIRE(viewport.paragraphHeight(50) > 0.0);
    }
}

// =============================================================================
// Document Change Signal Tests
// =============================================================================

TEST_CASE("ViewportManager - Document Change Signals", "[viewport_manager]") {
    auto doc = createTestDocument(100);
    ViewportManager viewport;
    viewport.setDocument(doc.get());
    viewport.setViewportSize(QSize(800, 600));

    SECTION("Document content change updates height") {
        int count = 0;
        QObject::connect(&viewport, &ViewportManager::documentHeightChanged,
                         [&count](double) { ++count; });

        // Modify document content
        QTextCursor cursor(doc.get());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("\nNew paragraph");

        // Height change signal should have been emitted
        REQUIRE(count >= 1);
    }
}

// =============================================================================
// Layout Requested Signal Tests
// =============================================================================

TEST_CASE("ViewportManager - Layout Requested Signal", "[viewport_manager]") {
    auto doc = createTestDocument(100);
    ViewportManager viewport;
    viewport.setDocument(doc.get());
    viewport.setViewportSize(QSize(800, 600));

    SECTION("Layout requested signal on scroll") {
        int count = 0;
        QObject::connect(&viewport, &ViewportManager::layoutRequested,
                         [&count](size_t, size_t) { ++count; });

        double scrollPos = std::min(500.0, viewport.maxScrollPosition());
        if (scrollPos > 0) {
            viewport.setScrollPosition(scrollPos);
            REQUIRE(count >= 1);
        }
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("ViewportManager - Edge Cases", "[viewport_manager]") {
    SECTION("Operations with no document") {
        ViewportManager viewport;
        viewport.setViewportSize(QSize(800, 600));

        REQUIRE(viewport.totalDocumentHeight() == 0.0);
        REQUIRE(viewport.maxScrollPosition() == 0.0);
        REQUIRE(viewport.paragraphAtY(100.0) == 0);
        REQUIRE(viewport.paragraphY(10) == 0.0);
        REQUIRE(viewport.paragraphHeight(10) == 0.0);
    }

    SECTION("Empty document") {
        auto doc = std::make_unique<QTextDocument>();
        ViewportManager viewport;
        viewport.setDocument(doc.get());
        viewport.setViewportSize(QSize(800, 600));

        // QTextDocument always has at least one empty block
        REQUIRE(viewport.totalDocumentHeight() > 0.0);
        REQUIRE(viewport.firstVisibleParagraph() == 0);
        REQUIRE(viewport.lastVisibleParagraph() == 0);
    }

    SECTION("Single paragraph") {
        auto doc = std::make_unique<QTextDocument>();
        doc->setPlainText("Single paragraph");

        ViewportManager viewport;
        viewport.setDocument(doc.get());
        viewport.setViewportSize(QSize(800, 600));

        REQUIRE(viewport.totalDocumentHeight() > 0.0);
        REQUIRE(viewport.firstVisibleParagraph() == 0);
        REQUIRE(viewport.lastVisibleParagraph() == 0);
        // Single paragraph should fit in 600px viewport
        REQUIRE_FALSE(viewport.isScrollbarNeeded());
    }

    SECTION("Scroll to invalid paragraph") {
        auto doc = createTestDocument(10);
        ViewportManager viewport;
        viewport.setDocument(doc.get());
        viewport.setViewportSize(QSize(800, 600));

        // Invalid paragraph index (beyond document)
        double result = viewport.scrollToMakeParagraphVisible(100);
        REQUIRE(result == viewport.scrollPosition());  // No change
    }
}
