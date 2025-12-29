/// @file test_book_editor_integration.cpp
/// @brief Integration tests for BookEditor (OpenSpec #00042 Task 7.18)

#include <catch2/catch_test_macros.hpp>
#include <QApplication>
#include <QClipboard>

#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/kml_document.h"
#include "kalahari/editor/kml_paragraph.h"
#include "kalahari/editor/kml_parser.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/view_modes.h"
#include "kalahari/editor/editor_appearance.h"

using namespace kalahari::editor;

// ============================================================================
// Full Document Workflow: Create, Edit, Serialize, Parse
// ============================================================================

TEST_CASE("Integration: Full document workflow", "[integration][editor]") {
    // Document MUST be declared before editor for correct destruction order
    KmlDocument doc;
    BookEditor editor;
    // Note: setDocument is called INSIDE each section AFTER adding content
    // because TextBuffer syncs on setDocument and won't re-sync if same document

    SECTION("create, edit, serialize, parse round-trip") {
        // 1. Create document with initial content
        auto para1 = std::make_unique<KmlParagraph>();
        para1->insertText(0, "First paragraph with some text.");
        doc.addParagraph(std::move(para1));

        auto para2 = std::make_unique<KmlParagraph>();
        para2->insertText(0, "Second paragraph here.");
        doc.addParagraph(std::move(para2));

        editor.setDocument(&doc);  // Set AFTER adding content

        REQUIRE(doc.paragraphCount() == 2);

        // 2. Serialize to KML
        QString kml = doc.toKml();
        REQUIRE_FALSE(kml.isEmpty());
        REQUIRE(kml.contains("First paragraph"));
        REQUIRE(kml.contains("Second paragraph"));

        // 3. Parse KML back
        KmlParser parser;
        auto result = parser.parseDocument(kml);
        REQUIRE(result.success);
        REQUIRE(result.result->paragraphCount() == 2);
        REQUIRE(result.result->paragraph(0)->plainText() == "First paragraph with some text.");
        REQUIRE(result.result->paragraph(1)->plainText() == "Second paragraph here.");
    }

    SECTION("edit operations preserve content integrity") {
        // Start with content - add paragraph BEFORE setDocument
        auto para = std::make_unique<KmlParagraph>();
        para->insertText(0, "Hello World");
        doc.addParagraph(std::move(para));

        editor.setDocument(&doc);

        // Edit via editor
        editor.setCursorPosition({0, 5}); // After "Hello"
        editor.insertText(" Beautiful");

        // Verify via BookEditor API (TextBuffer is source of truth)
        REQUIRE(editor.paragraphPlainText(0) == "Hello Beautiful World");

        // Serialize via editor's toKml() and parse
        QString kml = editor.toKml();
        KmlParser parser;
        auto result = parser.parseDocument(kml);
        REQUIRE(result.success);
        REQUIRE(result.result->paragraph(0)->plainText() == "Hello Beautiful World");
    }
}

// ============================================================================
// Undo/Redo Chain
// ============================================================================

TEST_CASE("Integration: Undo/Redo chain", "[integration][editor][undo]") {
    KmlDocument doc;
    BookEditor editor;

    // Add empty paragraph to doc BEFORE setDocument
    auto para = std::make_unique<KmlParagraph>();
    doc.addParagraph(std::move(para));

    editor.setDocument(&doc);

    SECTION("multiple operations with undo/redo") {
        // Type some text
        editor.setCursorPosition({0, 0});
        editor.insertText("A");
        editor.insertText("B");
        editor.insertText("C");

        // Use BookEditor's new API (TextBuffer is source of truth)
        REQUIRE(editor.paragraphPlainText(0) == "ABC");

        // Undo all
        editor.undo();
        // Note: Commands may be merged, so we might need multiple undos
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
    KmlDocument doc;
    BookEditor editor;

    // Add some content BEFORE setDocument (TextBuffer is source of truth)
    for (int i = 0; i < 5; i++) {
        auto para = std::make_unique<KmlParagraph>();
        para->insertText(0, QString("Paragraph %1").arg(i + 1));
        doc.addParagraph(std::move(para));
    }

    editor.setDocument(&doc);

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
    KmlDocument doc;
    BookEditor editor;

    SECTION("multiple paragraph operations") {
        // Create 3 paragraphs BEFORE setDocument
        auto p1 = std::make_unique<KmlParagraph>();
        p1->insertText(0, "Line 1");
        doc.addParagraph(std::move(p1));

        auto p2 = std::make_unique<KmlParagraph>();
        p2->insertText(0, "Line 2");
        doc.addParagraph(std::move(p2));

        auto p3 = std::make_unique<KmlParagraph>();
        p3->insertText(0, "Line 3");
        doc.addParagraph(std::move(p3));

        editor.setDocument(&doc);

        REQUIRE(editor.paragraphCount() == 3);

        // Delete at start of paragraph 2 (merge with paragraph 1)
        editor.setCursorPosition({1, 0});
        editor.deleteBackward();

        // Use BookEditor's new API (TextBuffer is source of truth)
        REQUIRE(editor.paragraphCount() == 2);
        REQUIRE(editor.paragraphPlainText(0) == "Line 1Line 2");
    }

    SECTION("selection spanning multiple paragraphs") {
        auto p1 = std::make_unique<KmlParagraph>();
        p1->insertText(0, "First");
        doc.addParagraph(std::move(p1));

        auto p2 = std::make_unique<KmlParagraph>();
        p2->insertText(0, "Second");
        doc.addParagraph(std::move(p2));

        auto p3 = std::make_unique<KmlParagraph>();
        p3->insertText(0, "Third");
        doc.addParagraph(std::move(p3));

        editor.setDocument(&doc);

        // Select from middle of first to middle of third
        editor.setSelection({{0, 2}, {2, 3}});

        // Delete selection
        editor.deleteSelectedText();

        REQUIRE(editor.paragraphCount() == 1);
        REQUIRE(editor.paragraphPlainText(0) == "Fird");
    }
}
