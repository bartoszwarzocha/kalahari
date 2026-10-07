/// @file test_buffer_commands.cpp
/// @brief Unit tests for the cursor position and marker helpers of buffer_commands.h
///
/// - Helper functions (position calculations)
/// - TextMarker serialization
/// - Marker utility functions

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/editor/buffer_commands.h>
#include <QTextDocument>
#include <QTextCursor>

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
    SECTION("toVariantMap and fromVariant roundtrip") {
        TextMarker original;
        original.text = QStringLiteral("Fix this bug");
        original.type = MarkerType::Todo;
        original.completed = true;
        original.priority = QStringLiteral("high");
        original.id = QStringLiteral("test-uuid-123");
        original.timestamp = QStringLiteral("2024-01-15T10:30:00Z");

        const QVariantMap map = original.toVariantMap();
        REQUIRE(map.value(QStringLiteral("id")).toString() == original.id);

        auto restored = TextMarker::fromVariant(map);
        REQUIRE(restored.has_value());
        REQUIRE(restored->text == original.text);
        REQUIRE(restored->type == original.type);
        REQUIRE(restored->completed == original.completed);
        REQUIRE(restored->priority == original.priority);
        REQUIRE(restored->id == original.id);
        REQUIRE(restored->timestamp == original.timestamp);
    }

    SECTION("map keys are the attributes of the KML todo tag") {
        TextMarker marker;
        marker.id = QStringLiteral("t1");
        marker.text = QStringLiteral("Check");
        marker.timestamp = QStringLiteral("2026-01-02T03:04:05");

        const QString attrs =
            KmlFormatRegistry::writeMetadataAttributes(QStringLiteral("todo"), marker.toVariantMap());
        REQUIRE(attrs == QStringLiteral(" id=\"t1\" text=\"Check\" created=\"2026-01-02T03:04:05\""));
    }

    SECTION("attributes without a field survive fromVariant and toVariantMap") {
        // Regression: only the fields were kept, so a toggled marker written back lost
        // every other attribute of its <todo> tag
        const QVariantMap loaded{{QStringLiteral("id"), QStringLiteral("t1")},
                                 {QStringLiteral("owner"), QStringLiteral("Ann")},
                                 {QStringLiteral("completed"), false}};
        auto marker = TextMarker::fromVariant(loaded);
        REQUIRE(marker.has_value());
        if (marker) {  // clang-tidy cannot see that a failed REQUIRE ends the test
            CHECK(marker->otherAttributes ==
                  QVariantMap{{QStringLiteral("owner"), QStringLiteral("Ann")}});
            marker->completed = true;
            const QVariantMap written = marker->toVariantMap();
            CHECK(written.value(QStringLiteral("owner")).toString() == QStringLiteral("Ann"));
            CHECK(written.value(QStringLiteral("id")).toString() == QStringLiteral("t1"));
            CHECK(written.value(QStringLiteral("completed")).toBool());
        }
    }

    SECTION("fromVariant with a non-map value returns nullopt") {
        REQUIRE(!TextMarker::fromVariant(QVariant()).has_value());
        REQUIRE(!TextMarker::fromVariant(QStringLiteral("{\"id\":\"x\"}")).has_value());
    }

    SECTION("a bare todo (empty map) is still a marker") {
        auto marker = TextMarker::fromVariant(QVariantMap());
        REQUIRE(marker.has_value());
        if (marker) {  // clang-tidy cannot see that a failed REQUIRE ends the test
            REQUIRE(marker->type == MarkerType::Todo);
            REQUIRE_FALSE(marker->completed);
        }
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

        auto restored = TextMarker::fromVariant(note.toVariantMap());

        REQUIRE(restored.has_value());
        REQUIRE(restored->type == MarkerType::Note);
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

        removeMarkerFromDocument(&document, newMarker.position, newMarker.length);
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
