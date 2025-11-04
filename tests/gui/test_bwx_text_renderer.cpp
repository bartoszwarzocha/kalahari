/// @file test_bwx_text_renderer.cpp
/// @brief Unit tests for FullViewRenderer (Task #00019 - Day 7-8)
///
/// Tests cover:
/// - Layout calculation (line breaks, word wrap)
/// - Hit testing (screen coordinates → document position)
/// - Cursor rectangle calculation
/// - Selection rectangle calculation
/// - Resize handling
/// - Layout invalidation
///
/// Note: These tests focus on layout logic, not pixel-perfect rendering.
/// Actual rendering is better tested manually with visual inspection.

#include <catch2/catch_test_macros.hpp>
#include <bwx_sdk/bwx_gui/bwx_text_document.h>
#include <bwx_sdk/bwx_gui/bwx_text_renderer.h>
#include <wx/wx.h>

using namespace bwx_sdk::gui;

// =============================================================================
// Test Helpers
// =============================================================================

/// Helper to create initialized renderer with document
class RendererTestFixture
{
public:
	RendererTestFixture()
	{
		// Create wxApp if not exists (needed for wxDC operations)
		if (!wxApp::GetInstance())
		{
			static int argc = 1;
			static char* argv[] = {(char*)"test", nullptr};
			wxApp::SetInstance(new wxApp());
			wxEntryStart(argc, argv);
		}

		renderer.SetDocument(&doc);
		renderer.OnResize(800, 600);  // Standard test size
	}

	~RendererTestFixture()
	{
		// Don't cleanup wxApp - shared across tests
	}

	bwxTextDocument doc;
	FullViewRenderer renderer;
};

// =============================================================================
// Layout Calculation Tests
// =============================================================================

TEST_CASE("FullViewRenderer layout calculation", "[gui][text][renderer][layout]") {
	RendererTestFixture fixture;

	SECTION("Empty document - no lines") {
		wxMemoryDC dc;
		dc.SelectObject(wxBitmap(800, 600));

		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

		// Should not crash, total height should be 0 or minimal
		REQUIRE(fixture.renderer.GetTotalHeight() >= 0);
	}

	SECTION("Single line - no wrap") {
		fixture.doc.SetText("Hello World");

		wxMemoryDC dc;
		dc.SelectObject(wxBitmap(800, 600));
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

		// Should create at least one line
		REQUIRE(fixture.renderer.GetTotalHeight() > 0);
	}

	SECTION("Multiple lines - with newlines") {
		fixture.doc.SetText("Line 1\nLine 2\nLine 3");

		wxMemoryDC dc;
		dc.SelectObject(wxBitmap(800, 600));
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

		// Should create multiple lines
		int totalHeight = fixture.renderer.GetTotalHeight();
		REQUIRE(totalHeight > 50);  // At least 3 lines with reasonable height
	}

	SECTION("Long line - word wrap") {
		// Create very long line that should wrap
		wxString longText = "This is a very long line that should definitely wrap ";
		for (int i = 0; i < 10; ++i)
			longText += "and keep wrapping ";

		fixture.doc.SetText(longText);

		wxMemoryDC dc;
		dc.SelectObject(wxBitmap(800, 600));
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

		// Should create multiple lines due to wrapping
		int totalHeight = fixture.renderer.GetTotalHeight();
		REQUIRE(totalHeight > 40);  // Multiple wrapped lines
	}
}

// =============================================================================
// Hit Testing Tests
// =============================================================================

TEST_CASE("FullViewRenderer hit testing", "[gui][text][renderer][hittest]") {
	RendererTestFixture fixture;
	fixture.doc.SetText("Hello World\nSecond Line");

	// Force layout calculation
	wxMemoryDC dc;
	dc.SelectObject(wxBitmap(800, 600));
	fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

	SECTION("Hit test - beginning of document") {
		int pos = fixture.renderer.HitTest(20, 20, 0);  // Left margin, top
		REQUIRE(pos == 0);
	}

	SECTION("Hit test - end of document") {
		int pos = fixture.renderer.HitTest(500, 500, 0);  // Far right, far down
		REQUIRE(pos >= fixture.doc.GetLength());  // Should be at or past end
	}

	SECTION("Hit test - left of text (in margin)") {
		int pos = fixture.renderer.HitTest(5, 20, 0);  // Before left margin
		REQUIRE(pos == 0);  // Should snap to start of line
	}
}

// =============================================================================
// Cursor Rectangle Tests
// =============================================================================

TEST_CASE("FullViewRenderer cursor rectangle", "[gui][text][renderer][cursor]") {
	RendererTestFixture fixture;
	fixture.doc.SetText("Hello World");

	// Force layout calculation
	wxMemoryDC dc;
	dc.SelectObject(wxBitmap(800, 600));
	fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

	SECTION("Cursor at position 0") {
		wxRect cursorRect = fixture.renderer.GetCursorRect(0);
		REQUIRE(cursorRect.x >= 0);
		REQUIRE(cursorRect.y >= 0);
		REQUIRE(cursorRect.width == 1);  // Cursor is 1 pixel wide
		REQUIRE(cursorRect.height > 0);
	}

	SECTION("Cursor in middle of text") {
		wxRect cursorRect = fixture.renderer.GetCursorRect(5);
		REQUIRE(cursorRect.x > 20);  // Past left margin
		REQUIRE(cursorRect.y >= 0);
		REQUIRE(cursorRect.height > 0);
	}

	SECTION("Cursor at end of text") {
		int endPos = fixture.doc.GetLength();
		wxRect cursorRect = fixture.renderer.GetCursorRect(endPos);
		REQUIRE(cursorRect.x > 20);  // Past left margin
		REQUIRE(cursorRect.y >= 0);
	}
}

// =============================================================================
// Selection Rectangle Tests
// =============================================================================

TEST_CASE("FullViewRenderer selection rectangles", "[gui][text][renderer][selection]") {
	RendererTestFixture fixture;
	fixture.doc.SetText("Hello World\nSecond Line");

	// Force layout calculation
	wxMemoryDC dc;
	dc.SelectObject(wxBitmap(800, 600));
	fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

	SECTION("Selection - single line") {
		auto rects = fixture.renderer.GetSelectionRects(0, 5);
		REQUIRE(rects.size() >= 1);  // At least one rectangle

		if (!rects.empty()) {
			REQUIRE(rects[0].x >= 0);
			REQUIRE(rects[0].width > 0);
			REQUIRE(rects[0].height > 0);
		}
	}

	SECTION("Selection - multiple lines") {
		auto rects = fixture.renderer.GetSelectionRects(0, 15);  // Spans newline
		REQUIRE(rects.size() >= 2);  // Should have rectangles for both lines
	}

	SECTION("Selection - empty range") {
		auto rects = fixture.renderer.GetSelectionRects(5, 5);
		REQUIRE(rects.empty());  // Empty selection
	}

	SECTION("Selection - reversed range") {
		auto rects = fixture.renderer.GetSelectionRects(10, 5);
		REQUIRE(rects.empty());  // Invalid range
	}
}

// =============================================================================
// Resize Handling Tests
// =============================================================================

TEST_CASE("FullViewRenderer resize handling", "[gui][text][renderer][resize]") {
	RendererTestFixture fixture;

	// Create text that will wrap differently at different widths
	fixture.doc.SetText("This is a moderately long line that will wrap at different widths.");

	wxMemoryDC dc;
	dc.SelectObject(wxBitmap(800, 600));

	SECTION("Resize invalidates layout") {
		// Initial render at 800px
		fixture.renderer.OnResize(800, 600);
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);
		int heightAt800 = fixture.renderer.GetTotalHeight();

		// Resize to narrower width (should cause more wrapping)
		fixture.renderer.OnResize(400, 600);
		fixture.renderer.Render(dc, wxRect(0, 0, 400, 600), 0);
		int heightAt400 = fixture.renderer.GetTotalHeight();

		// Narrower width should result in more lines (greater height)
		REQUIRE(heightAt400 >= heightAt800);
	}

	SECTION("Resize to same size - no change") {
		fixture.renderer.OnResize(800, 600);
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);
		int height1 = fixture.renderer.GetTotalHeight();

		fixture.renderer.OnResize(800, 600);  // Same size
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);
		int height2 = fixture.renderer.GetTotalHeight();

		REQUIRE(height1 == height2);
	}
}

// =============================================================================
// Layout Invalidation Tests
// =============================================================================

TEST_CASE("FullViewRenderer layout invalidation", "[gui][text][renderer][invalidate]") {
	RendererTestFixture fixture;
	fixture.doc.SetText("Hello World");

	wxMemoryDC dc;
	dc.SelectObject(wxBitmap(800, 600));

	SECTION("InvalidateLayout forces recalculation") {
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);
		int height1 = fixture.renderer.GetTotalHeight();

		// Invalidate and render again
		fixture.renderer.InvalidateLayout();
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);
		int height2 = fixture.renderer.GetTotalHeight();

		// Should be same (no content change)
		REQUIRE(height1 == height2);
	}
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_CASE("FullViewRenderer configuration", "[gui][text][renderer][config]") {
	RendererTestFixture fixture;
	fixture.doc.SetText("Hello World");

	wxMemoryDC dc;
	dc.SelectObject(wxBitmap(800, 600));

	SECTION("Margin configuration") {
		fixture.renderer.SetMarginLeft(50);
		fixture.renderer.SetMarginRight(50);

		REQUIRE(fixture.renderer.GetMarginLeft() == 50);
		REQUIRE(fixture.renderer.GetMarginRight() == 50);

		// Render to trigger layout
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

		// Cursor should respect margins
		wxRect cursorRect = fixture.renderer.GetCursorRect(0);
		REQUIRE(cursorRect.x >= 50);  // Should be past left margin
	}

	SECTION("Line spacing configuration") {
		fixture.renderer.SetLineSpacing(1.5);
		REQUIRE(fixture.renderer.GetLineSpacing() == 1.5);

		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);
		int heightWith1_5 = fixture.renderer.GetTotalHeight();

		fixture.renderer.SetLineSpacing(2.0);
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);
		int heightWith2_0 = fixture.renderer.GetTotalHeight();

		// More spacing should result in greater height
		REQUIRE(heightWith2_0 >= heightWith1_5);
	}
}

// =============================================================================
// Integration Tests with Document
// =============================================================================

TEST_CASE("FullViewRenderer integration with document", "[gui][text][renderer][integration]") {
	RendererTestFixture fixture;

	wxMemoryDC dc;
	dc.SelectObject(wxBitmap(800, 600));

	SECTION("Document text change invalidates layout") {
		fixture.doc.SetText("Short");
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);
		int shortHeight = fixture.renderer.GetTotalHeight();

		fixture.doc.SetText("This is a much longer text that will take more space");
		fixture.renderer.InvalidateLayout();  // Normally triggered by observer
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);
		int longHeight = fixture.renderer.GetTotalHeight();

		REQUIRE(longHeight > shortHeight);
	}

	SECTION("Cursor position affects cursor rect") {
		fixture.doc.SetText("Hello World");
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

		wxRect rect1 = fixture.renderer.GetCursorRect(0);
		wxRect rect2 = fixture.renderer.GetCursorRect(5);

		// Cursor should move horizontally
		REQUIRE(rect2.x > rect1.x);
	}

	SECTION("Selection spans work correctly") {
		fixture.doc.SetText("Line 1\nLine 2\nLine 3");
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

		// Select across multiple lines
		auto rects = fixture.renderer.GetSelectionRects(0, 20);
		REQUIRE(rects.size() >= 2);  // Should span at least 2 lines
	}
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST_CASE("FullViewRenderer edge cases", "[gui][text][renderer][edge]") {
	RendererTestFixture fixture;

	wxMemoryDC dc;
	dc.SelectObject(wxBitmap(800, 600));

	SECTION("Empty line (just newline)") {
		fixture.doc.SetText("\n\n\n");
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

		// Should handle empty lines gracefully
		REQUIRE(fixture.renderer.GetTotalHeight() > 0);
	}

	SECTION("Very long word (no spaces)") {
		wxString longWord = "Supercalifragilisticexpialidocious";
		for (int i = 0; i < 10; ++i)
			longWord += "verylongwordwithoutspaces";

		fixture.doc.SetText(longWord);
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

		// Should not crash, even if word doesn't fit
		REQUIRE(fixture.renderer.GetTotalHeight() > 0);
	}

	SECTION("Hit test outside document bounds") {
		fixture.doc.SetText("Short");
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

		// Hit test far beyond document
		int pos = fixture.renderer.HitTest(5000, 5000, 0);
		REQUIRE(pos <= fixture.doc.GetLength());
	}

	SECTION("Cursor rect for invalid position") {
		fixture.doc.SetText("Hello");
		fixture.renderer.Render(dc, wxRect(0, 0, 800, 600), 0);

		// Get cursor rect for position beyond end
		wxRect rect = fixture.renderer.GetCursorRect(100);
		REQUIRE(rect.width == 1);
		REQUIRE(rect.height > 0);
	}
}
