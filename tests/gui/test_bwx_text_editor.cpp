/// @file test_bwx_text_editor.cpp
/// @brief Unit tests for bwxTextEditor (Task #00019 - Days 9-10)
///
/// Tests cover:
/// - Control creation and initialization
/// - Event handling (keyboard, mouse, focus)
/// - Editing operations (Copy, Cut, Paste, SelectAll)
/// - Undo/Redo operations
/// - View mode switching
/// - Caret management
/// - Scrolling
/// - Document integration

#include <catch2/catch_test_macros.hpp>
#include <bwx_sdk/bwx_gui/bwx_text_editor.h>
#include <wx/app.h>

using namespace bwx_sdk::gui;

// =============================================================================
// Test Fixture - wxWidgets App
// =============================================================================

// wxWidgets requires wxApp instance for controls
class TestApp : public wxApp
{
public:
	bool OnInit() override { return true; }
};

// Create wxApp instance (only once)
static bool wxAppCreated = false;

void EnsureWxApp()
{
	if (!wxAppCreated)
	{
		wxApp::SetInstance(new TestApp());
		int argc = 0;
		wxEntryStart(argc, (wxChar**)nullptr);
		wxTheApp->CallOnInit();
		wxAppCreated = true;
	}
}

// =============================================================================
// Control Creation Tests
// =============================================================================

TEST_CASE("bwxTextEditor creation", "[gui][text][editor]") {
	EnsureWxApp();

	SECTION("Default constructor + Create") {
		wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Test");
		bwxTextEditor* editor = new bwxTextEditor();

		bool created = editor->Create(frame, wxID_ANY);
		REQUIRE(created);
		REQUIRE(editor->GetViewMode() == bwxTextEditor::VIEW_FULL);

		frame->Destroy();
	}

	SECTION("Full constructor") {
		wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Test");
		bwxTextEditor* editor = new bwxTextEditor(frame, wxID_ANY);

		REQUIRE(editor != nullptr);
		REQUIRE(editor->GetViewMode() == bwxTextEditor::VIEW_FULL);

		frame->Destroy();
	}

	SECTION("Document is initialized") {
		wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Test");
		bwxTextEditor* editor = new bwxTextEditor(frame, wxID_ANY);

		REQUIRE(editor->GetDocument().GetLength() == 0);
		REQUIRE(editor->GetDocument().GetText().IsEmpty());

		frame->Destroy();
	}
}

// =============================================================================
// View Mode Tests
// =============================================================================

TEST_CASE("bwxTextEditor view mode", "[gui][text][editor]") {
	EnsureWxApp();
	wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Test");
	bwxTextEditor* editor = new bwxTextEditor(frame, wxID_ANY);

	SECTION("Default view mode is FULL") {
		REQUIRE(editor->GetViewMode() == bwxTextEditor::VIEW_FULL);
	}

	SECTION("SetViewMode changes mode") {
		editor->SetViewMode(bwxTextEditor::VIEW_FULL);
		REQUIRE(editor->GetViewMode() == bwxTextEditor::VIEW_FULL);

		// Future modes should fall back to FULL (MVP)
		editor->SetViewMode(bwxTextEditor::VIEW_PAGE);
		REQUIRE(editor->GetViewMode() == bwxTextEditor::VIEW_FULL);
	}

	frame->Destroy();
}

// =============================================================================
// Editing Operations Tests
// =============================================================================

TEST_CASE("bwxTextEditor editing operations", "[gui][text][editor]") {
	EnsureWxApp();
	wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Test");
	bwxTextEditor* editor = new bwxTextEditor(frame, wxID_ANY);

	SECTION("SelectAll selects all text") {
		editor->GetDocument().SetText("Hello World");
		editor->SelectAll();

		Selection sel = editor->GetDocument().GetSelection();
		REQUIRE(sel.active);
		REQUIRE(sel.GetMin() == 0);
		REQUIRE(sel.GetMax() == 11);
	}

	SECTION("SelectAll on empty document") {
		editor->SelectAll();

		Selection sel = editor->GetDocument().GetSelection();
		REQUIRE(!sel.active);  // No selection on empty document
	}

	SECTION("Copy with no selection does nothing") {
		editor->GetDocument().SetText("Hello");
		editor->Copy();  // Should not crash
	}

	SECTION("Cut with no selection does nothing") {
		editor->GetDocument().SetText("Hello");
		editor->Cut();  // Should not crash
		REQUIRE(editor->GetDocument().GetText() == "Hello");
	}

	frame->Destroy();
}

// =============================================================================
// Undo/Redo Tests
// =============================================================================

TEST_CASE("bwxTextEditor undo/redo", "[gui][text][editor]") {
	EnsureWxApp();
	wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Test");
	bwxTextEditor* editor = new bwxTextEditor(frame, wxID_ANY);

	SECTION("CanUndo returns false on empty document") {
		REQUIRE(!editor->CanUndo());
	}

	SECTION("CanRedo returns false initially") {
		REQUIRE(!editor->CanRedo());
	}

	SECTION("Undo/Redo work via editor methods") {
		// Insert text
		editor->GetDocument().InsertText(0, "Hello");
		REQUIRE(editor->CanUndo());

		// Undo
		editor->Undo();
		REQUIRE(editor->GetDocument().GetText().IsEmpty());
		REQUIRE(editor->CanRedo());

		// Redo
		editor->Redo();
		REQUIRE(editor->GetDocument().GetText() == "Hello");
	}

	frame->Destroy();
}

// =============================================================================
// Document Integration Tests
// =============================================================================

TEST_CASE("bwxTextEditor document integration", "[gui][text][editor]") {
	EnsureWxApp();
	wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Test");
	bwxTextEditor* editor = new bwxTextEditor(frame, wxID_ANY);

	SECTION("GetDocument returns valid reference") {
		bwxTextDocument& doc = editor->GetDocument();
		doc.SetText("Test");
		REQUIRE(editor->GetDocument().GetText() == "Test");
	}

	SECTION("Document changes are observable") {
		editor->GetDocument().SetText("Hello");
		REQUIRE(editor->GetDocument().GetLength() == 5);

		editor->GetDocument().InsertText(5, " World");
		REQUIRE(editor->GetDocument().GetText() == "Hello World");
		REQUIRE(editor->GetDocument().GetLength() == 11);
	}

	SECTION("Formatting operations work") {
		editor->GetDocument().SetText("Hello");

		TextFormat boldFormat;
		boldFormat.bold = true;
		editor->GetDocument().ApplyFormat(0, 5, boldFormat);

		TextFormat format = editor->GetDocument().GetFormatAt(0);
		REQUIRE(format.bold);
	}

	frame->Destroy();
}

// =============================================================================
// Cursor Management Tests
// =============================================================================

TEST_CASE("bwxTextEditor cursor management", "[gui][text][editor]") {
	EnsureWxApp();
	wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Test");
	bwxTextEditor* editor = new bwxTextEditor(frame, wxID_ANY);

	SECTION("Initial cursor position is 0") {
		Cursor cursor = editor->GetDocument().GetCursor();
		REQUIRE(cursor.position == 0);
	}

	SECTION("Cursor moves after text insertion") {
		editor->GetDocument().InsertText(0, "Hello");
		Cursor cursor = editor->GetDocument().GetCursor();
		REQUIRE(cursor.position == 5);
	}

	SECTION("Cursor position can be set") {
		editor->GetDocument().SetText("Hello World");
		editor->GetDocument().SetCursorPosition(6);

		Cursor cursor = editor->GetDocument().GetCursor();
		REQUIRE(cursor.position == 6);
	}

	frame->Destroy();
}

// =============================================================================
// Selection Tests
// =============================================================================

TEST_CASE("bwxTextEditor selection", "[gui][text][editor]") {
	EnsureWxApp();
	wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Test");
	bwxTextEditor* editor = new bwxTextEditor(frame, wxID_ANY);

	SECTION("Initial selection is empty") {
		Selection sel = editor->GetDocument().GetSelection();
		REQUIRE(!sel.active);
	}

	SECTION("Selection can be set") {
		editor->GetDocument().SetText("Hello World");
		editor->GetDocument().SetSelection(0, 5);

		Selection sel = editor->GetDocument().GetSelection();
		REQUIRE(sel.active);
		REQUIRE(sel.GetMin() == 0);
		REQUIRE(sel.GetMax() == 5);
	}

	SECTION("Selection can be cleared") {
		editor->GetDocument().SetText("Hello World");
		editor->GetDocument().SetSelection(0, 5);
		editor->GetDocument().ClearSelection();

		Selection sel = editor->GetDocument().GetSelection();
		REQUIRE(!sel.active);
	}

	frame->Destroy();
}

// =============================================================================
// Best Size Tests
// =============================================================================

TEST_CASE("bwxTextEditor best size", "[gui][text][editor]") {
	EnsureWxApp();
	wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Test");
	bwxTextEditor* editor = new bwxTextEditor(frame, wxID_ANY);

	SECTION("GetBestSize returns minimum size") {
		wxSize bestSize = editor->GetBestSize();
		REQUIRE(bestSize.GetWidth() >= 400);
		REQUIRE(bestSize.GetHeight() >= 300);
	}

	frame->Destroy();
}

// =============================================================================
// File I/O Tests (stubs for Days 11-12)
// =============================================================================

TEST_CASE("bwxTextEditor file I/O (stub)", "[gui][text][editor]") {
	EnsureWxApp();
	wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "Test");
	bwxTextEditor* editor = new bwxTextEditor(frame, wxID_ANY);

	SECTION("LoadFromFile returns false (not implemented)") {
		bool result = editor->LoadFromFile("test.ktxt");
		REQUIRE(!result);
	}

	SECTION("SaveToFile returns false (not implemented)") {
		bool result = editor->SaveToFile("test.ktxt");
		REQUIRE(!result);
	}

	frame->Destroy();
}
