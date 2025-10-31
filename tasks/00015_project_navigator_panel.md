# Task #00015: Project Navigator Panel + wxTreeCtrl

**Status:** 📋 Planned
**Priority:** P0 (Critical - Core)
**Phase:** Phase 1 Week 11
**Estimated Duration:** 1 week
**Assigned To:** TBD
**Dependencies:** Task #00013 (wxAUI panels), Document Model (Phase 0)

---

## 🎯 Objective

Implement Project Navigator panel using wxTreeCtrl to display hierarchical book structure (Book → Parts → Chapters). Users can navigate document structure visually, with node selection synchronized to editor content.

---

## 📋 Requirements

### Functional Requirements
1. **wxTreeCtrl Integration**
   - Create wxTreeCtrl in NavigatorPanel
   - Display Book → Parts → Chapters hierarchy
   - Custom icons for each node type (book, part, chapter)
   - Expand/collapse nodes

2. **Tree Population**
   - Populate tree from Document model (Phase 0)
   - Root node: Book title
   - Child nodes: Parts (frontMatter, body, backMatter)
   - Leaf nodes: Chapters within Parts
   - Update tree when document structure changes

3. **Node Selection**
   - Click node → load chapter in editor
   - Synchronize selection: editor shows chapter → navigator highlights node
   - Double-click chapter → open in editor (if not already open)

4. **Context Menu (Right-Click)**
   - **On Part node:**
     - Add Chapter
     - Rename Part
     - Delete Part (with confirmation)
   - **On Chapter node:**
     - Rename Chapter
     - Delete Chapter (with confirmation)
     - Move Up / Move Down
   - **On Book node:**
     - Add Part
     - Document Properties...

5. **Visual Design**
   - Icons: 📚 Book, 📂 Part, 📄 Chapter
   - Font: Bold for current chapter
   - Color: Dimmed for empty chapters (0 words)
   - Tooltip: Show word count on hover

### Non-Functional Requirements
- **Performance:** Tree population <100ms for 100+ chapters
- **Usability:** Intuitive navigation, clear visual hierarchy
- **Responsiveness:** Node selection feels instant

---

## 🔧 Technical Approach

### Architecture
```
NavigatorPanel (wxPanel)
├── wxTreeCtrl (m_treeCtrl)
│   ├── Root Node (Book)
│   ├── Part Nodes (frontMatter, body, backMatter)
│   └── Chapter Nodes (leaves)
├── wxImageList (m_imageList)
│   ├── Icon 0: Book (📚)
│   ├── Icon 1: Part (📂)
│   └── Icon 2: Chapter (📄)
└── Context Menu
    ├── MenuItem: Add Chapter
    ├── MenuItem: Rename
    ├── MenuItem: Delete
    └── MenuItem: Move Up/Down
```

### Implementation Details

**1. NavigatorPanel Integration**
```cpp
// src/gui/navigator_panel.cpp
NavigatorPanel::NavigatorPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    m_treeCtrl = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition,
        wxDefaultSize, wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT |
        wxTR_SINGLE | wxTR_EDIT_LABELS);

    // Create image list
    m_imageList = new wxImageList(16, 16);
    m_imageList->Add(wxArtProvider::GetIcon(wxART_FOLDER, wxART_TOOLBAR, {16, 16}));  // Book
    m_imageList->Add(wxArtProvider::GetIcon(wxART_FOLDER_OPEN, wxART_TOOLBAR, {16, 16})); // Part
    m_imageList->Add(wxArtProvider::GetIcon(wxART_NORMAL_FILE, wxART_TOOLBAR, {16, 16})); // Chapter
    m_treeCtrl->AssignImageList(m_imageList);

    sizer->Add(m_treeCtrl, 1, wxEXPAND);
    SetSizer(sizer);

    // Bind events
    m_treeCtrl->Bind(wxEVT_TREE_SEL_CHANGED, &NavigatorPanel::onSelectionChanged, this);
    m_treeCtrl->Bind(wxEVT_TREE_ITEM_RIGHT_CLICK, &NavigatorPanel::onContextMenu, this);
    m_treeCtrl->Bind(wxEVT_TREE_ITEM_ACTIVATED, &NavigatorPanel::onItemActivated, this);
}
```

**2. Tree Population from Document**
```cpp
void NavigatorPanel::populateTree(std::shared_ptr<kalahari::core::Document> document) {
    m_treeCtrl->DeleteAllItems();

    // Root: Book title
    wxTreeItemId rootId = m_treeCtrl->AddRoot(
        document->getTitle(),
        0, // Book icon
        -1,
        new NodeData(NodeType::Book, document->getId()));

    // frontMatter
    wxTreeItemId frontMatterId = m_treeCtrl->AppendItem(rootId,
        "Front Matter", 1, -1, new NodeData(NodeType::Part, "frontMatter"));
    for (const auto& element : document->getBook().getFrontMatter()) {
        m_treeCtrl->AppendItem(frontMatterId,
            element->getTitle(), 2, -1,
            new NodeData(NodeType::Chapter, element->getId()));
    }

    // body (Parts → Chapters)
    for (const auto& part : document->getBook().getBody()) {
        wxTreeItemId partId = m_treeCtrl->AppendItem(rootId,
            part->getTitle(), 1, -1,
            new NodeData(NodeType::Part, part->getId()));

        for (const auto& chapter : part->getChapters()) {
            m_treeCtrl->AppendItem(partId,
                chapter->getTitle(), 2, -1,
                new NodeData(NodeType::Chapter, chapter->getId()));
        }
    }

    // backMatter
    // ... (similar to frontMatter)

    m_treeCtrl->ExpandAll();
}
```

**3. Node Selection → Load Chapter**
```cpp
void NavigatorPanel::onSelectionChanged(wxTreeEvent& event) {
    wxTreeItemId itemId = event.GetItem();
    NodeData* data = dynamic_cast<NodeData*>(m_treeCtrl->GetItemData(itemId));

    if (data && data->type == NodeType::Chapter) {
        // Notify MainWindow to load chapter in editor
        wxCommandEvent evt(EVT_LOAD_CHAPTER);
        evt.SetString(data->id);
        ProcessEvent(evt);
    }
}
```

**4. Context Menu**
```cpp
void NavigatorPanel::onContextMenu(wxTreeEvent& event) {
    wxTreeItemId itemId = event.GetItem();
    NodeData* data = dynamic_cast<NodeData*>(m_treeCtrl->GetItemData(itemId));

    if (!data) return;

    wxMenu menu;

    if (data->type == NodeType::Part) {
        menu.Append(ID_ADD_CHAPTER, "Add Chapter");
        menu.Append(ID_RENAME_PART, "Rename Part");
        menu.Append(ID_DELETE_PART, "Delete Part");
    } else if (data->type == NodeType::Chapter) {
        menu.Append(ID_RENAME_CHAPTER, "Rename Chapter");
        menu.Append(ID_DELETE_CHAPTER, "Delete Chapter");
        menu.AppendSeparator();
        menu.Append(ID_MOVE_UP, "Move Up");
        menu.Append(ID_MOVE_DOWN, "Move Down");
    } else if (data->type == NodeType::Book) {
        menu.Append(ID_ADD_PART, "Add Part");
        menu.Append(ID_DOCUMENT_PROPERTIES, "Document Properties...");
    }

    PopupMenu(&menu);
}
```

### Files to Create/Modify

**Modified Files:**
- `include/kalahari/gui/navigator_panel.h` (~120 lines: wxTreeCtrl, NodeData struct)
- `src/gui/navigator_panel.cpp` (~500 lines: full implementation)
- `include/kalahari/gui/main_window.h` (~15 lines: EVT_LOAD_CHAPTER handler)
- `src/gui/main_window.cpp` (~50 lines: load chapter on event)
- `tests/gui/test_navigator_panel.cpp` (~200 lines: tree population, selection tests)

**Total Estimated LOC:** ~1,000 lines

---

## ✅ Acceptance Criteria

### Must Have
1. ✅ wxTreeCtrl displays Book → Parts → Chapters hierarchy
2. ✅ Tree populated from Document model
3. ✅ Node selection loads chapter in editor
4. ✅ Context menu on right-click (all CRUD operations listed)
5. ✅ Custom icons for book/part/chapter
6. ✅ Tree updates when document structure changes

### Should Have
7. ✅ Bold font for current chapter
8. ✅ Word count tooltip on chapter nodes
9. ✅ Expand/collapse all buttons (toolbar)

---

## 🧪 Testing Strategy

### Unit Tests (Catch2)
1. Populate tree from Document
2. Select node → verify chapter ID
3. Context menu actions
4. Tree update on document change

### Integration Tests
1. Create document → populate tree → verify structure
2. Click chapter node → verify editor loads chapter
3. Right-click → delete chapter → verify tree updates

### Manual Testing
1. Navigate 50-chapter document
2. Test context menu operations
3. Test on all platforms

---

## 📊 Progress Tracking

### Checklist

#### Phase 1: TreeCtrl Integration (Day 1-2)
- [ ] Update NavigatorPanel with wxTreeCtrl
- [ ] Create image list (icons)
- [ ] Implement tree population from Document
- [ ] Test tree display

#### Phase 2: Node Selection (Day 3-4)
- [ ] Implement onSelectionChanged event
- [ ] Connect to MainWindow (load chapter)
- [ ] Synchronize editor → navigator selection
- [ ] Test navigation

#### Phase 3: Context Menu (Day 5-6)
- [ ] Implement context menu for Part/Chapter/Book
- [ ] Connect menu items to MainWindow handlers (stub)
- [ ] Test context menu display

#### Phase 4: Testing & Polish (Day 7)
- [ ] Add word count tooltips
- [ ] Bold current chapter
- [ ] Write unit tests
- [ ] Manual testing on all platforms

---

## 🚨 Risks & Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Performance with large trees (500+ chapters) | Medium | Low | Test with large documents, use virtual tree if needed |
| Synchronization issues (editor ↔ navigator) | High | Medium | Careful event handling, avoid infinite loops |
| Context menu complexity | Low | Low | Use command pattern for CRUD operations |

---

## 📚 Resources

- **wxTreeCtrl Docs:** https://docs.wxwidgets.org/3.3/classwx_tree_ctrl.html
- **wxWidgets Sample:** `samples/treectrl/treetest.cpp`
- **Document Model:** `include/kalahari/core/document.h` (Phase 0)

---

## 📝 Notes

- Context menu CRUD operations will be implemented in Task #00016
- This task focuses on navigation UI only (no editing logic yet)
- Word count aggregation uses existing Document Model methods

---

**Created:** 2025-10-31
**Last Updated:** 2025-10-31
**Version:** 1.0
