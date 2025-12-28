/// @file test_snapshot_roundtrip.cpp
/// @brief Snapshot round-trip tests for KML serialization (OpenSpec #00044 Task 9.15)
///
/// Tests verify that document content (text, formatting, comments, TODOs)
/// survives KML serialization/deserialization round-trip.

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/kml_converter.h>
#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/format_layer.h>

using namespace kalahari::editor;

// =============================================================================
// Plain Text Round-Trip Tests
// =============================================================================

TEST_CASE("KML round-trip preserves plain text", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Hello World\nSecond paragraph\nThird paragraph");

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);
    REQUIRE(!kml.isEmpty());

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.buffer->plainText() == buffer.plainText());
}

TEST_CASE("KML round-trip empty document", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    // Empty document
    buffer.setPlainText("");

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.buffer->plainText().isEmpty());
}

TEST_CASE("KML round-trip whitespace preservation", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("  Leading spaces\nTrailing spaces  \n  Both  ");

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.buffer->plainText() == buffer.plainText());
}

TEST_CASE("KML round-trip unicode text", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText(u8"Zażółć gęślą jaźń\nПривет мир\n你好世界");

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.buffer->plainText() == buffer.plainText());
}

// =============================================================================
// Formatting Round-Trip Tests
// =============================================================================

TEST_CASE("KML round-trip preserves formatting", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Bold and Italic text");

    // Add bold format to "Bold"
    TextFormat boldFormat;
    boldFormat.setBold(true);
    formatLayer.addFormat(0, 4, boldFormat);

    // Add italic format to "Italic"
    TextFormat italicFormat;
    italicFormat.setItalic(true);
    formatLayer.addFormat(9, 15, italicFormat);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);

    // Verify bold preserved
    auto formatsAtBold = result.formatLayer->getFormatsAt(2);
    REQUIRE(formatsAtBold.size() >= 1);
    bool hasBold = false;
    for (const auto& f : formatsAtBold) {
        if (f.format.hasFlag(FormatType::Bold)) hasBold = true;
    }
    REQUIRE(hasBold);

    // Verify italic preserved
    auto formatsAtItalic = result.formatLayer->getFormatsAt(10);
    REQUIRE(formatsAtItalic.size() >= 1);
    bool hasItalic = false;
    for (const auto& f : formatsAtItalic) {
        if (f.format.hasFlag(FormatType::Italic)) hasItalic = true;
    }
    REQUIRE(hasItalic);
}

TEST_CASE("KML round-trip preserves underline", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Underlined text here");

    TextFormat underlineFormat;
    underlineFormat.setUnderline(true);
    formatLayer.addFormat(0, 10, underlineFormat);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.formatLayer->hasFormatAt(5, FormatType::Underline));
}

TEST_CASE("KML round-trip preserves strikethrough", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Strikethrough text");

    TextFormat strikeFormat;
    strikeFormat.setStrikethrough(true);
    formatLayer.addFormat(0, 13, strikeFormat);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.formatLayer->hasFormatAt(5, FormatType::Strikethrough));
}

TEST_CASE("KML round-trip preserves nested formats", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Bold and italic text");

    // Bold from 0-4, Italic from 0-4 (both overlapping)
    TextFormat boldFormat;
    boldFormat.setBold(true);
    formatLayer.addFormat(0, 4, boldFormat);

    TextFormat italicFormat;
    italicFormat.setItalic(true);
    formatLayer.addFormat(0, 4, italicFormat);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);

    // Check both formats preserved at position 2
    REQUIRE(result.formatLayer->hasFormatAt(2, FormatType::Bold));
    REQUIRE(result.formatLayer->hasFormatAt(2, FormatType::Italic));
}

// =============================================================================
// Comments Round-Trip Tests
// =============================================================================

TEST_CASE("KML round-trip preserves comments", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Text with comment attached");

    TextComment comment;
    comment.id = "test-comment-1";
    comment.anchorStart = 5;
    comment.anchorEnd = 9;  // "with"
    comment.text = "This is a comment";
    comment.author = "Test Author";
    metadataLayer.addComment(comment);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.metadataLayer != nullptr);

    auto comments = result.metadataLayer->allComments();
    REQUIRE(comments.size() == 1);
    REQUIRE(comments[0].text == "This is a comment");
    REQUIRE(comments[0].anchorStart == 5);
    REQUIRE(comments[0].anchorEnd == 9);
}

TEST_CASE("KML round-trip preserves multiple comments", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("First word second word third word");

    TextComment comment1;
    comment1.id = "c1";
    comment1.anchorStart = 0;
    comment1.anchorEnd = 5;
    comment1.text = "Comment on first";
    metadataLayer.addComment(comment1);

    TextComment comment2;
    comment2.id = "c2";
    comment2.anchorStart = 11;
    comment2.anchorEnd = 17;
    comment2.text = "Comment on second";
    metadataLayer.addComment(comment2);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.metadataLayer->allComments().size() == 2);
}

// =============================================================================
// TODO/Note Marker Round-Trip Tests
// =============================================================================

TEST_CASE("KML round-trip preserves TODO markers", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Line with TODO marker");

    TextTodo todo;
    todo.id = "todo-1";
    todo.position = 10;
    todo.text = "Fix this";
    todo.type = MarkerType::Todo;
    todo.completed = false;
    todo.priority = "high";
    metadataLayer.addTodo(todo);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.metadataLayer != nullptr);

    auto todos = result.metadataLayer->allTodos();
    REQUIRE(todos.size() == 1);
    REQUIRE(todos[0].type == MarkerType::Todo);
    REQUIRE(todos[0].position == 10);
    REQUIRE(todos[0].text == "Fix this");
    REQUIRE(todos[0].completed == false);
    REQUIRE(todos[0].priority == "high");
}

TEST_CASE("KML round-trip preserves NOTE markers", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Line with note marker");

    TextTodo note;
    note.id = "note-1";
    note.position = 5;
    note.text = "Important info";
    note.type = MarkerType::Note;
    metadataLayer.addTodo(note);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);

    auto markers = result.metadataLayer->getMarkersByType(MarkerType::Note);
    REQUIRE(markers.size() == 1);
    REQUIRE(markers[0].type == MarkerType::Note);
    REQUIRE(markers[0].text == "Important info");
}

TEST_CASE("KML round-trip preserves completed TODO state", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Completed task");

    TextTodo todo;
    todo.id = "todo-completed";
    todo.position = 0;
    todo.text = "Done task";
    todo.type = MarkerType::Todo;
    todo.completed = true;
    metadataLayer.addTodo(todo);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);

    auto todos = result.metadataLayer->allTodos();
    REQUIRE(todos.size() == 1);
    REQUIRE(todos[0].completed == true);
}

TEST_CASE("KML round-trip multiple markers same position", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Line with multiple markers");

    TextTodo todo1;
    todo1.id = "t1";
    todo1.position = 5;
    todo1.text = "First";
    todo1.type = MarkerType::Todo;

    TextTodo todo2;
    todo2.id = "t2";
    todo2.position = 5;
    todo2.text = "Second";
    todo2.type = MarkerType::Note;

    metadataLayer.addTodo(todo1);
    metadataLayer.addTodo(todo2);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.metadataLayer->allTodos().size() == 2);
}

// =============================================================================
// Complex Document Round-Trip Tests
// =============================================================================

TEST_CASE("KML round-trip complex document", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    // Create document with multiple paragraphs
    buffer.setPlainText("Chapter One\nThis is bold text.\nAnother paragraph.\nFinal line.");

    // Add formatting
    TextFormat bold;
    bold.setBold(true);
    formatLayer.addFormat(20, 24, bold);  // "bold"

    // Add comment
    TextComment comment;
    comment.id = "c1";
    comment.anchorStart = 0;
    comment.anchorEnd = 11;  // "Chapter One"
    comment.text = "Review title";
    metadataLayer.addComment(comment);

    // Add TODO
    TextTodo todo;
    todo.id = "t1";
    todo.position = 32;
    todo.text = "Check grammar";
    todo.type = MarkerType::Todo;
    metadataLayer.addTodo(todo);

    // Add NOTE
    TextTodo note;
    note.id = "n1";
    note.position = 50;
    note.text = "Good ending";
    note.type = MarkerType::Note;
    metadataLayer.addTodo(note);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);

    // Verify all data preserved
    REQUIRE(result.buffer->plainText() == buffer.plainText());
    REQUIRE(result.metadataLayer->allComments().size() == 1);
    REQUIRE(result.metadataLayer->allTodos().size() == 2);

    auto todos = result.metadataLayer->getMarkersByType(MarkerType::Todo);
    auto notes = result.metadataLayer->getMarkersByType(MarkerType::Note);
    REQUIRE(todos.size() == 1);
    REQUIRE(notes.size() == 1);
}

TEST_CASE("KML round-trip document with all format types", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Bold Italic Underline Strike Sub Super");

    TextFormat bold;
    bold.setBold(true);
    formatLayer.addFormat(0, 4, bold);

    TextFormat italic;
    italic.setItalic(true);
    formatLayer.addFormat(5, 11, italic);

    TextFormat underline;
    underline.setUnderline(true);
    formatLayer.addFormat(12, 21, underline);

    TextFormat strike;
    strike.setStrikethrough(true);
    formatLayer.addFormat(22, 28, strike);

    // Note: Subscript and Superscript use flags directly
    TextFormat sub;
    sub.flags = FormatType::Subscript;
    formatLayer.addFormat(29, 32, sub);

    TextFormat super;
    super.flags = FormatType::Superscript;
    formatLayer.addFormat(33, 38, super);

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.buffer->plainText() == buffer.plainText());

    // Verify all format types
    REQUIRE(result.formatLayer->hasFormatAt(2, FormatType::Bold));
    REQUIRE(result.formatLayer->hasFormatAt(7, FormatType::Italic));
    REQUIRE(result.formatLayer->hasFormatAt(15, FormatType::Underline));
    REQUIRE(result.formatLayer->hasFormatAt(25, FormatType::Strikethrough));
    REQUIRE(result.formatLayer->hasFormatAt(30, FormatType::Subscript));
    REQUIRE(result.formatLayer->hasFormatAt(35, FormatType::Superscript));
}

TEST_CASE("KML round-trip special characters", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("<test> & \"quotes\" 'apostrophe'");

    KmlConverter converter;
    QString kml = converter.toKml(buffer, formatLayer, &metadataLayer);

    auto result = converter.parseKml(kml);
    REQUIRE(result.success);
    REQUIRE(result.buffer->plainText() == buffer.plainText());
}

TEST_CASE("KML round-trip multiple round-trips", "[snapshot][roundtrip]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    MetadataLayer metadataLayer;

    buffer.setPlainText("Test document for multiple round-trips");

    TextFormat bold;
    bold.setBold(true);
    formatLayer.addFormat(0, 4, bold);

    TextComment comment;
    comment.id = "c1";
    comment.anchorStart = 5;
    comment.anchorEnd = 13;
    comment.text = "Test comment";
    metadataLayer.addComment(comment);

    KmlConverter converter;

    // First round-trip
    QString kml1 = converter.toKml(buffer, formatLayer, &metadataLayer);
    auto result1 = converter.parseKml(kml1);
    REQUIRE(result1.success);

    // Second round-trip
    QString kml2 = converter.toKml(*result1.buffer, *result1.formatLayer, result1.metadataLayer.get());
    auto result2 = converter.parseKml(kml2);
    REQUIRE(result2.success);

    // Third round-trip
    QString kml3 = converter.toKml(*result2.buffer, *result2.formatLayer, result2.metadataLayer.get());
    auto result3 = converter.parseKml(kml3);
    REQUIRE(result3.success);

    // Verify content preserved after 3 round-trips
    REQUIRE(result3.buffer->plainText() == buffer.plainText());
    REQUIRE(result3.formatLayer->hasFormatAt(2, FormatType::Bold));
    REQUIRE(result3.metadataLayer->allComments().size() == 1);
}
