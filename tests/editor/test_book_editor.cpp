/// @file test_book_editor.cpp
/// @brief Unit tests for BookEditor (OpenSpec #00042 Phase 3.1-3.5)
/// Phase 11: Rewritten for QTextDocument-based architecture

#include "editor_test_utils.h"

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/book_editor_accessible.h>
#include <QAccessible>
#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <memory>
#include <vector>

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

TEST_CASE("BookEditor toolbar font change with no selection leaves global appearance untouched",
          "[editor][book_editor][regression]") {
    // Regression: the toolbar font combo is a selection-only formatting control.
    // With no selection it must NOT mutate the editor's global default font
    // (m_appearance) — that font is owned solely by the settings dialog. When it
    // did, saving settings appeared to "revert" the font, because
    // applyEditorSettingsToAllPanels re-applied editor.fontFamily over the
    // unpersisted live change. See BookEditor::setSelectionFontFamily/Size.
    BookEditor editor;
    editor.fromKml("<p></p>");
    editor.setCursorPosition({0, 0});
    editor.insertText("Z");  // forces edit mode on
    REQUIRE(editor.plainText().contains("Z"));  // edit took effect => edit mode active
    editor.clearSelection();
    REQUIRE_FALSE(editor.hasSelection());

    QFont before = editor.appearance().typography.textFont;
    REQUIRE(before.pointSize() > 0);
    REQUIRE(before.family() != QStringLiteral("Comic Sans MS"));

    // With no selection, these must be no-ops for the global default font.
    editor.setSelectionFontFamily("Comic Sans MS");
    editor.setSelectionFontSize(before.pointSize() + 7);

    QFont after = editor.appearance().typography.textFont;
    CHECK(after.family() == before.family());
    CHECK(after.pointSize() == before.pointSize());
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

TEST_CASE("BookEditor is editable text for screen readers", "[editor][book_editor][accessibility]") {
    static const bool installed = [] {
        installBookEditorAccessibility();
        return true;
    }();
    REQUIRE(installed);

    BookEditor editor;
    editor.fromKml(QStringLiteral("<kml><p>Ala ma kota</p><p>Drugi</p></kml>"));

    QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(&editor);
    REQUIRE(iface != nullptr);
    CHECK(iface->role() == QAccessible::EditableText);
    QAccessibleTextInterface* text = iface->textInterface();
    REQUIRE(text != nullptr);
    CHECK(text->text(0, 3) == QStringLiteral("Ala"));
    CHECK(text->characterCount() == editor.plainText().length());

    // After an edit the screen reader gets the new text, not the text read before
    editor.setCursorPosition(CursorPosition{0, 0});
    editor.insertText(QStringLiteral("X"));
    CHECK(text->text(0, 4) == QStringLiteral("XAla"));
    CHECK(text->characterCount() == editor.plainText().length());
}

TEST_CASE("BookEditor context menu offers Select All also without a selection",
          "[editor][book_editor]") {
    BookEditor editor;
    editor.fromKml(kalahari::test::kmlOf({QStringLiteral("First line."), QStringLiteral("Last.")}));
    editor.resize(400, 300);
    editor.show();
    editor.setCursorPosition(CursorPosition{0, 3});
    REQUIRE_FALSE(editor.hasSelection());

    // As in the program's text fields: the whole text is selected from the menu
    const QStringList texts = kalahari::test::runPopupMenu(
        [&editor]() {
            QContextMenuEvent event(QContextMenuEvent::Keyboard, QPoint(), QPoint());
            QCoreApplication::sendEvent(&editor, &event);
        },
        QStringLiteral("Select All"));
    CHECK(texts.contains(QStringLiteral("Paste")));
    CHECK(texts.contains(QStringLiteral("Select All")));
    const SelectionRange all = editor.selection();
    CHECK(all.start == CursorPosition{0, 0});
    CHECK(all.end == CursorPosition{1, 5});
}

// =============================================================================
// Saved content
// =============================================================================

namespace {

/// A chapter with what KML can hold, written as a person would write it (indented): the
/// editor writes it otherwise, and the content is the same
QString richKml() {
    return QStringLiteral(
        "<kml>\n"
        "  <annotations>\n"
        "    <annotation id=\"a1\" kind=\"comment\" author=\"Bartosz\" "
        "created=\"2026-10-08T12:00:00Z\" done=\"true\">Check the date.</annotation>\n"
        "    <annotation id=\"a2\" kind=\"todo\">Describe the weather.</annotation>\n"
        "  </annotations>\n"
        "  <p align=\"center\"><b>Chapter One</b></p>\n"
        "  <p>Plain text, <i>italic</i>, <b><i>bold italic</i></b> and "
        "<span color=\"#aa0000\">red</span>.</p>\n"
        "  <p>She met <charref id=\"r1\" target=\"anna\">Anna</charref> in <locref id=\"r2\" "
        "target=\"krakow\">Krakow</locref> <anchor ref=\"a1\">on a Tuesday</anchor>.<anchor "
        "ref=\"a2\"/></p>\n"
        "</kml>");
}

}  // anonymous namespace

TEST_CASE("BookEditor: the content is modified only when it differs from the saved one",
          "[editor][book_editor][modified]") {
    BookEditor editor;
    editor.fromKml(richKml());
    std::vector<bool> said;  // what modifiedChanged() said, in order
    QObject::connect(&editor, &BookEditor::modifiedChanged,
                     [&said](bool modified) { said.push_back(modified); });
    // A content as long as the saved one is compared with it when the editing pauses
    const auto saysAtPause = [&said](bool modified) {
        return kalahari::test::waitUntil(
            [&said, modified]() { return !said.empty() && said.back() == modified; });
    };
    REQUIRE_FALSE(editor.isModified());

    SECTION("a letter typed and deleted") {
        editor.setCursorPosition({1, 5});
        editor.insertText(QStringLiteral("x"));
        CHECK(editor.isModified());
        CHECK(said == std::vector<bool>{true});

        editor.deleteBackward();
        CHECK_FALSE(editor.isModified());  // at once, while the comparison waits
        CHECK(saysAtPause(false));
        CHECK(said == std::vector<bool>{true, false});
    }

    SECTION("a sentence and a paragraph typed and deleted") {
        const CursorPosition end{1, static_cast<int>(editor.paragraphPlainText(1).length())};
        editor.setCursorPosition(end);
        editor.insertText(QStringLiteral(" Another sentence."));
        editor.insertNewline();
        editor.insertText(QStringLiteral("A paragraph of its own."));
        CHECK(editor.isModified());

        editor.setSelection({end, editor.cursorPosition()});
        editor.deleteSelectedText();
        CHECK_FALSE(editor.isModified());
        CHECK(saysAtPause(false));
    }

    SECTION("another letter in place of one, then the first one again") {
        editor.setSelection({{1, 0}, {1, 1}});
        editor.insertText(QStringLiteral("Q"));
        CHECK(editor.isModified());
        CHECK(saysAtPause(true));

        editor.setSelection({{1, 0}, {1, 1}});
        editor.insertText(QStringLiteral("P"));
        CHECK_FALSE(editor.isModified());
        CHECK(saysAtPause(false));
    }

    SECTION("formatting given and taken back") {
        editor.setSelection({{1, 0}, {1, 5}});
        editor.toggleBold();
        CHECK(editor.isModified());
        CHECK(saysAtPause(true));

        editor.toggleBold();
        CHECK_FALSE(editor.isModified());
        CHECK(saysAtPause(false));
    }

    SECTION("an annotation added and removed") {
        editor.setSelection({{1, 0}, {1, 5}});
        const Annotation added =
            editor.addAnnotation(AnnotationKind::Comment, QStringLiteral("Why?"), QString());
        CHECK(editor.isModified());
        CHECK(saysAtPause(true));

        REQUIRE(editor.removeAnnotation(added.id));
        CHECK_FALSE(editor.isModified());
        CHECK(saysAtPause(false));
    }

    SECTION("an annotation's text and state changed and changed back") {
        // As the annotation frame and the Annotations panel change them
        const std::vector<AnnotationPlace> places = editor.annotations();
        const auto first = std::find_if(places.begin(), places.end(), [](const auto& place) {
            return place.annotation.id == QStringLiteral("a1");
        });
        REQUIRE(first != places.end());
        Annotation annotation = first->annotation;
        const Annotation saved = annotation;

        annotation.text = QStringLiteral("Check the time.");
        annotation.done = false;
        REQUIRE(editor.updateAnnotation(annotation, false));
        CHECK(editor.isModified());
        CHECK(saysAtPause(true));

        REQUIRE(editor.updateAnnotation(saved, false));
        CHECK_FALSE(editor.isModified());
        CHECK(saysAtPause(false));
    }

    SECTION("an edit undone and redone") {
        editor.setCursorPosition({1, 5});
        editor.insertText(QStringLiteral("x"));
        editor.undo();
        CHECK_FALSE(editor.isModified());
        CHECK(said == std::vector<bool>{true, false});  // at once: no edit is left
        editor.redo();
        CHECK(editor.isModified());
        CHECK(said == std::vector<bool>{true, false, true});
    }

    SECTION("the content saved") {
        editor.setCursorPosition({1, 5});
        editor.insertText(QStringLiteral("x"));
        editor.setUnmodified();
        CHECK_FALSE(editor.isModified());
        CHECK(said == std::vector<bool>{true, false});

        // The content loaded is no longer the saved one
        editor.undo();
        CHECK(editor.isModified());
        editor.redo();
        CHECK_FALSE(editor.isModified());

        editor.deleteBackward();
        CHECK(editor.isModified());
        editor.insertText(QStringLiteral("x"));
        CHECK_FALSE(editor.isModified());
        CHECK(saysAtPause(false));
    }

    SECTION("other content loaded") {
        editor.setCursorPosition({1, 5});
        editor.insertText(QStringLiteral("x"));
        editor.fromKml(kalahari::test::kmlOf({QStringLiteral("Other text.")}));
        CHECK_FALSE(editor.isModified());
        CHECK(said == std::vector<bool>{true, false});
    }

    SECTION("a snapshot of the saved content restored") {
        const QString saved = editor.toKml();
        editor.setCursorPosition({1, 5});
        editor.insertText(QStringLiteral("x"));
        editor.replaceWithKml(saved);
        CHECK_FALSE(editor.isModified());
        CHECK(saysAtPause(false));
    }

    SECTION("the undo history cleared") {
        editor.setCursorPosition({1, 5});
        editor.insertText(QStringLiteral("x"));
        editor.clearUndoStack();
        CHECK(editor.isModified());

        // Undoing back to the start of the history does not bring the saved content back,
        editor.insertText(QStringLiteral("y"));
        editor.undo();
        CHECK(editor.isModified());

        // unless the content was the saved one then
        editor.deleteBackward();
        REQUIRE_FALSE(editor.isModified());
        editor.clearUndoStack();
        editor.insertText(QStringLiteral("y"));
        editor.undo();
        CHECK_FALSE(editor.isModified());
        CHECK(saysAtPause(false));
    }
}

TEST_CASE("BookEditor: text typed into an empty editor and deleted leaves it unmodified",
          "[editor][book_editor][modified]") {
    BookEditor editor;  // nothing loaded: the first edit makes an empty document
    CHECK_FALSE(editor.isModified());

    editor.insertText(QStringLiteral("a"));
    CHECK(editor.isModified());
    editor.deleteBackward();
    CHECK_FALSE(editor.isModified());
}
