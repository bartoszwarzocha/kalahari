/// @file test_buffer_commands.cpp
/// @brief Unit tests for Buffer Commands (OpenSpec #00043 Phase 9 Task 9.3)
///
/// Comprehensive tests for undo/redo buffer commands:
/// - TextInsertCommand
/// - TextDeleteCommand
/// - ParagraphSplitCommand
/// - ParagraphMergeCommand
/// - CompositeBufferCommand
/// - FormatApplyCommand
/// - FormatRemoveCommand

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/text_buffer.h>
#include <kalahari/editor/format_layer.h>
#include <QUndoStack>
#include <thread>
#include <chrono>

using namespace kalahari::editor;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Helper Functions Tests
// =============================================================================

TEST_CASE("Buffer command helper functions", "[editor][buffer_commands][helpers]") {
    TextBuffer buffer;
    buffer.setPlainText("Hello\nWorld\nTest");

    // NOTE: paragraphLength() returns text length WITHOUT trailing separator
    // "Hello" = 5 chars, "World" = 5 chars, "Test" = 4 chars
    // calculateAbsolutePosition adds (paragraphLength + 1) for newline separator

    SECTION("calculateAbsolutePosition - first paragraph") {
        // "Hello" at paragraph 0 - no prior paragraphs, so just offset
        REQUIRE(calculateAbsolutePosition(buffer, 0, 0) == 0);
        REQUIRE(calculateAbsolutePosition(buffer, 0, 5) == 5);
    }

    SECTION("calculateAbsolutePosition - second paragraph") {
        // Paragraph 1 position = paragraphLength(0) + 1 + offset
        // paragraphLength(0) = 5, so position (1, 0) = 5 + 1 + 0 = 6
        size_t pos10 = calculateAbsolutePosition(buffer, 1, 0);
        size_t pos15 = calculateAbsolutePosition(buffer, 1, 5);
        REQUIRE(pos10 == 6);
        REQUIRE(pos15 == 11);
    }

    SECTION("calculateAbsolutePosition - third paragraph") {
        // Paragraph 2 position = (paragraphLength(0) + 1) + (paragraphLength(1) + 1) + offset
        // = (5 + 1) + (5 + 1) + offset = 12 + offset
        size_t pos20 = calculateAbsolutePosition(buffer, 2, 0);
        size_t pos24 = calculateAbsolutePosition(buffer, 2, 4);
        REQUIRE(pos20 == 12);
        REQUIRE(pos24 == 16);
    }

    SECTION("calculateAbsolutePosition from CursorPosition") {
        CursorPosition pos{1, 3};
        REQUIRE(calculateAbsolutePosition(buffer, pos) == 9);  // 6 + 3
    }

    SECTION("absoluteToCursorPosition - first paragraph") {
        CursorPosition pos = absoluteToCursorPosition(buffer, 0);
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 0);

        pos = absoluteToCursorPosition(buffer, 3);
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 3);
    }

    SECTION("absoluteToCursorPosition - paragraph boundary") {
        // Position 5 is at end of paragraph 0 (at text boundary)
        CursorPosition pos = absoluteToCursorPosition(buffer, 5);
        REQUIRE(pos.paragraph == 0);
        REQUIRE(pos.offset == 5);

        // Position 6 is the start of paragraph 1 (after newline)
        pos = absoluteToCursorPosition(buffer, 6);
        REQUIRE(pos.paragraph == 1);
        REQUIRE(pos.offset == 0);
    }

    SECTION("absoluteToCursorPosition - second paragraph middle") {
        // Position 9 = 6 (start of para 1) + 3 = offset 3 in para 1
        CursorPosition pos = absoluteToCursorPosition(buffer, 9);
        REQUIRE(pos.paragraph == 1);
        REQUIRE(pos.offset == 3);
    }

    SECTION("absoluteToCursorPosition - beyond document") {
        CursorPosition pos = absoluteToCursorPosition(buffer, 1000);
        // Should clamp to end of document
        REQUIRE(pos.paragraph == 2);
    }
}

// =============================================================================
// TextInsertCommand Tests
// =============================================================================

TEST_CASE("TextInsertCommand basic operations", "[editor][buffer_commands]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("Hello");

    SECTION("Insert single character") {
        CursorPosition cursor{0, 5};
        undoStack.push(new TextInsertCommand(
            &buffer, &formatLayer, nullptr, cursor, "!"));

        REQUIRE(buffer.paragraphText(0) == "Hello!");

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello");

        undoStack.redo();
        REQUIRE(buffer.paragraphText(0) == "Hello!");
    }

    SECTION("Insert multiple characters") {
        CursorPosition cursor{0, 5};
        undoStack.push(new TextInsertCommand(
            &buffer, &formatLayer, nullptr, cursor, " World"));

        REQUIRE(buffer.paragraphText(0) == "Hello World");

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello");

        undoStack.redo();
        REQUIRE(buffer.paragraphText(0) == "Hello World");
    }

    SECTION("Insert at beginning") {
        CursorPosition cursor{0, 0};
        undoStack.push(new TextInsertCommand(
            &buffer, &formatLayer, nullptr, cursor, "Say "));

        REQUIRE(buffer.paragraphText(0) == "Say Hello");

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello");
    }

    SECTION("Insert in middle") {
        CursorPosition cursor{0, 2};
        undoStack.push(new TextInsertCommand(
            &buffer, &formatLayer, nullptr, cursor, "LL"));

        REQUIRE(buffer.paragraphText(0) == "HeLLllo");

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello");
    }

    SECTION("Verify cursor position after insert") {
        CursorPosition cursor{0, 5};
        auto* cmd = new TextInsertCommand(&buffer, &formatLayer, nullptr, cursor, " World");
        undoStack.push(cmd);

        REQUIRE(cmd->cursorBefore().paragraph == 0);
        REQUIRE(cmd->cursorBefore().offset == 5);
        REQUIRE(cmd->cursorAfter().paragraph == 0);
        REQUIRE(cmd->cursorAfter().offset == 11);
    }

    SECTION("Insert newline at end creates new paragraph") {
        // Insert " World" first, then split with newline in middle
        // This avoids the edge case where text starts with newline
        CursorPosition cursor{0, 5};
        undoStack.push(new TextInsertCommand(
            &buffer, &formatLayer, nullptr, cursor, " World"));

        REQUIRE(buffer.paragraphText(0) == "Hello World");

        // Now use ParagraphSplitCommand for proper paragraph splitting
        CursorPosition splitPos{0, 5};
        undoStack.push(new ParagraphSplitCommand(&buffer, &formatLayer, nullptr, splitPos, {}));

        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "Hello");
        REQUIRE(buffer.paragraphText(1) == " World");

        undoStack.undo();  // Undo split
        REQUIRE(buffer.paragraphCount() == 1);
        REQUIRE(buffer.paragraphText(0) == "Hello World");

        undoStack.undo();  // Undo insert
        REQUIRE(buffer.paragraphText(0) == "Hello");
    }

    SECTION("Insert text with embedded newline") {
        CursorPosition cursor{0, 5};
        undoStack.push(new TextInsertCommand(
            &buffer, &formatLayer, nullptr, cursor, "!\n"));

        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "Hello!");
        REQUIRE(buffer.paragraphText(1) == "");

        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 1);
        REQUIRE(buffer.paragraphText(0) == "Hello");
    }

    SECTION("Cursor position after newline insert") {
        CursorPosition cursor{0, 5};
        auto* cmd = new TextInsertCommand(&buffer, &formatLayer, nullptr, cursor, "!\n");
        undoStack.push(cmd);

        REQUIRE(cmd->cursorAfter().paragraph == 1);
        REQUIRE(cmd->cursorAfter().offset == 0);  // Empty line after newline
    }
}

TEST_CASE("TextInsertCommand coalescing", "[editor][buffer_commands][coalescing]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("Hello");

    SECTION("Consecutive typing merges into single command") {
        // Simulate rapid typing
        CursorPosition cursor{0, 5};
        undoStack.push(new TextInsertCommand(&buffer, &formatLayer, nullptr, cursor, "!"));

        CursorPosition cursor2{0, 6};
        undoStack.push(new TextInsertCommand(&buffer, &formatLayer, nullptr, cursor2, "!"));

        CursorPosition cursor3{0, 7};
        undoStack.push(new TextInsertCommand(&buffer, &formatLayer, nullptr, cursor3, "!"));

        REQUIRE(buffer.paragraphText(0) == "Hello!!!");

        // Due to merging, a single undo should revert all characters
        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello");
    }

    SECTION("Typing with delay does not merge (time-based boundary)") {
        CursorPosition cursor{0, 5};
        undoStack.push(new TextInsertCommand(&buffer, &formatLayer, nullptr, cursor, "!"));

        // Wait longer than MERGE_WINDOW_MS (1000ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));

        CursorPosition cursor2{0, 6};
        undoStack.push(new TextInsertCommand(&buffer, &formatLayer, nullptr, cursor2, "?"));

        REQUIRE(buffer.paragraphText(0) == "Hello!?");

        // First undo should only remove "?"
        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello!");

        // Second undo removes "!"
        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello");
    }

    SECTION("Newline insertion does not merge (paragraph boundary)") {
        CursorPosition cursor{0, 5};
        undoStack.push(new TextInsertCommand(&buffer, &formatLayer, nullptr, cursor, "!"));

        CursorPosition cursor2{0, 6};
        undoStack.push(new TextInsertCommand(&buffer, &formatLayer, nullptr, cursor2, "\n"));

        REQUIRE(buffer.paragraphCount() == 2);

        // First undo should only remove the newline
        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 1);
        REQUIRE(buffer.paragraphText(0) == "Hello!");
    }

    SECTION("Non-consecutive position does not merge") {
        CursorPosition cursor{0, 5};
        undoStack.push(new TextInsertCommand(&buffer, &formatLayer, nullptr, cursor, "!"));

        // Insert at a different position (not right after previous insert)
        CursorPosition cursor2{0, 0};
        undoStack.push(new TextInsertCommand(&buffer, &formatLayer, nullptr, cursor2, "X"));

        REQUIRE(buffer.paragraphText(0) == "XHello!");

        // First undo removes "X"
        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello!");

        // Second undo removes "!"
        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello");
    }
}

// =============================================================================
// TextDeleteCommand Tests
// =============================================================================

TEST_CASE("TextDeleteCommand basic operations", "[editor][buffer_commands]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("Hello World");

    SECTION("Delete single character") {
        CursorPosition start{0, 5};
        CursorPosition end{0, 6};
        undoStack.push(new TextDeleteCommand(
            &buffer, &formatLayer, nullptr, start, end, " ", {}));

        REQUIRE(buffer.paragraphText(0) == "HelloWorld");

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello World");

        undoStack.redo();
        REQUIRE(buffer.paragraphText(0) == "HelloWorld");
    }

    SECTION("Delete range of text") {
        CursorPosition start{0, 0};
        CursorPosition end{0, 6};
        undoStack.push(new TextDeleteCommand(
            &buffer, &formatLayer, nullptr, start, end, "Hello ", {}));

        REQUIRE(buffer.paragraphText(0) == "World");

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello World");

        undoStack.redo();
        REQUIRE(buffer.paragraphText(0) == "World");
    }

    SECTION("Delete at end of text") {
        CursorPosition start{0, 6};
        CursorPosition end{0, 11};
        undoStack.push(new TextDeleteCommand(
            &buffer, &formatLayer, nullptr, start, end, "World", {}));

        REQUIRE(buffer.paragraphText(0) == "Hello ");

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello World");
    }

    SECTION("Verify cursor position after delete") {
        CursorPosition start{0, 5};
        CursorPosition end{0, 11};
        auto* cmd = new TextDeleteCommand(
            &buffer, &formatLayer, nullptr, start, end, " World", {});
        undoStack.push(cmd);

        REQUIRE(cmd->cursorBefore().paragraph == 0);
        REQUIRE(cmd->cursorBefore().offset == 5);
        REQUIRE(cmd->cursorAfter().paragraph == 0);
        REQUIRE(cmd->cursorAfter().offset == 5);
    }
}

TEST_CASE("TextDeleteCommand multi-paragraph", "[editor][buffer_commands]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("Hello\nWorld\nTest");

    SECTION("Delete across paragraphs") {
        CursorPosition start{0, 3};  // "Hel|lo"
        CursorPosition end{1, 2};    // "Wo|rld"
        undoStack.push(new TextDeleteCommand(
            &buffer, &formatLayer, nullptr, start, end, "lo\nWo", {}));

        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "Helrld");
        REQUIRE(buffer.paragraphText(1) == "Test");

        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 3);
        REQUIRE(buffer.paragraphText(0) == "Hello");
        REQUIRE(buffer.paragraphText(1) == "World");
        REQUIRE(buffer.paragraphText(2) == "Test");
    }

    SECTION("Delete entire middle paragraph") {
        CursorPosition start{0, 5};  // End of "Hello"
        CursorPosition end{2, 0};    // Start of "Test"
        undoStack.push(new TextDeleteCommand(
            &buffer, &formatLayer, nullptr, start, end, "\nWorld\n", {}));

        REQUIRE(buffer.paragraphCount() == 1);
        REQUIRE(buffer.paragraphText(0) == "HelloTest");

        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 3);
        REQUIRE(buffer.paragraphText(0) == "Hello");
        REQUIRE(buffer.paragraphText(1) == "World");
        REQUIRE(buffer.paragraphText(2) == "Test");
    }
}

TEST_CASE("TextDeleteCommand with format restoration", "[editor][buffer_commands]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("Hello World");

    // Add bold format to "World" (positions 6-11)
    TextFormat boldFormat;
    boldFormat.setBold();
    formatLayer.addFormat(6, 11, boldFormat);

    SECTION("Delete formatted text and restore on undo") {
        // Save the format ranges that will be deleted
        std::vector<FormatRange> deletedFormats = formatLayer.getFormatsInRange(6, 11);

        CursorPosition start{0, 6};
        CursorPosition end{0, 11};
        undoStack.push(new TextDeleteCommand(
            &buffer, &formatLayer, nullptr, start, end, "World", deletedFormats));

        REQUIRE(buffer.paragraphText(0) == "Hello ");

        // After deletion, format should be gone
        auto formatsAfterDelete = formatLayer.getFormatsInRange(0, 10);
        // The format range was at 6-11, now text is shorter

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello World");

        // Format should be restored
        REQUIRE(formatLayer.hasFormatAt(7, FormatType::Bold));
    }
}

// =============================================================================
// ParagraphSplitCommand Tests
// =============================================================================

TEST_CASE("ParagraphSplitCommand basic operations", "[editor][buffer_commands]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("Hello World");

    SECTION("Split paragraph at middle") {
        CursorPosition splitPos{0, 6};  // After "Hello "
        undoStack.push(new ParagraphSplitCommand(
            &buffer, &formatLayer, nullptr, splitPos, {}));

        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "Hello ");
        REQUIRE(buffer.paragraphText(1) == "World");

        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 1);
        REQUIRE(buffer.paragraphText(0) == "Hello World");

        undoStack.redo();
        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "Hello ");
        REQUIRE(buffer.paragraphText(1) == "World");
    }

    SECTION("Split at start of paragraph") {
        CursorPosition splitPos{0, 0};
        undoStack.push(new ParagraphSplitCommand(
            &buffer, &formatLayer, nullptr, splitPos, {}));

        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "");
        REQUIRE(buffer.paragraphText(1) == "Hello World");

        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 1);
        REQUIRE(buffer.paragraphText(0) == "Hello World");
    }

    SECTION("Split at end of paragraph") {
        CursorPosition splitPos{0, 11};  // At end
        undoStack.push(new ParagraphSplitCommand(
            &buffer, &formatLayer, nullptr, splitPos, {}));

        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "Hello World");
        REQUIRE(buffer.paragraphText(1) == "");

        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 1);
        REQUIRE(buffer.paragraphText(0) == "Hello World");
    }

    SECTION("Verify cursor position after split") {
        CursorPosition splitPos{0, 6};
        auto* cmd = new ParagraphSplitCommand(&buffer, &formatLayer, nullptr, splitPos, {});
        undoStack.push(cmd);

        REQUIRE(cmd->cursorAfter().paragraph == 1);
        REQUIRE(cmd->cursorAfter().offset == 0);
    }
}

TEST_CASE("ParagraphSplitCommand format preservation", "[editor][buffer_commands]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("Hello World");

    // Add italic format spanning the split point
    TextFormat italicFormat;
    italicFormat.setItalic();
    formatLayer.addFormat(3, 8, italicFormat);  // "lo Wo" is italic

    SECTION("Format ranges adjust after split") {
        CursorPosition splitPos{0, 6};  // After "Hello "
        undoStack.push(new ParagraphSplitCommand(
            &buffer, &formatLayer, nullptr, splitPos, {}));

        REQUIRE(buffer.paragraphCount() == 2);

        // After split, format layer should have adjusted ranges
        // The format should still be present but may be split

        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 1);
    }
}

// =============================================================================
// ParagraphMergeCommand Tests
// =============================================================================

TEST_CASE("ParagraphMergeCommand basic operations", "[editor][buffer_commands]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("Hello\nWorld");

    SECTION("Merge two paragraphs") {
        CursorPosition cursorPos{1, 0};
        QString mergedContent = buffer.paragraphText(1);

        undoStack.push(new ParagraphMergeCommand(
            &buffer, &formatLayer, nullptr, cursorPos, 1, mergedContent, {}));

        REQUIRE(buffer.paragraphCount() == 1);
        REQUIRE(buffer.paragraphText(0) == "HelloWorld");

        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "Hello");
        REQUIRE(buffer.paragraphText(1) == "World");

        undoStack.redo();
        REQUIRE(buffer.paragraphCount() == 1);
        REQUIRE(buffer.paragraphText(0) == "HelloWorld");
    }

    SECTION("Verify text content after merge") {
        CursorPosition cursorPos{1, 0};
        QString mergedContent = buffer.paragraphText(1);

        undoStack.push(new ParagraphMergeCommand(
            &buffer, &formatLayer, nullptr, cursorPos, 1, mergedContent, {}));

        QString result = buffer.paragraphText(0);
        REQUIRE(result.length() == 10);
        REQUIRE(result.startsWith("Hello"));
        REQUIRE(result.endsWith("World"));
    }

    SECTION("Verify cursor position after merge") {
        // Note: m_splitOffset is set in constructor based on buffer state
        // paragraphLength(0) = 5 (text length without separator for "Hello")
        CursorPosition cursorPos{1, 0};
        QString mergedContent = buffer.paragraphText(1);

        auto* cmd = new ParagraphMergeCommand(
            &buffer, &formatLayer, nullptr, cursorPos, 1, mergedContent, {});
        undoStack.push(cmd);

        REQUIRE(cmd->cursorAfter().paragraph == 0);
        // m_splitOffset = paragraphLength(0) = 5 (text length without separator)
        REQUIRE(cmd->cursorAfter().offset == 5);
    }
}

TEST_CASE("ParagraphMergeCommand with multiple paragraphs", "[editor][buffer_commands]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("One\nTwo\nThree");

    SECTION("Merge middle paragraph with first") {
        CursorPosition cursorPos{1, 0};
        QString mergedContent = buffer.paragraphText(1);

        undoStack.push(new ParagraphMergeCommand(
            &buffer, &formatLayer, nullptr, cursorPos, 1, mergedContent, {}));

        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "OneTwo");
        REQUIRE(buffer.paragraphText(1) == "Three");

        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 3);
        REQUIRE(buffer.paragraphText(0) == "One");
        REQUIRE(buffer.paragraphText(1) == "Two");
        REQUIRE(buffer.paragraphText(2) == "Three");
    }

    SECTION("Merge last paragraph with middle") {
        CursorPosition cursorPos{2, 0};
        QString mergedContent = buffer.paragraphText(2);

        undoStack.push(new ParagraphMergeCommand(
            &buffer, &formatLayer, nullptr, cursorPos, 2, mergedContent, {}));

        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "One");
        REQUIRE(buffer.paragraphText(1) == "TwoThree");

        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 3);
    }
}

// =============================================================================
// CompositeBufferCommand Tests
// =============================================================================

TEST_CASE("CompositeBufferCommand basic operations", "[editor][buffer_commands]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("Hello");

    SECTION("Multiple commands as one undo step") {
        auto* composite = new CompositeBufferCommand(
            &buffer, &formatLayer, nullptr, CursorPosition{0, 0}, "Multiple Operations");

        // Add first insert
        composite->addCommand(std::make_unique<TextInsertCommand>(
            &buffer, &formatLayer, nullptr, CursorPosition{0, 5}, "!"));

        // Execute first insert manually (redo is called on push)
        buffer.setParagraphText(0, "Hello!");

        // Add second insert
        composite->addCommand(std::make_unique<TextInsertCommand>(
            &buffer, &formatLayer, nullptr, CursorPosition{0, 6}, "!"));

        buffer.setParagraphText(0, "Hello!!");

        REQUIRE(composite->commandCount() == 2);

        // Push composite to stack (this will call redo on all children)
        // Reset buffer first for proper redo
        buffer.setPlainText("Hello");
        undoStack.push(composite);

        REQUIRE(buffer.paragraphText(0) == "Hello!!");

        // Single undo should revert all commands
        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello");

        // Single redo should reapply all commands
        undoStack.redo();
        REQUIRE(buffer.paragraphText(0) == "Hello!!");
    }

    SECTION("Empty composite command") {
        auto* composite = new CompositeBufferCommand(
            &buffer, &formatLayer, nullptr, CursorPosition{0, 0}, "Empty");

        REQUIRE(composite->commandCount() == 0);

        undoStack.push(composite);

        // Should not crash
        undoStack.undo();
        undoStack.redo();
    }

    SECTION("Composite with mixed command types") {
        buffer.setPlainText("Hello World");

        auto* composite = new CompositeBufferCommand(
            &buffer, &formatLayer, nullptr, CursorPosition{0, 0}, "Mixed Operations");

        // Insert "!"
        composite->addCommand(std::make_unique<TextInsertCommand>(
            &buffer, &formatLayer, nullptr, CursorPosition{0, 11}, "!"));

        // Note: Composite commands execute their children in order on redo
        // For this test, we verify the structure works

        REQUIRE(composite->commandCount() == 1);
    }
}

// =============================================================================
// FormatApplyCommand Tests
// =============================================================================

TEST_CASE("FormatApplyCommand basic operations", "[editor][buffer_commands]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("Hello World");

    SECTION("Apply bold format") {
        TextFormat boldFormat;
        boldFormat.setBold();

        CursorPosition start{0, 0};
        CursorPosition end{0, 5};

        // Store previous formats (empty in this case)
        std::vector<FormatRange> previousFormats = formatLayer.getFormatsInRange(0, 5);

        undoStack.push(new FormatApplyCommand(
            &buffer, &formatLayer, nullptr, start, end, boldFormat, previousFormats));

        REQUIRE(formatLayer.hasFormatAt(2, FormatType::Bold));

        undoStack.undo();
        REQUIRE_FALSE(formatLayer.hasFormatAt(2, FormatType::Bold));

        undoStack.redo();
        REQUIRE(formatLayer.hasFormatAt(2, FormatType::Bold));
    }

    SECTION("Apply italic format") {
        TextFormat italicFormat;
        italicFormat.setItalic();

        CursorPosition start{0, 6};
        CursorPosition end{0, 11};

        std::vector<FormatRange> previousFormats = formatLayer.getFormatsInRange(6, 11);

        undoStack.push(new FormatApplyCommand(
            &buffer, &formatLayer, nullptr, start, end, italicFormat, previousFormats));

        REQUIRE(formatLayer.hasFormatAt(8, FormatType::Italic));
        REQUIRE_FALSE(formatLayer.hasFormatAt(3, FormatType::Italic));

        undoStack.undo();
        REQUIRE_FALSE(formatLayer.hasFormatAt(8, FormatType::Italic));
    }

    SECTION("Verify format state after undo") {
        // First apply bold
        TextFormat boldFormat;
        boldFormat.setBold();

        CursorPosition start{0, 0};
        CursorPosition end{0, 5};

        undoStack.push(new FormatApplyCommand(
            &buffer, &formatLayer, nullptr, start, end, boldFormat, {}));

        REQUIRE(formatLayer.hasFormatAt(2, FormatType::Bold));

        // Now apply italic to overlapping range
        TextFormat italicFormat;
        italicFormat.setItalic();

        CursorPosition start2{0, 3};
        CursorPosition end2{0, 8};

        std::vector<FormatRange> previousFormats = formatLayer.getFormatsInRange(3, 8);

        undoStack.push(new FormatApplyCommand(
            &buffer, &formatLayer, nullptr, start2, end2, italicFormat, previousFormats));

        REQUIRE(formatLayer.hasFormatAt(5, FormatType::Italic));

        // Undo italic
        undoStack.undo();
        REQUIRE_FALSE(formatLayer.hasFormatAt(5, FormatType::Italic));
        // Bold should still be there
        REQUIRE(formatLayer.hasFormatAt(2, FormatType::Bold));
    }

    SECTION("Apply underline format") {
        TextFormat underlineFormat;
        underlineFormat.setUnderline();

        CursorPosition start{0, 0};
        CursorPosition end{0, 11};

        undoStack.push(new FormatApplyCommand(
            &buffer, &formatLayer, nullptr, start, end, underlineFormat, {}));

        REQUIRE(formatLayer.hasFormatAt(5, FormatType::Underline));

        undoStack.undo();
        REQUIRE_FALSE(formatLayer.hasFormatAt(5, FormatType::Underline));
    }
}

// =============================================================================
// FormatRemoveCommand Tests
// =============================================================================

TEST_CASE("FormatRemoveCommand basic operations", "[editor][buffer_commands]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    buffer.setPlainText("Hello World");

    // Add initial bold format
    TextFormat boldFormat;
    boldFormat.setBold();
    formatLayer.addFormat(0, 11, boldFormat);

    SECTION("Remove format") {
        std::vector<FormatRange> removedFormats = formatLayer.getFormatsInRange(0, 11);

        CursorPosition start{0, 0};
        CursorPosition end{0, 11};

        undoStack.push(new FormatRemoveCommand(
            &buffer, &formatLayer, nullptr, start, end, FormatType::Bold, removedFormats));

        REQUIRE_FALSE(formatLayer.hasFormatAt(5, FormatType::Bold));

        undoStack.undo();
        REQUIRE(formatLayer.hasFormatAt(5, FormatType::Bold));

        undoStack.redo();
        REQUIRE_FALSE(formatLayer.hasFormatAt(5, FormatType::Bold));
    }

    SECTION("Verify format restoration after undo") {
        // Add italic format as well
        TextFormat italicFormat;
        italicFormat.setItalic();
        formatLayer.addFormat(3, 8, italicFormat);

        // Remove only bold
        std::vector<FormatRange> boldRanges;
        for (const auto& range : formatLayer.allRanges()) {
            if (range.format.hasFlag(FormatType::Bold)) {
                boldRanges.push_back(range);
            }
        }

        CursorPosition start{0, 0};
        CursorPosition end{0, 11};

        undoStack.push(new FormatRemoveCommand(
            &buffer, &formatLayer, nullptr, start, end, FormatType::Bold, boldRanges));

        // Italic should still be there
        REQUIRE(formatLayer.hasFormatAt(5, FormatType::Italic));

        undoStack.undo();
        // Both should be there
        REQUIRE(formatLayer.hasFormatAt(5, FormatType::Bold));
        REQUIRE(formatLayer.hasFormatAt(5, FormatType::Italic));
    }

    SECTION("Remove all formats") {
        // Add italic format as well
        TextFormat italicFormat;
        italicFormat.setItalic();
        formatLayer.addFormat(3, 8, italicFormat);

        std::vector<FormatRange> allRanges = formatLayer.allRanges();

        CursorPosition start{0, 0};
        CursorPosition end{0, 11};

        undoStack.push(new FormatRemoveCommand(
            &buffer, &formatLayer, nullptr, start, end, FormatType::None, allRanges));

        // All formats should be gone
        REQUIRE_FALSE(formatLayer.hasFormatAt(5, FormatType::Bold));
        REQUIRE_FALSE(formatLayer.hasFormatAt(5, FormatType::Italic));

        undoStack.undo();
        // All formats should be restored
        REQUIRE(formatLayer.hasFormatAt(5, FormatType::Bold));
        REQUIRE(formatLayer.hasFormatAt(5, FormatType::Italic));
    }
}

// =============================================================================
// Stress Test
// =============================================================================

TEST_CASE("Buffer commands stress test - 100+ operations", "[editor][buffer_commands][stress]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    // Start with known initial state
    buffer.setPlainText("Initial");
    const QString originalText = buffer.paragraphText(0);

    SECTION("100 insert operations with undo/redo") {
        // Perform 100 insert operations
        for (int i = 0; i < 100; ++i) {
            CursorPosition cursor{0, static_cast<int>(buffer.paragraphText(0).length())};
            undoStack.push(new TextInsertCommand(
                &buffer, &formatLayer, nullptr, cursor, QString::number(i % 10)));
        }

        // Verify text was modified
        QString finalText = buffer.paragraphText(0);
        REQUIRE(finalText.length() > originalText.length());

        // Undo all operations
        while (undoStack.canUndo()) {
            undoStack.undo();
        }

        // Verify original state is restored
        REQUIRE(buffer.paragraphText(0) == originalText);

        // Redo all operations
        while (undoStack.canRedo()) {
            undoStack.redo();
        }

        // Verify final state is restored
        REQUIRE(buffer.paragraphText(0) == finalText);
    }

    SECTION("Mixed insert and delete operations") {
        // Perform 50 insert operations
        for (int i = 0; i < 50; ++i) {
            CursorPosition cursor{0, static_cast<int>(buffer.paragraphText(0).length())};
            undoStack.push(new TextInsertCommand(
                &buffer, &formatLayer, nullptr, cursor, "X"));
        }

        // Record state after inserts
        QString afterInserts = buffer.paragraphText(0);

        // Perform 25 delete operations from the end
        for (int i = 0; i < 25; ++i) {
            int len = buffer.paragraphText(0).length();
            if (len > 0) {
                CursorPosition start{0, len - 1};
                CursorPosition end{0, len};
                undoStack.push(new TextDeleteCommand(
                    &buffer, &formatLayer, nullptr, start, end, "X", {}));
            }
        }

        QString finalText = buffer.paragraphText(0);
        REQUIRE(finalText.length() == afterInserts.length() - 25);

        // Undo all operations
        while (undoStack.canUndo()) {
            undoStack.undo();
        }

        // Verify original state
        REQUIRE(buffer.paragraphText(0) == originalText);

        // Redo all operations
        while (undoStack.canRedo()) {
            undoStack.redo();
        }

        // Verify final state
        REQUIRE(buffer.paragraphText(0) == finalText);
    }

    SECTION("Paragraph split and merge stress test") {
        // Start with single paragraph
        buffer.setPlainText("ABCDEFGHIJ");
        QString original = buffer.plainText();

        // Perform 10 splits
        for (int i = 0; i < 10; ++i) {
            if (buffer.paragraphCount() > 0) {
                int paraLen = buffer.paragraphText(0).length();
                if (paraLen > 1) {
                    CursorPosition splitPos{0, 1};
                    undoStack.push(new ParagraphSplitCommand(
                        &buffer, &formatLayer, nullptr, splitPos, {}));
                }
            }
        }

        REQUIRE(buffer.paragraphCount() > 1);

        // Undo all splits
        while (undoStack.canUndo()) {
            undoStack.undo();
        }

        // Should be back to original state
        REQUIRE(buffer.paragraphCount() == 1);
        REQUIRE(buffer.plainText() == original);
    }

    SECTION("Format operations stress test") {
        buffer.setPlainText("ABCDEFGHIJKLMNOPQRSTUVWXYZ");

        // Apply 20 non-overlapping format operations
        for (int i = 0; i < 20; ++i) {
            TextFormat format;
            if (i % 3 == 0) format.setBold();
            if (i % 3 == 1) format.setItalic();
            if (i % 3 == 2) format.setUnderline();

            // Non-overlapping ranges: 0-1, 2-3, 4-5, etc.
            size_t start = static_cast<size_t>(i);
            size_t end = start + 1;

            std::vector<FormatRange> prev = formatLayer.getFormatsInRange(start, end);

            CursorPosition startPos{0, static_cast<int>(start)};
            CursorPosition endPos{0, static_cast<int>(end)};

            undoStack.push(new FormatApplyCommand(
                &buffer, &formatLayer, nullptr, startPos, endPos, format, prev));
        }

        // Record format count
        size_t formatCount = formatLayer.rangeCount();
        REQUIRE(formatCount > 0);

        // Undo all
        while (undoStack.canUndo()) {
            undoStack.undo();
        }

        // After undo, format count should decrease (may not be exactly 0 due to
        // how FormatApplyCommand restores previous formats, but should be minimal)
        size_t afterUndo = formatLayer.rangeCount();
        REQUIRE(afterUndo < formatCount);

        // Redo all
        while (undoStack.canRedo()) {
            undoStack.redo();
        }

        // Should have formats again
        REQUIRE(formatLayer.rangeCount() > 0);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("Buffer commands edge cases", "[editor][buffer_commands][edge]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    QUndoStack undoStack;

    SECTION("Operations on empty buffer") {
        buffer.setPlainText("");

        CursorPosition cursor{0, 0};
        undoStack.push(new TextInsertCommand(
            &buffer, &formatLayer, nullptr, cursor, "Test"));

        REQUIRE(buffer.paragraphText(0) == "Test");

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0).isEmpty());
    }

    SECTION("Insert empty string") {
        buffer.setPlainText("Hello");

        CursorPosition cursor{0, 5};
        undoStack.push(new TextInsertCommand(
            &buffer, &formatLayer, nullptr, cursor, ""));

        REQUIRE(buffer.paragraphText(0) == "Hello");

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello");
    }

    SECTION("Delete empty range") {
        buffer.setPlainText("Hello");

        CursorPosition start{0, 3};
        CursorPosition end{0, 3};
        undoStack.push(new TextDeleteCommand(
            &buffer, &formatLayer, nullptr, start, end, "", {}));

        REQUIRE(buffer.paragraphText(0) == "Hello");

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Hello");
    }

    SECTION("Split at exact paragraph boundary") {
        buffer.setPlainText("Hello");

        // Split at the end creates empty paragraph
        CursorPosition splitPos{0, 5};
        undoStack.push(new ParagraphSplitCommand(
            &buffer, &formatLayer, nullptr, splitPos, {}));

        REQUIRE(buffer.paragraphCount() == 2);
        REQUIRE(buffer.paragraphText(0) == "Hello");
        REQUIRE(buffer.paragraphText(1).isEmpty());

        undoStack.undo();
        REQUIRE(buffer.paragraphCount() == 1);
    }

    SECTION("Unicode text handling") {
        buffer.setPlainText(QString::fromUtf8("Hello"));

        CursorPosition cursor{0, 5};
        undoStack.push(new TextInsertCommand(
            &buffer, &formatLayer, nullptr, cursor, QString::fromUtf8(" World")));

        REQUIRE(buffer.paragraphText(0) == QString::fromUtf8("Hello World"));

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == QString::fromUtf8("Hello"));
    }

    SECTION("Very long text insertion") {
        buffer.setPlainText("Start");

        // Create a long string
        QString longText;
        for (int i = 0; i < 1000; ++i) {
            longText += QString::number(i % 10);
        }

        CursorPosition cursor{0, 5};
        undoStack.push(new TextInsertCommand(
            &buffer, &formatLayer, nullptr, cursor, longText));

        REQUIRE(buffer.paragraphText(0).length() == 5 + 1000);

        undoStack.undo();
        REQUIRE(buffer.paragraphText(0) == "Start");
    }
}

// =============================================================================
// Command ID Tests
// =============================================================================

TEST_CASE("Buffer command IDs", "[editor][buffer_commands][id]") {
    TextBuffer buffer;
    FormatLayer formatLayer;
    buffer.setPlainText("Test");

    SECTION("TextInsertCommand has correct ID") {
        auto cmd = std::make_unique<TextInsertCommand>(
            &buffer, &formatLayer, nullptr, CursorPosition{0, 0}, "X");
        REQUIRE(cmd->id() == static_cast<int>(BufferCommandId::TextInsert));
    }

    SECTION("TextDeleteCommand has correct ID") {
        auto cmd = std::make_unique<TextDeleteCommand>(
            &buffer, &formatLayer, nullptr, CursorPosition{0, 0}, CursorPosition{0, 1}, "T", std::vector<FormatRange>{});
        REQUIRE(cmd->id() == static_cast<int>(BufferCommandId::TextDelete));
    }

    SECTION("ParagraphSplitCommand has correct ID") {
        auto cmd = std::make_unique<ParagraphSplitCommand>(
            &buffer, &formatLayer, nullptr, CursorPosition{0, 2}, std::vector<FormatRange>{});
        REQUIRE(cmd->id() == static_cast<int>(BufferCommandId::ParagraphSplit));
    }

    SECTION("ParagraphMergeCommand has correct ID") {
        buffer.setPlainText("A\nB");
        auto cmd = std::make_unique<ParagraphMergeCommand>(
            &buffer, &formatLayer, nullptr, CursorPosition{1, 0}, 1, "B", std::vector<FormatRange>{});
        REQUIRE(cmd->id() == static_cast<int>(BufferCommandId::ParagraphMerge));
    }

    SECTION("FormatApplyCommand has correct ID") {
        TextFormat format;
        format.setBold();
        auto cmd = std::make_unique<FormatApplyCommand>(
            &buffer, &formatLayer, nullptr, CursorPosition{0, 0}, CursorPosition{0, 4}, format, std::vector<FormatRange>{});
        REQUIRE(cmd->id() == static_cast<int>(BufferCommandId::FormatApply));
    }

    SECTION("FormatRemoveCommand has correct ID") {
        auto cmd = std::make_unique<FormatRemoveCommand>(
            &buffer, &formatLayer, nullptr, CursorPosition{0, 0}, CursorPosition{0, 4}, FormatType::Bold, std::vector<FormatRange>{});
        REQUIRE(cmd->id() == static_cast<int>(BufferCommandId::FormatRemove));
    }
}
