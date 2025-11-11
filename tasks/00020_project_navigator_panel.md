# Task #00020: Navigator Panel + wxAuiNotebook (3 zakładki)

**Status:** ⚠️ COMPLETE WITH BUGS (2025-11-09, Structure done, 6 bugs identified)
**Priority:** P0 (Critical - Core)
**Phase:** Phase 1 Week 13 (after Task #00019)
**Estimated Duration:** 1.5-2 weeks (12-16 hours) | **Actual:** 1 day (structure only)
**Assigned To:** Claude Code + User
**Completed:** 2025-11-09
**Known Issues:** Tasks #00021-#00030 (10 atomic fixes required)
**Dependencies:**
- ✅ Task #00013 (wxAUI panels)
- ✅ Task #00019 (Custom Text Editor Control)
- ✅ Document Model (Phase 0)

---

## 🎯 Objective

Implement Navigator Panel with **wxAuiNotebook** containing 3 tabs:
1. **Outline** - Book structure (Book → Parts → Chapters) - **FULL FUNCTIONALITY**
2. **Files** - Project files browser - **SKELETON PLACEHOLDER** (Phase 2)
3. **Libraries** - Character/Place/Item resources - **SKELETON PLACEHOLDER** (Phase 2+)

**MVP Focus:** Outline tab with full tree navigation, CRUD operations, and editor synchronization.

---

## 📊 Architecture

```
┌─ NavigatorPanel (wxPanel, wxAUI dockable) ────────────────┐
│ ┌─ wxAuiNotebook ──────────────────────────────────────┐ │
│ │ 📋 Outline │ 📁 Files │ 📚 Libraries                 │ │
│ ├──────────────────────────────────────────────────────┤ │
│ │ [Active Tab Content Area]                            │ │
│ │                                                       │ │
│ │ ┌─ OutlineTab ─────────────────────────────────────┐ │ │
│ │ │ wxTreeCtrl (Book → Parts → Chapters)            │ │ │
│ │ │                                                   │ │ │
│ │ │ 📖 My Novel                                       │ │ │
│ │ │  ├─ 📁 Front Matter                              │ │ │
│ │ │  │   └─ 📄 Prologue                              │ │ │
│ │ │  ├─ 📁 Body                                      │ │ │
│ │ │  │   ├─ 📁 Part I: The Beginning                │ │ │
│ │ │  │   │   ├─ 📄 Chapter 1                        │ │ │
│ │ │  │   │   └─ 📄 Chapter 2                        │ │ │
│ │ │  │   └─ 📁 Part II: The Journey                 │ │ │
│ │ │  │       └─ 📄 Chapter 3                        │ │ │
│ │ │  └─ 📁 Back Matter                               │ │ │
│ │ │      └─ 📄 Epilogue                              │ │ │
│ │ └───────────────────────────────────────────────────┘ │ │
│ └──────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────┘
```

---

## 📋 Requirements

### Functional Requirements

#### 1. Navigator Panel Structure
- ✅ Inherit from `wxPanel`
- ✅ Contain `wxAuiNotebook` with 3 tabs
- ✅ Dockable via wxAUI (left side, 280px default width)
- ✅ Resizable, closeable, floatable (wxAUI features)

#### 2. Tab Icons (SVG from IconRegistry)
- ✅ **Outline:** `FOLDER` icon (placeholder, will be `account_tree` later)
- ✅ **Files:** `FILE_OPEN` icon (placeholder, will be `folder_open` later)
- ✅ **Libraries:** `FOLDER` icon (placeholder, will be `library_books` later)

#### 3. Outline Tab - Full Functionality
**3.1. wxTreeCtrl Setup**
- ✅ Display Book → Parts → Chapters hierarchy
- ✅ Icons for tree nodes:
  - Book: `FILE_NEW` (placeholder, will be `menu_book` later)
  - Part: `FOLDER` (already correct)
  - Chapter: `FILE_OPEN` (placeholder, will be `description` later)
- ✅ Expand/collapse nodes
- ✅ Single selection mode

**3.2. Tree Population**
- ✅ `populateTree()` method - populate from `Document` model
- ✅ Root node: Book title (`document->getTitle()`)
- ✅ Child nodes: Parts (`frontMatter`, `body`, `backMatter`)
- ✅ Leaf nodes: Chapters within Parts
- ✅ Update tree when document structure changes

**3.3. Node Selection & Navigation**
- ✅ Click chapter node → load chapter in EditorPanel
- ✅ Two-way sync: editor shows chapter → navigator highlights node
- ✅ Double-click chapter → open in editor (if not already open)

**3.4. Context Menu (Right-Click CRUD)**
**On Part node:**
- ✅ Add Chapter
- ✅ Rename Part
- ✅ Delete Part (with confirmation)

**On Chapter node:**
- ✅ Rename Chapter
- ✅ Delete Chapter (with confirmation)
- ✅ Move Up
- ✅ Move Down

**On Book node:**
- ✅ Add Part (frontMatter/body/backMatter)
- ✅ Rename Book

**3.5. Integration with EditorPanel**
- ✅ Connect to EditorPanel via MainWindow
- ✅ Load chapter content on node click
- ✅ Highlight current chapter in tree when editor changes

#### 4. Files Tab - Skeleton Placeholder
- ✅ wxPanel with centered wxStaticText: "Files browser - Coming in Phase 2"
- ✅ Gray background to distinguish from functional tabs

#### 5. Libraries Tab - Skeleton Placeholder
- ✅ wxPanel with centered wxStaticText: "Libraries (Characters/Places/Items) - Coming in Phase 2+"
- ✅ Gray background to distinguish from functional tabs

---

## 🔧 Technical Design

### Class Structure

#### NavigatorPanel
```cpp
class NavigatorPanel : public wxPanel {
public:
    NavigatorPanel(wxWindow* parent, Document* document);
    ~NavigatorPanel();

    // Update tree when document changes
    void refresh();

    // Highlight specific chapter in tree
    void selectChapter(const std::string& chapterId);

private:
    wxAuiNotebook* m_notebook = nullptr;
    OutlineTab* m_outlineTab = nullptr;
    FilesTab* m_filesTab = nullptr;
    LibrariesTab* m_librariesTab = nullptr;

    Document* m_document = nullptr;

    void createTabs();
    void setupIcons();
};
```

#### OutlineTab
```cpp
class OutlineTab : public wxPanel {
public:
    OutlineTab(wxWindow* parent, Document* document);

    void populateTree();
    void selectChapter(const std::string& chapterId);

private:
    wxTreeCtrl* m_tree = nullptr;
    wxImageList* m_imageList = nullptr;
    Document* m_document = nullptr;

    // Event handlers
    void onTreeItemActivated(wxTreeEvent& event);
    void onTreeItemRightClick(wxTreeEvent& event);
    void onTreeSelectionChanged(wxTreeEvent& event);

    // Context menu handlers
    void onAddChapter(wxCommandEvent& event);
    void onRenameChapter(wxCommandEvent& event);
    void onDeleteChapter(wxCommandEvent& event);
    void onMoveChapterUp(wxCommandEvent& event);
    void onMoveChapterDown(wxCommandEvent& event);

    void showContextMenu(wxTreeItemId item, const wxPoint& pos);

    wxDECLARE_EVENT_TABLE();
};
```

#### FilesTab (Placeholder)
```cpp
class FilesTab : public wxPanel {
public:
    FilesTab(wxWindow* parent);

private:
    wxStaticText* m_placeholder = nullptr;
};
```

#### LibrariesTab (Placeholder)
```cpp
class LibrariesTab : public wxPanel {
public:
    LibrariesTab(wxWindow* parent);

private:
    wxStaticText* m_placeholder = nullptr;
};
```

---

## 📐 Implementation Plan

### Phase 1: Panel Structure + Notebook (3-4 hours)

**Step 1.1: Create NavigatorPanel class**
- [ ] Create files: `include/kalahari/gui/panels/navigator_panel.h`
- [ ] Create files: `src/gui/panels/navigator_panel.cpp`
- [ ] Inherit from `wxPanel`
- [ ] Add constructor with `Document*` parameter

**Step 1.2: Add wxAuiNotebook**
- [ ] Create `wxAuiNotebook` in NavigatorPanel constructor
- [ ] Set notebook style: `wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS`
- [ ] Add to panel sizer (proportion=1, wxEXPAND)

**Step 1.3: Create placeholder tabs**
- [ ] Create `FilesTab` class (wxPanel + wxStaticText placeholder)
- [ ] Create `LibrariesTab` class (wxPanel + wxStaticText placeholder)
- [ ] Add both tabs to notebook with icons (use `FOLDER`, `FILE_OPEN` from IconRegistry)

**Step 1.4: Register with PanelRegistry**
- [ ] Register NavigatorPanel in `src/gui/panel_registry.cpp`
- [ ] Panel ID: `"navigator"`
- [ ] Category: `PanelCategory::Navigation`

**Step 1.5: Add to MainWindow**
- [ ] Create NavigatorPanel instance in MainWindow
- [ ] Add to wxAUI manager: `m_auiManager.AddPane(m_navigatorPanel, wxAuiPaneInfo()...)`
- [ ] Position: left side, 280px width
- [ ] Allow: docking, resizing, closing, floating

**Verification:**
- ✅ Navigator panel visible on left side
- ✅ 3 tabs visible with icons
- ✅ Placeholder text in Files and Libraries tabs
- ✅ Panel dockable, resizable, closeable

---

### Phase 2: OutlineTab - wxTreeCtrl (3-4 hours)

**Step 2.1: Create OutlineTab class**
- [ ] Create files: `include/kalahari/gui/panels/outline_tab.h`
- [ ] Create files: `src/gui/panels/outline_tab.cpp`
- [ ] Add `wxTreeCtrl` member
- [ ] Add `wxImageList` for icons

**Step 2.2: Setup tree control**
- [ ] Create wxTreeCtrl with style: `wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT | wxTR_SINGLE`
- [ ] Create wxImageList (16x16)
- [ ] Load icons from IconRegistry:
  - Icon 0: `FILE_NEW` (Book)
  - Icon 1: `FOLDER` (Part)
  - Icon 2: `FILE_OPEN` (Chapter)
- [ ] Assign image list to tree: `m_tree->AssignImageList(m_imageList)`

**Step 2.3: Implement populateTree()**
- [ ] Clear tree: `m_tree->DeleteAllItems()`
- [ ] Add hidden root: `wxTreeItemId root = m_tree->AddRoot("Root")`
- [ ] Add Book node: `m_tree->AppendItem(root, document->getTitle(), ICON_BOOK)`
- [ ] Add Part nodes: `frontMatter`, `body`, `backMatter`
- [ ] Add Chapter nodes under each Part
- [ ] Expand Book and Body by default

**Step 2.4: Connect to Document**
- [ ] Pass `Document*` to OutlineTab constructor
- [ ] Call `populateTree()` in constructor
- [ ] Store document pointer for future updates

**Step 2.5: Add OutlineTab to NavigatorPanel**
- [ ] Create OutlineTab instance in NavigatorPanel
- [ ] Add as first tab to notebook
- [ ] Set icon: `FOLDER` (placeholder for `account_tree`)

**Verification:**
- ✅ Outline tab shows tree structure
- ✅ Icons visible for Book/Part/Chapter nodes
- ✅ Tree populated from Document model
- ✅ Nodes expandable/collapsible

---

### Phase 3: Node Selection & Editor Integration (3-4 hours)

**Step 3.1: Handle tree selection event**
- [ ] Add event handler: `onTreeSelectionChanged(wxTreeEvent& event)`
- [ ] Bind to `wxEVT_TREE_SEL_CHANGED`
- [ ] Get selected item: `wxTreeItemId item = event.GetItem()`
- [ ] Check if item is Chapter node

**Step 3.2: Load chapter in EditorPanel**
- [ ] Get chapter ID from tree item data: `ChapterItemData* data = (ChapterItemData*)m_tree->GetItemData(item)`
- [ ] Call MainWindow method: `m_mainWindow->loadChapterInEditor(chapterId)`
- [ ] MainWindow forwards to EditorPanel: `m_editorPanel->loadChapter(chapterId)`

**Step 3.3: Store chapter ID in tree item data**
- [ ] Create `ChapterItemData` class (inherits `wxTreeItemData`)
- [ ] Store chapter ID in each Chapter node
- [ ] Retrieve ID on node selection

**Step 3.4: Implement two-way sync (editor → navigator)**
- [ ] Add method: `NavigatorPanel::selectChapter(const std::string& chapterId)`
- [ ] Find tree item by chapter ID
- [ ] Call: `m_tree->SelectItem(item)`
- [ ] Call from EditorPanel when chapter loaded

**Step 3.5: Handle double-click**
- [ ] Add event handler: `onTreeItemActivated(wxTreeEvent& event)`
- [ ] Bind to `wxEVT_TREE_ITEM_ACTIVATED`
- [ ] Same logic as single-click (load chapter)

**Verification:**
- ✅ Click chapter → loads in editor
- ✅ Editor changes → tree highlights current chapter
- ✅ Double-click works same as single-click

---

### Phase 4: Context Menu CRUD Operations (4-5 hours)

**Step 4.1: Show context menu on right-click**
- [ ] Add event handler: `onTreeItemRightClick(wxTreeEvent& event)`
- [ ] Bind to `wxEVT_TREE_ITEM_RIGHT_CLICK`
- [ ] Determine node type (Book/Part/Chapter)
- [ ] Call: `showContextMenu(item, event.GetPoint())`

**Step 4.2: Create context menu for Chapter nodes**
- [ ] Menu items:
  - "Rename Chapter"
  - "Delete Chapter"
  - Separator
  - "Move Up"
  - "Move Down"
- [ ] Bind menu events to handlers

**Step 4.3: Implement Rename Chapter**
- [ ] Handler: `onRenameChapter(wxCommandEvent& event)`
- [ ] Show wxTextEntryDialog: "Enter new chapter name"
- [ ] Validate input (not empty)
- [ ] Call: `document->renameChapter(chapterId, newName)`
- [ ] Update tree node text: `m_tree->SetItemText(item, newName)`

**Step 4.4: Implement Delete Chapter**
- [ ] Handler: `onDeleteChapter(wxCommandEvent& event)`
- [ ] Show confirmation: `wxMessageBox("Delete chapter?", wxYES_NO | wxICON_WARNING)`
- [ ] If YES:
  - Call: `document->deleteChapter(chapterId)`
  - Remove from tree: `m_tree->Delete(item)`
  - If chapter open in editor: close it

**Step 4.5: Implement Move Up/Down**
- [ ] Handler: `onMoveChapterUp(wxCommandEvent& event)`
- [ ] Get sibling node above: `wxTreeItemId prevSibling = m_tree->GetPrevSibling(item)`
- [ ] If exists:
  - Call: `document->moveChapterUp(chapterId)`
  - Re-populate tree (or swap nodes)
- [ ] Same logic for Move Down

**Step 4.6: Create context menu for Part nodes**
- [ ] Menu items:
  - "Add Chapter"
  - Separator
  - "Rename Part"
  - "Delete Part" (with confirmation)

**Step 4.7: Implement Add Chapter**
- [ ] Handler: `onAddChapter(wxCommandEvent& event)`
- [ ] Show wxTextEntryDialog: "Enter chapter name"
- [ ] Call: `document->addChapter(partId, chapterName)`
- [ ] Add to tree: `m_tree->AppendItem(partItem, chapterName, ICON_CHAPTER)`

**Step 4.8: Create context menu for Book node**
- [ ] Menu items:
  - "Add Part"
  - "Rename Book"

**Verification:**
- ✅ Right-click shows correct context menu
- ✅ Rename works (Chapter, Part, Book)
- ✅ Delete works with confirmation
- ✅ Move Up/Down works
- ✅ Add Chapter works

---

### Phase 5: Polish & Testing (2-3 hours)

**Step 5.1: Icon SVG integration**
- [ ] Verify all icons load correctly
- [ ] Fallback if icon missing (wxArtProvider default)

**Step 5.2: Error handling**
- [ ] Handle null Document pointer
- [ ] Handle empty document (no chapters)
- [ ] Handle invalid chapter ID selection

**Step 5.3: Memory management**
- [ ] Check wxTreeItemData cleanup (wxWidgets handles automatically)
- [ ] Verify no memory leaks: `valgrind` on Linux

**Step 5.4: wxAUI integration**
- [ ] Test docking/undocking
- [ ] Test floating panel
- [ ] Test closing/reopening via View menu
- [ ] Save/restore panel state in perspective

**Step 5.5: Keyboard shortcuts**
- [ ] F2: Rename selected node
- [ ] Delete: Delete selected node (with confirmation)
- [ ] Ctrl+Up/Down: Move chapter

**Step 5.6: Manual testing**
- [ ] Create new document
- [ ] Add parts and chapters via context menu
- [ ] Navigate between chapters
- [ ] Rename chapters, parts, book
- [ ] Delete chapters (verify editor updates)
- [ ] Move chapters up/down
- [ ] Close panel, reopen from View menu
- [ ] Dock panel to different sides

**Verification:**
- ✅ No crashes or errors
- ✅ All CRUD operations work
- ✅ Navigation smooth and responsive
- ✅ wxAUI docking works correctly

---

## ✅ Success Criteria

### Must Have (MVP)
- [x] NavigatorPanel visible on left side (280px width)
- [x] wxAuiNotebook with 3 tabs (Outline, Files, Libraries)
- [x] Tab icons from IconRegistry (placeholder icons)
- [x] Outline tab with wxTreeCtrl showing Book → Parts → Chapters
- [x] Tree icons for Book/Part/Chapter nodes
- [x] Click chapter → loads in EditorPanel
- [x] Two-way sync: editor ↔ navigator
- [x] Context menu CRUD operations work (Add/Rename/Delete/Move)
- [x] Tree updates when document structure changes
- [x] Panel dockable, resizable, closeable via wxAUI
- [x] No memory leaks (valgrind verification)

### Nice to Have (Post-MVP)
- [ ] Drag & drop reordering in tree
- [ ] Multi-select (Ctrl+Click, Shift+Click)
- [ ] Search/filter in tree
- [ ] Keyboard navigation (arrows, F2, Delete)
- [ ] Undo/Redo for structure changes
- [ ] Custom icons (replace placeholders with Material Design SVG)

---

## 📁 Files to Create/Modify

### New Files
- `include/kalahari/gui/panels/navigator_panel.h` (~100 LOC)
- `src/gui/panels/navigator_panel.cpp` (~200 LOC)
- `include/kalahari/gui/panels/outline_tab.h` (~80 LOC)
- `src/gui/panels/outline_tab.cpp` (~350 LOC)
- `include/kalahari/gui/panels/files_tab.h` (~30 LOC - placeholder)
- `src/gui/panels/files_tab.cpp` (~40 LOC - placeholder)
- `include/kalahari/gui/panels/libraries_tab.h` (~30 LOC - placeholder)
- `src/gui/panels/libraries_tab.cpp` (~40 LOC - placeholder)

**Total New Code:** ~870 LOC

### Modified Files
- `src/gui/main_window.h` (+10 LOC - add NavigatorPanel member)
- `src/gui/main_window.cpp` (+50 LOC - create and add panel to wxAUI)
- `src/gui/panel_registry.cpp` (+15 LOC - register NavigatorPanel)
- `src/CMakeLists.txt` (+4 LOC - add new source files)

**Total Modified Code:** ~79 LOC

**Grand Total:** ~950 LOC

---

## 🧪 Testing Strategy

### Unit Tests (Catch2)
- [ ] Test OutlineTab::populateTree() with mock Document
- [ ] Test node selection returns correct chapter ID
- [ ] Test context menu item enabling/disabling logic
- [ ] Test Move Up/Down with boundary conditions (first/last chapter)

### Integration Tests
- [ ] Test NavigatorPanel ↔ EditorPanel communication
- [ ] Test Document model updates trigger tree refresh
- [ ] Test wxAUI panel docking/undocking
- [ ] Test perspective save/restore includes Navigator state

### Manual Testing Checklist
- [ ] Open Navigator panel
- [ ] Switch between 3 tabs
- [ ] Expand/collapse tree nodes
- [ ] Click chapter → verify loads in editor
- [ ] Edit chapter in editor → verify tree highlights
- [ ] Right-click nodes → verify correct context menu
- [ ] Rename chapter/part/book
- [ ] Delete chapter (verify confirmation dialog)
- [ ] Add chapter via context menu
- [ ] Move chapter up/down
- [ ] Close panel → reopen from View menu
- [ ] Dock panel to right/bottom/float
- [ ] Save perspective → restart → restore

---

## 🔗 Related Tasks

**Depends on:**
- ✅ Task #00013 - wxAUI Docking System (provides PanelRegistry, wxAUI integration)
- ✅ Task #00019 - Custom Text Editor Control (provides EditorPanel to integrate with)

**Blocks:**
- Task #00021 - Chapter Management CRUD Operations (extends context menu functionality)
- Task #00022 - Content Save/Load Integration (uses Navigator for chapter selection)

**Future Enhancements:**
- Task #TBD - Files Tab Implementation (Phase 2)
- Task #TBD - Libraries Tab Implementation (Phase 2+)
- Task #TBD - Icon Set Expansion (replace placeholder icons with Material Design)

---

## 📝 Notes

### Icon Placeholders Strategy
We're using **existing icons as placeholders** to focus on functionality:
- Outline tab: `FOLDER` → will be `account_tree` later
- Files tab: `FILE_OPEN` → will be `folder_open` later
- Libraries tab: `FOLDER` → will be `library_books` later
- Book node: `FILE_NEW` → will be `menu_book` later
- Part node: `FOLDER` → **already correct**
- Chapter node: `FILE_OPEN` → will be `description` later

**Rationale:** MVP principle - working functionality > perfect icons. Icons can be swapped later without touching business logic.

### wxAuiNotebook Tab Configuration
- **Style:** `wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE | wxAUI_NB_SCROLL_BUTTONS`
- **Top tabs:** Most common for navigation (cf. Visual Studio Code, JetBrains IDEs)
- **Tab split/move:** User can reorder tabs (future configurability)
- **Scroll buttons:** If more tabs added later (Research, Timeline, etc.)

### Future Tab Ideas (from GUI concept)
- Research / Badania (Phase 2+ - for non-fiction authors)
- Timeline / Oś czasu (Phase 2+ - for complex narratives)
- Scenes / Sceny (Phase 3+ - for screenwriters)
- Bibliography / Bibliografia (Phase 3+ - for academics)

**User requested:** "Wszystko w programie powinno być konfigurowalne!"
→ Future: Settings → Customize Navigator tabs (enable/disable, reorder)

---

## 🚧 SUBTASK: Diagnostic Log Panel (2025-11-08)

**Priority:** P0 (HIGHEST - blocks testing)
**Status:** 📋 Planned
**Estimated Duration:** 3-4 hours

### Objective
Add in-app log viewer panel (diagnostic mode only) to streamline testing without opening external log files.

### Requirements

**Panel Structure:**
- **Left:** wxTextCtrl (multi-line, read-only, monospace) - displays log lines
- **Right:** Vertical wxToolBar with 3 buttons (16px SVG icons):
  1. **Options** - opens Settings Dialog focused on Log tab
  2. **Open Log Folder** - opens OS file explorer at logs directory
  3. **Copy to Clipboard** - copies visible log text

**Position:** Bottom of main window (wxAUI), BELOW EditorPanel, visible ONLY when `m_diagnosticMode == true`

**Log Settings (in Settings Dialog, only visible in diagnostic mode):**
- Background color (RGB) - default: (60, 60, 60)
- Text color (RGB) - default: (255, 255, 255)
- Font family (dropdown) - default: "Consolas" / "Courier New" / monospace
- Font size (spinner, 8-24) - default: 12
- Ring buffer size (spinner, 1-1000) - default: 500 (last N lines)

**spdlog Integration:**
- Custom sink that appends to wxTextCtrl
- Thread-safe (GUI marshalling via wxTheApp->CallAfter)
- Ring buffer: keep only last N lines (configurable)

### Implementation Plan

**Step 1: Create LogPanel class** (1 hour)
- `include/kalahari/gui/panels/log_panel.h`
- `src/gui/panels/log_panel.cpp`
- wxTextCtrl (wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2)
- Vertical wxToolBar with 3 SVG buttons
- Ring buffer (std::deque<wxString>, max 500 lines)
- appendLog(const std::string& message) method

**Step 2: spdlog custom sink** (1 hour)
- Create `GuiLogSink` (inherits spdlog::sinks::base_sink)
- Thread-safe: wxMutexLocker + wxTheApp->CallAfter
- Register in Logger::initialize() when diagnostic mode enabled

**Step 3: Settings Dialog integration** (1 hour)
- Add "Log" tree node (only visible when diagnostic mode)
- LogSettingsPanel with:
  - wxColourPickerCtrl (background)
  - wxColourPickerCtrl (text color)
  - wxFontPickerCtrl (font + size)
  - wxSpinCtrl (ring buffer size, 1-1000)
- Save/load from SettingsManager

**Step 4: wxAUI integration** (30 min)
- Add LogPanel to MainWindow (create only if m_diagnosticMode)
- wxAuiPaneInfo: Bottom(), Height(200), Hide() initially
- Show/hide based on diagnostic mode toggle

**Step 5: Toolbar actions** (30 min)
- Options → SettingsDialog::ShowModal() + SelectTreeItem("Log")
- Open Log Folder → wxLaunchDefaultBrowser(logDir)
- Copy to Clipboard → wxTheClipboard->SetData(wxTextDataObject(logText))

### Files to Create
- `include/kalahari/gui/panels/log_panel.h` (~80 LOC)
- `src/gui/panels/log_panel.cpp` (~250 LOC)
- `include/kalahari/core/gui_log_sink.h` (~60 LOC)
- `src/core/gui_log_sink.cpp` (~100 LOC)
- `src/gui/log_settings_panel.h` (~70 LOC)
- `src/gui/log_settings_panel.cpp` (~200 LOC)

**Total:** ~760 LOC

### Success Criteria
- ✅ Log panel visible ONLY in diagnostic mode
- ✅ Real-time log updates (< 100ms latency)
- ✅ Ring buffer works (keeps last N lines)
- ✅ All 3 toolbar buttons functional
- ✅ Settings persist across sessions
- ✅ No GUI freezing (thread-safe appending)

---

## 🚀 Ready to Start

Task #00020 is now fully planned with:
- ✅ Clear MVP scope (3 tabs, Outline functional, 2 placeholders)
- ✅ Detailed implementation phases (5 phases, 12-16 hours)
- ✅ Icon strategy (use existing placeholders)
- ✅ Success criteria (10 must-have items)
- ✅ Testing strategy (unit + integration + manual)
- 🔴 **SUBTASK:** Diagnostic Log Panel (P0 - highest priority)

**Status:** Phase 2-4 COMPLETE, Subtask in progress
