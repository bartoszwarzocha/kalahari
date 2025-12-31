/// @file test_book_editor_integration.cpp
/// @brief Integration tests for BookEditor (OpenSpec #00042 Task 7.18)
/// Phase 11: Rewritten for QTextDocument-based architecture

#include <catch2/catch_test_macros.hpp>
#include <QApplication>
#include <QClipboard>
#include <QTextDocument>

#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/kml_parser.h"
#include "kalahari/editor/view_modes.h"
#include "kalahari/editor/editor_appearance.h"

using namespace kalahari::editor;

// ============================================================================
// Full Document Workflow: Create, Edit, Serialize, Parse
// ============================================================================

TEST_CASE("Integration: Full document workflow", "[integration][editor]") {
    BookEditor editor;

    SECTION("create, edit, serialize, parse round-trip") {
        // 1. Load content via fromKml
        QString kml = "<p>First paragraph with some text.</p>\n<p>Second paragraph here.</p>\n";
        editor.fromKml(kml);

        REQUIRE(editor.paragraphCount() == 2);

        // 2. Serialize to KML
        QString serialized = editor.toKml();
        REQUIRE_FALSE(serialized.isEmpty());
        REQUIRE(serialized.contains("First paragraph"));
        REQUIRE(serialized.contains("Second paragraph"));

        // 3. Parse KML back using KmlParser
        KmlParser parser;
        QTextDocument* parsedDoc = parser.parseKml(serialized);
        REQUIRE(parsedDoc != nullptr);
        REQUIRE(parsedDoc->blockCount() == 2);
        REQUIRE(parsedDoc->toPlainText().contains("First paragraph with some text."));
        REQUIRE(parsedDoc->toPlainText().contains("Second paragraph here."));
        delete parsedDoc;
    }

    SECTION("edit operations preserve content integrity") {
        // Start with content
        editor.fromKml("<p>Hello World</p>");

        // Edit via editor
        editor.setCursorPosition({0, 5}); // After "Hello"
        editor.insertText(" Beautiful");

        // Verify via BookEditor API
        REQUIRE(editor.paragraphPlainText(0) == "Hello Beautiful World");

        // Serialize and parse back
        QString kml = editor.toKml();
        KmlParser parser;
        QTextDocument* parsedDoc = parser.parseKml(kml);
        REQUIRE(parsedDoc != nullptr);
        REQUIRE(parsedDoc->toPlainText().contains("Hello Beautiful World"));
        delete parsedDoc;
    }
}

// ============================================================================
// Undo/Redo Chain
// ============================================================================

TEST_CASE("Integration: Undo/Redo chain", "[integration][editor][undo]") {
    BookEditor editor;

    // Start with empty paragraph
    editor.fromKml("<p></p>");

    SECTION("multiple operations with undo/redo") {
        // Type some text
        editor.setCursorPosition({0, 0});
        editor.insertText("A");
        editor.insertText("B");
        editor.insertText("C");

        REQUIRE(editor.paragraphPlainText(0) == "ABC");

        // Undo all
        while (editor.canUndo()) {
            editor.undo();
        }

        REQUIRE(editor.paragraphPlainText(0).isEmpty());

        // Redo all
        while (editor.canRedo()) {
            editor.redo();
        }

        REQUIRE(editor.paragraphPlainText(0) == "ABC");
    }

    SECTION("undo after delete") {
        editor.setCursorPosition({0, 0});
        editor.insertText("Hello");

        // Clear undo stack for clean test
        editor.clearUndoStack();

        // Delete character
        editor.setCursorPosition({0, 5});
        editor.deleteBackward();

        REQUIRE(editor.paragraphPlainText(0) == "Hell");

        // Undo delete
        editor.undo();
        REQUIRE(editor.paragraphPlainText(0) == "Hello");
    }

    SECTION("undo paragraph split") {
        editor.setCursorPosition({0, 0});
        editor.insertText("First line");
        editor.clearUndoStack();

        // Split paragraph (Enter)
        editor.setCursorPosition({0, 5});
        editor.insertNewline();

        REQUIRE(editor.paragraphCount() == 2);
        REQUIRE(editor.paragraphPlainText(0) == "First");
        REQUIRE(editor.paragraphPlainText(1) == " line");

        // Undo split
        editor.undo();
        REQUIRE(editor.paragraphCount() == 1);
        REQUIRE(editor.paragraphPlainText(0) == "First line");
    }
}

// ============================================================================
// Clipboard Round-trip (requires GUI environment - tested separately)
// ============================================================================

// Note: Clipboard tests require a running display/GUI environment.
// These tests are covered in test_clipboard_handler.cpp which tests
// the ClipboardHandler class directly without system clipboard.

// ============================================================================
// View Mode Switching
// ============================================================================

TEST_CASE("Integration: View mode switching", "[integration][editor][view_modes]") {
    BookEditor editor;

    // Load content via fromKml
    QString kml = "<p>Paragraph 1</p>\n<p>Paragraph 2</p>\n<p>Paragraph 3</p>\n"
                  "<p>Paragraph 4</p>\n<p>Paragraph 5</p>\n";
    editor.fromKml(kml);

    SECTION("switch between all view modes") {
        // Start in continuous mode
        REQUIRE(editor.viewMode() == ViewMode::Continuous);

        // Switch to page mode
        editor.setViewMode(ViewMode::Page);
        REQUIRE(editor.viewMode() == ViewMode::Page);

        // Switch to typewriter mode
        editor.setViewMode(ViewMode::Typewriter);
        REQUIRE(editor.viewMode() == ViewMode::Typewriter);

        // Switch to distraction-free mode
        editor.setViewMode(ViewMode::DistractionFree);
        REQUIRE(editor.viewMode() == ViewMode::DistractionFree);

        // Back to continuous
        editor.setViewMode(ViewMode::Continuous);
        REQUIRE(editor.viewMode() == ViewMode::Continuous);
    }

    SECTION("focus mode toggle via appearance") {
        // Focus mode is controlled via EditorAppearance
        auto appearance = editor.appearance();
        REQUIRE_FALSE(appearance.focusMode.enabled);

        appearance.focusMode.enabled = true;
        editor.setAppearance(appearance);
        REQUIRE(editor.appearance().focusMode.enabled);

        appearance.focusMode.enabled = false;
        editor.setAppearance(appearance);
        REQUIRE_FALSE(editor.appearance().focusMode.enabled);
    }

    SECTION("cursor position preserved across view mode changes") {
        CursorPosition pos{2, 5};
        editor.setCursorPosition(pos);

        // Change view mode
        editor.setViewMode(ViewMode::Page);

        // Cursor should be preserved
        REQUIRE(editor.cursorPosition().paragraph == 2);
        REQUIRE(editor.cursorPosition().offset == 5);
    }
}

// ============================================================================
// Complex Editing Scenarios
// ============================================================================

TEST_CASE("Integration: Complex editing scenarios", "[integration][editor]") {
    BookEditor editor;

    SECTION("multiple paragraph operations") {
        // Load 3 paragraphs via fromKml
        editor.fromKml("<p>Line 1</p>\n<p>Line 2</p>\n<p>Line 3</p>\n");

        REQUIRE(editor.paragraphCount() == 3);

        // Delete at start of paragraph 2 (merge with paragraph 1)
        editor.setCursorPosition({1, 0});
        editor.deleteBackward();

        REQUIRE(editor.paragraphCount() == 2);
        REQUIRE(editor.paragraphPlainText(0) == "Line 1Line 2");
    }

    SECTION("selection spanning multiple paragraphs") {
        editor.fromKml("<p>First</p>\n<p>Second</p>\n<p>Third</p>\n");

        // Select from middle of first to middle of third
        editor.setSelection({{0, 2}, {2, 3}});

        // Delete selection
        editor.deleteSelectedText();

        REQUIRE(editor.paragraphCount() == 1);
        REQUIRE(editor.paragraphPlainText(0) == "Fird");
    }
}
