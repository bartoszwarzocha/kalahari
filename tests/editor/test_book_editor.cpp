/// @file test_book_editor.cpp
/// @brief Unit tests for BookEditor (OpenSpec #00042 Phase 3.1-3.5)
/// Phase 11: Rewritten for QTextDocument-based architecture

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
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

/// Create KML content with specified number of paragraphs
QString createTestKml(int paragraphCount) {
    QString kml;
    for (int i = 0; i < paragraphCount; ++i) {
        kml += QString("<p>Paragraph %1 with some text content for testing.</p>\n").arg(i);
    }
    return kml;
}

}  // anonymous namespace

// =============================================================================
// Constructor Tests (Phase 11)
// =============================================================================

TEST_CASE("BookEditor default constructor", "[editor][book_editor]") {
    BookEditor editor;

    SECTION("Initial paragraph count is minimal") {
        // Empty document has 0 paragraphs via paragraphCount()
        REQUIRE(editor.paragraphCount() <= 1);
    }

    SECTION("Plain text is empty") {
        REQUIRE(editor.plainText().isEmpty());
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
// Document Management Tests (Phase 11)
// =============================================================================

TEST_CASE("BookEditor fromKml", "[editor][book_editor]") {
    BookEditor editor;
    QString kml = createTestKml(10);

    SECTION("Load KML content") {
        editor.fromKml(kml);
        REQUIRE(editor.paragraphCount() == 10);
    }

    SECTION("Load empty KML") {
        editor.fromKml("");
        // Empty doc has minimal paragraphs
        REQUIRE(editor.paragraphCount() <= 1);
    }

    SECTION("Get plain text after loading") {
        editor.fromKml(createTestKml(3));
        QString text = editor.plainText();
        REQUIRE(text.contains("Paragraph 0"));
        REQUIRE(text.contains("Paragraph 1"));
        REQUIRE(text.contains("Paragraph 2"));
    }
}

TEST_CASE("BookEditor toKml", "[editor][book_editor]") {
    BookEditor editor;

    SECTION("Empty editor produces minimal KML") {
        QString kml = editor.toKml();
        // Empty or single empty paragraph
        REQUIRE(!kml.isNull());
    }

    SECTION("Round-trip KML") {
        QString originalKml = "<p>Test paragraph one.</p>\n<p>Test paragraph two.</p>\n";
        editor.fromKml(originalKml);
        QString roundTripped = editor.toKml();
        // Should contain the same text
        REQUIRE(roundTripped.contains("Test paragraph one"));
        REQUIRE(roundTripped.contains("Test paragraph two"));
    }
}

TEST_CASE("BookEditor changing content", "[editor][book_editor]") {
    BookEditor editor;

    SECTION("Can replace content") {
        editor.fromKml(createTestKml(5));
        REQUIRE(editor.paragraphCount() == 5);

        editor.fromKml(createTestKml(10));
        REQUIRE(editor.paragraphCount() == 10);
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

    SECTION("Size hint is at least minimum") {
        QSize minSize = editor.minimumSizeHint();
        QSize prefSize = editor.sizeHint();
        REQUIRE(prefSize.width() >= minSize.width());
        REQUIRE(prefSize.height() >= minSize.height());
    }
}

// =============================================================================
// Cursor Position Tests (Phase 11)
// =============================================================================

TEST_CASE("BookEditor cursor position", "[editor][book_editor]") {
    BookEditor editor;
    editor.fromKml(createTestKml(5));

    SECTION("Initial cursor at start") {
        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 0);
    }

    SECTION("Set cursor position") {
        editor.setCursorPosition({2, 5});
        CursorPosition pos = editor.cursorPosition();
        REQUIRE(pos.paragraph == 2);
        REQUIRE(pos.offset == 5);
    }

    SECTION("Cursor position clamped to valid range") {
        editor.setCursorPosition({100, 1000});
        CursorPosition pos = editor.cursorPosition();
        // Should be clamped to last paragraph
        REQUIRE(pos.paragraph < static_cast<int>(editor.paragraphCount()));
    }
}

// =============================================================================
// Selection Tests
// =============================================================================

TEST_CASE("BookEditor selection", "[editor][book_editor]") {
    BookEditor editor;
    editor.fromKml(createTestKml(5));

    SECTION("Initially no selection") {
        REQUIRE_FALSE(editor.hasSelection());
    }

    SECTION("Set selection") {
        SelectionRange sel;
        sel.start = {0, 0};
        sel.end = {0, 10};
        editor.setSelection(sel);
        REQUIRE(editor.hasSelection());
    }

    SECTION("Clear selection") {
        SelectionRange sel;
        sel.start = {0, 0};
        sel.end = {0, 10};
        editor.setSelection(sel);
        editor.clearSelection();
        REQUIRE_FALSE(editor.hasSelection());
    }

    SECTION("Get selected text") {
        editor.fromKml("<p>Hello World</p>");
        SelectionRange sel;
        sel.start = {0, 0};
        sel.end = {0, 5};
        editor.setSelection(sel);
        QString selected = editor.selectedText();
        REQUIRE(selected == "Hello");
    }
}

// =============================================================================
// Scroll Tests (Phase 11)
// =============================================================================

TEST_CASE("BookEditor scrolling", "[editor][book_editor]") {
    BookEditor editor;
    editor.resize(400, 300);
    editor.fromKml(createTestKml(100));  // Many paragraphs

    SECTION("Initial scroll offset is zero") {
        REQUIRE(editor.scrollOffset() == 0.0);
    }

    SECTION("Set scroll offset") {
        editor.setScrollOffset(100.0);
        REQUIRE(editor.scrollOffset() >= 0.0);
    }

    SECTION("Scroll offset clamped to valid range") {
        editor.setScrollOffset(-100.0);
        REQUIRE(editor.scrollOffset() >= 0.0);
    }
}

// =============================================================================
// Appearance Tests
// =============================================================================

TEST_CASE("BookEditor appearance", "[editor][book_editor]") {
    BookEditor editor;

    SECTION("Get default appearance") {
        EditorAppearance appearance = editor.appearance();
        // Should have some default values
        REQUIRE(appearance.typography.textFont.pointSize() > 0);
    }

    SECTION("Set appearance") {
        EditorAppearance appearance = editor.appearance();
        appearance.typography.textFont.setPointSize(16);
        editor.setAppearance(appearance);

        EditorAppearance retrieved = editor.appearance();
        REQUIRE(retrieved.typography.textFont.pointSize() == 16);
    }
}

// =============================================================================
// Text Editing Tests (Phase 11)
// =============================================================================

TEST_CASE("BookEditor text editing", "[editor][book_editor]") {
    BookEditor editor;

    SECTION("Insert text") {
        editor.fromKml("<p></p>");
        editor.setCursorPosition({0, 0});
        editor.insertText("Hello");
        REQUIRE(editor.plainText().contains("Hello"));
    }

    SECTION("Delete text") {
        editor.fromKml("<p>Hello World</p>");
        // Select "World"
        SelectionRange sel;
        sel.start = {0, 6};
        sel.end = {0, 11};
        editor.setSelection(sel);
        editor.deleteSelectedText();
        REQUIRE(editor.plainText() == "Hello ");
    }
}

// =============================================================================
// Paragraph Access Tests (Phase 11)
// =============================================================================

TEST_CASE("BookEditor paragraph access", "[editor][book_editor]") {
    BookEditor editor;
    editor.fromKml(createTestKml(5));

    SECTION("Paragraph count correct") {
        REQUIRE(editor.paragraphCount() == 5);
    }

    SECTION("Get paragraph text by index") {
        QString text = editor.paragraphPlainText(0);
        REQUIRE(text.contains("Paragraph 0"));
    }

    SECTION("Out of range paragraph returns empty") {
        QString text = editor.paragraphPlainText(100);
        REQUIRE(text.isEmpty());
    }

    SECTION("Character count is positive for non-empty doc") {
        REQUIRE(editor.characterCount() > 0);
    }
}
