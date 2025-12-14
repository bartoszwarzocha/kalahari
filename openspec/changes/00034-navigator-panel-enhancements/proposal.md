# 00034: Navigator Panel Enhancements

## Status
DEPLOYED

## Goal
Enhance the Navigator Panel with advanced functionality including context menu, drag & drop reordering, search/filter, improved icons, and editor synchronization. This addresses ROADMAP.md section 1.4.

## Context

### Current State
The Navigator Panel (`navigator_panel.h/cpp`) currently provides:
- QTreeWidget displaying project structure (Document > Sections > Parts > Chapters)
- Icons for element types using ArtProvider (folder, chapter)
- Double-click opens chapter in editor (via `elementSelected` signal)
- "Other Files" section for standalone files
- Theme-aware icon refresh via `resourcesChanged` signal

### Missing Features (from ROADMAP 1.4)
1. Context menu (right-click) for element operations
2. Drag & drop reordering of chapters/parts
3. Search/filter to limit tree view (user-requested priority)
4. Improved icons for different element types (Part, Chapter, Scene, Note)
5. Synchronization with editor (highlight current chapter)
6. Navigator expansion state persistence

### Related OpenSpec
- #00033 Project File System: Added "Other Files" section and element IDs
- Context menu was noted as pending in #00033 Phase D

## Architecture

### Context Menu System

```cpp
// Right-click menu structure (depends on element type)
NavigatorPanel::showContextMenu(QTreeWidgetItem* item, const QPoint& pos) {
    QString type = item->data(0, Qt::UserRole + 1).toString();

    QMenu menu;
    if (type == "chapter" || type == "title_page" || ...) {
        menu.addAction(tr("Open"));
        menu.addSeparator();
        menu.addAction(tr("Rename..."));
        menu.addAction(tr("Delete"));
        menu.addSeparator();
        menu.addAction(tr("Move Up"));
        menu.addAction(tr("Move Down"));
        menu.addSeparator();
        menu.addAction(tr("Properties..."));
    } else if (type == "part") {
        menu.addAction(tr("Add Chapter"));
        menu.addSeparator();
        menu.addAction(tr("Rename..."));
        menu.addAction(tr("Delete"));
        menu.addSeparator();
        menu.addAction(tr("Expand All"));
        menu.addAction(tr("Collapse All"));
    } else if (type == "section") {
        menu.addAction(tr("Add Part"));  // Body section only
        menu.addAction(tr("Add Item"));  // Front/Back Matter
    } else if (type == "document") {
        menu.addAction(tr("Project Properties..."));
    } else if (type == "standalone_file") {
        menu.addAction(tr("Open"));
        menu.addSeparator();
        menu.addAction(tr("Add to Project..."));
        menu.addAction(tr("Remove from List"));
    }

    menu.exec(pos);
}
```

### Drag & Drop Reordering

```cpp
// Enable drag & drop
m_treeWidget->setDragEnabled(true);
m_treeWidget->setAcceptDrops(true);
m_treeWidget->setDropIndicatorShown(true);
m_treeWidget->setDragDropMode(QAbstractItemView::InternalMove);

// Custom drop handler for constraints
class NavigatorTreeWidget : public QTreeWidget {
protected:
    bool dropMimeData(QTreeWidgetItem* parent, int index,
                      const QMimeData* data, Qt::DropAction action) override {
        // Only allow dropping chapters within same part
        // Only allow reordering parts within body
        // Prevent dropping sections or cross-section moves
    }

    Qt::DropActions supportedDropActions() const override {
        return Qt::MoveAction;
    }
};
```

### Search/Filter System

```
Navigator Panel Layout:
+------------------------------------------+
| [Search/Filter...]          [x] Clear    |
|------------------------------------------|
| - MyNovel                                |
|   + Front Matter                         |
|   - Body                                 |
|     - Part 1: Introduction               |
|       > Chapter 1: Beginning  (matches)  |
|       > Chapter 2: Conflict              |
+------------------------------------------+
```

```cpp
class NavigatorPanel : public QWidget {
private:
    QLineEdit* m_searchEdit;          // Search input
    QToolButton* m_clearButton;       // Clear search

    void filterTree(const QString& text) {
        // Hide non-matching items
        // Keep parent visible if any child matches
        // Highlight matching text (optional)
    }

    void clearFilter() {
        m_searchEdit->clear();
        showAllItems();
    }
};
```

### Editor Synchronization

```cpp
// When editor tab changes, highlight corresponding item in navigator
void NavigatorPanel::highlightElement(const QString& elementId) {
    if (m_highlightedItem) {
        m_highlightedItem->setBackground(0, QBrush());  // Clear old highlight
    }

    QTreeWidgetItem* item = findItemByElementId(elementId);
    if (item) {
        item->setBackground(0, m_highlightColor);
        m_treeWidget->scrollToItem(item);
        m_highlightedItem = item;
    }
}

// MainWindow connection
connect(m_editorPanel, &EditorPanel::currentTabChanged,
        m_navigatorPanel, &NavigatorPanel::highlightElement);
```

### Improved Element Icons

| Element Type | Icon ID | Description |
|--------------|---------|-------------|
| document | `project.book` | Book/project root |
| section (Front Matter) | `structure.frontmatter` | Front matter section |
| section (Body) | `structure.body` | Body section |
| section (Back Matter) | `structure.backmatter` | Back matter section |
| section (Other Files) | `common.folder.open` | Standalone files |
| part | `structure.part` | Part container |
| chapter | `template.chapter` | Regular chapter |
| title_page | `template.titlepage` | Title page |
| dedication | `template.dedication` | Dedication |
| prologue | `template.prologue` | Prologue |
| epilogue | `template.epilogue` | Epilogue |
| note | `template.note` | Note |
| standalone_file (rtf) | `common.file.text` | RTF file |
| standalone_file (kmap) | `book.newMindMap` | Mind map |
| standalone_file (ktl) | `book.newTimeline` | Timeline |

### Expansion State Persistence

```cpp
// Save expansion state on close/project change
void NavigatorPanel::saveExpansionState() {
    QStringList expanded;
    collectExpandedIds(nullptr, expanded);
    SettingsManager::getInstance().setValue(
        QString("navigator.expansion.%1").arg(m_projectId),
        expanded.join(",")
    );
}

// Restore on project load
void NavigatorPanel::restoreExpansionState() {
    QString key = QString("navigator.expansion.%1").arg(m_projectId);
    QString state = SettingsManager::getInstance().getValue(key, "");
    QStringList expanded = state.split(",", Qt::SkipEmptyParts);
    expandItemsById(expanded);
}
```

## Scope

### Included
1. **Context Menu** - Right-click menu for all element types
   - Open, Rename, Delete, Move Up/Down for chapters
   - Add Chapter for parts
   - Add Part/Item for sections
   - Add to Project, Remove from List for standalone files
2. **Drag & Drop** - Reorder chapters within part, parts within body
3. **Search/Filter** - Filter tree by title (user priority)
4. **Editor Sync** - Highlight current chapter in navigator
5. **Icon Improvements** - Different icons for sections and element types
6. **Expansion Persistence** - Remember which nodes are expanded

### Excluded
- Multi-select operations (future enhancement)
- Keyboard shortcuts for navigation (future)
- Inline rename (use dialog instead)
- Copy/Paste chapters between parts (future)
- Scene-level elements (deferred to Custom Editor phase)

## Acceptance Criteria
- [ ] Right-click on chapter shows context menu with Open, Rename, Delete, Move
- [ ] Right-click on part shows Add Chapter, Rename, Delete
- [ ] Right-click on section shows Add Part (body) or Add Item (front/back)
- [ ] Right-click on standalone file shows Open, Add to Project, Remove
- [ ] Drag chapter to different position within same part reorders it
- [ ] Drag part to different position within body reorders it
- [ ] Invalid drops are rejected (cross-section, dropping sections)
- [ ] Search box filters tree items by title
- [ ] Clear button resets filter
- [ ] Parents remain visible if child matches filter
- [ ] Current editor chapter is highlighted in navigator
- [ ] Switching tabs updates navigator highlight
- [ ] Different icons display for section types
- [ ] Expansion state persists between sessions
- [ ] All operations update ProjectManager (if applicable)
- [ ] Build passes, no regressions

## Technical Notes

### QTreeWidget Drag & Drop
Qt's QTreeWidget supports internal drag & drop via `InternalMove` mode. We need custom logic to:
1. Restrict valid drop targets (same parent type)
2. Update ProjectManager on drop completion
3. Handle manifest reordering

### Filter Performance
For large projects (100+ chapters), filter should:
- Use QTreeWidget item visibility (setHidden)
- Not recreate tree on every keystroke
- Debounce input (300ms delay)

### Icon Additions
Some icons may need to be added to the icon set:
- `structure.frontmatter`, `structure.body`, `structure.backmatter`
- `structure.part`
- Consider using existing icons with color variants

## Files to Modify

### Modified Files
| File | Changes |
|------|---------|
| `include/kalahari/gui/panels/navigator_panel.h` | Add filter UI, context menu, sync methods |
| `src/gui/panels/navigator_panel.cpp` | Implement all new features |
| `src/core/art_provider.cpp` | Register new icon IDs (if needed) |
| `src/gui/main_window.cpp` | Connect editor sync signal |

### New Files
| File | Purpose |
|------|---------|
| `include/kalahari/gui/panels/navigator_tree_widget.h` | Custom QTreeWidget for drag & drop (optional) |
| `src/gui/panels/navigator_tree_widget.cpp` | Implementation |

### Icon Resources
May need to add SVG icons:
- `structure_frontmatter.svg`
- `structure_body.svg`
- `structure_backmatter.svg`
- `structure_part.svg`

## User Stories

### US1: Writer organizes chapters
As a writer, I want to right-click on a chapter and move it up/down,
so that I can reorder my story without editing the project file manually.

### US2: Writer finds chapter quickly
As a writer, I want to type in a search box to filter the navigator,
so that I can quickly find a specific chapter in a large project.

### US3: Writer sees current location
As a writer, I want the navigator to highlight the chapter I'm editing,
so that I always know where I am in the project structure.

### US4: Writer manages standalone files
As a writer, I want to right-click on a standalone file and add it to my project,
so that I can integrate loose notes into my book structure.

## Implementation Order

1. **Phase A: Context Menu** - Most requested, immediate usability win
2. **Phase B: Search/Filter** - User-requested priority feature
3. **Phase C: Editor Synchronization** - Quick to implement, high value
4. **Phase D: Drag & Drop** - More complex, requires ProjectManager changes
5. **Phase E: Icon Improvements** - Polish, can be done incrementally
6. **Phase F: Expansion Persistence** - Nice-to-have, low priority

## References
- ROADMAP.md Section 1.4 Navigator Panel
- OpenSpec #00033 Project File System (Navigator updates)
- Qt Documentation: QTreeWidget Drag and Drop
