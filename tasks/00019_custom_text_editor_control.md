# Task #00019: Custom wxWidgets Text Editor Control

**Status:** 📋 Planned (Next task after Phase 0)
**Priority:** P0 (Critical - Core Functionality)
**Phase:** Phase 1 Week 10-12
**Estimated Duration:** 2-3 weeks
**Assigned To:** TBD
**Dependencies:** Task #00013 (wxAUI completed), Task #00017 (bwx_sdk integration), Task #00018 (bwx_sdk refactoring), Phase 0 (Document Model)

**Rationale for Task #00019 Number:**
This is the logical continuation of bwx_sdk integration (Tasks #00017, #00018). The custom text editor control will be built using bwx_sdk patterns and utilities, making it a direct application of the bwx_sdk foundation.

---

## 🎯 Objective

Implement a custom wxWidgets-based text editor control for EditorPanel, leveraging bwx_sdk patterns and utilities. This custom control provides full control over features, performance, and architecture, avoiding limitations of wxRichTextCtrl and complexity of web-based solutions.

**Strategic Decision Context:**
- ❌ wxRichTextCtrl rejected (Task #00014_01) - insufficient feature control
- ❌ TipTap+wxWebView rejected (Task #00016) - browser overhead, complexity
- ✅ Custom wxWidgets control approved - native performance, full control, bwx_sdk integration

---

## 📋 Requirements

### Functional Requirements

1. **wxRichTextCtrl Integration**
   - Replace wxTextCtrl stub with wxRichTextCtrl
   - Full multiline text editing with word wrap
   - Undo/Redo (built-in wxRichTextCtrl)
   - Standard keyboard shortcuts (Ctrl+Z, Ctrl+Y, Ctrl+B, Ctrl+I)

2. **Text Formatting**
   - **Bold**: Ctrl+B or Format menu
   - **Italic**: Ctrl+I or Format menu
   - **Underline**: Ctrl+U or Format menu
   - **Font selection**: Dialog with system fonts
   - **Font size**: 8pt - 72pt, default 12pt
   - **Text color**: Color picker (black default)
   - **Clear formatting**: Remove all styles

3. **Load/Save Integration with BookElement**
   - Load RTF from `BookElement::getFile()` path
   - Save RTF to same path
   - Auto-create directory structure if missing
   - Handle missing files gracefully (new chapter = empty editor)
   - Update `BookElement::modified` timestamp on save

4. **Word Count Integration**
   - Real-time word count calculation
   - Update StatusBar on text change (debounced 500ms)
   - Update BookElement::wordCount on save
   - Trigger StatisticsPanel refresh

5. **Menu Integration**
   - **Format menu** (new): Bold, Italic, Underline, Font, Clear Formatting
   - **Edit menu enhancements**: Cut, Copy, Paste, Select All (wxRichTextCtrl events)
   - File → Save: Trigger editor save

6. **Keyboard Shortcuts**
   - Ctrl+B: Toggle Bold
   - Ctrl+I: Toggle Italic
   - Ctrl+U: Toggle Underline
   - Ctrl+S: Save current chapter
   - Ctrl+Z: Undo
   - Ctrl+Y / Ctrl+Shift+Z: Redo
   - Ctrl+A: Select All

### Non-Functional Requirements

- **Performance**: Smooth editing for 10,000 word chapters (<100ms keystroke latency)
- **Memory**: Lazy content loading (only current chapter in memory)
- **UX**: No blocking operations (file I/O in background if >1MB)
- **Compatibility**: RTF 1.9 format (wxRichTextCtrl native)

---

## 🔧 Technical Approach

### Architecture

```
EditorPanel (wxPanel)
├── wxRichTextCtrl (m_richTextCtrl)
│   ├── Content Buffer (RTF document)
│   ├── Undo/Redo Stack (built-in)
│   └── Style Manager (wxTextAttrEx)
├── Format Menu Handler
├── Load/Save Manager
│   ├── LoadFromBookElement(BookElement*)
│   └── SaveToBookElement(BookElement*)
└── Word Count Timer (500ms debounce)

MainWindow
├── Format Menu (new)
│   ├── Bold (Ctrl+B)
│   ├── Italic (Ctrl+I)
│   ├── Underline (Ctrl+U)
│   ├── Font... (dialog)
│   └── Clear Formatting
└── StatusBar Update (from EditorPanel event)
```

### Implementation Details

**1. wxRichTextCtrl Creation**

```cpp
// src/gui/panels/editor_panel.h
#include <wx/richtext/richtextctrl.h>

class EditorPanel : public wxPanel {
public:
    explicit EditorPanel(wxWindow* parent);

    // New public API
    bool loadChapter(const core::BookElement& element);
    bool saveChapter(core::BookElement& element);
    int getWordCount() const;
    void clearContent();

private:
    wxRichTextCtrl* m_richTextCtrl = nullptr;
    wxTimer* m_wordCountTimer = nullptr;
    core::BookElement* m_currentElement = nullptr;  // Non-owning pointer

    void setupLayout();
    void onTextChanged(wxCommandEvent& event);
    void onWordCountTimer(wxTimerEvent& event);

    // Format handlers
    void onFormatBold(wxCommandEvent& event);
    void onFormatItalic(wxCommandEvent& event);
    void onFormatUnderline(wxCommandEvent& event);
    void onFormatFont(wxCommandEvent& event);
    void onFormatClear(wxCommandEvent& event);
};
```

```cpp
// src/gui/panels/editor_panel.cpp
void EditorPanel::setupLayout() {
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    // Create wxRichTextCtrl
    m_richTextCtrl = new wxRichTextCtrl(this, wxID_ANY,
        wxEmptyString,
        wxDefaultPosition,
        wxDefaultSize,
        wxVSCROLL | wxHSCROLL | wxWANTS_CHARS);

    // Set default font
    wxFont defaultFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
                       wxFONTWEIGHT_NORMAL);
    wxTextAttr attr;
    attr.SetFont(defaultFont);
    m_richTextCtrl->SetBasicStyle(attr);

    sizer->Add(m_richTextCtrl, 1, wxALL | wxEXPAND, 0);
    SetSizer(sizer);

    // Word count timer (500ms debounce)
    m_wordCountTimer = new wxTimer(this, ID_WORDCOUNT_TIMER);

    // Bind events
    Bind(wxEVT_TEXT, &EditorPanel::onTextChanged, this, wxID_ANY);
    Bind(wxEVT_TIMER, &EditorPanel::onWordCountTimer, this, ID_WORDCOUNT_TIMER);
}
```

**2. Load/Save from BookElement**

```cpp
bool EditorPanel::loadChapter(const core::BookElement& element) {
    std::filesystem::path rtfPath = element.getFile();

    if (!std::filesystem::exists(rtfPath)) {
        // New chapter - empty editor
        m_richTextCtrl->Clear();
        m_currentElement = const_cast<core::BookElement*>(&element);
        core::Logger::getInstance().info("New chapter, editor cleared");
        return true;
    }

    // Load RTF file
    wxString wxPath = wxString::FromUTF8(rtfPath.string());
    if (m_richTextCtrl->LoadFile(wxPath, wxRICHTEXT_TYPE_RTF)) {
        m_currentElement = const_cast<core::BookElement*>(&element);
        core::Logger::getInstance().info("Loaded chapter: {}", element.getTitle());
        return true;
    } else {
        core::Logger::getInstance().error("Failed to load RTF: {}", rtfPath.string());
        wxMessageBox(wxString::Format("Failed to load chapter '%s'", element.getTitle()),
                     "Load Error", wxOK | wxICON_ERROR);
        return false;
    }
}

bool EditorPanel::saveChapter(core::BookElement& element) {
    if (!m_currentElement) return false;

    std::filesystem::path rtfPath = element.getFile();

    // Create directory structure if missing
    std::filesystem::path dir = rtfPath.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    // Save RTF
    wxString wxPath = wxString::FromUTF8(rtfPath.string());
    if (m_richTextCtrl->SaveFile(wxPath, wxRICHTEXT_TYPE_RTF)) {
        // Update metadata
        element.setWordCount(getWordCount());
        element.updateModifiedTime();

        core::Logger::getInstance().info("Saved chapter: {} ({} words)",
                                         element.getTitle(), getWordCount());
        return true;
    } else {
        core::Logger::getInstance().error("Failed to save RTF: {}", rtfPath.string());
        return false;
    }
}
```

**3. Word Count Calculation**

```cpp
int EditorPanel::getWordCount() const {
    wxString text = m_richTextCtrl->GetValue();

    // Simple word count: split by whitespace
    int count = 0;
    bool inWord = false;

    for (size_t i = 0; i < text.length(); ++i) {
        wxChar ch = text[i];
        if (wxIsspace(ch) || wxIspunct(ch)) {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            count++;
        }
    }

    return count;
}

void EditorPanel::onTextChanged(wxCommandEvent& event) {
    // Restart debounce timer
    m_wordCountTimer->Start(500, wxTIMER_ONE_SHOT);
    event.Skip();
}

void EditorPanel::onWordCountTimer(wxTimerEvent& event) {
    // Update StatusBar
    int wordCount = getWordCount();

    wxWindow* parent = GetParent();
    while (parent && !parent->IsKindOf(CLASSINFO(MainWindow))) {
        parent = parent->GetParent();
    }

    if (parent) {
        MainWindow* mainWindow = static_cast<MainWindow*>(parent);
        mainWindow->updateStatusBar(wxString::Format("Words: %d", wordCount));
    }
}
```

**4. Format Menu Integration**

```cpp
// src/gui/main_window.cpp (createMenus())

// Format menu
wxMenu* formatMenu = new wxMenu();
formatMenu->Append(ID_FORMAT_BOLD, _("&Bold\tCtrl+B"), _("Toggle bold formatting"));
formatMenu->Append(ID_FORMAT_ITALIC, _("&Italic\tCtrl+I"), _("Toggle italic formatting"));
formatMenu->Append(ID_FORMAT_UNDERLINE, _("&Underline\tCtrl+U"), _("Toggle underline"));
formatMenu->AppendSeparator();
formatMenu->Append(ID_FORMAT_FONT, _("&Font..."), _("Choose font and size"));
formatMenu->AppendSeparator();
formatMenu->Append(ID_FORMAT_CLEAR, _("&Clear Formatting"), _("Remove all formatting"));

m_menuBar->Insert(3, formatMenu, _("F&ormat"));  // Insert after Edit menu
```

**5. Event Handlers**

```cpp
// EditorPanel::onFormatBold
void EditorPanel::onFormatBold(wxCommandEvent& event) {
    m_richTextCtrl->ApplyBoldToSelection();
}

// EditorPanel::onFormatItalic
void EditorPanel::onFormatItalic(wxCommandEvent& event) {
    m_richTextCtrl->ApplyItalicToSelection();
}

// EditorPanel::onFormatUnderline
void EditorPanel::onFormatUnderline(wxCommandEvent& event) {
    m_richTextCtrl->ApplyUnderlineToSelection();
}

// EditorPanel::onFormatFont
void EditorPanel::onFormatFont(wxCommandEvent& event) {
    wxFontData data;
    data.SetInitialFont(m_richTextCtrl->GetBasicStyle().GetFont());

    wxFontDialog dialog(this, data);
    if (dialog.ShowModal() == wxID_OK) {
        wxFont font = dialog.GetFontData().GetChosenFont();

        wxTextAttr attr;
        attr.SetFont(font);
        m_richTextCtrl->ApplyStyle(attr);
    }
}

// EditorPanel::onFormatClear
void EditorPanel::onFormatClear(wxCommandEvent& event) {
    wxTextAttr attr = m_richTextCtrl->GetBasicStyle();
    m_richTextCtrl->ApplyStyle(attr);
}
```

### Files to Create/Modify

**Modified Files:**
- `include/kalahari/gui/panels/editor_panel.h` (~80 → ~150 lines: add API methods)
- `src/gui/panels/editor_panel.cpp` (~40 → ~350 lines: full implementation)
- `src/gui/main_window.h` (~340 → ~360 lines: Format menu IDs)
- `src/gui/main_window.cpp` (~1,450 → ~1,550 lines: Format menu + handlers)
- `CMakeLists.txt` (add wxrichtext library)

**No New Files Required** (using existing EditorPanel stub)

**Total Estimated LOC:** ~650 lines (implementation + integration)

---

## ✅ Acceptance Criteria

### Must Have
1. ✅ wxRichTextCtrl replaces wxTextCtrl stub
2. ✅ Load RTF from BookElement::file path
3. ✅ Save RTF back to same path
4. ✅ Bold/Italic/Underline formatting works
5. ✅ Font dialog allows font/size selection
6. ✅ Word count updates in real-time (StatusBar)
7. ✅ Ctrl+B, Ctrl+I, Ctrl+U shortcuts work
8. ✅ Ctrl+S triggers save
9. ✅ Undo/Redo works (Ctrl+Z, Ctrl+Y)
10. ✅ Format menu accessible from menubar

### Should Have
11. ✅ Word count debounced (no lag on fast typing)
12. ✅ Directory auto-creation for new chapters
13. ✅ Graceful handling of missing RTF files
14. ✅ BookElement metadata updated on save (wordCount, modified)

### Nice to Have
15. ⏳ Background file I/O for large files (>1MB) - deferred to Phase 2
16. ⏳ Paragraph styles (Heading 1, Heading 2, etc.) - deferred to Phase 2
17. ⏳ Find & Replace integration - separate task (Task #00018)

---

## 🧪 Testing Strategy

### Unit Tests
- **LoadFromBookElement**: Load existing RTF → verify content matches
- **LoadFromBookElement (missing)**: Load non-existent RTF → verify empty editor
- **SaveToBookElement**: Save text → verify RTF file created
- **SaveToBookElement (update)**: Save twice → verify metadata updated
- **getWordCount**: Various texts → verify accurate count (whitespace, punctuation)

### Integration Tests
1. **End-to-End Workflow**
   - Create BookElement → Load in Editor → Type text → Apply formatting → Save → Reload → Verify
2. **Format Persistence**
   - Apply Bold/Italic → Save → Close app → Reopen → Verify formatting intact
3. **Word Count Accuracy**
   - Type 100 words → Verify StatusBar shows "100 words"
4. **Undo/Redo**
   - Type → Format → Undo → Verify unformatted → Redo → Verify formatted

### Manual Testing
1. **Cross-Platform**
   - Windows: Test RTF load/save, formatting
   - macOS: Test RTF load/save, formatting
   - Linux: Test RTF load/save, formatting
2. **Performance**
   - Paste 10,000 word chapter → Type → Verify no lag
3. **Usability**
   - Format menu intuitive?
   - Keyboard shortcuts responsive?
   - Word count updates smoothly?

---

## 📊 Progress Tracking

### Checklist

#### Phase 1: Core Editor (Day 1-3)
- [ ] Replace wxTextCtrl with wxRichTextCtrl in EditorPanel
- [ ] Implement loadChapter(BookElement&)
- [ ] Implement saveChapter(BookElement&)
- [ ] Test load/save with sample RTF files
- [ ] Add word count calculation (getWordCount())

#### Phase 2: Formatting (Day 4-5)
- [ ] Add Format menu to MainWindow
- [ ] Implement Bold/Italic/Underline handlers
- [ ] Implement Font dialog handler
- [ ] Implement Clear Formatting handler
- [ ] Add keyboard shortcut bindings (Ctrl+B, Ctrl+I, Ctrl+U)

#### Phase 3: Integration & Polish (Day 6-8)
- [ ] Add word count timer (500ms debounce)
- [ ] Integrate StatusBar updates
- [ ] Update BookElement metadata on save
- [ ] Handle missing files gracefully
- [ ] Auto-create directory structure
- [ ] Write unit tests
- [ ] Write integration tests
- [ ] Manual testing on all platforms
- [ ] Fix bugs, optimize performance

---

## 🚨 Risks & Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| wxRichTextCtrl RTF incompatibility | High | Low | Use wxRICHTEXT_TYPE_RTF (standard), test early |
| Word count performance on large files | Medium | Medium | Debounce timer, optimize algorithm |
| RTF file corruption | High | Low | Backup before save, validate after load |
| Cross-platform RTF differences | Medium | Medium | Test on all 3 platforms, use vcpkg wxWidgets |
| Font selection limited on Linux | Low | Medium | Document limitation, fallback to defaults |

---

## 📚 Resources

- **wxWidgets Documentation:**
  - wxRichTextCtrl: https://docs.wxwidgets.org/3.3/classwx_rich_text_ctrl.html
  - wxTextAttr: https://docs.wxwidgets.org/3.3/classwx_text_attr.html
  - RTF Format: https://docs.wxwidgets.org/3.3/overview_richtextctrl.html
- **Context7 Research:** wxRichTextCtrl LoadFile/SaveFile, formatting APIs
- **Similar Projects:** Scrivener (text editor patterns), LibreOffice Writer (RTF handling)
- **BookElement API:** `include/kalahari/core/book_element.h`

---

## 📝 Notes

- EditorPanel currently has a wxTextCtrl stub (42 lines) - full replacement needed
- BookElement stores RTF file path (relative): `content/body/part-001/chapter-001.rtf`
- Document Model already supports lazy loading - only metadata loaded until chapter opened
- Word count formula: split by whitespace/punctuation, count tokens
- StatusBar update handled via MainWindow::updateStatusBar() (to be implemented)
- Navigator integration (load chapter on click) deferred to Task #00015

---

**Created:** 2025-11-01
**Last Updated:** 2025-11-01
**Version:** 1.0
