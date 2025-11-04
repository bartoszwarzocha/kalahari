/// @file test_bwx_text_document.cpp
/// @brief Unit tests for bwxTextDocument (Task #00019 - Day 1-6)
///
/// Tests cover:
/// - Text operations (insert, delete, get, clear)
/// - Gap Buffer storage
/// - Cursor management (position, line, column)
/// - Selection operations
/// - Formatting operations (apply, get, format runs)
/// - Undo/Redo operations (including command merging)
/// - Word count and metadata
/// - Observer pattern notifications

#include <catch2/catch_test_macros.hpp>
#include <bwx_sdk/bwx_gui/bwx_text_document.h>

using namespace bwx_sdk::gui;

// =============================================================================
// Test Observer - captures notifications
// =============================================================================

class TestObserver : public IDocumentObserver
{
public:
	int textChangedCount = 0;
	int cursorMovedCount = 0;
	int selectionChangedCount = 0;
	int formatChangedCount = 0;

	void OnTextChanged() override { textChangedCount++; }
	void OnCursorMoved() override { cursorMovedCount++; }
	void OnSelectionChanged() override { selectionChangedCount++; }
	void OnFormatChanged() override { formatChangedCount++; }

	void Reset() {
		textChangedCount = 0;
		cursorMovedCount = 0;
		selectionChangedCount = 0;
		formatChangedCount = 0;
	}
};

// =============================================================================
// Text Operations Tests
// =============================================================================

TEST_CASE("bwxTextDocument text operations", "[gui][text][document]") {
	bwxTextDocument doc;

	SECTION("Initial state - empty document") {
		REQUIRE(doc.GetText() == "");
		REQUIRE(doc.GetLength() == 0);
	}

	SECTION("SetText - replaces content") {
		doc.SetText("Hello World");
		REQUIRE(doc.GetText() == "Hello World");
		REQUIRE(doc.GetLength() == 11);
	}

	SECTION("InsertText - at start") {
		doc.SetText("World");
		doc.InsertText(0, "Hello ");
		REQUIRE(doc.GetText() == "Hello World");
	}

	SECTION("InsertText - at end") {
		doc.SetText("Hello");
		doc.InsertText(5, " World");
		REQUIRE(doc.GetText() == "Hello World");
	}

	SECTION("InsertText - in middle") {
		doc.SetText("HelloWorld");
		doc.InsertText(5, " ");
		REQUIRE(doc.GetText() == "Hello World");
	}

	SECTION("DeleteText - from start") {
		doc.SetText("Hello World");
		doc.DeleteText(0, 6);
		REQUIRE(doc.GetText() == "World");
	}

	SECTION("DeleteText - from end") {
		doc.SetText("Hello World");
		doc.DeleteText(5, 11);
		REQUIRE(doc.GetText() == "Hello");
	}

	SECTION("DeleteText - from middle") {
		doc.SetText("Hello Beautiful World");
		doc.DeleteText(6, 16);
		REQUIRE(doc.GetText() == "Hello World");
	}

	SECTION("GetText - range extraction") {
		doc.SetText("Hello World");
		REQUIRE(doc.GetText(0, 5) == "Hello");
		REQUIRE(doc.GetText(6, 11) == "World");
		REQUIRE(doc.GetText(0, 11) == "Hello World");
	}

	SECTION("GetChar - individual character access") {
		doc.SetText("Hello");
		REQUIRE(doc.GetChar(0) == 'H');
		REQUIRE(doc.GetChar(1) == 'e');
		REQUIRE(doc.GetChar(4) == 'o');
	}

	SECTION("Clear - removes all text") {
		doc.SetText("Hello World");
		doc.Clear();
		REQUIRE(doc.GetText() == "");
		REQUIRE(doc.GetLength() == 0);
	}
}

// =============================================================================
// Cursor Tests
// =============================================================================

TEST_CASE("bwxTextDocument cursor operations", "[gui][text][document][cursor]") {
	bwxTextDocument doc;
	doc.SetText("Line 1\nLine 2\nLine 3");

	SECTION("Initial cursor at position 0") {
		Cursor cursor = doc.GetCursor();
		REQUIRE(cursor.position == 0);
		REQUIRE(cursor.line == 0);
		REQUIRE(cursor.column == 0);
	}

	SECTION("SetCursorPosition - moves cursor") {
		doc.SetCursorPosition(7);
		Cursor cursor = doc.GetCursor();
		REQUIRE(cursor.position == 7);
		REQUIRE(cursor.line == 1);
		REQUIRE(cursor.column == 0);
	}

	SECTION("SetCursorPosition - clamps to valid range") {
		doc.SetCursorPosition(100);
		REQUIRE(doc.GetCursor().position == doc.GetLength());

		doc.SetCursorPosition(-5);
		REQUIRE(doc.GetCursor().position == 0);
	}

	SECTION("MoveCursor - relative movement") {
		doc.SetCursorPosition(5);
		doc.MoveCursor(2);
		REQUIRE(doc.GetCursor().position == 7);

		doc.MoveCursor(-3);
		REQUIRE(doc.GetCursor().position == 4);
	}

	SECTION("Cursor line/column calculation") {
		doc.SetCursorPosition(10);
		Cursor cursor = doc.GetCursor();
		REQUIRE(cursor.line == 1);
		REQUIRE(cursor.column == 3);
	}
}

// =============================================================================
// Selection Tests
// =============================================================================

TEST_CASE("bwxTextDocument selection operations", "[gui][text][document][selection]") {
	bwxTextDocument doc;
	doc.SetText("Hello World");

	SECTION("Initial state - no selection") {
		Selection sel = doc.GetSelection();
		REQUIRE(!sel.active);
		REQUIRE(sel.IsEmpty());
	}

	SECTION("SetSelection - creates selection") {
		doc.SetSelection(0, 5);
		Selection sel = doc.GetSelection();
		REQUIRE(sel.active);
		REQUIRE(sel.GetMin() == 0);
		REQUIRE(sel.GetMax() == 5);
		REQUIRE(sel.GetLength() == 5);
	}

	SECTION("GetSelectedText - returns selected text") {
		doc.SetSelection(0, 5);
		REQUIRE(doc.GetSelectedText() == "Hello");

		doc.SetSelection(6, 11);
		REQUIRE(doc.GetSelectedText() == "World");
	}

	SECTION("SelectAll - selects entire document") {
		doc.SelectAll();
		Selection sel = doc.GetSelection();
		REQUIRE(sel.GetMin() == 0);
		REQUIRE(sel.GetMax() == doc.GetLength());
	}

	SECTION("ClearSelection - removes selection") {
		doc.SetSelection(0, 5);
		doc.ClearSelection();
		REQUIRE(!doc.GetSelection().active);
	}

	SECTION("DeleteSelection - removes selected text") {
		doc.SetSelection(0, 6);
		bool deleted = doc.DeleteSelection();
		REQUIRE(deleted);
		REQUIRE(doc.GetText() == "World");
		REQUIRE(!doc.GetSelection().active);
	}

	SECTION("DeleteSelection - returns false if no selection") {
		doc.ClearSelection();
		bool deleted = doc.DeleteSelection();
		REQUIRE(!deleted);
	}
}

// =============================================================================
// Formatting Tests
// =============================================================================

TEST_CASE("bwxTextDocument formatting operations", "[gui][text][document][format]") {
	bwxTextDocument doc;
	doc.SetText("Hello World");

	SECTION("Initial format - default") {
		TextFormat fmt = doc.GetFormatAt(0);
		REQUIRE(fmt.fontName == "Arial");
		REQUIRE(fmt.fontSize == 12);
		REQUIRE(!fmt.bold);
		REQUIRE(!fmt.italic);
		REQUIRE(!fmt.underline);
	}

	SECTION("ApplyFormat - single run") {
		TextFormat boldFmt;
		boldFmt.bold = true;
		doc.ApplyFormat(0, 5, boldFmt);

		TextFormat fmt0 = doc.GetFormatAt(0);
		REQUIRE(fmt0.bold == true);

		TextFormat fmt6 = doc.GetFormatAt(6);
		REQUIRE(fmt6.bold == false);
	}

	SECTION("ApplyFormat - multiple runs") {
		TextFormat boldFmt;
		boldFmt.bold = true;
		doc.ApplyFormat(0, 5, boldFmt);

		TextFormat italicFmt;
		italicFmt.italic = true;
		doc.ApplyFormat(6, 11, italicFmt);

		REQUIRE(doc.GetFormatAt(0).bold == true);
		REQUIRE(doc.GetFormatAt(0).italic == false);
		REQUIRE(doc.GetFormatAt(6).bold == false);
		REQUIRE(doc.GetFormatAt(6).italic == true);
	}

	SECTION("ApplyFormat - overlapping runs merge") {
		TextFormat boldFmt;
		boldFmt.bold = true;
		doc.ApplyFormat(0, 11, boldFmt);

		const auto& runs = doc.GetFormatRuns();
		REQUIRE(runs.size() == 1);
		REQUIRE(runs[0].startPos == 0);
		REQUIRE(runs[0].endPos == 11);
	}

	SECTION("GetFormatRuns - returns runs in range") {
		TextFormat boldFmt;
		boldFmt.bold = true;
		doc.ApplyFormat(0, 5, boldFmt);

		auto runs = doc.GetFormatRuns(0, 5);
		REQUIRE(runs.size() == 1);
		REQUIRE(runs[0].startPos == 0);
		REQUIRE(runs[0].endPos == 5);
	}

	SECTION("ClearFormatting - resets to default") {
		TextFormat boldFmt;
		boldFmt.bold = true;
		doc.ApplyFormat(0, 11, boldFmt);

		doc.ClearFormatting();
		REQUIRE(doc.GetFormatAt(0).bold == false);
	}
}

// =============================================================================
// Undo/Redo Tests
// =============================================================================

TEST_CASE("bwxTextDocument undo/redo operations", "[gui][text][document][undo]") {
	bwxTextDocument doc;

	SECTION("Initial state - no undo/redo") {
		REQUIRE(!doc.CanUndo());
		REQUIRE(!doc.CanRedo());
	}

	SECTION("InsertText - creates undo entry") {
		doc.InsertText(0, "Hello");
		REQUIRE(doc.CanUndo());
		REQUIRE(!doc.CanRedo());
	}

	SECTION("Undo insert - reverts text") {
		doc.InsertText(0, "Hello");
		doc.Undo();
		REQUIRE(doc.GetText() == "");
		REQUIRE(doc.CanRedo());
	}

	SECTION("Redo insert - reapplies text") {
		doc.InsertText(0, "Hello");
		doc.Undo();
		doc.Redo();
		REQUIRE(doc.GetText() == "Hello");
	}

	SECTION("Undo delete - restores text") {
		doc.SetText("Hello World");
		doc.DeleteText(0, 6);
		REQUIRE(doc.GetText() == "World");

		doc.Undo();
		REQUIRE(doc.GetText() == "Hello World");
	}

	SECTION("Undo format - restores formatting") {
		doc.SetText("Hello");
		TextFormat boldFmt;
		boldFmt.bold = true;
		doc.ApplyFormat(0, 5, boldFmt);
		REQUIRE(doc.GetFormatAt(0).bold == true);

		doc.Undo();
		REQUIRE(doc.GetFormatAt(0).bold == false);
	}

	SECTION("Command merging - consecutive inserts merge") {
		doc.InsertText(0, "H");
		doc.InsertText(1, "e");
		doc.InsertText(2, "l");
		doc.InsertText(3, "l");
		doc.InsertText(4, "o");

		// Should merge to single undo step
		REQUIRE(doc.GetUndoStackSize() == 1);

		doc.Undo();
		REQUIRE(doc.GetText() == "");
	}

	SECTION("New action clears redo stack") {
		doc.InsertText(0, "Hello");
		doc.Undo();
		REQUIRE(doc.CanRedo());

		doc.InsertText(0, "World");
		REQUIRE(!doc.CanRedo());
	}

	SECTION("Undo limit - enforces max stack size") {
		doc.SetMaxUndoStack(5);

		for (int i = 0; i < 10; ++i)
			doc.InsertText(0, "A");

		REQUIRE(doc.GetUndoStackSize() <= 5);
	}

	SECTION("ClearUndoHistory - removes all undo/redo") {
		doc.InsertText(0, "Hello");
		doc.ClearUndoHistory();
		REQUIRE(!doc.CanUndo());
		REQUIRE(!doc.CanRedo());
	}
}

// =============================================================================
// Metadata Tests
// =============================================================================

TEST_CASE("bwxTextDocument metadata operations", "[gui][text][document][metadata]") {
	bwxTextDocument doc;

	SECTION("Initial word count - zero") {
		doc.UpdateWordCount();
		REQUIRE(doc.GetWordCount() == 0);
		REQUIRE(doc.GetCharacterCount() == 0);
	}

	SECTION("UpdateWordCount - counts words correctly") {
		doc.SetText("Hello World Test");
		doc.UpdateWordCount();
		REQUIRE(doc.GetWordCount() == 3);
		REQUIRE(doc.GetCharacterCount() == 16);
	}

	SECTION("Word count - handles multiple spaces") {
		doc.SetText("Hello    World");
		doc.UpdateWordCount();
		REQUIRE(doc.GetWordCount() == 2);
	}

	SECTION("Word count - handles newlines") {
		doc.SetText("Hello\nWorld\nTest");
		doc.UpdateWordCount();
		REQUIRE(doc.GetWordCount() == 3);
	}

	SECTION("Word count - handles tabs") {
		doc.SetText("Hello\tWorld");
		doc.UpdateWordCount();
		REQUIRE(doc.GetWordCount() == 2);
	}

	SECTION("Metadata - can set and get") {
		DocumentMetadata meta;
		meta.title = "Chapter 1";
		meta.author = "Test Author";
		doc.SetMetadata(meta);

		DocumentMetadata retrieved = doc.GetMetadata();
		REQUIRE(retrieved.title == "Chapter 1");
		REQUIRE(retrieved.author == "Test Author");
	}
}

// =============================================================================
// Observer Pattern Tests
// =============================================================================

TEST_CASE("bwxTextDocument observer pattern", "[gui][text][document][observer]") {
	bwxTextDocument doc;
	TestObserver observer;
	doc.AddObserver(&observer);

	SECTION("InsertText - notifies OnTextChanged") {
		doc.InsertTextInternal(0, "Hello");
		REQUIRE(observer.textChangedCount == 1);
	}

	SECTION("DeleteText - notifies OnTextChanged") {
		doc.SetText("Hello World");
		observer.Reset();
		doc.DeleteTextInternal(0, 5);
		REQUIRE(observer.textChangedCount == 1);
	}

	SECTION("SetCursorPosition - notifies OnCursorMoved") {
		doc.SetCursorPosition(5);
		REQUIRE(observer.cursorMovedCount == 1);
	}

	SECTION("SetSelection - notifies OnSelectionChanged") {
		doc.SetSelection(0, 5);
		REQUIRE(observer.selectionChangedCount == 1);
	}

	SECTION("ApplyFormat - notifies OnFormatChanged") {
		doc.SetText("Hello");
		observer.Reset();
		TextFormat boldFmt;
		boldFmt.bold = true;
		doc.ApplyFormatInternal(0, 5, boldFmt);
		REQUIRE(observer.formatChangedCount == 1);
	}

	SECTION("RemoveObserver - stops notifications") {
		doc.RemoveObserver(&observer);
		observer.Reset();
		doc.InsertTextInternal(0, "Hello");
		REQUIRE(observer.textChangedCount == 0);
	}

	SECTION("Multiple observers - all notified") {
		TestObserver observer2;
		doc.AddObserver(&observer2);

		doc.InsertTextInternal(0, "Hello");
		REQUIRE(observer.textChangedCount == 1);
		REQUIRE(observer2.textChangedCount == 1);
	}
}

// =============================================================================
// Integration Tests - Complex Scenarios
// =============================================================================

TEST_CASE("bwxTextDocument integration scenarios", "[gui][text][document][integration]") {
	bwxTextDocument doc;

	SECTION("Typing scenario - insert, cursor move, selection") {
		doc.InsertText(0, "H");
		doc.InsertText(1, "e");
		doc.InsertText(2, "l");
		doc.InsertText(3, "l");
		doc.InsertText(4, "o");

		REQUIRE(doc.GetText() == "Hello");

		doc.SetCursorPosition(5);
		doc.InsertText(5, " World");

		REQUIRE(doc.GetText() == "Hello World");

		doc.SetSelection(0, 5);
		TextFormat boldFmt;
		boldFmt.bold = true;
		doc.ApplyFormat(0, 5, boldFmt);

		REQUIRE(doc.GetFormatAt(0).bold == true);
		REQUIRE(doc.GetFormatAt(6).bold == false);
	}

	SECTION("Edit scenario - insert, delete, undo, redo") {
		doc.InsertText(0, "Hello");
		doc.InsertText(5, " World");
		REQUIRE(doc.GetText() == "Hello World");

		doc.DeleteText(5, 11);
		REQUIRE(doc.GetText() == "Hello");

		doc.Undo();
		REQUIRE(doc.GetText() == "Hello World");

		doc.Undo();
		REQUIRE(doc.GetText() == "Hello");

		doc.Redo();
		REQUIRE(doc.GetText() == "Hello World");
	}

	SECTION("Format preservation during text operations") {
		doc.SetText("Hello World");

		TextFormat boldFmt;
		boldFmt.bold = true;
		doc.ApplyFormat(0, 5, boldFmt);

		doc.InsertText(5, " Beautiful");
		REQUIRE(doc.GetText() == "Hello Beautiful World");

		// Bold formatting should still be at positions 0-4
		REQUIRE(doc.GetFormatAt(0).bold == true);
		REQUIRE(doc.GetFormatAt(4).bold == true);
		REQUIRE(doc.GetFormatAt(6).bold == false);
	}
}
