# Task #00014: wxRichTextCtrl Integration + Basic Formatting

**Status:** 📋 Planned
**Priority:** P0 (Critical - Core)
**Phase:** Phase 1 Week 10
**Estimated Duration:** 1 week
**Assigned To:** TBD
**Dependencies:** Task #00013 (wxAUI panels ready)

---

## 🎯 Objective

Integrate wxRichTextCtrl into EditorPanel to create a functional rich text editor with basic formatting (bold, italic, underline, font selection, text alignment). This is the core writing interface for Kalahari.

---

## 📋 Requirements

### Functional Requirements
1. **wxRichTextCtrl Integration**
   - Create wxRichTextCtrl in EditorPanel (center pane)
   - Configure text control: multiline, rich text, auto URL detection
   - Handle text events: text changed, cursor moved, selection changed

2. **Basic Text Formatting**
   - **Bold:** Ctrl+B, toolbar button, Format menu
   - **Italic:** Ctrl+I, toolbar button, Format menu
   - **Underline:** Ctrl+U, toolbar button, Format menu
   - Toggle behavior: apply/remove formatting

3. **Font Selection**
   - Font family dropdown (toolbar): Arial, Times New Roman, Courier, etc.
   - Font size dropdown (toolbar): 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 36, 48
   - Apply to selection or insertion point

4. **Font Color**
   - Text color picker (toolbar button)
   - Recent colors (5 most recent)
   - Apply to selection

5. **Text Alignment**
   - Align left: Ctrl+L
   - Align center: Ctrl+E
   - Align right: Ctrl+R
   - Align justify: Ctrl+J
   - Toolbar buttons + Format menu

6. **Event Handling**
   - Text changed event → mark document as modified
   - Cursor moved event → update status bar (line:column)
   - Selection changed event → update format toolbar (bold/italic/underline state)

### Non-Functional Requirements
- **Performance:** Typing feels responsive (<50ms latency)
- **Usability:** Formatting applies immediately with visual feedback
- **Compatibility:** RTF format for save/load (native wxRichTextCtrl support)

---

## 🔧 Technical Approach

### Architecture
```
EditorPanel (wxPanel)
└── wxRichTextCtrl (m_richText)
    ├── Event Handlers
    │   ├── EVT_TEXT (onTextChanged)
    │   ├── EVT_RICHTEXT_SELECTION_CHANGED (onSelectionChanged)
    │   └── EVT_RICHTEXT_CONTENT_INSERTED (onContentInserted)
    └── Formatting Commands
        ├── ApplyBoldToSelection()
        ├── ApplyItalicToSelection()
        ├── ApplyUnderlineToSelection()
        ├── ApplyFontToSelection()
        └── ApplyAlignmentToSelection()
```

### Implementation Details

**1. EditorPanel Integration**
```cpp
// src/gui/editor_panel.cpp
EditorPanel::EditorPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    m_richText = new wxRichTextCtrl(this, wxID_ANY, wxEmptyString,
        wxDefaultPosition, wxDefaultSize,
        wxRE_MULTILINE | wxRE_READONLY | wxVSCROLL | wxHSCROLL);

    m_richText->SetFont(wxFont(12, wxFONTFAMILY_DEFAULT,
        wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

    sizer->Add(m_richText, 1, wxEXPAND);
    SetSizer(sizer);

    // Bind events
    m_richText->Bind(wxEVT_TEXT, &EditorPanel::onTextChanged, this);
    m_richText->Bind(wxEVT_RICHTEXT_SELECTION_CHANGED,
        &EditorPanel::onSelectionChanged, this);
}
```

**2. Formatting Commands**
```cpp
void EditorPanel::applyBold() {
    if (!m_richText->HasSelection()) {
        // Toggle at insertion point
        m_richText->BeginBold();
        // Or EndBold() if already bold
    } else {
        // Apply to selection
        m_richText->ApplyBoldToSelection();
    }
}

void EditorPanel::applyFont(const wxFont& font) {
    wxRichTextAttr attr;
    attr.SetFont(font);

    if (m_richText->HasSelection()) {
        m_richText->SetStyle(m_richText->GetSelectionRange(), attr);
    } else {
        m_richText->SetDefaultStyle(attr);
    }
}
```

**3. Format Toolbar Update**
```cpp
void EditorPanel::onSelectionChanged(wxRichTextEvent& event) {
    // Get format at cursor/selection
    wxRichTextAttr attr;
    m_richText->GetStyle(m_richText->GetInsertionPoint(), attr);

    // Update toolbar button states
    bool isBold = attr.GetFontWeight() == wxFONTWEIGHT_BOLD;
    bool isItalic = attr.GetFontStyle() == wxFONTSTYLE_ITALIC;
    bool isUnderlined = attr.GetFontUnderlined();

    // Notify MainWindow to update toolbar
    wxCommandEvent evt(EVT_FORMAT_CHANGED);
    evt.SetInt(isBold ? 1 : 0);
    ProcessEvent(evt);
}
```

### Files to Create/Modify

**Modified Files:**
- `include/kalahari/gui/editor_panel.h` (~100 lines: add wxRichTextCtrl, formatting methods)
- `src/gui/editor_panel.cpp` (~400 lines: full implementation)
- `include/kalahari/gui/main_window.h` (~20 lines: format toolbar buttons)
- `src/gui/main_window.cpp` (~100 lines: Format menu, toolbar handlers)
- `tests/gui/test_editor_panel.cpp` (~200 lines: formatting tests)

**Total Estimated LOC:** ~900 lines

---

## ✅ Acceptance Criteria

### Must Have
1. ✅ wxRichTextCtrl displays in EditorPanel
2. ✅ User can type and edit text
3. ✅ Bold, Italic, Underline formatting works (Ctrl+B/I/U)
4. ✅ Font family and size selectors work
5. ✅ Text alignment works (left, center, right, justify)
6. ✅ Format toolbar reflects current selection formatting
7. ✅ Text changes mark document as modified

### Should Have
8. ✅ Font color picker works
9. ✅ Keyboard shortcuts for alignment (Ctrl+L/E/R/J)
10. ✅ Format menu with all formatting options

---

## 🧪 Testing Strategy

### Unit Tests (Catch2)
1. Apply bold to selection
2. Apply italic to empty selection (insertion point)
3. Apply font family/size
4. Apply text color
5. Apply alignment
6. Get format at cursor

### Integration Tests
1. Type text → apply bold → verify formatting
2. Select text → change font → verify all text updated
3. Multi-paragraph alignment

### Manual Testing
1. Type 500-word document, apply various formats
2. Test performance (no lag while typing)
3. Test on all platforms

---

## 📊 Progress Tracking

### Checklist

#### Phase 1: Core Integration (Day 1-2)
- [ ] Update EditorPanel with wxRichTextCtrl
- [ ] Implement basic text input/display
- [ ] Test text typing and editing

#### Phase 2: Basic Formatting (Day 3-4)
- [ ] Implement Bold, Italic, Underline
- [ ] Add Format toolbar buttons
- [ ] Add Format menu items
- [ ] Implement keyboard shortcuts

#### Phase 3: Advanced Formatting (Day 5-6)
- [ ] Implement font family/size selectors
- [ ] Implement font color picker
- [ ] Implement text alignment
- [ ] Update format toolbar on selection change

#### Phase 4: Testing (Day 7)
- [ ] Write unit tests
- [ ] Write integration tests
- [ ] Manual testing on all platforms
- [ ] Performance testing

---

## 🚨 Risks & Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| wxRichTextCtrl API complexity | High | High | Study samples, prototype first |
| Performance with large documents | Medium | Medium | Test with 50,000+ word documents |
| Platform-specific rendering | Low | Low | Test early on all platforms |

---

## 📚 Resources

- **wxRichTextCtrl Docs:** https://docs.wxwidgets.org/3.3/classwx_rich_text_ctrl.html
- **wxWidgets Sample:** `samples/richtext/richtext.cpp`
- **RTF Format Reference:** https://www.microsoft.com/typography/rtf/rtf.html

---

**Created:** 2025-10-31
**Last Updated:** 2025-10-31
**Version:** 1.0
