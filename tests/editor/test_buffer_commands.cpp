/// @file test_buffer_commands.cpp
/// @brief Unit tests for Buffer Commands (OpenSpec #00043 Phase 11.5)
///
/// Tests for the simplified QTextDocument-based undo/redo commands:
/// - Helper functions (position calculations)
/// - TextMarker serialization
/// - MarkerAddCommand / MarkerRemoveCommand / MarkerToggleCommand
/// - CompositeDocumentCommand
/// - Marker utility functions

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/editor/buffer_commands.h>
#include <QTextDocument>
#include <QTextCursor>
#include <QUndoStack>

using namespace kalahari::editor;

// =============================================================================
// Helper Functions Tests
// =============================================================================

TEST_CASE("Buffer command helper functions", "[editor][buffer_commands][helpers]") {
    QTextDocument document;
    document.setPlainText(QStringLiteral("Hello\nWorld\nTest"));

    // Document structure:
    // Block 0: "Hello" (positions 0-5, then \n at 5)
    // Block 1: "World" (positions 6-11, then \n at 11)
    // Block 2: "Test" (positions 12-15)

    SECTION("calculateAbsolutePosition - first block") {
        REQUIRE(calculateAbsolutePosition(&document, 0, 0) == 0);
        REQUIRE(calculateAbsolutePosition(&document, 0, 5) == 5);
    }

    SECTION("calculateAbsolutePosition - second block") {
        // Block 1 starts at position 6 (after "Hello\n")
        int pos10 = calculateAbsolutePosition(&document, 1, 0);
        int pos15 = calculateAbsolutePosition(&document, 1, 5);
        REQUIRE(pos10 == 6);
        REQUIRE(pos15 == 11);
    }

    SECTION("calculateAbsolutePosition - third block") {
        // Block 2 starts at position 12 (after "Hello\nWorld\n")
        int pos20 = calculateAbsolutePosition(&document, 2, 0);
        int pos24 = calculateAbsolutePosition(&document, 2, 4);
        REQUIRE(pos20 == 12);
        REQUIRE(pos24 == 16);
    }

    SECTION("calculateAbsolutePosition from CursorPosition") {
        CursorPosition pos{1, 3};
        REQUIRE(calculateAbsolutePosition(&document, pos) == 9);  // 6 + 3
    }

    SECTION("absoluteToCursorPosition - first block") {
        CursorPosition pos = absoluteToCursorPosition(&document, 0);
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 0);

        pos = absoluteToCursorPosition(&document, 3);
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 3);
    }

    SECTION("absoluteToCursorPosition - block boundary") {
        // Position 5 is at "o" in "Hello"
        CursorPosition pos = absoluteToCursorPosition(&document, 5);
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 5);

        // Position 6 is at "W" in "World" (start of block 1)
        pos = absoluteToCursorPosition(&document, 6);
        REQUIRE(pos.paragraph == 1);
        REQUIRE(pos.offset == 0);
    }

    SECTION("absoluteToCursorPosition - second block middle") {
        // Position 9 = 6 (start of block 1) + 3 = offset 3 in block 1
        CursorPosition pos = absoluteToCursorPosition(&document, 9);
        REQUIRE(pos.paragraph == 1);
        REQUIRE(pos.offset == 3);
    }

    SECTION("createCursor - single position") {
        CursorPosition pos{1, 2};
        QTextCursor cursor = createCursor(&document, pos);

        REQUIRE(cursor.position() == 8);  // 6 + 2
        REQUIRE(!cursor.hasSelection());
    }

    SECTION("createCursor - selection") {
        CursorPosition start{0, 0};
        CursorPosition end{0, 5};
        QTextCursor cursor = createCursor(&document, start, end);

        REQUIRE(cursor.hasSelection());
        REQUIRE(cursor.selectedText() == QStringLiteral("Hello"));
    }
}

// =============================================================================
// TextMarker Tests
// =============================================================================

TEST_CASE("TextMarker serialization", "[editor][buffer_commands][marker]") {
    SECTION("toJson and fromJson roundtrip") {
        TextMarker original;
        original.position = 42;
        original.length = 5;
        original.text = QStringLiteral("Fix this bug");
        original.type = MarkerType::Todo;
        original.completed = false;
        original.priority = QStringLiteral("high");
        original.id = QStringLiteral("test-uuid-123");
        original.timestamp = QStringLiteral("2024-01-15T10:30:00Z");

        QString json = original.toJson();
        REQUIRE(!json.isEmpty());

        auto restored = TextMarker::fromJson(json);
        REQUIRE(restored.has_value());
        REQUIRE(restored->position == original.position);
        REQUIRE(restored->length == original.length);
        REQUIRE(restored->text == original.text);
        REQUIRE(restored->type == original.type);
        REQUIRE(restored->completed == original.completed);
        REQUIRE(restored->priority == original.priority);
        REQUIRE(restored->id == original.id);
        REQUIRE(restored->timestamp == original.timestamp);
    }

    SECTION("fromJson with invalid JSON returns nullopt") {
        auto result = TextMarker::fromJson(QStringLiteral("not valid json"));
        REQUIRE(!result.has_value());
    }

    SECTION("fromJson with empty string returns nullopt") {
        auto result = TextMarker::fromJson(QString());
        REQUIRE(!result.has_value());
    }

    SECTION("generateId creates unique IDs") {
        QString id1 = TextMarker::generateId();
        QString id2 = TextMarker::generateId();
        QString id3 = TextMarker::generateId();

        REQUIRE(!id1.isEmpty());
        REQUIRE(!id2.isEmpty());
        REQUIRE(!id3.isEmpty());
        REQUIRE(id1 != id2);
        REQUIRE(id2 != id3);
        REQUIRE(id1 != id3);
    }

    SECTION("Note type serialization") {
        TextMarker note;
        note.type = MarkerType::Note;
        note.text = QStringLiteral("Just a note");
        note.id = TextMarker::generateId();

        QString json = note.toJson();
        auto restored = TextMarker::fromJson(json);

        REQUIRE(restored.has_value());
        REQUIRE(restored->type == MarkerType::Note);
    }
}

// =============================================================================
// MarkerAddCommand Tests
// =============================================================================

TEST_CASE("MarkerAddCommand basic operations", "[editor][buffer_commands]") {
    QTextDocument document;
    document.setPlainText(QStringLiteral("Hello World"));
    QUndoStack undoStack;

    SECTION("Add TODO marker") {
        TextMarker marker;
        marker.position = 0;
        marker.length = 5;
        marker.text = QStringLiteral("Check this");
        marker.type = MarkerType::Todo;
        marker.id = TextMarker::generateId();

        CursorPosition cursor{0, 0};
        undoStack.push(new MarkerAddCommand(&document, cursor, marker));

        // Verify marker was added
        auto markers = findAllMarkers(&document);
        REQUIRE(markers.size() == 1);
        REQUIRE(markers[0].text == QStringLiteral("Check this"));
        REQUIRE(markers[0].type == MarkerType::Todo);

        // Undo
        undoStack.undo();
        markers = findAllMarkers(&document);
        REQUIRE(markers.empty());

        // Redo
        undoStack.redo();
        markers = findAllMarkers(&document);
        REQUIRE(markers.size() == 1);
    }

    SECTION("Add Note marker") {
        TextMarker marker;
        marker.position = 6;
        marker.length = 5;
        marker.text = QStringLiteral("World is here");
        marker.type = MarkerType::Note;
        marker.id = TextMarker::generateId();

        CursorPosition cursor{0, 6};
        undoStack.push(new MarkerAddCommand(&document, cursor, marker));

        auto markers = findAllMarkers(&document, MarkerType::Note);
        REQUIRE(markers.size() == 1);
        REQUIRE(markers[0].type == MarkerType::Note);
    }

    SECTION("Cursor position preserved") {
        TextMarker marker;
        marker.position = 3;
        marker.length = 2;
        marker.id = TextMarker::generateId();

        CursorPosition cursor{0, 5};
        auto* cmd = new MarkerAddCommand(&document, cursor, marker);
        undoStack.push(cmd);

        REQUIRE(cmd->cursorBefore().paragraph == 0);
        REQUIRE(cmd->cursorBefore().offset == 5);
        REQUIRE(cmd->cursorAfter().paragraph == 0);
        REQUIRE(cmd->cursorAfter().offset == 5);
    }
}

// =============================================================================
// MarkerRemoveCommand Tests
// =============================================================================

TEST_CASE("MarkerRemoveCommand basic operations", "[editor][buffer_commands]") {
    QTextDocument document;
    document.setPlainText(QStringLiteral("Hello World"));
    QUndoStack undoStack;

    // First add a marker
    TextMarker marker;
    marker.position = 0;
    marker.length = 5;
    marker.text = QStringLiteral("Check this");
    marker.type = MarkerType::Todo;
    marker.id = TextMarker::generateId();
    setMarkerInDocument(&document, marker);

    SECTION("Remove marker") {
        CursorPosition cursor{0, 0};
        undoStack.push(new MarkerRemoveCommand(&document, cursor, marker));

        // Verify marker was removed
        auto markers = findAllMarkers(&document);
        REQUIRE(markers.empty());

        // Undo
        undoStack.undo();
        markers = findAllMarkers(&document);
        REQUIRE(markers.size() == 1);

        // Redo
        undoStack.redo();
        markers = findAllMarkers(&document);
        REQUIRE(markers.empty());
    }
}

// =============================================================================
// MarkerToggleCommand Tests
// =============================================================================

TEST_CASE("MarkerToggleCommand basic operations", "[editor][buffer_commands]") {
    QTextDocument document;
    document.setPlainText(QStringLiteral("Hello World"));
    QUndoStack undoStack;

    // First add a TODO marker
    TextMarker marker;
    marker.position = 0;
    marker.length = 5;
    marker.text = QStringLiteral("Fix this");
    marker.type = MarkerType::Todo;
    marker.completed = false;
    marker.id = TextMarker::generateId();
    setMarkerInDocument(&document, marker);

    SECTION("Toggle completes TODO") {
        CursorPosition cursor{0, 0};
        undoStack.push(new MarkerToggleCommand(&document, cursor, marker.id, marker.position));

        // Verify marker is now completed
        auto markers = findAllMarkers(&document);
        REQUIRE(markers.size() == 1);
        REQUIRE(markers[0].completed == true);

        // Undo
        undoStack.undo();
        markers = findAllMarkers(&document);
        REQUIRE(markers[0].completed == false);

        // Redo
        undoStack.redo();
        markers = findAllMarkers(&document);
        REQUIRE(markers[0].completed == true);
    }

    SECTION("Double toggle returns to original state") {
        CursorPosition cursor{0, 0};
        undoStack.push(new MarkerToggleCommand(&document, cursor, marker.id, marker.position));
        undoStack.push(new MarkerToggleCommand(&document, cursor, marker.id, marker.position));

        auto markers = findAllMarkers(&document);
        REQUIRE(markers.size() == 1);
        REQUIRE(markers[0].completed == false);
    }
}

// =============================================================================
// CompositeDocumentCommand Tests
// =============================================================================

TEST_CASE("CompositeDocumentCommand basic operations", "[editor][buffer_commands]") {
    QTextDocument document;
    document.setPlainText(QStringLiteral("Hello World"));
    QUndoStack undoStack;

    SECTION("Multiple marker operations as one undo step") {
        auto* composite = new CompositeDocumentCommand(
            &document, CursorPosition{0, 0}, QStringLiteral("Multiple Markers"));

        // Add two markers
        TextMarker marker1;
        marker1.position = 0;
        marker1.length = 5;
        marker1.text = QStringLiteral("First");
        marker1.type = MarkerType::Todo;
        marker1.id = TextMarker::generateId();

        TextMarker marker2;
        marker2.position = 6;
        marker2.length = 5;
        marker2.text = QStringLiteral("Second");
        marker2.type = MarkerType::Note;
        marker2.id = TextMarker::generateId();

        composite->addCommand(std::make_unique<MarkerAddCommand>(
            &document, CursorPosition{0, 0}, marker1));
        composite->addCommand(std::make_unique<MarkerAddCommand>(
            &document, CursorPosition{0, 6}, marker2));

        REQUIRE(composite->commandCount() == 2);

        undoStack.push(composite);

        // Both markers should exist
        auto markers = findAllMarkers(&document);
        REQUIRE(markers.size() == 2);

        // Single undo should remove both
        undoStack.undo();
        markers = findAllMarkers(&document);
        REQUIRE(markers.empty());

        // Single redo should add both
        undoStack.redo();
        markers = findAllMarkers(&document);
        REQUIRE(markers.size() == 2);
    }

    SECTION("Empty composite command") {
        auto* composite = new CompositeDocumentCommand(
            &document, CursorPosition{0, 0}, QStringLiteral("Empty"));

        REQUIRE(composite->commandCount() == 0);

        undoStack.push(composite);

        // Should not crash
        undoStack.undo();
        undoStack.redo();
    }
}

// =============================================================================
// Marker Utility Functions Tests
// =============================================================================

TEST_CASE("Marker utility functions", "[editor][buffer_commands][utilities]") {
    QTextDocument document;
    document.setPlainText(QStringLiteral("Line one\nLine two\nLine three"));

    // Add some markers
    TextMarker todo1;
    todo1.position = 0;
    todo1.length = 4;
    todo1.text = QStringLiteral("First TODO");
    todo1.type = MarkerType::Todo;
    todo1.id = QStringLiteral("todo-1");
    setMarkerInDocument(&document, todo1);

    TextMarker note1;
    note1.position = 9;
    note1.length = 4;
    note1.text = QStringLiteral("A note");
    note1.type = MarkerType::Note;
    note1.id = QStringLiteral("note-1");
    setMarkerInDocument(&document, note1);

    TextMarker todo2;
    todo2.position = 18;
    todo2.length = 4;
    todo2.text = QStringLiteral("Second TODO");
    todo2.type = MarkerType::Todo;
    todo2.id = QStringLiteral("todo-2");
    setMarkerInDocument(&document, todo2);

    SECTION("findAllMarkers - no filter") {
        auto markers = findAllMarkers(&document);
        REQUIRE(markers.size() == 3);
    }

    SECTION("findAllMarkers - TODO filter") {
        auto markers = findAllMarkers(&document, MarkerType::Todo);
        REQUIRE(markers.size() == 2);
        REQUIRE(markers[0].type == MarkerType::Todo);
        REQUIRE(markers[1].type == MarkerType::Todo);
    }

    SECTION("findAllMarkers - Note filter") {
        auto markers = findAllMarkers(&document, MarkerType::Note);
        REQUIRE(markers.size() == 1);
        REQUIRE(markers[0].type == MarkerType::Note);
    }

    SECTION("findMarkerById") {
        auto marker = findMarkerById(&document, QStringLiteral("note-1"));
        REQUIRE(marker.has_value());
        REQUIRE(marker->text == QStringLiteral("A note"));

        auto notFound = findMarkerById(&document, QStringLiteral("nonexistent"));
        REQUIRE(!notFound.has_value());
    }

    SECTION("findNextMarker") {
        auto next = findNextMarker(&document, 0);
        REQUIRE(next.has_value());
        REQUIRE(next->id == QStringLiteral("note-1"));

        next = findNextMarker(&document, 10);
        REQUIRE(next.has_value());
        REQUIRE(next->id == QStringLiteral("todo-2"));
    }

    SECTION("findNextMarker with type filter") {
        auto next = findNextMarker(&document, 0, MarkerType::Todo);
        REQUIRE(next.has_value());
        REQUIRE(next->id == QStringLiteral("todo-2"));
    }

    SECTION("findPreviousMarker") {
        // Position 20 is after todo-2 at 18, so previous should be todo-2
        auto prev = findPreviousMarker(&document, 20);
        REQUIRE(prev.has_value());
        REQUIRE(prev->id == QStringLiteral("todo-2"));
    }

    SECTION("setMarkerInDocument and removeMarkerFromDocument") {
        TextMarker newMarker;
        newMarker.position = 5;
        newMarker.length = 3;
        newMarker.text = QStringLiteral("New marker");
        newMarker.type = MarkerType::Todo;
        newMarker.id = QStringLiteral("new-marker");

        setMarkerInDocument(&document, newMarker);
        auto markers = findAllMarkers(&document);
        REQUIRE(markers.size() == 4);

        removeMarkerFromDocument(&document, 5);
        markers = findAllMarkers(&document);
        REQUIRE(markers.size() == 3);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("Buffer commands edge cases", "[editor][buffer_commands][edge]") {
    SECTION("Operations on empty document") {
        QTextDocument document;
        document.setPlainText(QString());
        QUndoStack undoStack;

        auto markers = findAllMarkers(&document);
        REQUIRE(markers.empty());

        auto next = findNextMarker(&document, 0);
        REQUIRE(!next.has_value());
    }

    SECTION("Null document handling") {
        REQUIRE(calculateAbsolutePosition(nullptr, 0, 0) == 0);
        REQUIRE(absoluteToCursorPosition(nullptr, 0).paragraph == 0);
        REQUIRE(!createCursor(nullptr, CursorPosition{0, 0}).isNull() == false);

        auto markers = findAllMarkers(nullptr);
        REQUIRE(markers.empty());
    }

    SECTION("Position beyond document bounds") {
        QTextDocument document;
        document.setPlainText(QStringLiteral("Short"));

        CursorPosition pos = absoluteToCursorPosition(&document, 1000);
        // Should clamp to end of document
        REQUIRE(pos.paragraph == 0);
    }
}

// =============================================================================
// Command ID Tests
// =============================================================================

TEST_CASE("Buffer command IDs", "[editor][buffer_commands][id]") {
    QTextDocument document;
    document.setPlainText(QStringLiteral("Test"));

    SECTION("MarkerAddCommand has correct ID") {
        TextMarker marker;
        marker.id = TextMarker::generateId();
        auto cmd = std::make_unique<MarkerAddCommand>(
            &document, CursorPosition{0, 0}, marker);
        REQUIRE(cmd->id() == static_cast<int>(BufferCommandId::MarkerAdd));
    }

    SECTION("MarkerRemoveCommand has correct ID") {
        TextMarker marker;
        marker.id = TextMarker::generateId();
        auto cmd = std::make_unique<MarkerRemoveCommand>(
            &document, CursorPosition{0, 0}, marker);
        REQUIRE(cmd->id() == static_cast<int>(BufferCommandId::MarkerRemove));
    }

    SECTION("MarkerToggleCommand has correct ID") {
        auto cmd = std::make_unique<MarkerToggleCommand>(
            &document, CursorPosition{0, 0}, QStringLiteral("id"), 0);
        REQUIRE(cmd->id() == static_cast<int>(BufferCommandId::MarkerToggle));
    }
}
