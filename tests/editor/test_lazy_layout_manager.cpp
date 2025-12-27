/// @file test_lazy_layout_manager.cpp
/// @brief Unit tests for LazyLayoutManager (OpenSpec #00043 Phase 2)

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/editor/lazy_layout_manager.h>
#include <kalahari/editor/text_buffer.h>
#include <QFont>

using namespace kalahari::editor;
using Catch::Matchers::WithinAbs;

// QApplication is created in test_main.cpp

TEST_CASE("LazyLayoutManager - Construction", "[lazy_layout_manager]") {
    TextBuffer buffer;

    SECTION("Construct with buffer") {
        LazyLayoutManager manager(&buffer);
        REQUIRE(manager.buffer() == &buffer);
        REQUIRE(manager.layoutCount() == 0);
        REQUIRE(manager.width() == 0.0);
    }

    SECTION("Construct with nullptr") {
        LazyLayoutManager manager(nullptr);
        REQUIRE(manager.buffer() == nullptr);
        REQUIRE(manager.layoutCount() == 0);
    }
}

TEST_CASE("LazyLayoutManager - Configuration", "[lazy_layout_manager]") {
    TextBuffer buffer;
    buffer.setPlainText("Test paragraph.\nSecond paragraph.");
    LazyLayoutManager manager(&buffer);

    SECTION("Set width") {
        manager.setWidth(800.0);
        REQUIRE(manager.width() == 800.0);
    }

    SECTION("Set font") {
        QFont font("Arial", 14);
        manager.setFont(font);
        REQUIRE(manager.font().family() == font.family());
        REQUIRE(manager.font().pointSize() == font.pointSize());
    }

    SECTION("Width change invalidates layouts") {
        manager.setWidth(400.0);
        manager.layoutParagraph(0);
        REQUIRE(manager.hasLayout(0));

        manager.setWidth(600.0);
        // Layout should be marked dirty
        REQUIRE(manager.layoutCount() > 0);
    }
}

TEST_CASE("LazyLayoutManager - Viewport", "[lazy_layout_manager]") {
    TextBuffer buffer;
    // Create multiple paragraphs
    QString text;
    for (int i = 0; i < 100; ++i) {
        text += QString("Paragraph %1 with some content.\n").arg(i);
    }
    text.chop(1);  // Remove trailing newline
    buffer.setPlainText(text);

    LazyLayoutManager manager(&buffer);
    manager.setWidth(400.0);
    manager.setFont(QFont("Arial", 12));

    SECTION("Set viewport") {
        // Set viewport at top
        manager.setViewport(0, 200);
        REQUIRE(manager.firstVisibleParagraph() == 0);
        REQUIRE(manager.lastVisibleParagraph() >= 0);
    }

    SECTION("Buffer zone") {
        manager.setViewport(0, 200);
        REQUIRE(manager.bufferStart() == 0);
        REQUIRE(manager.bufferEnd() >= manager.lastVisibleParagraph());
    }
}

TEST_CASE("LazyLayoutManager - Layout Operations", "[lazy_layout_manager]") {
    TextBuffer buffer;
    buffer.setPlainText("First paragraph with text.\nSecond paragraph.\nThird paragraph.");

    LazyLayoutManager manager(&buffer);
    manager.setWidth(400.0);
    manager.setFont(QFont("Arial", 12));

    SECTION("Layout single paragraph") {
        double height = manager.layoutParagraph(0);
        REQUIRE(height > 0.0);
        REQUIRE(manager.hasLayout(0));
    }

    SECTION("Get layout returns nullptr before layout") {
        REQUIRE(manager.getLayout(0) == nullptr);
    }

    SECTION("Get layout returns pointer after layout") {
        manager.layoutParagraph(0);
        QTextLayout* layout = manager.getLayout(0);
        REQUIRE(layout != nullptr);
        REQUIRE(layout->text() == buffer.paragraphText(0));
    }

    SECTION("Layout visible paragraphs") {
        manager.setViewport(0, 100);
        double totalHeight = manager.layoutVisibleParagraphs();
        REQUIRE(totalHeight > 0.0);
        REQUIRE(manager.layoutCount() > 0);
    }

    SECTION("Layout count increases") {
        REQUIRE(manager.layoutCount() == 0);
        manager.layoutParagraph(0);
        REQUIRE(manager.layoutCount() == 1);
        manager.layoutParagraph(1);
        REQUIRE(manager.layoutCount() == 2);
    }
}

TEST_CASE("LazyLayoutManager - Height Integration", "[lazy_layout_manager]") {
    TextBuffer buffer;
    buffer.setPlainText("Paragraph one.\nParagraph two.\nParagraph three.");

    LazyLayoutManager manager(&buffer);
    manager.setWidth(400.0);
    manager.setFont(QFont("Arial", 12));

    SECTION("Height updates TextBuffer") {
        // Before layout - estimated height
        HeightState stateBefore = buffer.getHeightState(0);
        REQUIRE(stateBefore == HeightState::Estimated);

        // After layout - calculated height
        manager.layoutParagraph(0);
        HeightState stateAfter = buffer.getHeightState(0);
        REQUIRE(stateAfter == HeightState::Calculated);
    }

    SECTION("Paragraph Y position") {
        manager.layoutParagraph(0);
        manager.layoutParagraph(1);

        double y0 = manager.paragraphY(0);
        double y1 = manager.paragraphY(1);
        double h0 = manager.paragraphHeight(0);

        REQUIRE(y0 == 0.0);
        REQUIRE_THAT(y1, WithinAbs(h0, 0.1));
    }

    SECTION("Find paragraph at Y") {
        manager.setViewport(0, 500);
        manager.layoutVisibleParagraphs();

        size_t idx = manager.findParagraphAtY(0);
        REQUIRE(idx == 0);
    }

    SECTION("Total height") {
        double height = manager.totalHeight();
        REQUIRE(height > 0.0);
    }
}

TEST_CASE("LazyLayoutManager - Cache Management", "[lazy_layout_manager]") {
    TextBuffer buffer;
    buffer.setPlainText("Test paragraph.\nSecond paragraph.");
    LazyLayoutManager manager(&buffer);
    manager.setWidth(400.0);

    SECTION("Invalidate layout") {
        manager.layoutParagraph(0);
        REQUIRE(manager.hasLayout(0));

        manager.invalidateLayout(0);
        // Layout still exists but is dirty
        REQUIRE(manager.layoutCount() == 1);
    }

    SECTION("Clear layouts") {
        manager.layoutParagraph(0);
        manager.layoutParagraph(1);
        REQUIRE(manager.layoutCount() == 2);

        manager.clearLayouts();
        REQUIRE(manager.layoutCount() == 0);
    }

    SECTION("Constants") {
        REQUIRE(LazyLayoutManager::maxCachedLayouts() == LAZY_MAX_CACHED_LAYOUTS);
        REQUIRE(LazyLayoutManager::bufferSize() == LAZY_BUFFER_SIZE);
    }
}

TEST_CASE("LazyLayoutManager - Observer Pattern", "[lazy_layout_manager]") {
    TextBuffer buffer;
    buffer.setPlainText("First paragraph.\nSecond paragraph.");

    LazyLayoutManager manager(&buffer);
    manager.setWidth(400.0);
    manager.layoutParagraph(0);
    manager.layoutParagraph(1);

    SECTION("Paragraph inserted shifts layouts") {
        REQUIRE(manager.layoutCount() == 2);

        // Insert paragraph at beginning
        buffer.insertParagraph(0, "New first paragraph.");

        // Layout indices should shift
        REQUIRE(manager.layoutCount() == 2);  // Still 2 layouts
    }

    SECTION("Paragraph removed clears layout") {
        REQUIRE(manager.layoutCount() == 2);

        buffer.removeParagraph(0);

        // First layout removed, second shifted
        REQUIRE(manager.layoutCount() == 1);
    }

    SECTION("Text changed invalidates all") {
        REQUIRE(manager.layoutCount() == 2);
        REQUIRE(manager.hasLayout(0));
        REQUIRE(manager.hasLayout(1));

        buffer.setPlainText("Completely new text.\nNew paragraph.");

        // All layouts invalidated (may still exist but dirty)
        // hasLayout returns false for dirty layouts
        REQUIRE_FALSE(manager.hasLayout(0));
        REQUIRE_FALSE(manager.hasLayout(1));
    }
}

TEST_CASE("LazyLayoutManager - Move Semantics", "[lazy_layout_manager]") {
    TextBuffer buffer;
    buffer.setPlainText("Test paragraph.");

    SECTION("Move constructor") {
        LazyLayoutManager manager1(&buffer);
        manager1.setWidth(400.0);
        manager1.layoutParagraph(0);

        LazyLayoutManager manager2(std::move(manager1));

        REQUIRE(manager2.buffer() == &buffer);
        REQUIRE(manager2.width() == 400.0);
        REQUIRE(manager2.layoutCount() == 1);
    }

    SECTION("Move assignment") {
        LazyLayoutManager manager1(&buffer);
        manager1.setWidth(400.0);
        manager1.layoutParagraph(0);

        TextBuffer buffer2;
        LazyLayoutManager manager2(&buffer2);

        manager2 = std::move(manager1);

        REQUIRE(manager2.buffer() == &buffer);
        REQUIRE(manager2.width() == 400.0);
        REQUIRE(manager2.layoutCount() == 1);
    }
}

TEST_CASE("LazyLayoutManager - Paragraph Rect", "[lazy_layout_manager]") {
    TextBuffer buffer;
    buffer.setPlainText("Test paragraph.\nSecond paragraph.");

    LazyLayoutManager manager(&buffer);
    manager.setWidth(400.0);
    manager.setFont(QFont("Arial", 12));
    manager.layoutParagraph(0);

    SECTION("Paragraph rect") {
        QRectF rect = manager.paragraphRect(0);
        REQUIRE(rect.x() == 0.0);
        REQUIRE(rect.y() == 0.0);
        REQUIRE(rect.width() == 400.0);
        REQUIRE(rect.height() > 0.0);
    }
}

TEST_CASE("LazyLayoutManager - LRU Eviction", "[lazy_layout_manager]") {
    TextBuffer buffer;
    // Create many paragraphs
    QString text;
    for (size_t i = 0; i < 200; ++i) {
        text += QString("Paragraph %1.\n").arg(i);
    }
    text.chop(1);
    buffer.setPlainText(text);

    LazyLayoutManager manager(&buffer);
    manager.setWidth(400.0);
    manager.setFont(QFont("Arial", 12));

    SECTION("Eviction happens after max") {
        // Layout more than max cached
        for (size_t i = 0; i < LAZY_MAX_CACHED_LAYOUTS + 20; ++i) {
            manager.layoutParagraph(i);
        }

        // Set viewport to trigger eviction
        manager.setViewport(0, 100);
        manager.releaseDistantLayouts();

        // Should be under or at max
        REQUIRE(manager.layoutCount() <= LAZY_MAX_CACHED_LAYOUTS);
    }
}
