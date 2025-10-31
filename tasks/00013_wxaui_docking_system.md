# Task #00013: wxAUI Docking System + Panel Management

**Status:** 📋 Planned
**Priority:** P0 (Critical - Foundation)
**Phase:** Phase 1 Week 9
**Estimated Duration:** 1 week
**Assigned To:** TBD
**Dependencies:** Phase 0 Complete

---

## 🎯 Objective

Implement wxAUI docking system to create a professional multi-panel workspace with 6 core panels (Navigator, Editor, Properties, Statistics, Search, Assistant). Users can dock, undock, resize, and save custom layouts (Perspectives).

---

## 📋 Requirements

### Functional Requirements
1. **wxAUI Manager Integration**
   - Initialize wxAuiManager in MainWindow
   - Configure default flags (allow floating, resizing, docking)
   - Manage panel lifecycle (add, remove, show, hide)

2. **6 Core Panels**
   - **Navigator Panel:** Hierarchical document structure (wxTreeCtrl)
   - **Editor Panel:** Rich text editor (wxRichTextCtrl) - center, main content
   - **Properties Panel:** Chapter/document metadata (wxPropertyGrid)
   - **Statistics Panel:** Writing stats and charts (custom panel)
   - **Search Panel:** Find & replace UI (custom panel)
   - **Assistant Panel:** AI assistant placeholder (custom panel)

3. **Panel Visibility Controls**
   - View menu: checkable items for each panel (☑ Navigator, ☑ Editor, etc.)
   - Toolbar buttons: toggle buttons for common panels
   - Keyboard shortcuts: F9-F12 for quick panel toggles

4. **Default Layout**
   - **Left:** Navigator (250px width, docked left)
   - **Center:** Editor (maximum space, center pane)
   - **Right Top:** Properties (200px width, top-right)
   - **Right Bottom:** Statistics (200px width, bottom-right)
   - **Bottom:** Search (hidden by default, floatable)
   - **Bottom Right:** Assistant (hidden by default, floatable)

5. **Perspective System**
   - Save current layout as perspective (JSON)
   - Load perspective (restore panel positions)
   - 4 default perspectives:
     - "Default": All panels visible (default layout)
     - "Writing": Navigator + Editor only
     - "Editing": Editor + Statistics + Search
     - "Research": All panels visible + Assistant
   - Perspective management: save, load, delete, rename
   - Persistence: ~/.config/kalahari/perspectives/

### Non-Functional Requirements
- **Performance:** Panel docking/undocking feels instant (<100ms)
- **Usability:** Intuitive drag-and-drop panel management
- **Persistence:** Layout survives application restart
- **Cross-Platform:** Works identically on Windows, macOS, Linux

---

## 🔧 Technical Approach

### Architecture
```
MainWindow (wxFrame)
├── wxAuiManager (m_auiManager)
│   ├── NavigatorPanel (left pane)
│   ├── EditorPanel (center pane)
│   ├── PropertiesPanel (right-top pane)
│   ├── StatisticsPanel (right-bottom pane)
│   ├── SearchPanel (bottom pane, hidden)
│   └── AssistantPanel (bottom-right pane, hidden)
├── Menu Bar
│   └── View Menu
│       ├── Navigator (checkable)
│       ├── Properties (checkable)
│       ├── Statistics (checkable)
│       ├── Search (checkable)
│       ├── Assistant (checkable)
│       ├── ---
│       └── Perspectives submenu
│           ├── Default
│           ├── Writing
│           ├── Editing
│           ├── Research
│           ├── ---
│           ├── Save Perspective...
│           └── Manage Perspectives...
└── Toolbar
    ├── Toggle Navigator (F9)
    ├── Toggle Properties (F10)
    └── Toggle Statistics (F11)
```

### Implementation Details

**1. MainWindow Initialization**
```cpp
// src/gui/main_window.cpp
void MainWindow::initializeAUI() {
    m_auiManager = new wxAuiManager(this, wxAUI_MGR_DEFAULT);

    // Create panels
    m_navigatorPanel = new NavigatorPanel(this);
    m_editorPanel = new EditorPanel(this);
    m_propertiesPanel = new PropertiesPanel(this);
    m_statisticsPanel = new StatisticsPanel(this);
    m_searchPanel = new SearchPanel(this);
    m_assistantPanel = new AssistantPanel(this);

    // Add panels to AUI manager
    m_auiManager->AddPane(m_navigatorPanel,
        wxAuiPaneInfo()
            .Name("navigator")
            .Caption("Navigator")
            .Left()
            .MinSize(200, -1)
            .BestSize(250, -1)
            .CloseButton(false)
            .MaximizeButton(false));

    m_auiManager->AddPane(m_editorPanel,
        wxAuiPaneInfo()
            .Name("editor")
            .Caption("Editor")
            .CenterPane()
            .CloseButton(false)
            .PaneBorder(false));

    // ... (other panels)

    // Load default perspective
    loadPerspective("Default");

    m_auiManager->Update();
}
```

**2. Perspective Management**
```cpp
// include/kalahari/gui/perspective_manager.h
class PerspectiveManager {
public:
    static PerspectiveManager& getInstance();

    bool savePerspective(const std::string& name, const std::string& layout);
    std::optional<std::string> loadPerspective(const std::string& name);
    std::vector<std::string> listPerspectives();
    bool deletePerspective(const std::string& name);
    bool renamePerspective(const std::string& oldName, const std::string& newName);

private:
    std::filesystem::path getPerspectivesDir();
    std::filesystem::path getPerspectiveFile(const std::string& name);
};

// Usage:
std::string layout = m_auiManager->SavePerspective();
PerspectiveManager::getInstance().savePerspective("My Layout", layout);

auto layout = PerspectiveManager::getInstance().loadPerspective("Writing");
if (layout) {
    m_auiManager->LoadPerspective(*layout);
    m_auiManager->Update();
}
```

**3. Panel Visibility Control**
```cpp
void MainWindow::onViewNavigator(wxCommandEvent& event) {
    wxAuiPaneInfo& pane = m_auiManager->GetPane("navigator");
    pane.Show(event.IsChecked());
    m_auiManager->Update();
}

void MainWindow::updateViewMenu() {
    // Sync View menu checkboxes with actual panel visibility
    m_viewNavigatorItem->Check(m_auiManager->GetPane("navigator").IsShown());
    m_viewPropertiesItem->Check(m_auiManager->GetPane("properties").IsShown());
    // ... (other panels)
}
```

**4. Default Perspectives (JSON)**
```json
{
  "Default": "layout2|name=navigator;caption=Navigator;state=2044;dir=4;layer=0;row=0;pos=0;...",
  "Writing": "layout2|name=navigator;caption=Navigator;state=2044;dir=4;layer=0;...",
  "Editing": "layout2|name=editor;caption=Editor;state=768;dir=5;layer=0;...",
  "Research": "layout2|name=navigator;caption=Navigator;state=2044;dir=4;..."
}
```

### Files to Create/Modify

**New Files:**
- `include/kalahari/gui/perspective_manager.h` (~80 lines)
- `src/gui/perspective_manager.cpp` (~200 lines)
- `include/kalahari/gui/navigator_panel.h` (~50 lines, stub for Task #00015)
- `src/gui/navigator_panel.cpp` (~80 lines, stub)
- `include/kalahari/gui/editor_panel.h` (~50 lines, stub for Task #00014)
- `src/gui/editor_panel.cpp` (~80 lines, stub)
- `include/kalahari/gui/properties_panel.h` (~50 lines, stub)
- `src/gui/properties_panel.cpp` (~80 lines, stub)
- `include/kalahari/gui/statistics_panel.h` (~50 lines, stub)
- `src/gui/statistics_panel.cpp` (~80 lines, stub)
- `include/kalahari/gui/search_panel.h` (~50 lines, stub)
- `src/gui/search_panel.cpp` (~80 lines, stub)
- `include/kalahari/gui/assistant_panel.h` (~50 lines, stub)
- `src/gui/assistant_panel.cpp` (~80 lines, stub)
- `tests/gui/test_perspective_manager.cpp` (~200 lines)
- `data/perspectives/default.json` (default perspectives)

**Modified Files:**
- `include/kalahari/gui/main_window.h` (~30 lines: add wxAuiManager, panels)
- `src/gui/main_window.cpp` (~150 lines: AUI initialization, menu handlers)
- `CMakeLists.txt` (add new source files)

**Total Estimated LOC:** ~1,500 lines (implementation + tests + stubs)

---

## ✅ Acceptance Criteria

### Must Have
1. ✅ wxAUI manager initialized and working
2. ✅ 6 core panels created (even if stubs)
3. ✅ Panels can be docked, undocked, resized
4. ✅ View menu controls panel visibility
5. ✅ Default layout loads on startup
6. ✅ Perspective save/load works
7. ✅ 4 default perspectives available
8. ✅ Layout persists across application restarts
9. ✅ Works on Windows, macOS, Linux

### Should Have
10. ✅ Toolbar buttons for common panels
11. ✅ Keyboard shortcuts (F9-F12)
12. ✅ Perspective management UI (save, load, delete, rename)

### Nice to Have
13. ⏳ Panel drag-and-drop visual feedback (built-in wxAUI)
14. ⏳ Perspective preview thumbnails (deferred to Phase 2)

---

## 🧪 Testing Strategy

### Unit Tests (Catch2)
1. **PerspectiveManager Tests** (15 test cases)
   - Save perspective
   - Load perspective
   - List perspectives
   - Delete perspective
   - Rename perspective
   - Edge cases: invalid names, missing files

### Integration Tests
1. **Panel Lifecycle**
   - Create panels → add to AUI → show → hide → remove
2. **Layout Persistence**
   - Set layout → save → restart app → verify layout restored
3. **Perspective Switching**
   - Load "Default" → switch to "Writing" → verify panels hidden/shown

### Manual Testing
1. **Cross-Platform**
   - Windows: Test panel docking on Windows 10/11
   - macOS: Test panel docking on macOS ARM64
   - Linux: Test panel docking on Ubuntu 22.04
2. **Usability**
   - Drag panels to different positions
   - Float panels (undock)
   - Maximize/restore panels
   - Close/reopen panels via View menu
3. **Performance**
   - Panel docking feels instant (<100ms)
   - No lag when switching perspectives

---

## 📊 Progress Tracking

### Checklist

#### Phase 1: Core Implementation (Day 1-3)
- [ ] Create panel stub classes (Navigator, Editor, Properties, Statistics, Search, Assistant)
- [ ] Initialize wxAuiManager in MainWindow
- [ ] Add panels to AUI manager with default layout
- [ ] Implement View menu handlers (show/hide panels)
- [ ] Test basic panel visibility toggling

#### Phase 2: Perspective System (Day 4-5)
- [ ] Implement PerspectiveManager (save, load, list, delete, rename)
- [ ] Create 4 default perspectives (JSON)
- [ ] Implement Perspectives submenu (load perspective)
- [ ] Implement "Save Perspective..." dialog
- [ ] Test perspective switching

#### Phase 3: Polish & Testing (Day 6-7)
- [ ] Add toolbar buttons for common panels
- [ ] Implement keyboard shortcuts (F9-F12)
- [ ] Write unit tests for PerspectiveManager
- [ ] Write integration tests for panel lifecycle
- [ ] Manual testing on all platforms
- [ ] Fix bugs, optimize performance

---

## 🚨 Risks & Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| wxAUI API complexity | High | Medium | Study wxWidgets AUI sample, read docs |
| Cross-platform layout differences | Medium | Low | Test early on all platforms |
| Performance issues (large layouts) | Medium | Low | Use wxAUI built-in optimization |
| Perspective JSON corruption | High | Low | Validate JSON on load, fallback to default |

---

## 📚 Resources

- **wxWidgets Documentation:** https://docs.wxwidgets.org/3.3/classwx_aui_manager.html
- **wxWidgets AUI Sample:** `samples/aui/aui.cpp` (wxWidgets source)
- **Perspective Persistence:** JSON serialization with nlohmann_json
- **Similar Projects:** Code::Blocks IDE (uses wxAUI extensively)

---

## 📝 Notes

- Panel stubs will be implemented in this task (basic skeleton only)
- Full panel implementations happen in subsequent tasks:
  - Navigator: Task #00015
  - Editor: Task #00014
  - Properties/Statistics/Search/Assistant: Phase 1 later tasks
- wxAUI is mature and well-tested (used in Code::Blocks, wxSmith, etc.)

---

**Created:** 2025-10-31
**Last Updated:** 2025-10-31
**Version:** 1.0
