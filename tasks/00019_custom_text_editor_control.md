# Task #00019: Custom Text Editor Control - Phase 1 (MVP)

**Status:** ✅ COMPLETE (2025-11-04 to 2025-11-06, 15 days, 100%)
**Priority:** P0 (Critical - Core Functionality)
**Phase:** Phase 1 Week 10-12
**Estimated Duration:** 2-3 weeks (12-15 days) | **Actual:** 3 days (2025-11-04 to 2025-11-06)
**Assigned To:** Claude Code + User
**Dependencies:** Task #00013 (wxAUI completed ✅), Task #00017 (bwx_sdk integration ✅), Task #00018 (bwx_sdk refactoring ✅), Phase 0 (Document Model ✅)

**Rationale for Task #00019 Number:**
This is the logical continuation of bwx_sdk integration (Tasks #00017, #00018). The custom text editor control will be built using bwx_sdk patterns and utilities, making it a direct application of the bwx_sdk foundation.

**Related Documentation:**
- **Architecture:** `project_docs/15_text_editor_architecture.md` (complete specification)
- **Patterns:** `project_docs/14_bwx_sdk_patterns.md` (custom control patterns)
- **Skill:** `.claude/skills/kalahari-bwx-custom-controls.md` (implementation workflow)

---

## 🎯 Objective

Implement **Phase 1 (MVP)** of custom wxWidgets text editor control: Full View mode with basic editing capabilities. This establishes foundation for Kalahari Phase 1 while designing extensible architecture for 4 view modes + advanced features (comments, footnotes, citations, indexes) in future phases.

**Strategic Context:**
- ❌ wxRichTextCtrl rejected (Task #00015) - insufficient feature control
- ❌ TipTap+wxWebView rejected (Task #00016) - browser overhead
- ✅ **Custom control approved** - complete architectural control

**MVP Scope (This Task):**
- ✅ Full View Mode ONLY (simplest renderer)
- ✅ Basic text editing (insert, delete, cursor, selection)
- ✅ Basic formatting (bold, italic, underline, font, size, color)
- ✅ Undo/Redo (Command pattern, 100 commands)
- ✅ Copy/Cut/Paste
- ✅ Load/Save (custom .ktxt JSON format)
- ✅ Word count

**Out of Scope (Future Tasks #00020-#00026):**
- ⏳ Page View Mode (Task #00020, 3 weeks)
- ⏳ Typewriter Mode (Task #00021, 3 weeks)
- ⏳ Publisher View (Task #00022, 2 weeks)
- ⏳ Comments (Task #00023, 3 weeks)
- ⏳ Footnotes (Task #00024, 2 weeks)
- ⏳ Citations/Bibliography (Task #00025, 2 weeks)
- ⏳ Indexes (Task #00026, 2 weeks)

**Total Project:** 20 weeks for full feature set (this task = Week 1-3 of 20)

---

## 📋 Requirements

### Functional Requirements (MVP)

**Text Editing:**
1. ✅ Multiline text input/output
2. ✅ Cursor positioning (keyboard arrows, mouse click, Home/End/PgUp/PgDn)
3. ✅ Text selection (mouse drag, Shift+arrows, Ctrl+A)
4. ✅ Character insertion at cursor
5. ✅ Character deletion (Backspace, Delete)
6. ✅ Word wrap (wrap at control width minus margins)
7. ✅ Smooth scrolling (vertical only for MVP)

**Text Formatting:**
8. ✅ Bold (Ctrl+B or Format menu)
9. ✅ Italic (Ctrl+I or Format menu)
10. ✅ Underline (Ctrl+U or Format menu)
11. ✅ Font selection (dialog with system fonts)
12. ✅ Font size (8pt - 72pt, default 12pt)
13. ✅ Text color (color picker, default black)
14. ✅ Background color (color picker, default white)
15. ✅ Clear formatting (remove all styles from selection)

**Clipboard:**
16. ✅ Copy (Ctrl+C)
17. ✅ Cut (Ctrl+X)
18. ✅ Paste (Ctrl+V)
19. ✅ Select All (Ctrl+A)

**Undo/Redo:**
20. ✅ Undo last operation (Ctrl+Z, up to 100 commands)
21. ✅ Redo undone operation (Ctrl+Y or Ctrl+Shift+Z)
22. ✅ Command pattern implementation (extensible for future features)

**File Operations:**
23. ✅ Load from .ktxt file (custom JSON format)
24. ✅ Save to .ktxt file
25. ✅ Auto-create directory structure if missing
26. ✅ Handle missing files gracefully (new document = empty editor)

**Metadata:**
27. ✅ Word count (real-time calculation, debounced 500ms)
28. ✅ Character count
29. ✅ Update BookElement metadata on save

**Menu Integration:**
30. ✅ Format menu (new): Bold, Italic, Underline, Font, Color, Clear Formatting
31. ✅ Edit menu enhancements: Cut, Copy, Paste, Select All, Undo, Redo
32. ✅ File → Save: Trigger editor save

### Non-Functional Requirements

- **Performance:** Smooth editing for 10,000+ words, <100ms keystroke latency, 60 FPS rendering
- **Memory:** Efficient Gap Buffer implementation, <50MB for 10K word document
- **UX:** Native look and feel, responsive, professional
- **Extensibility:** Architecture supports future view modes (Page, Typewriter, Publisher) and features (comments, footnotes, citations, indexes)
- **Testing:** Unit tests (text operations, formatting, undo/redo), integration tests (end-to-end workflows)
- **Cross-Platform:** Works on Linux, macOS, Windows (all tested)

---

## 🔧 Technical Approach

### Architecture Overview

**Three-Layer Architecture (MVC-inspired):**

```
┌─────────────────────────────────────────┐
│       bwxTextEditor (Controller)        │  ← Main control (wxControl)
│  - Event handling (keyboard, mouse)    │
│  - View mode switching (future)        │
│  - File I/O coordination               │
└────────────┬────────────────────────────┘
             │
    ┌────────┴────────┐
    │                 │
    ▼                 ▼
┌───────────────┐   ┌──────────────────┐
│bwxTextDocument│   │ FullViewRenderer │  ← View (MVP: one renderer)
│   (Model)     │   │                  │
│               │   │ - Layout calc    │
│ - Text storage│   │ - Rendering      │
│ - Formatting  │   │ - Hit testing    │
│ - Cursor      │   └──────────────────┘
│ - Selection   │
│ - Undo/Redo   │
└───────────────┘
```

**Design Patterns:**
- **Strategy Pattern:** Swappable renderers (FullViewRenderer for MVP, more in future)
- **Command Pattern:** Undo/Redo system (InsertTextCommand, DeleteTextCommand, ApplyFormatCommand)
- **Observer Pattern:** Document changes notify UI (IDocumentObserver interface)

### Component 1: Document Model (bwxTextDocument)

**Purpose:** Stores all document data independently of how it's displayed.

**Key Decisions:**
1. **Text Storage:** Gap Buffer (simple, good for typical editing, proven)
2. **Formatting:** Character-level (TextFormat struct per position range)
3. **File Format:** Custom JSON (.ktxt - "Kalahari Text")

**Class Structure:**
```cpp
class bwxTextDocument {
public:
    // Text operations
    wxString GetText() const;
    void SetText(const wxString& text);
    void InsertText(int pos, const wxString& text);
    void DeleteText(int startPos, int endPos);
    int GetLength() const;

    // Formatting
    struct TextFormat {
        bool bold = false;
        bool italic = false;
        bool underline = false;
        wxString fontName = "Arial";
        int fontSize = 12;
        wxColour textColor = *wxBLACK;
        wxColour backgroundColor = *wxWHITE;
    };

    void ApplyFormat(int startPos, int endPos, const TextFormat& format);
    TextFormat GetFormatAt(int pos) const;

    // Cursor & Selection
    struct Cursor { int position; int line; int column; };
    struct Selection { int startPos; int endPos; bool active; };

    Cursor GetCursor() const;
    void SetCursor(const Cursor& cursor);
    Selection GetSelection() const;
    void SetSelection(int startPos, int endPos);

    // Undo/Redo
    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;

    // File I/O
    bool LoadFromFile(const wxString& path);
    bool SaveToFile(const wxString& path);

    // Metadata
    struct Metadata {
        wxString title;
        wxString author;
        wxDateTime created;
        wxDateTime modified;
        int wordCount;
        int characterCount;
    };
    Metadata GetMetadata() const;
    void UpdateWordCount();

private:
    std::unique_ptr<ITextStorage> m_storage;  // Gap Buffer
    std::map<int, TextFormat> m_formatting;
    Cursor m_cursor;
    Selection m_selection;
    std::vector<std::unique_ptr<ICommand>> m_undoStack;
    std::vector<std::unique_ptr<ICommand>> m_redoStack;
    Metadata m_metadata;
};
```

**Gap Buffer Concept:**
```
Text: "Hello World"
Buffer: [H][e][l][l][o][ ][_][_][_][W][o][r][l][d]
                         ↑ Gap ↑
Cursor at position 6
```

**Why Gap Buffer?**
- ✅ Simple to implement (critical for 2-3 week timeline)
- ✅ Good performance for typical editing (insertion at cursor)
- ✅ Cache-friendly (contiguous memory)
- ✅ Proven in many editors (Emacs, etc.)
- ⚠️ Can migrate to Rope/Piece Table later if profiling shows issues

### Component 2: Full View Renderer (FullViewRenderer)

**Purpose:** Renders document in continuous view (no page boundaries).

**Responsibilities:**
- Layout calculation (line breaks, word wrap)
- Text rendering (with formatting)
- Cursor rendering (blinking caret)
- Selection rendering (highlight)
- Hit testing (screen coordinates → document position)

**Class Structure:**
```cpp
class FullViewRenderer : public ITextRenderer {
public:
    void Render(wxGraphicsContext* gc, const wxRect& clientRect) override;

    int HitTest(int x, int y) const override;
    wxRect GetCursorRect(int position) const override;
    std::vector<wxRect> GetSelectionRects(int startPos, int endPos) const override;

    void OnResize(int width, int height) override;
    void OnScroll(int deltaX, int deltaY) override;

    // Configuration
    void SetMargins(int left, int right);
    void SetLineSpacing(double spacing);

private:
    struct LayoutLine {
        int startPos;
        int endPos;
        int y;
        int height;
        std::vector<wxRect> charRects;  // For hit testing
    };

    std::vector<LayoutLine> m_lines;
    int m_marginLeft = 20;
    int m_marginRight = 20;
    double m_lineSpacing = 1.2;
    int m_scrollY = 0;

    void CalculateLayout(int width);
    void RenderLine(wxGraphicsContext* gc, const LayoutLine& line);
    void RenderCursor(wxGraphicsContext* gc);
    void RenderSelection(wxGraphicsContext* gc);
};
```

**Rendering Pipeline:**
```
1. CalculateLayout() - compute line breaks, word wrap
2. For each visible line:
   a. Get text + formatting from document
   b. Apply formatting (font, size, color, bold, italic, underline)
   c. Draw text at (x, y) position
3. RenderSelection() - highlight selected text (blue overlay)
4. RenderCursor() - draw blinking caret at cursor position
```

### Component 3: Main Control (bwxTextEditor)

**Purpose:** Coordinates document model and renderer, handles user input.

**Responsibilities:**
- Event handling (keyboard, mouse, focus)
- Input processing (character insertion, delete, selection)
- View mode switching (future: Full, Page, Typewriter, Publisher)
- File I/O coordination
- Integration with Kalahari (EditorPanel, StatusBar, Format menu)

**Class Structure:**
```cpp
class bwxTextEditor : public wxControl, public bwxTextDocument::IDocumentObserver {
public:
    enum ViewMode {
        VIEW_FULL,               // MVP: Continuous, no pages
        VIEW_PAGE,               // Future: MS Word-like
        VIEW_TYPEWRITER,         // Future: Immersive
        VIEW_PUBLISHER           // Future: Manuscript
    };

    bwxTextEditor(wxWindow* parent, wxWindowID id, /*...*/);

    // View mode (MVP: only VIEW_FULL)
    void SetViewMode(ViewMode mode);
    ViewMode GetViewMode() const { return m_viewMode; }

    // Document access
    bwxTextDocument& GetDocument() { return m_document; }

    // File I/O
    bool LoadFromFile(const wxString& path);
    bool SaveToFile(const wxString& path);

    // Editing (convenience methods)
    void Copy();
    void Cut();
    void Paste();
    void SelectAll();
    void Undo() { m_document.Undo(); }
    void Redo() { m_document.Redo(); }

protected:
    // wxWidgets overrides
    wxSize DoGetBestSize() const override;
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnChar(wxKeyEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);
    void OnSetFocus(wxFocusEvent& event);

    // IDocumentObserver
    void OnTextChanged() override;
    void OnCursorMoved() override;

private:
    bwxTextDocument m_document;
    ViewMode m_viewMode = VIEW_FULL;
    std::unique_ptr<ITextRenderer> m_renderer;
    wxTimer* m_caretTimer = nullptr;
    bool m_caretVisible = true;

    void HandleCharInput(wxChar ch);
    void HandleKeyCommand(int keyCode, bool ctrl, bool shift);
    void UpdateCaret();
};
```

### File Format (.ktxt - Kalahari Text)

**Format:** JSON (human-readable, extensible, version-controlled)

**Example:**
```json
{
  "version": "1.0",
  "format": "kalahari-text",

  "metadata": {
    "title": "Chapter 1: The Beginning",
    "author": "Author Name",
    "created": "2025-11-04T10:00:00Z",
    "modified": "2025-11-04T14:30:00Z",
    "wordCount": 1523,
    "characterCount": 8945
  },

  "content": {
    "text": "The quick brown fox jumps over the lazy dog...",

    "formatting": [
      {"start": 0, "end": 9, "bold": true, "fontSize": 14},
      {"start": 20, "end": 30, "italic": true, "textColor": "#FF0000"}
    ]
  },

  "viewSettings": {
    "lastViewMode": "full",
    "zoom": 1.0,
    "cursorPosition": 150
  }
}
```

**Why JSON?**
- ✅ Human-readable (debugging, version control, diffs)
- ✅ Extensible (add new fields without breaking old files)
- ✅ Easy to parse (nlohmann_json library already in Kalahari)
- ✅ Supports advanced features (comments, footnotes, citations - future)
- ⚠️ Slightly larger than binary (acceptable trade-off)

**Future Fields (Tasks #00023-#00026):**
```json
{
  "comments": [...],      // Task #00023
  "footnotes": [...],     // Task #00024
  "citations": [...],     // Task #00025
  "indexEntries": [...]   // Task #00026
}
```

---

## 📁 File Structure

### bwx_sdk Structure

```
external/bwx_sdk/
├── include/bwx_sdk/bwx_gui/
│   ├── bwx_text_document.h         # Document model interface
│   ├── bwx_text_renderer.h         # ITextRenderer interface
│   ├── bwx_text_editor.h           # Main control interface
│   └── renderers/
│       └── full_view_renderer.h    # Full View implementation
├── src/bwx_gui/
│   ├── bwx_text_document.cpp       # ~800 lines
│   ├── bwx_text_renderer.cpp       # ~100 lines (base)
│   ├── bwx_text_editor.cpp         # ~800 lines
│   ├── renderers/
│   │   └── full_view_renderer.cpp  # ~600 lines
│   └── CMakeLists.txt              # Updated
└── tests/
    ├── test_text_document.cpp      # Unit tests (~150 lines)
    ├── test_text_editor.cpp        # Integration tests (~150 lines)
    └── test_full_view_renderer.cpp # Renderer tests (~100 lines)
```

### Kalahari Integration

```
src/gui/panels/editor_panel.cpp:
    m_editor = new bwx_sdk::gui::bwxTextEditor(this, wxID_ANY);
    sizer->Add(m_editor, 1, wxALL | wxEXPAND, 0);

src/gui/main_window.cpp:
    // Format menu
    formatMenu->Append(ID_FORMAT_BOLD, _("&Bold\tCtrl+B"));
    formatMenu->Append(ID_FORMAT_ITALIC, _("&Italic\tCtrl+I"));
    formatMenu->Append(ID_FORMAT_UNDERLINE, _("&Underline\tCtrl+U"));
```

---

## ✅ Acceptance Criteria

### Must Have (MVP)

**Core Editing:**
1. ✅ User can type text and see it on screen
2. ✅ Cursor moves with arrow keys (Up, Down, Left, Right)
3. ✅ Cursor moves to clicked position
4. ✅ Backspace deletes character before cursor
5. ✅ Delete deletes character after cursor
6. ✅ Enter creates new line
7. ✅ Text wraps at control width minus margins

**Selection:**
8. ✅ User can select text with mouse drag
9. ✅ User can extend selection with Shift+arrows
10. ✅ Ctrl+A selects all text
11. ✅ Selected text is highlighted (blue background)

**Clipboard:**
12. ✅ Ctrl+C copies selected text to clipboard
13. ✅ Ctrl+X cuts selected text
14. ✅ Ctrl+V pastes text at cursor

**Formatting:**
15. ✅ Ctrl+B makes selected text bold
16. ✅ Ctrl+I makes selected text italic
17. ✅ Ctrl+U underlines selected text
18. ✅ Format → Font dialog allows font/size selection
19. ✅ Format → Color dialog allows text color selection
20. ✅ Format → Clear Formatting removes all styles
21. ✅ Formatting is visible in editor
22. ✅ Formatting persists after save/load

**Undo/Redo:**
23. ✅ Ctrl+Z undoes last text change
24. ✅ Ctrl+Z undoes last formatting change
25. ✅ Ctrl+Y redoes undone operation
26. ✅ Undo/Redo works for at least 100 operations
27. ✅ Edit menu shows Undo/Redo status (enabled/disabled)

**File I/O:**
28. ✅ Editor loads text from .ktxt file
29. ✅ Editor saves text to .ktxt file
30. ✅ Formatting is preserved in saved file
31. ✅ Missing file handled gracefully (empty editor)
32. ✅ Directory auto-created for new files

**Metadata:**
33. ✅ Word count updates in real-time (debounced 500ms)
34. ✅ Word count visible in StatusBar
35. ✅ BookElement metadata updated on save

**Performance:**
36. ✅ 10,000 word document: keystroke latency <100ms
37. ✅ Rendering at 60 FPS (smooth scrolling, cursor blink)
38. ✅ No visible flicker (double buffering)

**Cross-Platform:**
39. ✅ Works on Linux (Ubuntu 22.04, GCC 10+)
40. ✅ Works on macOS (macOS 14, Clang 10+)
41. ✅ Works on Windows (Windows 11, MSVC 2019+)

### Should Have (Nice to Have for MVP)

42. ⚠️ Smooth scroll animation (200ms transition)
43. ⚠️ Find & Replace (Ctrl+F, Ctrl+H) - **Deferred to Phase 2**
44. ⚠️ Line numbers in margin - **Deferred to Phase 2**

### Won't Have (Explicitly Out of Scope for MVP)

45. ❌ Page View Mode - Task #00020
46. ❌ Typewriter Mode - Task #00021
47. ❌ Publisher View - Task #00022
48. ❌ Comments - Task #00023
49. ❌ Footnotes - Task #00024
50. ❌ Citations/Bibliography - Task #00025
51. ❌ Indexes - Task #00026

---

## 🧪 Testing Strategy

### Unit Tests (Catch2 v3)

**bwxTextDocument Tests (test_text_document.cpp):**
```cpp
TEST_CASE("Text operations", "[document]") {
    bwxTextDocument doc;

    SECTION("Insert text") {
        doc.SetText("Hello");
        doc.InsertText(5, " World");
        REQUIRE(doc.GetText() == "Hello World");
    }

    SECTION("Delete text") {
        doc.SetText("Hello World");
        doc.DeleteText(5, 11);  // Delete " World"
        REQUIRE(doc.GetText() == "Hello");
    }
}

TEST_CASE("Formatting", "[document]") {
    bwxTextDocument doc;
    doc.SetText("Hello World");

    TextFormat boldFormat;
    boldFormat.bold = true;
    doc.ApplyFormat(0, 5, boldFormat);

    REQUIRE(doc.GetFormatAt(0).bold == true);
    REQUIRE(doc.GetFormatAt(6).bold == false);
}

TEST_CASE("Undo/Redo", "[document]") {
    bwxTextDocument doc;
    doc.SetText("Hello");
    doc.InsertText(5, " World");

    doc.Undo();
    REQUIRE(doc.GetText() == "Hello");

    doc.Redo();
    REQUIRE(doc.GetText() == "Hello World");
}
```

**FullViewRenderer Tests (test_full_view_renderer.cpp):**
```cpp
TEST_CASE("Hit testing", "[renderer]") {
    FullViewRenderer renderer;
    bwxTextDocument doc;
    doc.SetText("Hello\nWorld");
    renderer.Initialize(doc);

    // Click at (10, 5) should hit first line
    int pos = renderer.HitTest(10, 5);
    REQUIRE(pos >= 0);
    REQUIRE(pos <= 5);
}
```

### Integration Tests (test_text_editor.cpp)

**End-to-End Workflows:**
```cpp
TEST_CASE("Edit workflow", "[editor]") {
    bwxTextEditor editor;

    // Type text
    editor.GetDocument().InsertText(0, "Hello");

    // Apply formatting
    TextFormat boldFormat;
    boldFormat.bold = true;
    editor.GetDocument().ApplyFormat(0, 5, boldFormat);

    // Save
    REQUIRE(editor.SaveToFile("test.ktxt"));

    // Load
    bwxTextEditor editor2;
    REQUIRE(editor2.LoadFromFile("test.ktxt"));

    // Verify
    REQUIRE(editor2.GetDocument().GetText() == "Hello");
    REQUIRE(editor2.GetDocument().GetFormatAt(0).bold == true);
}
```

### Manual Testing

**Interactive Test Scenarios:**
1. **Typing Test:** Type 100 words, verify smooth performance
2. **Selection Test:** Select text with mouse, Shift+arrows, Ctrl+A
3. **Clipboard Test:** Copy, Cut, Paste between editor instances
4. **Formatting Test:** Apply bold/italic/underline, change font/size/color
5. **Undo/Redo Test:** Make 20 changes, undo all, redo all
6. **File I/O Test:** Save, close app, reopen, load - verify all preserved
7. **Performance Test:** Load 10,000 word document, type - verify <100ms latency
8. **Cross-Platform Test:** Repeat all tests on Linux, macOS, Windows

### Performance Profiling

**Benchmarks:**
```bash
# Keystroke latency
./benchmark_keystroke  # Target: <100ms for 10K words

# Rendering FPS
./benchmark_rendering  # Target: 60 FPS

# Memory usage
valgrind --tool=massif ./kalahari_test  # Target: <50MB for 10K words
```

---

## 📊 Progress Tracking

### Implementation Checklist (MVP)

**Day 1-2: Document Model Foundation**
- [ ] Create bwx_text_document.h interface
- [ ] Implement GapBufferStorage class (ITextStorage)
- [ ] Implement text operations (insert, delete, get)
- [ ] Implement cursor management
- [ ] Implement selection management
- [ ] Write unit tests for text operations

**Day 3-4: Undo/Redo System**
- [ ] Design ICommand interface
- [ ] Implement InsertTextCommand
- [ ] Implement DeleteTextCommand
- [ ] Implement command stack (undo/redo)
- [ ] Add undo limit (100 commands)
- [ ] Write unit tests for undo/redo

**Day 5-6: Formatting System**
- [ ] Design TextFormat struct
- [ ] Implement formatting storage (map<int, TextFormat>)
- [ ] Implement ApplyFormat()
- [ ] Implement GetFormatAt()
- [ ] Implement format runs for rendering
- [ ] Write unit tests for formatting

**Day 7-8: Full View Renderer**
- [ ] Create FullViewRenderer class (ITextRenderer)
- [ ] Implement layout calculation (line breaks, word wrap)
- [ ] Implement text rendering (with formatting)
- [ ] Implement cursor rendering (blinking caret)
- [ ] Implement selection rendering (highlight)
- [ ] Implement hit testing (screen → document position)
- [ ] Write unit tests for hit testing

**Day 9-10: Main Control & Event Handling**
- [ ] Create bwxTextEditor class (wxControl)
- [ ] Implement two-phase construction (Init, Create)
- [ ] Implement OnPaint (call renderer.Render)
- [ ] Implement OnChar (character input)
- [ ] Implement OnKeyDown (arrow keys, Home/End, Backspace/Delete)
- [ ] Implement OnLeftDown (cursor position)
- [ ] Implement OnMotion (selection)
- [ ] Implement focus handling (caret blink)
- [ ] Implement clipboard (Copy, Cut, Paste)

**Day 11-12: File I/O & Integration**
- [ ] Implement .ktxt format (JSON)
- [ ] Implement LoadFromFile()
- [ ] Implement SaveToFile()
- [ ] Integrate with EditorPanel (Kalahari)
- [ ] Add Format menu to MainWindow
- [ ] Add Format menu handlers (Bold, Italic, Underline, Font, Color)
- [ ] Add word count timer (500ms debounce)
- [ ] Update StatusBar with word count
- [ ] Write integration tests (end-to-end workflows)

**Day 13-14: Testing & Polish**
- [ ] Run all unit tests (fix failures)
- [ ] Run integration tests (fix failures)
- [ ] Manual testing on Linux (Debug + Release)
- [ ] Manual testing on macOS (Debug + Release)
- [ ] Manual testing on Windows (Debug + Release)
- [ ] Performance profiling (identify bottlenecks)
- [ ] Fix bugs found in testing
- [ ] Optimize performance if needed
- [ ] Update documentation

**Day 15: Final Review & Commit**
- [ ] Code review (check against patterns from `.claude/skills/kalahari-bwx-custom-controls.md`)
- [ ] Verify zero compiler warnings (all platforms)
- [ ] Verify all acceptance criteria met
- [ ] Commit to bwx_sdk repo
- [ ] Update Kalahari submodule
- [ ] Update CHANGELOG.md
- [ ] Update ROADMAP.md

---

## 🚨 Risks & Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Gap Buffer performance issues | High | Low | Profile with 10K+ words; can migrate to Rope if needed |
| wxGraphicsContext overhead | Medium | Medium | Use wxDC for text rendering (simpler, faster); wxGraphicsContext for antialiasing only if needed |
| Layout calculation slow | High | Medium | Cache layout; only recalculate on resize/text change; render visible lines only |
| Cross-platform rendering differences | Medium | High | Test early on all 3 platforms; use platform-specific renderer selection if needed |
| Undo/Redo memory consumption | Medium | Low | Limit to 100 commands (configurable); optimize command storage |
| Schedule overrun (2-3 weeks → 4 weeks) | High | Medium | **Prioritize MVP features**; defer nice-to-haves; use skill workflow for efficiency |
| Integration issues with Kalahari | Medium | Low | Design clean interface (bwxTextEditor control); test standalone before integration |

---

## 📚 Resources

### Internal Documentation

- **Complete Architecture:** `project_docs/15_text_editor_architecture.md` (15,000+ words, all 4 view modes + advanced features)
- **Custom Control Patterns:** `project_docs/14_bwx_sdk_patterns.md` (architectural rationale)
- **Implementation Workflow:** `.claude/skills/kalahari-bwx-custom-controls.md` (step-by-step guide, 3,000+ lines)
- **bwx_sdk Integration:** Serena memory `bwx_sdk_kalahari_integration_strategy_MASTER`
- **Custom Control Template:** Serena memory `bwx_sdk_custom_control_template_comprehensive`

### External References

**wxWidgets Documentation:**
- wxControl: https://docs.wxwidgets.org/3.3/classwx_control.html
- wxDC: https://docs.wxwidgets.org/3.3/classwx_d_c.html
- wxGraphicsContext: https://docs.wxwidgets.org/3.3/classwx_graphics_context.html
- wxBufferedDC: https://docs.wxwidgets.org/3.3/classwx_buffered_d_c.html

**Text Editor Data Structures:**
- Gap Buffer: https://en.wikipedia.org/wiki/Gap_buffer
- Rope: https://en.wikipedia.org/wiki/Rope_(data_structure)
- Piece Table: https://en.wikipedia.org/wiki/Piece_table
- VS Code Text Buffer: https://code.visualstudio.com/blogs/2018/03/23/text-buffer-reimplementation

**Similar Projects:**
- Scrivener (text editor for writers)
- Notepad++ (custom text editor control)
- Sublime Text (rendering engine)

### Context7 MCP

**Before implementing, check current wxWidgets documentation:**
```
1. resolve-library-id("wxWidgets")
2. get-library-docs(id, topic="wxControl custom control")
3. get-library-docs(id, topic="wxDC text rendering")
4. get-library-docs(id, topic="wxGraphicsContext antialiasing")
```

---

## 📝 Notes

### Extensibility for Future Phases

**Architecture designed for:**
- ✅ **4 View Modes:** Full (MVP), Page (#00020), Typewriter (#00021), Publisher (#00022)
- ✅ **Advanced Features:** Comments (#00023), Footnotes (#00024), Citations (#00025), Indexes (#00026)
- ✅ **File Format:** .ktxt JSON supports all future fields
- ✅ **Document Model:** Extensible (add fields without breaking MVP)
- ✅ **Renderer Strategy:** Swappable (add new renderers without changing document/control)

**Interfaces allow:**
- New view modes: Implement ITextRenderer, swap in bwxTextEditor::SetViewMode()
- New features: Add fields to bwxTextDocument, update .ktxt format (versioned)
- Migration: Swap GapBufferStorage for RopeStorage (both implement ITextStorage)

### Dependencies on Other Systems

**Kalahari Integration:**
- EditorPanel.cpp: Create bwxTextEditor control, add to sizer
- MainWindow.cpp: Add Format menu, connect to editor
- StatusBar: Update with word count (editor emits event)
- BookElement: Load/save .ktxt file path (relative to .klh)

**bwx_sdk:**
- Custom control patterns (from Tasks #00017, #00018)
- Coding style (tabs, namespaces, RTTI macros)
- Build system (CMakeLists.txt)
- Git submodule workflow

### Known Limitations (MVP)

**Not Implemented in Phase 1:**
- ❌ Page boundaries (continuous text only)
- ❌ Headers/Footers
- ❌ Page numbers
- ❌ Comments system
- ❌ Footnotes
- ❌ Citations/Bibliography
- ❌ Indexes
- ❌ Find & Replace (Ctrl+F, Ctrl+H)
- ❌ Spell checking
- ❌ Track changes
- ❌ Collaborative editing

**All above deferred to Tasks #00020-#00026 (Phases 2-8, 17 additional weeks)**

---

**Created:** 2025-11-01
**Last Updated:** 2025-11-04
**Version:** 3.0 (MVP Specification - Full View Only)
**Related Tasks:** #00020 (Page View), #00021 (Typewriter), #00022 (Publisher), #00023-#00026 (Advanced Features)
