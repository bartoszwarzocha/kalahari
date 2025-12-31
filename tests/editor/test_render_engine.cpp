/// @file test_render_engine.cpp
/// @brief Unit tests for RenderEngine (OpenSpec #00043 Phase 11.8)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <kalahari/editor/render_engine.h>
#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/viewport_manager.h>
#include <QImage>
#include <QPainter>
#include <QTextDocument>
#include <memory>

using namespace kalahari::editor;

// =============================================================================
// Helper: Create document with test paragraphs
// =============================================================================

static std::unique_ptr<QTextDocument> createTestDocument(size_t paragraphCount) {
    auto doc = std::make_unique<QTextDocument>();
    QStringList paragraphs;
    for (size_t i = 0; i < paragraphCount; ++i) {
        paragraphs << QString("Paragraph %1 with some text content").arg(i + 1);
    }
    doc->setPlainText(paragraphs.join("\n"));
    return doc;
}

// Helper: Create TextBuffer for tests that need height management
static TextBuffer createTestBuffer(size_t paragraphCount, double height = 20.0) {
    TextBuffer buffer;
    QString text;
    for (size_t i = 0; i < paragraphCount; ++i) {
        if (i > 0) text += "\n";
        text += QString("Paragraph %1 with some text content").arg(i + 1);
    }
    buffer.setPlainText(text);

    for (size_t i = 0; i < paragraphCount; ++i) {
        buffer.setParagraphHeight(i, height);
    }

    return buffer;
}

// =============================================================================
// Constructor / Destructor Tests
// =============================================================================

TEST_CASE("RenderEngine - Construction", "[render_engine]") {
    SECTION("Default construction") {
        RenderEngine engine;

        REQUIRE(engine.document() == nullptr);
        REQUIRE(engine.viewportManager() == nullptr);
        REQUIRE(engine.backgroundColor() == QColor(255, 255, 255));
        REQUIRE(engine.textColor() == QColor(0, 0, 0));
        REQUIRE(engine.leftMargin() == 10.0);
        REQUIRE(engine.topMargin() == 10.0);
        REQUIRE(engine.cursorWidth() == 2.0);
    }

    SECTION("Construction with parent") {
        QObject parent;
        RenderEngine engine(&parent);

        REQUIRE(engine.parent() == &parent);
    }
}

// =============================================================================
// Component Integration Tests
// =============================================================================

TEST_CASE("RenderEngine - Component Integration", "[render_engine]") {
    auto doc = createTestDocument(10);
    ViewportManager viewport;
    viewport.setDocument(doc.get());
    viewport.setViewportSize(QSize(800, 600));

    RenderEngine engine;

    SECTION("Set document") {
        engine.setDocument(doc.get());
        REQUIRE(engine.document() == doc.get());
    }

    SECTION("Set viewport manager") {
        engine.setViewportManager(&viewport);
        REQUIRE(engine.viewportManager() == &viewport);
    }
}

// =============================================================================
// Appearance Configuration Tests
// =============================================================================

TEST_CASE("RenderEngine - Appearance", "[render_engine]") {
    RenderEngine engine;

    SECTION("Set font") {
        QFont font("Arial", 14);
        engine.setFont(font);
        REQUIRE(engine.font().family() == "Arial");
        REQUIRE(engine.font().pointSize() == 14);
    }

    SECTION("Set background color") {
        engine.setBackgroundColor(QColor(240, 240, 240));
        REQUIRE(engine.backgroundColor() == QColor(240, 240, 240));
    }

    SECTION("Set text color") {
        engine.setTextColor(QColor(30, 30, 30));
        REQUIRE(engine.textColor() == QColor(30, 30, 30));
    }

    SECTION("Set selection colors") {
        engine.setSelectionColor(QColor(100, 150, 200, 128));
        engine.setSelectionTextColor(QColor(255, 255, 255));
        REQUIRE(engine.selectionColor() == QColor(100, 150, 200, 128));
        REQUIRE(engine.selectionTextColor() == QColor(255, 255, 255));
    }

    SECTION("Set cursor color") {
        engine.setCursorColor(QColor(0, 0, 255));
        REQUIRE(engine.cursorColor() == QColor(0, 0, 255));
    }

    SECTION("Set margins") {
        engine.setLeftMargin(20.0);
        engine.setTopMargin(15.0);
        engine.setRightMargin(25.0);
        REQUIRE(engine.leftMargin() == 20.0);
        REQUIRE(engine.topMargin() == 15.0);
        REQUIRE(engine.rightMargin() == 25.0);
    }

    SECTION("Set line spacing") {
        engine.setLineSpacing(1.5);
        REQUIRE(engine.lineSpacing() == 1.5);
    }

    SECTION("Set cursor width") {
        engine.setCursorWidth(3.0);
        REQUIRE(engine.cursorWidth() == 3.0);
    }
}

// =============================================================================
// Dirty Region Tracking Tests
// =============================================================================

TEST_CASE("RenderEngine - Dirty Region Tracking", "[render_engine]") {
    auto doc = createTestDocument(10);
    ViewportManager viewport;
    viewport.setDocument(doc.get());
    viewport.setViewportSize(QSize(800, 600));

    RenderEngine engine;
    engine.setDocument(doc.get());
    engine.setViewportManager(&viewport);

    SECTION("Initial state is clean") {
        engine.clearDirtyRegion();
        REQUIRE_FALSE(engine.isDirty());
        REQUIRE(engine.dirtyRegion().isEmpty());
    }

    SECTION("Mark rect dirty") {
        engine.clearDirtyRegion();
        engine.markDirty(QRect(10, 20, 100, 50));

        REQUIRE(engine.isDirty());
        REQUIRE(engine.dirtyRegion().contains(QPoint(50, 40)));
    }

    SECTION("Mark paragraph dirty") {
        engine.clearDirtyRegion();
        engine.markParagraphDirty(5);

        REQUIRE(engine.isDirty());
    }

    SECTION("Mark all dirty") {
        engine.clearDirtyRegion();
        engine.markAllDirty();

        REQUIRE(engine.isDirty());
    }

    SECTION("Clear dirty region") {
        engine.markDirty(QRect(0, 0, 100, 100));
        REQUIRE(engine.isDirty());

        engine.clearDirtyRegion();
        REQUIRE_FALSE(engine.isDirty());
    }

    SECTION("Dirty region union") {
        engine.clearDirtyRegion();
        engine.markDirty(QRect(0, 0, 50, 50));
        engine.markDirty(QRect(100, 100, 50, 50));

        QRegion region = engine.dirtyRegion();
        REQUIRE(region.contains(QPoint(25, 25)));
        REQUIRE(region.contains(QPoint(125, 125)));
    }
}

// =============================================================================
// Selection Tests
// =============================================================================

TEST_CASE("RenderEngine - Selection", "[render_engine]") {
    RenderEngine engine;

    SECTION("No initial selection") {
        REQUIRE_FALSE(engine.hasSelection());
        REQUIRE(engine.selection().isEmpty());
    }

    SECTION("Set selection") {
        SelectionRange sel;
        sel.start = {0, 5};
        sel.end = {2, 10};

        engine.setSelection(sel);

        REQUIRE(engine.hasSelection());
        REQUIRE(engine.selection().start.paragraph == 0);
        REQUIRE(engine.selection().start.offset == 5);
        REQUIRE(engine.selection().end.paragraph == 2);
        REQUIRE(engine.selection().end.offset == 10);
    }

    SECTION("Clear selection") {
        SelectionRange sel;
        sel.start = {0, 5};
        sel.end = {0, 10};
        engine.setSelection(sel);

        REQUIRE(engine.hasSelection());

        engine.clearSelection();
        REQUIRE_FALSE(engine.hasSelection());
    }
}

// =============================================================================
// Cursor Tests
// =============================================================================

TEST_CASE("RenderEngine - Cursor", "[render_engine]") {
    RenderEngine engine;

    SECTION("Initial cursor position") {
        REQUIRE(engine.cursorPosition().paragraph == 0);
        REQUIRE(engine.cursorPosition().offset == 0);
    }

    SECTION("Set cursor position") {
        CursorPosition pos{5, 10};
        engine.setCursorPosition(pos);

        REQUIRE(engine.cursorPosition().paragraph == 5);
        REQUIRE(engine.cursorPosition().offset == 10);
    }

    SECTION("Cursor visibility") {
        REQUIRE(engine.isCursorVisible());

        engine.setCursorVisible(false);
        REQUIRE_FALSE(engine.isCursorVisible());

        engine.setCursorVisible(true);
        REQUIRE(engine.isCursorVisible());
    }

    SECTION("Cursor blink interval") {
        REQUIRE(engine.cursorBlinkInterval() == 500);

        engine.setCursorBlinkInterval(250);
        REQUIRE(engine.cursorBlinkInterval() == 250);
    }

    SECTION("Cursor rect without components") {
        // Without buffer/viewport, cursor rect should be empty
        QRectF rect = engine.cursorRect();
        REQUIRE(rect.isEmpty());
    }
}

// =============================================================================
// Cursor Rect with Components Tests
// =============================================================================

TEST_CASE("RenderEngine - Cursor Rect", "[render_engine]") {
    TextBuffer buffer = createTestBuffer(10);
    ViewportManager viewport;
    viewport.setDocument(buffer.document());
    viewport.setViewportSize(QSize(800, 600));

    RenderEngine engine;
    engine.setDocument(buffer.document());
    engine.setViewportManager(&viewport);

    SECTION("Cursor rect at start") {
        engine.setCursorPosition({0, 0});
        QRectF rect = engine.cursorRect();

        // With components set, cursor rect should have dimensions
        REQUIRE(rect.width() > 0);
        REQUIRE(rect.height() > 0);
        REQUIRE(rect.x() >= engine.leftMargin());
    }

    SECTION("Cursor rect at different paragraph") {
        engine.setCursorPosition({5, 0});
        QRectF rect = engine.cursorRect();

        // Y position should be offset by paragraph heights
        REQUIRE(rect.y() > engine.topMargin());
    }
}

// =============================================================================
// Paint Tests
// =============================================================================

TEST_CASE("RenderEngine - Paint", "[render_engine]") {
    TextBuffer buffer = createTestBuffer(10);
    ViewportManager viewport;
    viewport.setDocument(buffer.document());
    viewport.setViewportSize(QSize(800, 600));

    RenderEngine engine;
    engine.setDocument(buffer.document());
    engine.setViewportManager(&viewport);
    engine.setBackgroundColor(Qt::white);

    SECTION("Paint to image") {
        QImage image(800, 600, QImage::Format_ARGB32);
        QPainter painter(&image);

        // Should not crash
        engine.paint(&painter, QRect(0, 0, 800, 600), QSize(800, 600));

        painter.end();

        // Check background was painted
        REQUIRE(image.pixelColor(0, 0) == Qt::white);
    }

    SECTION("Paint clears dirty region") {
        engine.markAllDirty();
        REQUIRE(engine.isDirty());

        QImage image(800, 600, QImage::Format_ARGB32);
        QPainter painter(&image);
        engine.paint(&painter, QRect(0, 0, 800, 600), QSize(800, 600));
        painter.end();

        REQUIRE_FALSE(engine.isDirty());
    }

    SECTION("Paint with selection") {
        SelectionRange sel;
        sel.start = {0, 0};
        sel.end = {0, 5};
        engine.setSelection(sel);

        QImage image(800, 600, QImage::Format_ARGB32);
        QPainter painter(&image);

        // Should not crash with selection
        engine.paint(&painter, QRect(0, 0, 800, 600), QSize(800, 600));
        painter.end();
    }

    SECTION("Paint with cursor") {
        engine.setCursorPosition({0, 5});
        engine.setCursorVisible(true);

        QImage image(800, 600, QImage::Format_ARGB32);
        QPainter painter(&image);

        // Should not crash with cursor
        engine.paint(&painter, QRect(0, 0, 800, 600), QSize(800, 600));
        painter.end();
    }
}

// =============================================================================
// Geometry Query Tests
// =============================================================================

TEST_CASE("RenderEngine - Geometry Queries", "[render_engine]") {
    TextBuffer buffer = createTestBuffer(10);
    ViewportManager viewport;
    viewport.setDocument(buffer.document());
    viewport.setViewportSize(QSize(800, 600));

    RenderEngine engine;
    engine.setDocument(buffer.document());
    engine.setViewportManager(&viewport);
    engine.setTopMargin(10.0);

    SECTION("Paragraph Y") {
        // Paragraph Y positions should be monotonically increasing
        double y0 = engine.paragraphY(0);
        double y1 = engine.paragraphY(1);
        double y5 = engine.paragraphY(5);

        // First paragraph should have some Y position (may include document margin)
        REQUIRE(y0 >= 0.0);

        // Later paragraphs should be at higher Y positions
        REQUIRE(y1 > y0);
        REQUIRE(y5 > y1);
    }

    SECTION("Document to widget Y") {
        // At scroll 0: docY + topMargin
        REQUIRE(engine.documentToWidgetY(0.0) == 10.0);
        REQUIRE(engine.documentToWidgetY(100.0) == 110.0);
    }

    SECTION("Widget to document Y") {
        REQUIRE(engine.widgetToDocumentY(10.0) == 0.0);
        REQUIRE(engine.widgetToDocumentY(110.0) == 100.0);
    }

    SECTION("Document to widget with scroll") {
        // Create buffer where content > viewport so scrolling is possible
        // 50 paragraphs × 20px = 1000px content > 600px viewport
        TextBuffer scrollBuffer = createTestBuffer(50);
        ViewportManager scrollViewport;
        scrollViewport.setDocument(scrollBuffer.document());
        scrollViewport.setViewportSize(QSize(800, 600));

        RenderEngine scrollEngine;
        scrollEngine.setDocument(scrollBuffer.document());
        scrollEngine.setViewportManager(&scrollViewport);
        scrollEngine.setTopMargin(10.0);

        scrollViewport.setScrollPosition(50.0);

        // At scroll 50: docY + topMargin - scrollPos
        REQUIRE(scrollEngine.documentToWidgetY(0.0) == -40.0);  // 0 + 10 - 50
        REQUIRE(scrollEngine.documentToWidgetY(100.0) == 60.0);  // 100 + 10 - 50
    }
}

// =============================================================================
// Signal Tests
// =============================================================================

TEST_CASE("RenderEngine - Signals", "[render_engine]") {
    auto doc = createTestDocument(10);
    ViewportManager viewport;
    viewport.setDocument(doc.get());
    viewport.setViewportSize(QSize(800, 600));

    RenderEngine engine;
    engine.setDocument(doc.get());
    engine.setViewportManager(&viewport);

    SECTION("Repaint requested on mark dirty") {
        int signalCount = 0;
        QObject::connect(&engine, &RenderEngine::repaintRequested,
                         [&signalCount](const QRegion&) { ++signalCount; });

        engine.markDirty(QRect(0, 0, 100, 100));

        REQUIRE(signalCount == 1);
    }

    SECTION("Repaint requested on mark all dirty") {
        int signalCount = 0;
        QObject::connect(&engine, &RenderEngine::repaintRequested,
                         [&signalCount](const QRegion&) { ++signalCount; });

        engine.markAllDirty();

        REQUIRE(signalCount == 1);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("RenderEngine - Edge Cases", "[render_engine]") {
    SECTION("Paint without components") {
        RenderEngine engine;
        QImage image(800, 600, QImage::Format_ARGB32);
        QPainter painter(&image);

        // Should not crash
        engine.paint(&painter, QRect(0, 0, 800, 600), QSize(800, 600));
        painter.end();
    }

    SECTION("Cursor rect with invalid paragraph") {
        auto doc = createTestDocument(5);
        ViewportManager viewport;
        viewport.setDocument(doc.get());
        viewport.setViewportSize(QSize(800, 600));

        RenderEngine engine;
        engine.setDocument(doc.get());
        engine.setViewportManager(&viewport);

        engine.setCursorPosition({100, 0});  // Invalid paragraph
        QRectF rect = engine.cursorRect();

        // Should return empty rect for invalid paragraph
        REQUIRE(rect.isEmpty());
    }

    SECTION("Selection normalization") {
        RenderEngine engine;

        // Set reversed selection (end before start)
        SelectionRange sel;
        sel.start = {5, 20};
        sel.end = {2, 5};
        engine.setSelection(sel);

        // Engine should handle reversed selection
        REQUIRE(engine.hasSelection());
    }
}
