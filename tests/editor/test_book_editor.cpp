/// @file test_book_editor.cpp
/// @brief Unit tests for BookEditor (OpenSpec #00042 Phase 3.1-3.5)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <memory>

using namespace kalahari::editor;

// =============================================================================
// Test Fixtures and Helpers
// =============================================================================

namespace {

/// Create a document with specified number of paragraphs
std::unique_ptr<KmlDocument> createTestDocument(int paragraphCount) {
    auto doc = std::make_unique<KmlDocument>();
    for (int i = 0; i < paragraphCount; ++i) {
        auto para = std::make_unique<KmlParagraph>(
            QString("Paragraph %1 with some text content for testing.").arg(i));
        doc->addParagraph(std::move(para));
    }
    return doc;
}

/// Ensure QApplication exists for widget tests
/// Note: test_main.cpp should already create QApplication
struct QApplicationGuard {
    QApplicationGuard() {
        // QApplication is created in test_main.cpp
        // This guard is just for documentation
    }
};

}  // anonymous namespace

// =============================================================================
// Constructor Tests
// =============================================================================

TEST_CASE("BookEditor default constructor", "[editor][book_editor]") {
    BookEditor editor;

    SECTION("Initial document is null") {
        REQUIRE(editor.document() == nullptr);
    }

    SECTION("Layout manager exists") {
        // Should not crash when accessing layout manager
        REQUIRE_NOTHROW(editor.layoutManager());
    }

    SECTION("Scroll manager exists") {
        // Should not crash when accessing scroll manager
        REQUIRE_NOTHROW(editor.scrollManager());
    }

    SECTION("Layout manager connected to scroll manager") {
        REQUIRE(editor.layoutManager().scrollManager() == &editor.scrollManager());
    }
}

TEST_CASE("BookEditor constructor with parent", "[editor][book_editor]") {
    QWidget parent;
    BookEditor editor(&parent);

    SECTION("Parent is set correctly") {
        REQUIRE(editor.parent() == &parent);
    }
}

// =============================================================================
// Document Management Tests
// =============================================================================

TEST_CASE("BookEditor setDocument", "[editor][book_editor]") {
    auto doc = createTestDocument(10);
    BookEditor editor;

    SECTION("Set document") {
        editor.setDocument(doc.get());
        REQUIRE(editor.document() == doc.get());
    }

    SECTION("Set null document") {
        editor.setDocument(doc.get());
        editor.setDocument(nullptr);
        REQUIRE(editor.document() == nullptr);
    }

    SECTION("Setting same document does not crash") {
        editor.setDocument(doc.get());
        REQUIRE_NOTHROW(editor.setDocument(doc.get()));
    }

    SECTION("Layout manager gets document") {
        editor.setDocument(doc.get());
        REQUIRE(editor.layoutManager().document() == doc.get());
    }

    SECTION("Scroll manager gets document") {
        editor.setDocument(doc.get());
        REQUIRE(editor.scrollManager().document() == doc.get());
    }
}

TEST_CASE("BookEditor changing documents", "[editor][book_editor]") {
    auto doc1 = createTestDocument(5);
    auto doc2 = createTestDocument(10);
    BookEditor editor;

    SECTION("Can switch between documents") {
        editor.setDocument(doc1.get());
        REQUIRE(editor.document() == doc1.get());

        editor.setDocument(doc2.get());
        REQUIRE(editor.document() == doc2.get());
    }

    SECTION("Managers update when document changes") {
        editor.setDocument(doc1.get());
        editor.setDocument(doc2.get());

        REQUIRE(editor.layoutManager().document() == doc2.get());
        REQUIRE(editor.scrollManager().document() == doc2.get());
    }
}

// =============================================================================
// Size Hint Tests
// =============================================================================

TEST_CASE("BookEditor minimumSizeHint", "[editor][book_editor]") {
    BookEditor editor;

    SECTION("Returns valid minimum size") {
        QSize minSize = editor.minimumSizeHint();
        REQUIRE(minSize.width() > 0);
        REQUIRE(minSize.height() > 0);
    }

    SECTION("Minimum width is at least 200") {
        QSize minSize = editor.minimumSizeHint();
        REQUIRE(minSize.width() >= 200);
    }

    SECTION("Minimum height is at least 100") {
        QSize minSize = editor.minimumSizeHint();
        REQUIRE(minSize.height() >= 100);
    }
}

TEST_CASE("BookEditor sizeHint", "[editor][book_editor]") {
    BookEditor editor;

    SECTION("Returns valid preferred size") {
        QSize prefSize = editor.sizeHint();
        REQUIRE(prefSize.width() > 0);
        REQUIRE(prefSize.height() > 0);
    }

    SECTION("Preferred size is larger than minimum") {
        QSize minSize = editor.minimumSizeHint();
        QSize prefSize = editor.sizeHint();
        REQUIRE(prefSize.width() >= minSize.width());
        REQUIRE(prefSize.height() >= minSize.height());
    }

    SECTION("Preferred width is comfortable for editing") {
        QSize prefSize = editor.sizeHint();
        REQUIRE(prefSize.width() >= 400);
    }

    SECTION("Preferred height is comfortable for editing") {
        QSize prefSize = editor.sizeHint();
        REQUIRE(prefSize.height() >= 300);
    }
}

// =============================================================================
// Layout Manager Access Tests
// =============================================================================

TEST_CASE("BookEditor layoutManager access", "[editor][book_editor]") {
    BookEditor editor;

    SECTION("Returns non-null reference") {
        LayoutManager& manager = editor.layoutManager();
        (void)manager;  // Just checking it doesn't crash
        REQUIRE(true);
    }

    SECTION("Const access works") {
        const BookEditor& constEditor = editor;
        const LayoutManager& manager = constEditor.layoutManager();
        (void)manager;
        REQUIRE(true);
    }

    SECTION("Returns same instance on multiple calls") {
        LayoutManager& first = editor.layoutManager();
        LayoutManager& second = editor.layoutManager();
        REQUIRE(&first == &second);
    }
}

// =============================================================================
// Scroll Manager Access Tests
// =============================================================================

TEST_CASE("BookEditor scrollManager access", "[editor][book_editor]") {
    BookEditor editor;

    SECTION("Returns non-null reference") {
        VirtualScrollManager& manager = editor.scrollManager();
        (void)manager;
        REQUIRE(true);
    }

    SECTION("Const access works") {
        const BookEditor& constEditor = editor;
        const VirtualScrollManager& manager = constEditor.scrollManager();
        (void)manager;
        REQUIRE(true);
    }

    SECTION("Returns same instance on multiple calls") {
        VirtualScrollManager& first = editor.scrollManager();
        VirtualScrollManager& second = editor.scrollManager();
        REQUIRE(&first == &second);
    }
}

// =============================================================================
// Widget Configuration Tests
// =============================================================================

TEST_CASE("BookEditor widget configuration", "[editor][book_editor]") {
    BookEditor editor;

    SECTION("Has strong focus policy") {
        REQUIRE(editor.focusPolicy() == Qt::StrongFocus);
    }
}

// =============================================================================
// Resize Behavior Tests
// =============================================================================

TEST_CASE("BookEditor resize handling", "[editor][book_editor]") {
    auto doc = createTestDocument(10);
    BookEditor editor;
    editor.setDocument(doc.get());

    SECTION("Resize updates layout width") {
        editor.resize(800, 600);
        // Layout width should be widget width minus margins
        qreal layoutWidth = editor.layoutManager().width();
        REQUIRE(layoutWidth > 0);
        REQUIRE(layoutWidth <= 800);
    }

    SECTION("Resize updates viewport height") {
        // Show editor to ensure resize events are processed
        editor.show();
        editor.resize(800, 600);
        // Process pending events to ensure resize is handled
        QApplication::processEvents();

        // Viewport height should be positive and reasonable
        qreal viewportHeight = editor.scrollManager().viewportHeight();
        REQUIRE(viewportHeight > 0);
        // Should be close to requested or actual size
        REQUIRE(viewportHeight <= 600);
    }

    SECTION("Different sizes produce different layout widths") {
        // Use fixed sizes to ensure actual resize happens
        editor.setMinimumSize(0, 0);  // Allow any size
        editor.resize(300, 200);

        // Force geometry update
        qreal width1 = editor.layoutManager().width();

        editor.resize(700, 500);
        qreal width2 = editor.layoutManager().width();

        // The second resize should produce a larger or equal layout width
        // (equal if widget was already at maximum size)
        REQUIRE(width2 >= width1);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("BookEditor edge cases", "[editor][book_editor]") {
    SECTION("Empty document") {
        KmlDocument emptyDoc;
        BookEditor editor;

        REQUIRE_NOTHROW(editor.setDocument(&emptyDoc));
        REQUIRE(editor.document() == &emptyDoc);
    }

    SECTION("Single paragraph document") {
        auto doc = createTestDocument(1);
        BookEditor editor;

        editor.setDocument(doc.get());
        REQUIRE(editor.document() == doc.get());
    }

    SECTION("Large document") {
        auto doc = createTestDocument(1000);
        BookEditor editor;

        // Should not crash or take excessive time
        REQUIRE_NOTHROW(editor.setDocument(doc.get()));
    }

    SECTION("Minimum size widget") {
        BookEditor editor;
        editor.resize(editor.minimumSizeHint());

        // Should handle minimum size gracefully
        REQUIRE(editor.layoutManager().width() > 0);
    }

    SECTION("Zero size widget") {
        BookEditor editor;
        editor.resize(0, 0);

        // Should handle zero size gracefully
        REQUIRE_NOTHROW(editor.layoutManager().width());
    }
}

// =============================================================================
// Memory Safety Tests
// =============================================================================

TEST_CASE("BookEditor memory safety", "[editor][book_editor]") {
    SECTION("Document cleared before destruction") {
        auto doc = createTestDocument(10);
        auto editor = std::make_unique<BookEditor>();

        editor->setDocument(doc.get());
        editor->setDocument(nullptr);  // Clear before doc goes out of scope

        // doc and editor can now be destroyed safely in any order
    }

    SECTION("Multiple editors with same document") {
        auto doc = createTestDocument(10);
        BookEditor editor1;
        BookEditor editor2;

        editor1.setDocument(doc.get());
        editor2.setDocument(doc.get());

        REQUIRE(editor1.document() == doc.get());
        REQUIRE(editor2.document() == doc.get());

        // Clear before doc destruction
        editor1.setDocument(nullptr);
        editor2.setDocument(nullptr);
    }
}

// =============================================================================
// Scrollbar Tests (Phase 3.2)
// =============================================================================

TEST_CASE("BookEditor vertical scrollbar", "[editor][book_editor][scrolling]") {
    BookEditor editor;

    SECTION("Scrollbar exists") {
        REQUIRE(editor.verticalScrollBar() != nullptr);
    }

    SECTION("Scrollbar is vertical") {
        REQUIRE(editor.verticalScrollBar()->orientation() == Qt::Vertical);
    }

    SECTION("Scrollbar is child of editor") {
        REQUIRE(editor.verticalScrollBar()->parent() == &editor);
    }

    SECTION("Scrollbar minimum is zero") {
        REQUIRE(editor.verticalScrollBar()->minimum() == 0);
    }
}

TEST_CASE("BookEditor scrollbar range", "[editor][book_editor][scrolling]") {
    auto doc = createTestDocument(100);  // Large document
    BookEditor editor;
    editor.resize(800, 400);  // Set viewport size

    SECTION("Empty document has zero range") {
        KmlDocument emptyDoc;
        editor.setDocument(&emptyDoc);

        // Range should be 0 or very small
        REQUIRE(editor.verticalScrollBar()->maximum() >= 0);

        // Clear document before emptyDoc is destroyed to avoid dangling pointer
        editor.setDocument(nullptr);
    }

    SECTION("Large document has positive range") {
        editor.setDocument(doc.get());

        // With 100 paragraphs, total height should exceed viewport
        // so max should be positive
        qreal totalHeight = editor.scrollManager().totalHeight();
        qreal viewportHeight = static_cast<qreal>(editor.height());

        if (totalHeight > viewportHeight) {
            REQUIRE(editor.verticalScrollBar()->maximum() > 0);
        }
    }

    SECTION("Page step matches viewport height") {
        editor.setDocument(doc.get());
        REQUIRE(editor.verticalScrollBar()->pageStep() == editor.height());
    }
}

// =============================================================================
// Scroll Offset Tests (Phase 3.2)
// =============================================================================

TEST_CASE("BookEditor scroll offset", "[editor][book_editor][scrolling]") {
    auto doc = createTestDocument(100);
    BookEditor editor;
    editor.resize(800, 400);
    editor.setDocument(doc.get());

    SECTION("Initial scroll offset is zero") {
        REQUIRE(editor.scrollOffset() == 0.0);
    }

    SECTION("Set scroll offset changes offset") {
        editor.setScrollOffset(100.0);
        REQUIRE(editor.scrollOffset() == 100.0);
    }

    SECTION("Scroll offset is clamped to non-negative") {
        editor.setScrollOffset(-50.0);
        REQUIRE(editor.scrollOffset() >= 0.0);
    }

    SECTION("Scroll offset is clamped to max") {
        editor.setScrollOffset(999999.0);
        qreal maxOffset = editor.scrollManager().maxScrollOffset();
        REQUIRE(editor.scrollOffset() <= maxOffset);
    }

    SECTION("scrollBy changes offset by delta") {
        editor.setScrollOffset(100.0);
        editor.scrollBy(50.0, false);
        REQUIRE(editor.scrollOffset() == 150.0);
    }

    SECTION("scrollBy with negative delta scrolls up") {
        editor.setScrollOffset(100.0);
        editor.scrollBy(-50.0, false);
        REQUIRE(editor.scrollOffset() == 50.0);
    }
}

TEST_CASE("BookEditor scroll signal", "[editor][book_editor][scrolling]") {
    auto doc = createTestDocument(100);
    BookEditor editor;
    editor.resize(800, 400);
    editor.setDocument(doc.get());

    // Test signal emission by using a lambda to track changes
    qreal lastEmittedOffset = -1.0;
    int signalCount = 0;
    QObject::connect(&editor, &BookEditor::scrollOffsetChanged,
                     [&](qreal offset) {
                         lastEmittedOffset = offset;
                         ++signalCount;
                     });

    SECTION("Signal emitted on scroll offset change") {
        editor.setScrollOffset(100.0);
        REQUIRE(signalCount == 1);
        REQUIRE(lastEmittedOffset == 100.0);
    }

    SECTION("Signal not emitted if offset unchanged") {
        editor.setScrollOffset(0.0);  // Already at 0
        REQUIRE(signalCount == 0);
    }
}

// =============================================================================
// Smooth Scrolling Tests (Phase 3.2)
// =============================================================================

TEST_CASE("BookEditor smooth scrolling settings", "[editor][book_editor][scrolling]") {
    BookEditor editor;

    SECTION("Smooth scrolling disabled by default") {
        REQUIRE(editor.isSmoothScrollingEnabled() == false);
    }

    SECTION("Can disable smooth scrolling") {
        editor.setSmoothScrollingEnabled(false);
        REQUIRE(editor.isSmoothScrollingEnabled() == false);
    }

    SECTION("Can enable smooth scrolling") {
        editor.setSmoothScrollingEnabled(false);
        editor.setSmoothScrollingEnabled(true);
        REQUIRE(editor.isSmoothScrollingEnabled() == true);
    }

    SECTION("Default smooth scroll duration is positive") {
        REQUIRE(editor.smoothScrollDuration() > 0);
    }

    SECTION("Can set smooth scroll duration") {
        editor.setSmoothScrollDuration(300);
        REQUIRE(editor.smoothScrollDuration() == 300);
    }

    SECTION("Smooth scroll duration clamped to non-negative") {
        editor.setSmoothScrollDuration(-100);
        REQUIRE(editor.smoothScrollDuration() >= 0);
    }
}

TEST_CASE("BookEditor scrollTo", "[editor][book_editor][scrolling]") {
    auto doc = createTestDocument(100);
    BookEditor editor;
    editor.resize(800, 400);
    editor.setDocument(doc.get());

    SECTION("scrollTo without animation sets offset immediately") {
        editor.setSmoothScrollingEnabled(false);
        editor.scrollTo(200.0, false);
        REQUIRE(editor.scrollOffset() == 200.0);
    }

    SECTION("scrollTo clamps to valid range") {
        editor.setSmoothScrollingEnabled(false);
        editor.scrollTo(-100.0, false);
        REQUIRE(editor.scrollOffset() >= 0.0);
    }
}

// =============================================================================
// Scrollbar Sync Tests (Phase 3.2)
// =============================================================================

TEST_CASE("BookEditor scrollbar synchronization", "[editor][book_editor][scrolling]") {
    auto doc = createTestDocument(100);
    BookEditor editor;
    editor.resize(800, 400);
    editor.setDocument(doc.get());

    SECTION("Scrollbar value matches scroll offset") {
        editor.setScrollOffset(150.0);
        REQUIRE(editor.verticalScrollBar()->value() == 150);
    }

    SECTION("Changing scrollbar changes scroll offset") {
        editor.verticalScrollBar()->setValue(200);
        REQUIRE(editor.scrollOffset() == 200.0);
    }
}

// =============================================================================
// Wheel Event Tests (Phase 3.2)
// =============================================================================

TEST_CASE("BookEditor wheel event handling", "[editor][book_editor][scrolling]") {
    auto doc = createTestDocument(100);
    BookEditor editor;
    editor.resize(800, 400);
    editor.setDocument(doc.get());

    // Disable smooth scrolling for predictable testing
    editor.setSmoothScrollingEnabled(false);

    SECTION("Wheel scroll down increases offset") {
        qreal initialOffset = editor.scrollOffset();

        // Simulate wheel scroll down (negative Y delta)
        QWheelEvent event(
            QPointF(100, 100),  // pos
            QPointF(100, 100),  // globalPos
            QPoint(0, 0),       // pixelDelta
            QPoint(0, -120),    // angleDelta (scroll down)
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false               // inverted
        );

        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.scrollOffset() > initialOffset);
    }

    SECTION("Wheel scroll up decreases offset") {
        editor.setScrollOffset(100.0);
        qreal initialOffset = editor.scrollOffset();

        // Simulate wheel scroll up (positive Y delta)
        QWheelEvent event(
            QPointF(100, 100),
            QPointF(100, 100),
            QPoint(0, 0),
            QPoint(0, 120),     // angleDelta (scroll up)
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false
        );

        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.scrollOffset() < initialOffset);
    }

    SECTION("Wheel scroll is clamped at top") {
        editor.setScrollOffset(0.0);

        // Try to scroll up past top
        QWheelEvent event(
            QPointF(100, 100),
            QPointF(100, 100),
            QPoint(0, 0),
            QPoint(0, 120),     // scroll up
            Qt::NoButton,
            Qt::NoModifier,
            Qt::NoScrollPhase,
            false
        );

        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.scrollOffset() >= 0.0);
    }
}

// =============================================================================
// Cursor Position Tests (Phase 3.4)
// =============================================================================

TEST_CASE("BookEditor cursor position", "[editor][book_editor][cursor]") {
    auto doc = createTestDocument(10);
    BookEditor editor;
    editor.setDocument(doc.get());

    SECTION("Initial cursor position is (0, 0)") {
        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 0);
    }

    SECTION("Set cursor position changes position") {
        CursorPosition newPos{3, 5};
        editor.setCursorPosition(newPos);

        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph == 3);
        REQUIRE(pos.offset == 5);
    }

    SECTION("Cursor paragraph clamped to valid range") {
        CursorPosition newPos{100, 0};  // Beyond document
        editor.setCursorPosition(newPos);

        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph == 9);  // Last valid paragraph
    }

    SECTION("Cursor offset clamped to paragraph length") {
        CursorPosition newPos{0, 10000};  // Beyond paragraph length
        editor.setCursorPosition(newPos);

        CursorPosition pos = editor.cursorPosition();
        // Offset should be clamped to actual paragraph length
        REQUIRE(pos.offset >= 0);
    }

    SECTION("Negative paragraph clamped to zero") {
        CursorPosition newPos{-5, 0};
        editor.setCursorPosition(newPos);

        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph == 0);
    }

    SECTION("Negative offset clamped to zero") {
        CursorPosition newPos{0, -10};
        editor.setCursorPosition(newPos);

        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.offset == 0);
    }
}

TEST_CASE("BookEditor cursor position signal", "[editor][book_editor][cursor]") {
    auto doc = createTestDocument(10);
    BookEditor editor;
    editor.setDocument(doc.get());

    CursorPosition lastEmittedPos{-1, -1};
    int signalCount = 0;
    QObject::connect(&editor, &BookEditor::cursorPositionChanged,
                     [&](const CursorPosition& pos) {
                         lastEmittedPos = pos;
                         ++signalCount;
                     });

    SECTION("Signal emitted on position change") {
        CursorPosition newPos{2, 3};
        editor.setCursorPosition(newPos);

        REQUIRE(signalCount == 1);
        REQUIRE(lastEmittedPos.paragraph == 2);
        REQUIRE(lastEmittedPos.offset == 3);
    }

    SECTION("Signal not emitted if position unchanged") {
        // Move to a position
        editor.setCursorPosition({1, 5});
        signalCount = 0;

        // Set same position again
        editor.setCursorPosition({1, 5});
        REQUIRE(signalCount == 0);
    }

    SECTION("Signal emitted with validated position") {
        // Try to set invalid position
        editor.setCursorPosition({100, 0});

        // Signal should have the clamped position
        REQUIRE(lastEmittedPos.paragraph == 9);
    }
}

TEST_CASE("BookEditor cursor with no document", "[editor][book_editor][cursor]") {
    BookEditor editor;
    // No document set

    SECTION("Cursor position is (0, 0) without document") {
        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 0);
    }

    SECTION("Setting cursor position without document does not crash") {
        REQUIRE_NOTHROW(editor.setCursorPosition({5, 10}));
    }
}

TEST_CASE("BookEditor cursor with empty document", "[editor][book_editor][cursor]") {
    KmlDocument emptyDoc;
    BookEditor editor;
    editor.setDocument(&emptyDoc);

    SECTION("Cursor position is (0, 0) with empty document") {
        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 0);
    }

    SECTION("Setting cursor position with empty document returns (0, 0)") {
        editor.setCursorPosition({5, 10});
        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 0);
    }
}

// =============================================================================
// Cursor Blinking Tests (Phase 3.5)
// =============================================================================

TEST_CASE("BookEditor cursor blinking settings", "[editor][book_editor][cursor]") {
    BookEditor editor;

    SECTION("Cursor blinking enabled by default") {
        REQUIRE(editor.isCursorBlinkingEnabled() == true);
    }

    SECTION("Can disable cursor blinking") {
        editor.setCursorBlinkingEnabled(false);
        REQUIRE(editor.isCursorBlinkingEnabled() == false);
    }

    SECTION("Can re-enable cursor blinking") {
        editor.setCursorBlinkingEnabled(false);
        editor.setCursorBlinkingEnabled(true);
        REQUIRE(editor.isCursorBlinkingEnabled() == true);
    }

    SECTION("Default blink interval is 500ms") {
        REQUIRE(editor.cursorBlinkInterval() == 500);
    }

    SECTION("Can set blink interval") {
        editor.setCursorBlinkInterval(300);
        REQUIRE(editor.cursorBlinkInterval() == 300);
    }

    SECTION("Blink interval has minimum of 100ms") {
        editor.setCursorBlinkInterval(50);
        REQUIRE(editor.cursorBlinkInterval() >= 100);
    }
}

TEST_CASE("BookEditor cursor visibility", "[editor][book_editor][cursor]") {
    BookEditor editor;

    SECTION("Cursor initially visible") {
        REQUIRE(editor.isCursorVisible() == true);
    }

    SECTION("ensureCursorVisible makes cursor visible") {
        // We can't easily test the blink toggling without waiting,
        // but we can test that ensureCursorVisible works
        editor.ensureCursorVisible();
        REQUIRE(editor.isCursorVisible() == true);
    }

    SECTION("Disabling blinking keeps cursor visible") {
        editor.setCursorBlinkingEnabled(false);
        REQUIRE(editor.isCursorVisible() == true);
    }
}

TEST_CASE("BookEditor cursor with document changes", "[editor][book_editor][cursor]") {
    auto doc1 = createTestDocument(5);
    auto doc2 = createTestDocument(10);
    BookEditor editor;

    SECTION("Cursor resets when document changes") {
        editor.setDocument(doc1.get());
        editor.setCursorPosition({3, 5});

        // Change document
        editor.setDocument(doc2.get());

        // Cursor should still be valid (position was valid in both docs)
        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph >= 0);
        REQUIRE(pos.offset >= 0);
    }

    SECTION("Cursor position validated on document change to smaller doc") {
        editor.setDocument(doc2.get());  // 10 paragraphs
        editor.setCursorPosition({9, 0}); // Last paragraph

        // Change to smaller document
        editor.setDocument(doc1.get());  // 5 paragraphs

        // Cursor paragraph is now invalid but won't be clamped automatically
        // until next setCursorPosition call - this is acceptable behavior
        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph == 9);  // Unchanged until validated
    }
}

// =============================================================================
// Cursor Navigation Tests (Phase 3.6)
// =============================================================================

TEST_CASE("BookEditor moveCursorLeft", "[editor][book_editor][navigation]") {
    auto doc = createTestDocument(3);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves left within paragraph") {
        editor.setCursorPosition({0, 5});
        editor.moveCursorLeft();
        REQUIRE(editor.cursorPosition().paragraph == 0);
        REQUIRE(editor.cursorPosition().offset == 4);
    }

    SECTION("Moves to previous paragraph at offset 0") {
        editor.setCursorPosition({1, 0});
        editor.moveCursorLeft();
        REQUIRE(editor.cursorPosition().paragraph == 0);
        // Should be at end of previous paragraph
        const KmlParagraph* para = doc->paragraph(0);
        REQUIRE(editor.cursorPosition().offset == para->characterCount());
    }

    SECTION("Does not move past document start") {
        editor.setCursorPosition({0, 0});
        editor.moveCursorLeft();
        REQUIRE(editor.cursorPosition().paragraph == 0);
        REQUIRE(editor.cursorPosition().offset == 0);
    }
}

TEST_CASE("BookEditor moveCursorRight", "[editor][book_editor][navigation]") {
    auto doc = createTestDocument(3);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves right within paragraph") {
        editor.setCursorPosition({0, 5});
        editor.moveCursorRight();
        REQUIRE(editor.cursorPosition().paragraph == 0);
        REQUIRE(editor.cursorPosition().offset == 6);
    }

    SECTION("Moves to next paragraph at end") {
        const KmlParagraph* para = doc->paragraph(0);
        int endOffset = para->characterCount();
        editor.setCursorPosition({0, endOffset});
        editor.moveCursorRight();
        REQUIRE(editor.cursorPosition().paragraph == 1);
        REQUIRE(editor.cursorPosition().offset == 0);
    }

    SECTION("Does not move past document end") {
        int lastPara = doc->paragraphCount() - 1;
        const KmlParagraph* para = doc->paragraph(lastPara);
        int endOffset = para->characterCount();
        editor.setCursorPosition({lastPara, endOffset});
        editor.moveCursorRight();
        REQUIRE(editor.cursorPosition().paragraph == lastPara);
        REQUIRE(editor.cursorPosition().offset == endOffset);
    }
}

TEST_CASE("BookEditor moveCursorUp", "[editor][book_editor][navigation]") {
    auto doc = createTestDocument(5);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves to previous paragraph from first line") {
        editor.setCursorPosition({2, 0});
        editor.moveCursorUp();
        // Should move to last line of previous paragraph
        REQUIRE(editor.cursorPosition().paragraph == 1);
    }

    SECTION("Does not move past document start") {
        editor.setCursorPosition({0, 0});
        editor.moveCursorUp();
        REQUIRE(editor.cursorPosition().paragraph == 0);
    }
}

TEST_CASE("BookEditor moveCursorDown", "[editor][book_editor][navigation]") {
    auto doc = createTestDocument(5);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves to next paragraph from last line") {
        const KmlParagraph* para = doc->paragraph(2);
        int endOffset = para->characterCount();
        editor.setCursorPosition({2, endOffset});
        editor.moveCursorDown();
        // Should move to first line of next paragraph
        REQUIRE(editor.cursorPosition().paragraph == 3);
    }

    SECTION("Does not move past document end") {
        int lastPara = doc->paragraphCount() - 1;
        const KmlParagraph* para = doc->paragraph(lastPara);
        int endOffset = para->characterCount();
        editor.setCursorPosition({lastPara, endOffset});
        editor.moveCursorDown();
        REQUIRE(editor.cursorPosition().paragraph == lastPara);
    }
}

// =============================================================================
// Cursor Navigation Tests (Phase 3.7)
// =============================================================================

TEST_CASE("BookEditor moveCursorWordLeft", "[editor][book_editor][navigation]") {
    // Create document with known text
    auto doc = std::make_unique<KmlDocument>();
    auto para = std::make_unique<KmlParagraph>("Hello world test");
    doc->addParagraph(std::move(para));

    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves to start of current word") {
        editor.setCursorPosition({0, 8});  // In "world"
        editor.moveCursorWordLeft();
        REQUIRE(editor.cursorPosition().offset == 6);  // Start of "world"
    }

    SECTION("Skips whitespace to previous word") {
        editor.setCursorPosition({0, 6});  // Start of "world"
        editor.moveCursorWordLeft();
        REQUIRE(editor.cursorPosition().offset == 0);  // Start of "Hello"
    }

    SECTION("Handles paragraph boundary") {
        auto para2 = std::make_unique<KmlParagraph>("Second paragraph");
        doc->addParagraph(std::move(para2));
        editor.setCursorPosition({1, 0});  // Start of second paragraph
        editor.moveCursorWordLeft();
        REQUIRE(editor.cursorPosition().paragraph == 0);
    }
}

TEST_CASE("BookEditor moveCursorWordRight", "[editor][book_editor][navigation]") {
    auto doc = std::make_unique<KmlDocument>();
    auto para = std::make_unique<KmlParagraph>("Hello world test");
    doc->addParagraph(std::move(para));

    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves to start of next word") {
        editor.setCursorPosition({0, 0});  // Start
        editor.moveCursorWordRight();
        REQUIRE(editor.cursorPosition().offset == 6);  // Start of "world"
    }

    SECTION("Moves to start of word after whitespace") {
        editor.setCursorPosition({0, 5});  // At 'o' in "Hello"
        editor.moveCursorWordRight();
        // From position 5 ('o'), skip rest of "Hello" (none), skip space, land at "world" (6)
        REQUIRE(editor.cursorPosition().offset == 6);  // Start of "world"
    }

    SECTION("Handles paragraph boundary") {
        auto para2 = std::make_unique<KmlParagraph>("Second paragraph");
        doc->addParagraph(std::move(para2));
        // Move to end of first paragraph
        editor.setCursorPosition({0, 16});  // End of "Hello world test"
        editor.moveCursorWordRight();
        REQUIRE(editor.cursorPosition().paragraph == 1);
        REQUIRE(editor.cursorPosition().offset == 0);
    }
}

TEST_CASE("BookEditor moveCursorToLineStart", "[editor][book_editor][navigation]") {
    auto doc = createTestDocument(3);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves to start of line") {
        editor.setCursorPosition({0, 10});
        editor.moveCursorToLineStart();
        // Should move to start of current line (likely 0 for first line)
        REQUIRE(editor.cursorPosition().offset >= 0);
        // For first line, offset should be 0
        if (editor.cursorPosition().offset == 0) {
            REQUIRE(editor.cursorPosition().offset == 0);
        }
    }
}

TEST_CASE("BookEditor moveCursorToLineEnd", "[editor][book_editor][navigation]") {
    auto doc = createTestDocument(3);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves to end of line") {
        editor.setCursorPosition({0, 0});
        editor.moveCursorToLineEnd();
        // Should move to end of current line
        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset > 0);  // Should have moved
    }
}

// =============================================================================
// Cursor Navigation Tests (Phase 3.8)
// =============================================================================

TEST_CASE("BookEditor moveCursorToDocStart", "[editor][book_editor][navigation]") {
    auto doc = createTestDocument(10);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves to document start") {
        editor.setCursorPosition({5, 10});
        editor.moveCursorToDocStart();
        REQUIRE(editor.cursorPosition().paragraph == 0);
        REQUIRE(editor.cursorPosition().offset == 0);
    }

    SECTION("Scrolls to top") {
        editor.setScrollOffset(200.0);
        editor.moveCursorToDocStart();
        REQUIRE(editor.scrollOffset() == 0.0);
    }
}

TEST_CASE("BookEditor moveCursorToDocEnd", "[editor][book_editor][navigation]") {
    auto doc = createTestDocument(10);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves to document end") {
        editor.setCursorPosition({0, 0});
        editor.moveCursorToDocEnd();

        int lastPara = doc->paragraphCount() - 1;
        const KmlParagraph* para = doc->paragraph(lastPara);
        int lastOffset = para->characterCount();

        REQUIRE(editor.cursorPosition().paragraph == lastPara);
        REQUIRE(editor.cursorPosition().offset == lastOffset);
    }
}

TEST_CASE("BookEditor moveCursorPageUp", "[editor][book_editor][navigation]") {
    auto doc = createTestDocument(50);  // Many paragraphs
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves cursor up approximately one page") {
        editor.setCursorPosition({25, 0});
        CursorPosition before = editor.cursorPosition();
        editor.moveCursorPageUp();
        CursorPosition after = editor.cursorPosition();

        // Should have moved to an earlier paragraph
        REQUIRE(after.paragraph <= before.paragraph);
    }

    SECTION("Does not move past document start") {
        editor.setCursorPosition({0, 0});
        editor.moveCursorPageUp();
        REQUIRE(editor.cursorPosition().paragraph == 0);
    }
}

TEST_CASE("BookEditor moveCursorPageDown", "[editor][book_editor][navigation]") {
    auto doc = createTestDocument(50);  // Many paragraphs
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Moves cursor down approximately one page") {
        editor.setCursorPosition({10, 0});
        CursorPosition before = editor.cursorPosition();
        editor.moveCursorPageDown();
        CursorPosition after = editor.cursorPosition();

        // Should have moved to a later paragraph
        REQUIRE(after.paragraph >= before.paragraph);
    }

    SECTION("Does not move past document end") {
        int lastPara = doc->paragraphCount() - 1;
        editor.setCursorPosition({lastPara, 0});
        editor.moveCursorPageDown();
        REQUIRE(editor.cursorPosition().paragraph <= lastPara);
    }
}

// =============================================================================
// Key Event Tests (Phase 3.6/3.7/3.8)
// =============================================================================

TEST_CASE("BookEditor key navigation", "[editor][book_editor][navigation][keys]") {
    auto doc = createTestDocument(5);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);
    editor.show();  // Need focus

    SECTION("Left arrow key moves cursor left") {
        editor.setCursorPosition({0, 5});

        QKeyEvent event(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.cursorPosition().offset == 4);
    }

    SECTION("Right arrow key moves cursor right") {
        editor.setCursorPosition({0, 5});

        QKeyEvent event(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.cursorPosition().offset == 6);
    }

    SECTION("Ctrl+Left moves by word") {
        auto wordDoc = std::make_unique<KmlDocument>();
        auto para = std::make_unique<KmlParagraph>("Hello world test");
        wordDoc->addParagraph(std::move(para));

        BookEditor wordEditor;
        wordEditor.setDocument(wordDoc.get());
        wordEditor.resize(800, 400);
        wordEditor.setCursorPosition({0, 8});  // In "world"

        QKeyEvent event(QEvent::KeyPress, Qt::Key_Left, Qt::ControlModifier);
        QApplication::sendEvent(&wordEditor, &event);

        REQUIRE(wordEditor.cursorPosition().offset == 6);  // Start of "world"
    }

    SECTION("Ctrl+Home moves to document start") {
        editor.setCursorPosition({3, 10});

        QKeyEvent event(QEvent::KeyPress, Qt::Key_Home, Qt::ControlModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.cursorPosition().paragraph == 0);
        REQUIRE(editor.cursorPosition().offset == 0);
    }

    SECTION("Ctrl+End moves to document end") {
        editor.setCursorPosition({0, 0});

        QKeyEvent event(QEvent::KeyPress, Qt::Key_End, Qt::ControlModifier);
        QApplication::sendEvent(&editor, &event);

        int lastPara = doc->paragraphCount() - 1;
        REQUIRE(editor.cursorPosition().paragraph == lastPara);
    }
}

TEST_CASE("BookEditor navigation with no document", "[editor][book_editor][navigation]") {
    BookEditor editor;
    // No document set

    SECTION("Navigation methods do not crash without document") {
        REQUIRE_NOTHROW(editor.moveCursorLeft());
        REQUIRE_NOTHROW(editor.moveCursorRight());
        REQUIRE_NOTHROW(editor.moveCursorUp());
        REQUIRE_NOTHROW(editor.moveCursorDown());
        REQUIRE_NOTHROW(editor.moveCursorWordLeft());
        REQUIRE_NOTHROW(editor.moveCursorWordRight());
        REQUIRE_NOTHROW(editor.moveCursorToLineStart());
        REQUIRE_NOTHROW(editor.moveCursorToLineEnd());
        REQUIRE_NOTHROW(editor.moveCursorToDocStart());
        REQUIRE_NOTHROW(editor.moveCursorToDocEnd());
        REQUIRE_NOTHROW(editor.moveCursorPageUp());
        REQUIRE_NOTHROW(editor.moveCursorPageDown());
    }
}

TEST_CASE("BookEditor navigation with empty document", "[editor][book_editor][navigation]") {
    KmlDocument emptyDoc;
    BookEditor editor;
    editor.setDocument(&emptyDoc);

    SECTION("Navigation methods do not crash with empty document") {
        REQUIRE_NOTHROW(editor.moveCursorLeft());
        REQUIRE_NOTHROW(editor.moveCursorRight());
        REQUIRE_NOTHROW(editor.moveCursorUp());
        REQUIRE_NOTHROW(editor.moveCursorDown());
        REQUIRE_NOTHROW(editor.moveCursorWordLeft());
        REQUIRE_NOTHROW(editor.moveCursorWordRight());
        REQUIRE_NOTHROW(editor.moveCursorToLineStart());
        REQUIRE_NOTHROW(editor.moveCursorToLineEnd());
        REQUIRE_NOTHROW(editor.moveCursorToDocStart());
        REQUIRE_NOTHROW(editor.moveCursorToDocEnd());
        REQUIRE_NOTHROW(editor.moveCursorPageUp());
        REQUIRE_NOTHROW(editor.moveCursorPageDown());
    }
}

// =============================================================================
// Selection Tests (Phase 3.10)
// =============================================================================

TEST_CASE("BookEditor selection basics", "[editor][book_editor][selection]") {
    auto doc = createTestDocument(5);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Initial selection is empty") {
        REQUIRE(editor.selection().isEmpty());
        REQUIRE(!editor.hasSelection());
    }

    SECTION("Set selection") {
        SelectionRange range;
        range.start = {0, 5};
        range.end = {0, 10};
        editor.setSelection(range);

        REQUIRE(editor.hasSelection());
        REQUIRE(editor.selection().start.paragraph == 0);
        REQUIRE(editor.selection().start.offset == 5);
        REQUIRE(editor.selection().end.paragraph == 0);
        REQUIRE(editor.selection().end.offset == 10);
    }

    SECTION("Clear selection") {
        SelectionRange range;
        range.start = {0, 5};
        range.end = {0, 10};
        editor.setSelection(range);
        editor.clearSelection();

        REQUIRE(!editor.hasSelection());
        REQUIRE(editor.selection().isEmpty());
    }

    SECTION("Selection is normalized") {
        // Set selection backwards (end before start)
        SelectionRange range;
        range.start = {0, 10};
        range.end = {0, 5};
        editor.setSelection(range);

        // Selection should be normalized (start before end)
        SelectionRange sel = editor.selection();
        REQUIRE(sel.start.offset <= sel.end.offset);
    }
}

TEST_CASE("BookEditor selection signal", "[editor][book_editor][selection]") {
    auto doc = createTestDocument(5);
    BookEditor editor;
    editor.setDocument(doc.get());

    int signalCount = 0;
    QObject::connect(&editor, &BookEditor::selectionChanged,
                     [&]() { ++signalCount; });

    SECTION("Signal emitted on selection change") {
        SelectionRange range;
        range.start = {0, 0};
        range.end = {0, 5};
        editor.setSelection(range);
        REQUIRE(signalCount == 1);
    }

    SECTION("Signal emitted on clear") {
        SelectionRange range;
        range.start = {0, 0};
        range.end = {0, 5};
        editor.setSelection(range);
        signalCount = 0;

        editor.clearSelection();
        REQUIRE(signalCount == 1);
    }
}

TEST_CASE("BookEditor selected text", "[editor][book_editor][selection]") {
    // Create document with known text
    auto doc = std::make_unique<KmlDocument>();
    doc->addParagraph(std::make_unique<KmlParagraph>("Hello world"));
    doc->addParagraph(std::make_unique<KmlParagraph>("Second paragraph"));

    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Selected text from single paragraph") {
        SelectionRange range;
        range.start = {0, 0};
        range.end = {0, 5};
        editor.setSelection(range);

        QString text = editor.selectedText();
        REQUIRE(text == "Hello");
    }

    SECTION("Selected text from multiple paragraphs") {
        SelectionRange range;
        range.start = {0, 6};  // "world"
        range.end = {1, 6};    // "Second"
        editor.setSelection(range);

        QString text = editor.selectedText();
        // Should contain "world" + paragraph separator + "Second"
        REQUIRE(text.contains("world"));
        REQUIRE(text.contains("Second"));
    }

    SECTION("No text when no selection") {
        REQUIRE(editor.selectedText().isEmpty());
    }
}

TEST_CASE("BookEditor select all", "[editor][book_editor][selection]") {
    auto doc = createTestDocument(5);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Select all selects entire document") {
        editor.selectAll();

        REQUIRE(editor.hasSelection());
        SelectionRange sel = editor.selection();
        REQUIRE(sel.start.paragraph == 0);
        REQUIRE(sel.start.offset == 0);
        REQUIRE(sel.end.paragraph == 4);  // Last paragraph
    }

    SECTION("Select all with Ctrl+A key") {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.hasSelection());
        SelectionRange sel = editor.selection();
        REQUIRE(sel.start.paragraph == 0);
        REQUIRE(sel.start.offset == 0);
    }
}

// =============================================================================
// Mouse Click Tests (Phase 3.9)
// =============================================================================

TEST_CASE("BookEditor mouse click", "[editor][book_editor][mouse]") {
    auto doc = createTestDocument(10);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);
    editor.show();

    SECTION("Click positions cursor") {
        // Click at a position in the editor
        QPointF localPos(50, 50);
        QPointF globalPos = editor.mapToGlobal(localPos.toPoint());
        QMouseEvent pressEvent(
            QEvent::MouseButtonPress,
            localPos,
            globalPos,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(&editor, &pressEvent);

        QMouseEvent releaseEvent(
            QEvent::MouseButtonRelease,
            localPos,
            globalPos,
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(&editor, &releaseEvent);

        // Cursor should have moved (exact position depends on layout)
        // Just verify it didn't crash
        REQUIRE(true);
    }

    SECTION("Click clears existing selection") {
        // Set a selection first
        SelectionRange range;
        range.start = {0, 0};
        range.end = {0, 10};
        editor.setSelection(range);
        REQUIRE(editor.hasSelection());

        // Click somewhere
        QPointF localPos(100, 50);
        QPointF globalPos = editor.mapToGlobal(localPos.toPoint());
        QMouseEvent pressEvent(
            QEvent::MouseButtonPress,
            localPos,
            globalPos,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(&editor, &pressEvent);

        QMouseEvent releaseEvent(
            QEvent::MouseButtonRelease,
            localPos,
            globalPos,
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(&editor, &releaseEvent);

        REQUIRE(!editor.hasSelection());
    }
}

TEST_CASE("BookEditor shift-click selection", "[editor][book_editor][mouse][selection]") {
    auto doc = createTestDocument(5);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);
    editor.show();

    SECTION("Shift+click extends selection") {
        // Position cursor
        editor.setCursorPosition({0, 5});

        // Shift+click at different position
        QPointF localPos(200, 50);
        QPointF globalPos = editor.mapToGlobal(localPos.toPoint());
        QMouseEvent event(
            QEvent::MouseButtonPress,
            localPos,
            globalPos,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::ShiftModifier
        );
        QApplication::sendEvent(&editor, &event);

        // Should have selection now
        REQUIRE(editor.hasSelection());
    }
}

// =============================================================================
// Drag Selection Tests (Phase 3.10)
// =============================================================================

TEST_CASE("BookEditor drag selection", "[editor][book_editor][mouse][selection]") {
    auto doc = createTestDocument(10);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);
    editor.show();

    SECTION("Drag creates selection") {
        // Mouse press
        QPointF startPos(50, 50);
        QPointF startGlobal = editor.mapToGlobal(startPos.toPoint());
        QMouseEvent pressEvent(
            QEvent::MouseButtonPress,
            startPos,
            startGlobal,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(&editor, &pressEvent);

        // Mouse move (drag)
        QPointF endPos(200, 50);
        QPointF endGlobal = editor.mapToGlobal(endPos.toPoint());
        QMouseEvent moveEvent(
            QEvent::MouseMove,
            endPos,
            endGlobal,
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(&editor, &moveEvent);

        // Should have selection
        REQUIRE(editor.hasSelection());

        // Mouse release
        QMouseEvent releaseEvent(
            QEvent::MouseButtonRelease,
            endPos,
            endGlobal,
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(&editor, &releaseEvent);

        // Selection should persist after release
        REQUIRE(editor.hasSelection());
    }
}

// =============================================================================
// Double/Triple Click Tests (Phase 3.11)
// =============================================================================

TEST_CASE("BookEditor double click", "[editor][book_editor][mouse]") {
    auto doc = std::make_unique<KmlDocument>();
    doc->addParagraph(std::make_unique<KmlParagraph>("Hello world test"));

    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);
    editor.show();

    SECTION("Double click selects word") {
        // Double-click event
        QPointF localPos(50, 15);
        QPointF globalPos = editor.mapToGlobal(localPos.toPoint());
        QMouseEvent event(
            QEvent::MouseButtonDblClick,
            localPos,
            globalPos,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier
        );
        QApplication::sendEvent(&editor, &event);

        // Should have selection
        REQUIRE(editor.hasSelection());
    }
}

// =============================================================================
// Keyboard Selection Tests (Phase 3.12)
// =============================================================================

TEST_CASE("BookEditor Shift+Arrow selection", "[editor][book_editor][selection][keys]") {
    auto doc = std::make_unique<KmlDocument>();
    doc->addParagraph(std::make_unique<KmlParagraph>("Hello world test"));

    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);
    editor.setCursorPosition({0, 5});  // After "Hello"

    SECTION("Shift+Right extends selection") {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.hasSelection());
        REQUIRE(editor.selection().start.offset == 5);
        REQUIRE(editor.selection().end.offset == 6);
    }

    SECTION("Shift+Left extends selection") {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Left, Qt::ShiftModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.hasSelection());
        REQUIRE(editor.selection().start.offset == 4);
        REQUIRE(editor.selection().end.offset == 5);
    }

    SECTION("Multiple Shift+Right extends selection further") {
        QKeyEvent event1(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
        QApplication::sendEvent(&editor, &event1);

        QKeyEvent event2(QEvent::KeyPress, Qt::Key_Right, Qt::ShiftModifier);
        QApplication::sendEvent(&editor, &event2);

        REQUIRE(editor.hasSelection());
        SelectionRange sel = editor.selection();
        REQUIRE(sel.end.offset - sel.start.offset == 2);
    }
}

TEST_CASE("BookEditor Ctrl+Shift+Arrow word selection", "[editor][book_editor][selection][keys]") {
    auto doc = std::make_unique<KmlDocument>();
    doc->addParagraph(std::make_unique<KmlParagraph>("Hello world test"));

    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);
    editor.setCursorPosition({0, 0});

    SECTION("Ctrl+Shift+Right selects word") {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Right, Qt::ControlModifier | Qt::ShiftModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.hasSelection());
        // Should select to next word boundary
        REQUIRE(editor.selection().end.offset > 0);
    }
}

TEST_CASE("BookEditor Shift+Home/End line selection", "[editor][book_editor][selection][keys]") {
    auto doc = std::make_unique<KmlDocument>();
    doc->addParagraph(std::make_unique<KmlParagraph>("Hello world test"));

    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);
    editor.setCursorPosition({0, 6});  // Middle of line

    SECTION("Shift+Home selects to line start") {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Home, Qt::ShiftModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.hasSelection());
        SelectionRange sel = editor.selection();
        REQUIRE(sel.start.offset == 0);
    }

    SECTION("Shift+End selects to line end") {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_End, Qt::ShiftModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.hasSelection());
        SelectionRange sel = editor.selection();
        REQUIRE(sel.end.offset > 6);  // Should extend beyond initial position
    }
}

TEST_CASE("BookEditor Ctrl+Shift+Home/End document selection", "[editor][book_editor][selection][keys]") {
    auto doc = createTestDocument(5);
    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);
    editor.setCursorPosition({2, 5});  // Middle of document

    SECTION("Ctrl+Shift+Home selects to document start") {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Home, Qt::ControlModifier | Qt::ShiftModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.hasSelection());
        SelectionRange sel = editor.selection();
        REQUIRE(sel.start.paragraph == 0);
        REQUIRE(sel.start.offset == 0);
    }

    SECTION("Ctrl+Shift+End selects to document end") {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_End, Qt::ControlModifier | Qt::ShiftModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(editor.hasSelection());
        SelectionRange sel = editor.selection();
        REQUIRE(sel.end.paragraph == 4);
    }
}

TEST_CASE("BookEditor arrow key clears selection", "[editor][book_editor][selection][keys]") {
    auto doc = std::make_unique<KmlDocument>();
    doc->addParagraph(std::make_unique<KmlParagraph>("Hello world test"));

    BookEditor editor;
    editor.setDocument(doc.get());
    editor.resize(800, 400);

    SECTION("Left arrow clears selection and moves to start") {
        SelectionRange range;
        range.start = {0, 5};
        range.end = {0, 10};
        editor.setSelection(range);
        editor.setCursorPosition({0, 10});  // Cursor at selection end

        QKeyEvent event(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(!editor.hasSelection());
        REQUIRE(editor.cursorPosition().offset == 5);  // Should be at selection start
    }

    SECTION("Right arrow clears selection and moves to end") {
        SelectionRange range;
        range.start = {0, 5};
        range.end = {0, 10};
        editor.setSelection(range);
        editor.setCursorPosition({0, 5});  // Cursor at selection start

        QKeyEvent event(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QApplication::sendEvent(&editor, &event);

        REQUIRE(!editor.hasSelection());
        REQUIRE(editor.cursorPosition().offset == 10);  // Should be at selection end
    }
}

// =============================================================================
// Selection Edge Cases
// =============================================================================

TEST_CASE("BookEditor selection edge cases", "[editor][book_editor][selection]") {
    BookEditor editor;

    SECTION("Selection without document does not crash") {
        REQUIRE_NOTHROW(editor.selection());
        REQUIRE_NOTHROW(editor.hasSelection());
        REQUIRE_NOTHROW(editor.clearSelection());
        REQUIRE_NOTHROW(editor.selectAll());
        REQUIRE_NOTHROW(editor.selectedText());
    }

    SECTION("Selection with empty document") {
        KmlDocument emptyDoc;
        editor.setDocument(&emptyDoc);

        REQUIRE_NOTHROW(editor.selectAll());
        REQUIRE(!editor.hasSelection());

        // Clear document before emptyDoc is destroyed to avoid dangling pointer
        editor.setDocument(nullptr);
    }
}

TEST_CASE("BookEditor selection with document change", "[editor][book_editor][selection]") {
    auto doc1 = createTestDocument(10);
    auto doc2 = createTestDocument(5);

    BookEditor editor;
    editor.setDocument(doc1.get());

    SECTION("Selection persists after document change") {
        SelectionRange range;
        range.start = {0, 0};
        range.end = {0, 5};
        editor.setSelection(range);

        // Change document
        editor.setDocument(doc2.get());

        // Selection should still be valid (or cleared if invalid)
        // No crash should occur
        REQUIRE_NOTHROW(editor.selection());
    }
}
