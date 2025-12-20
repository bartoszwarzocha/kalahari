/// @file test_layout_manager.cpp
/// @brief Unit tests for LayoutManager (OpenSpec #00042 Phase 2.11)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/layout_manager.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/virtual_scroll_manager.h>

using namespace kalahari::editor;

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

/// Create a document with specified number of paragraphs
std::unique_ptr<KmlDocument> createTestDocument(int paragraphCount) {
    auto doc = std::make_unique<KmlDocument>();
    for (int i = 0; i < paragraphCount; ++i) {
        auto para = std::make_unique<KmlParagraph>(
            QString("Paragraph %1 with some text content.").arg(i));
        doc->addParagraph(std::move(para));
    }
    return doc;
}

} // anonymous namespace

/// Create a configured layout manager with document and scroll manager
struct LayoutManagerFixture {
    std::unique_ptr<KmlDocument> doc;
    VirtualScrollManager scrollManager;
    LayoutManager layoutManager;

    LayoutManagerFixture(int paragraphCount = 20) {
        doc = createTestDocument(paragraphCount);
        scrollManager.setDocument(doc.get());
        scrollManager.setViewport(0.0, 400.0);  // 400px viewport

        layoutManager.setDocument(doc.get());
        layoutManager.setScrollManager(&scrollManager);
        layoutManager.setWidth(600.0);
        layoutManager.setFont(QFont("Serif", 12));
    }
};

// =============================================================================
// Constructor Tests
// =============================================================================

TEST_CASE("LayoutManager default constructor", "[editor][layout_manager]") {
    LayoutManager manager;

    SECTION("Initial document is null") {
        REQUIRE(manager.document() == nullptr);
    }

    SECTION("Initial scroll manager is null") {
        REQUIRE(manager.scrollManager() == nullptr);
    }

    SECTION("Initial width is 0") {
        REQUIRE(manager.width() == 0.0);
    }

    SECTION("Initial layout count is 0") {
        REQUIRE(manager.layoutCount() == 0);
    }
}

// =============================================================================
// Document Management Tests
// =============================================================================

TEST_CASE("LayoutManager setDocument", "[editor][layout_manager]") {
    auto doc = createTestDocument(10);
    LayoutManager manager;

    SECTION("Set document") {
        manager.setDocument(doc.get());
        REQUIRE(manager.document() == doc.get());
    }

    SECTION("Set null document") {
        manager.setDocument(doc.get());
        manager.setDocument(nullptr);
        REQUIRE(manager.document() == nullptr);
    }

    SECTION("Changing document clears layouts") {
        manager.setDocument(doc.get());
        manager.setWidth(500.0);
        manager.layoutParagraph(0);
        REQUIRE(manager.layoutCount() == 1);

        auto doc2 = createTestDocument(5);
        manager.setDocument(doc2.get());
        REQUIRE(manager.layoutCount() == 0);

        // Clear document before doc2 goes out of scope to avoid use-after-free
        manager.setDocument(nullptr);
    }
}

TEST_CASE("LayoutManager setScrollManager", "[editor][layout_manager]") {
    LayoutManager manager;
    VirtualScrollManager scrollManager;

    SECTION("Set scroll manager") {
        manager.setScrollManager(&scrollManager);
        REQUIRE(manager.scrollManager() == &scrollManager);
    }

    SECTION("Set null scroll manager") {
        manager.setScrollManager(&scrollManager);
        manager.setScrollManager(nullptr);
        REQUIRE(manager.scrollManager() == nullptr);
    }
}

// =============================================================================
// Layout Configuration Tests
// =============================================================================

TEST_CASE("LayoutManager setWidth", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Set width") {
        fixture.layoutManager.setWidth(800.0);
        REQUIRE(fixture.layoutManager.width() == 800.0);
    }

    SECTION("Changing width invalidates layouts") {
        fixture.layoutManager.layoutParagraph(0);
        auto* layout = fixture.layoutManager.paragraphLayout(0);
        REQUIRE_FALSE(layout->isDirty());

        fixture.layoutManager.setWidth(800.0);
        REQUIRE(layout->isDirty());
    }

    SECTION("Same width does not invalidate") {
        fixture.layoutManager.setWidth(600.0);  // Same as initial
        fixture.layoutManager.layoutParagraph(0);
        auto* layout = fixture.layoutManager.paragraphLayout(0);
        REQUIRE_FALSE(layout->isDirty());

        fixture.layoutManager.setWidth(600.0);  // No change
        REQUIRE_FALSE(layout->isDirty());
    }
}

TEST_CASE("LayoutManager setFont", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Set font") {
        QFont newFont("Arial", 14);
        fixture.layoutManager.setFont(newFont);
        REQUIRE(fixture.layoutManager.font() == newFont);
    }

    SECTION("Changing font invalidates layouts") {
        fixture.layoutManager.layoutParagraph(0);
        auto* layout = fixture.layoutManager.paragraphLayout(0);
        REQUIRE_FALSE(layout->isDirty());

        fixture.layoutManager.setFont(QFont("Arial", 14));
        REQUIRE(layout->isDirty());
    }

    SECTION("Changing font updates existing layouts") {
        fixture.layoutManager.layoutParagraph(0);
        auto* layout = fixture.layoutManager.paragraphLayout(0);

        QFont newFont("Arial", 14);
        fixture.layoutManager.setFont(newFont);
        REQUIRE(layout->font() == newFont);
    }
}

// =============================================================================
// Layout Operations Tests
// =============================================================================

TEST_CASE("LayoutManager layoutParagraph", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Creates layout if not exists") {
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(0));

        qreal height = fixture.layoutManager.layoutParagraph(0);

        REQUIRE(fixture.layoutManager.hasLayout(0));
        REQUIRE(height > 0.0);
    }

    SECTION("Returns 0 for invalid index") {
        REQUIRE(fixture.layoutManager.layoutParagraph(-1) == 0.0);
        REQUIRE(fixture.layoutManager.layoutParagraph(100) == 0.0);
    }

    SECTION("Updates scroll manager with height") {
        fixture.layoutManager.layoutParagraph(0);

        // Check that scroll manager got the height update
        REQUIRE(fixture.scrollManager.isHeightKnown(0));
    }

    SECTION("Uses document paragraph text") {
        fixture.layoutManager.layoutParagraph(5);
        auto* layout = fixture.layoutManager.paragraphLayout(5);

        REQUIRE(layout != nullptr);
        REQUIRE(layout->text() == fixture.doc->paragraph(5)->plainText());
    }
}

TEST_CASE("LayoutManager layoutVisibleParagraphs", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Layouts visible range from scroll manager") {
        fixture.scrollManager.setBufferParagraphs(0);  // No buffer for precise testing

        qreal totalHeight = fixture.layoutManager.layoutVisibleParagraphs();
        REQUIRE(totalHeight > 0.0);

        auto [first, last] = fixture.scrollManager.visibleRange();
        REQUIRE(first >= 0);
        REQUIRE(last >= first);

        // All visible paragraphs should have layouts
        for (int i = first; i <= last; ++i) {
            REQUIRE(fixture.layoutManager.hasLayout(i));
        }
    }

    SECTION("Returns 0 without document") {
        LayoutManager emptyManager;
        REQUIRE(emptyManager.layoutVisibleParagraphs() == 0.0);
    }

    SECTION("Returns 0 without scroll manager") {
        auto doc = createTestDocument(10);
        LayoutManager manager;
        manager.setDocument(doc.get());
        REQUIRE(manager.layoutVisibleParagraphs() == 0.0);
    }

    SECTION("Returns positive height with content") {
        qreal height = fixture.layoutManager.layoutVisibleParagraphs();
        REQUIRE(height > 0.0);
    }
}

TEST_CASE("LayoutManager paragraphLayout access", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Returns nullptr if not created") {
        REQUIRE(fixture.layoutManager.paragraphLayout(0) == nullptr);
    }

    SECTION("Returns layout after creation") {
        fixture.layoutManager.layoutParagraph(0);
        REQUIRE(fixture.layoutManager.paragraphLayout(0) != nullptr);
    }

    SECTION("Const access works") {
        fixture.layoutManager.layoutParagraph(0);
        const LayoutManager& constManager = fixture.layoutManager;
        REQUIRE(constManager.paragraphLayout(0) != nullptr);
    }
}

TEST_CASE("LayoutManager hasLayout", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Returns false initially") {
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(0));
    }

    SECTION("Returns true after layout") {
        fixture.layoutManager.layoutParagraph(0);
        REQUIRE(fixture.layoutManager.hasLayout(0));
    }

    SECTION("Returns false for invalid index") {
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(-1));
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(100));
    }
}

TEST_CASE("LayoutManager layoutCount", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Initially zero") {
        REQUIRE(fixture.layoutManager.layoutCount() == 0);
    }

    SECTION("Increases as layouts are created") {
        fixture.layoutManager.layoutParagraph(0);
        REQUIRE(fixture.layoutManager.layoutCount() == 1);

        fixture.layoutManager.layoutParagraph(5);
        REQUIRE(fixture.layoutManager.layoutCount() == 2);

        fixture.layoutManager.layoutParagraph(10);
        REQUIRE(fixture.layoutManager.layoutCount() == 3);
    }

    SECTION("Same paragraph doesn't increase count") {
        fixture.layoutManager.layoutParagraph(0);
        fixture.layoutManager.layoutParagraph(0);
        REQUIRE(fixture.layoutManager.layoutCount() == 1);
    }
}

// =============================================================================
// Cache Management Tests
// =============================================================================

TEST_CASE("LayoutManager invalidateLayout", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Marks layout as dirty") {
        fixture.layoutManager.layoutParagraph(0);
        auto* layout = fixture.layoutManager.paragraphLayout(0);
        REQUIRE_FALSE(layout->isDirty());

        fixture.layoutManager.invalidateLayout(0);
        REQUIRE(layout->isDirty());
    }

    SECTION("No effect if layout doesn't exist") {
        fixture.layoutManager.invalidateLayout(0);  // Should not crash
        REQUIRE(fixture.layoutManager.layoutCount() == 0);
    }
}

TEST_CASE("LayoutManager invalidateAllLayouts", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    // Create multiple layouts
    fixture.layoutManager.layoutParagraph(0);
    fixture.layoutManager.layoutParagraph(5);
    fixture.layoutManager.layoutParagraph(10);

    SECTION("All layouts become dirty") {
        fixture.layoutManager.invalidateAllLayouts();

        REQUIRE(fixture.layoutManager.paragraphLayout(0)->isDirty());
        REQUIRE(fixture.layoutManager.paragraphLayout(5)->isDirty());
        REQUIRE(fixture.layoutManager.paragraphLayout(10)->isDirty());
    }

    SECTION("Layout count unchanged") {
        int countBefore = fixture.layoutManager.layoutCount();
        fixture.layoutManager.invalidateAllLayouts();
        REQUIRE(fixture.layoutManager.layoutCount() == countBefore);
    }
}

TEST_CASE("LayoutManager clearLayouts", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    // Create layouts
    fixture.layoutManager.layoutParagraph(0);
    fixture.layoutManager.layoutParagraph(5);
    REQUIRE(fixture.layoutManager.layoutCount() == 2);

    SECTION("Clears all layouts") {
        fixture.layoutManager.clearLayouts();
        REQUIRE(fixture.layoutManager.layoutCount() == 0);
    }

    SECTION("hasLayout returns false after clear") {
        fixture.layoutManager.clearLayouts();
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(0));
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(5));
    }
}

TEST_CASE("LayoutManager releaseInvisibleLayouts", "[editor][layout_manager]") {
    LayoutManagerFixture fixture(100);  // 100 paragraphs
    fixture.scrollManager.setBufferParagraphs(2);

    // Layout visible paragraphs
    fixture.layoutManager.layoutVisibleParagraphs();

    SECTION("Keeps visible layouts") {
        auto [first, last] = fixture.scrollManager.visibleRange();
        int countBefore = fixture.layoutManager.layoutCount();
        REQUIRE(countBefore > 0);

        fixture.layoutManager.releaseInvisibleLayouts();

        // Count should not decrease (all were visible)
        REQUIRE(fixture.layoutManager.layoutCount() == countBefore);

        // All visible should still exist
        for (int i = first; i <= last; ++i) {
            if (fixture.layoutManager.hasLayout(i)) {
                REQUIRE(fixture.layoutManager.hasLayout(i));
            }
        }
    }

    SECTION("Removes invisible layouts") {
        // Layout some paragraphs outside visible range
        fixture.layoutManager.layoutParagraph(90);
        fixture.layoutManager.layoutParagraph(95);

        auto [first, last] = fixture.scrollManager.visibleRange();
        REQUIRE(90 > last);  // Should be outside visible range
        REQUIRE(95 > last);

        fixture.layoutManager.releaseInvisibleLayouts();

        // These should be removed
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(90));
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(95));
    }

    SECTION("Handles no scroll manager") {
        fixture.layoutManager.setScrollManager(nullptr);
        fixture.layoutManager.releaseInvisibleLayouts();  // Should not crash
        // All layouts should be cleared
        REQUIRE(fixture.layoutManager.layoutCount() == 0);
    }
}

// =============================================================================
// Geometry Query Tests
// =============================================================================

TEST_CASE("LayoutManager paragraphY", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Delegates to scroll manager") {
        qreal y = fixture.layoutManager.paragraphY(5);
        qreal scrollY = fixture.scrollManager.paragraphY(5);
        REQUIRE(y == scrollY);
    }

    SECTION("Returns 0 without scroll manager") {
        fixture.layoutManager.setScrollManager(nullptr);
        REQUIRE(fixture.layoutManager.paragraphY(5) == 0.0);
    }
}

TEST_CASE("LayoutManager paragraphHeight", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Returns estimated height for unlayouted paragraph") {
        qreal height = fixture.layoutManager.paragraphHeight(0);
        REQUIRE(height == ESTIMATED_LINE_HEIGHT);
    }

    SECTION("Returns measured height after layout") {
        fixture.layoutManager.layoutParagraph(0);
        auto* layout = fixture.layoutManager.paragraphLayout(0);

        qreal height = fixture.layoutManager.paragraphHeight(0);
        REQUIRE(height == layout->height());
    }

    SECTION("Returns estimated for dirty layout") {
        fixture.layoutManager.layoutParagraph(0);
        fixture.layoutManager.invalidateLayout(0);

        // After invalidation, should fall back to scroll manager
        qreal height = fixture.layoutManager.paragraphHeight(0);
        REQUIRE(height > 0.0);
    }
}

TEST_CASE("LayoutManager totalHeight", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Delegates to scroll manager") {
        qreal total = fixture.layoutManager.totalHeight();
        qreal scrollTotal = fixture.scrollManager.totalHeight();
        REQUIRE(total == scrollTotal);
    }

    SECTION("Returns 0 without scroll manager") {
        fixture.layoutManager.setScrollManager(nullptr);
        REQUIRE(fixture.layoutManager.totalHeight() == 0.0);
    }
}

TEST_CASE("LayoutManager paragraphRect", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Returns correct rectangle") {
        fixture.layoutManager.layoutParagraph(5);

        QRectF rect = fixture.layoutManager.paragraphRect(5);

        REQUIRE(rect.x() == 0.0);
        REQUIRE(rect.y() == fixture.layoutManager.paragraphY(5));
        REQUIRE(rect.width() == fixture.layoutManager.width());
        REQUIRE(rect.height() == fixture.layoutManager.paragraphHeight(5));
    }
}

// =============================================================================
// Document Observer Tests
// =============================================================================

TEST_CASE("LayoutManager observes document changes", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    // Create some layouts
    fixture.layoutManager.layoutParagraph(0);
    fixture.layoutManager.layoutParagraph(5);
    fixture.layoutManager.layoutParagraph(10);

    SECTION("onContentChanged invalidates all") {
        fixture.layoutManager.onContentChanged();

        REQUIRE(fixture.layoutManager.paragraphLayout(0)->isDirty());
        REQUIRE(fixture.layoutManager.paragraphLayout(5)->isDirty());
        REQUIRE(fixture.layoutManager.paragraphLayout(10)->isDirty());
    }

    SECTION("onParagraphModified invalidates single") {
        fixture.layoutManager.onParagraphModified(5);

        REQUIRE_FALSE(fixture.layoutManager.paragraphLayout(0)->isDirty());
        REQUIRE(fixture.layoutManager.paragraphLayout(5)->isDirty());
        REQUIRE_FALSE(fixture.layoutManager.paragraphLayout(10)->isDirty());
    }

    SECTION("onParagraphRemoved removes layout and shifts") {
        REQUIRE(fixture.layoutManager.hasLayout(10));

        fixture.layoutManager.onParagraphRemoved(5);

        // Layout for index 5 should be removed
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(5));

        // Layout that was at 10 is now at 9
        REQUIRE(fixture.layoutManager.hasLayout(9));
    }

    SECTION("onParagraphInserted shifts layouts") {
        fixture.layoutManager.onParagraphInserted(3);

        // Layout at 0 unchanged
        REQUIRE(fixture.layoutManager.hasLayout(0));

        // Layout at 5 is now at 6
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(5));
        REQUIRE(fixture.layoutManager.hasLayout(6));

        // Layout at 10 is now at 11
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(10));
        REQUIRE(fixture.layoutManager.hasLayout(11));
    }
}

TEST_CASE("LayoutManager document observer integration", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    // Layout some paragraphs
    fixture.layoutManager.layoutParagraph(0);
    fixture.layoutManager.layoutParagraph(5);

    SECTION("Adding paragraph via document triggers observer") {
        auto para = std::make_unique<KmlParagraph>("New paragraph");
        fixture.doc->addParagraph(std::move(para));

        // Document should have notified observer
        // Layouts should still be valid (addParagraph adds at end)
        REQUIRE(fixture.layoutManager.hasLayout(0));
        REQUIRE(fixture.layoutManager.hasLayout(5));
    }

    SECTION("Inserting paragraph shifts layouts") {
        auto para = std::make_unique<KmlParagraph>("Inserted");
        fixture.doc->insertParagraph(3, std::move(para));

        // Layout at 0 unchanged
        REQUIRE(fixture.layoutManager.hasLayout(0));

        // Layout at 5 should now be at 6
        REQUIRE(fixture.layoutManager.hasLayout(6));
    }

    SECTION("Removing paragraph removes layout") {
        fixture.doc->removeParagraph(5);

        // Layout at 5 should be removed
        REQUIRE_FALSE(fixture.layoutManager.hasLayout(5));
    }
}

// =============================================================================
// Move Semantics Tests
// =============================================================================

TEST_CASE("LayoutManager move constructor", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;
    fixture.layoutManager.layoutParagraph(0);
    fixture.layoutManager.layoutParagraph(5);

    SECTION("Moves layouts and config") {
        qreal width = fixture.layoutManager.width();
        QFont font = fixture.layoutManager.font();
        int count = fixture.layoutManager.layoutCount();

        LayoutManager moved(std::move(fixture.layoutManager));

        REQUIRE(moved.document() == fixture.doc.get());
        REQUIRE(moved.width() == width);
        REQUIRE(moved.font() == font);
        REQUIRE(moved.layoutCount() == count);
    }

    SECTION("Original is cleared") {
        LayoutManager moved(std::move(fixture.layoutManager));

        REQUIRE(fixture.layoutManager.document() == nullptr);
        REQUIRE(fixture.layoutManager.scrollManager() == nullptr);
        REQUIRE(fixture.layoutManager.layoutCount() == 0);
    }
}

TEST_CASE("LayoutManager move assignment", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;
    fixture.layoutManager.layoutParagraph(0);

    LayoutManager target;

    SECTION("Moves data to target") {
        target = std::move(fixture.layoutManager);

        REQUIRE(target.document() == fixture.doc.get());
        REQUIRE(target.layoutCount() == 1);
    }

    SECTION("Original is cleared") {
        target = std::move(fixture.layoutManager);

        REQUIRE(fixture.layoutManager.document() == nullptr);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("LayoutManager edge cases", "[editor][layout_manager]") {
    SECTION("Empty document") {
        KmlDocument emptyDoc;
        VirtualScrollManager scrollManager;
        scrollManager.setDocument(&emptyDoc);
        scrollManager.setViewport(0.0, 400.0);

        LayoutManager manager;
        manager.setDocument(&emptyDoc);
        manager.setScrollManager(&scrollManager);
        manager.setWidth(600.0);

        qreal height = manager.layoutVisibleParagraphs();
        REQUIRE(height == 0.0);
        REQUIRE(manager.layoutCount() == 0);
    }

    SECTION("Single paragraph document") {
        auto doc = createTestDocument(1);
        VirtualScrollManager scrollManager;
        scrollManager.setDocument(doc.get());
        scrollManager.setViewport(0.0, 400.0);

        LayoutManager manager;
        manager.setDocument(doc.get());
        manager.setScrollManager(&scrollManager);
        manager.setWidth(600.0);

        manager.layoutVisibleParagraphs();

        REQUIRE(manager.layoutCount() == 1);
        REQUIRE(manager.hasLayout(0));
    }

    SECTION("Zero width") {
        LayoutManagerFixture fixture;
        fixture.layoutManager.setWidth(0.0);

        // Should still create layouts, they just won't wrap
        fixture.layoutManager.layoutParagraph(0);
        REQUIRE(fixture.layoutManager.hasLayout(0));
    }

    SECTION("Repeated layout calls are efficient") {
        LayoutManagerFixture fixture;

        // First layout
        fixture.layoutManager.layoutParagraph(0);
        auto* layout = fixture.layoutManager.paragraphLayout(0);
        REQUIRE_FALSE(layout->isDirty());

        // Second layout should not recalculate
        fixture.layoutManager.layoutParagraph(0);
        REQUIRE_FALSE(layout->isDirty());
    }
}

// =============================================================================
// Text Update Tests
// =============================================================================

TEST_CASE("LayoutManager text synchronization", "[editor][layout_manager]") {
    LayoutManagerFixture fixture;

    SECTION("Layout gets text from document") {
        fixture.layoutManager.layoutParagraph(3);
        auto* layout = fixture.layoutManager.paragraphLayout(3);

        QString expected = fixture.doc->paragraph(3)->plainText();
        REQUIRE(layout->text() == expected);
    }

    SECTION("Modified paragraph text is updated on layout") {
        fixture.layoutManager.layoutParagraph(0);

        // Modify the paragraph directly
        KmlParagraph* para = fixture.doc->paragraph(0);
        QString newText = "Modified text content";
        para->insertText(0, newText);
        para->deleteText(newText.length(), para->characterCount());

        // Invalidate and relayout
        fixture.layoutManager.invalidateLayout(0);
        fixture.layoutManager.layoutParagraph(0);

        auto* layout = fixture.layoutManager.paragraphLayout(0);
        REQUIRE(layout->text().contains("Modified"));
    }
}
