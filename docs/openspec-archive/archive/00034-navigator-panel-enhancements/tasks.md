# Tasks for #00034: Navigator Panel Enhancements

## Phase A: Context Menu
- [ ] Add `customContextMenuRequested` signal handler
- [ ] Implement `showContextMenu(QTreeWidgetItem*, QPoint)` method
- [ ] Create menu structure based on element type
- [ ] Chapter menu: Open, Rename, Delete, Move Up/Down, Properties
- [ ] Part menu: Add Chapter, Rename, Delete, Expand/Collapse All
- [ ] Section menu: Add Part (body), Add Item (front/back matter)
- [ ] Document menu: Project Properties
- [ ] Standalone file menu: Open, Add to Project, Remove from List
- [ ] Connect menu actions to appropriate handlers
- [ ] Emit signals for operations requiring MainWindow/ProjectManager

## Phase B: Search/Filter (User Priority)
- [ ] Add QLineEdit search field above tree
- [ ] Add QToolButton clear button (x icon)
- [ ] Implement `filterTree(const QString& text)` method
- [ ] Implement `clearFilter()` method
- [ ] Filter shows matching items and their parents
- [ ] Hide non-matching leaf items
- [ ] Add debounce timer (300ms) for filter input
- [ ] Preserve expansion state during filtering
- [ ] Connect search field to filter method
- [ ] Connect clear button to clearFilter()
- [ ] Add placeholder text "Filter tree..."

## Phase C: Editor Synchronization
- [ ] Add `highlightElement(const QString& elementId)` slot
- [ ] Store pointer to currently highlighted item
- [ ] Implement highlight color (theme-aware)
- [ ] Clear previous highlight when setting new one
- [ ] Scroll to highlighted item if not visible
- [ ] Connect EditorPanel tab change signal
- [ ] Update MainWindow to bridge editor and navigator signals
- [ ] Handle case when element not found in tree

## Phase D: Drag & Drop Reordering
- [x] Enable drag & drop on QTreeWidget
  - [x] setDragEnabled(true)
  - [x] setAcceptDrops(true)
  - [x] setDropIndicatorShown(true)
  - [x] setDragDropMode(InternalMove)
- [ ] Create custom NavigatorTreeWidget class (optional) - NOT NEEDED, using rowsMoved signal
- [ ] Override dropMimeData for validation - NOT NEEDED, using Qt::ItemFlags
- [x] Allow chapter reordering within same part
- [x] Allow part reordering within body
- [x] Block invalid drops (cross-section, dropping sections) - via Qt::ItemIsDragEnabled/ItemIsDropEnabled flags
- [x] Emit signal on successful drop with old/new positions (chapterReordered, partReordered)
- [x] Connect to ProjectManager for manifest update
- [x] Implement `moveChapter(partId, fromIndex, toIndex)` - Part::moveChapter already existed
- [x] Implement `movePart(fromIndex, toIndex)` - Book::movePart added
- [ ] Test visual feedback during drag

## Phase E: Icon Improvements
- [ ] Audit existing icons in ArtProvider
- [ ] Define new icon IDs for structure elements:
  - [ ] `structure.frontmatter`
  - [ ] `structure.body`
  - [ ] `structure.backmatter`
  - [ ] `structure.part`
  - [ ] `project.book`
- [ ] Update `getIconIdForType()` method
- [ ] Add SVG icons if missing (or reuse existing with aliases)
- [ ] Update refreshItemIcons() for new icons
- [ ] Test icon display in light/dark themes

## Phase F: Expansion Persistence
- [ ] Add `saveExpansionState()` method
- [ ] Add `restoreExpansionState()` method
- [ ] Store expansion as comma-separated element IDs
- [ ] Use SettingsManager with project-specific key
- [ ] Call saveExpansionState on:
  - [ ] Project close
  - [ ] Application close
- [ ] Call restoreExpansionState on:
  - [ ] Project load (after tree populated)
- [ ] Handle missing items gracefully
- [ ] Add `collectExpandedIds()` helper
- [ ] Add `expandItemsById()` helper

## Integration Tasks
- [x] Update MainWindow for new signals
  - [ ] Context menu "Rename" -> show rename dialog
  - [ ] Context menu "Delete" -> confirm and delete
  - [ ] Context menu "Add Chapter" -> NewItemDialog
  - [ ] Context menu "Properties" -> PropertiesPanel focus
  - [x] Drag & drop chapterReordered signal -> ProjectManager::reorderChapter
  - [x] Drag & drop partReordered signal -> ProjectManager::reorderPart
- [x] Update ProjectManager for structure changes
  - [x] `reorderChapter(partId, fromIndex, toIndex)`
  - [x] `reorderPart(fromIndex, toIndex)`
  - [ ] `deleteElement(elementId)`
  - [ ] `renameElement(elementId, newTitle)`

## Testing
- [ ] Unit tests for filter logic
- [ ] Manual testing:
  - [ ] Context menu appears on right-click
  - [ ] Menu items work correctly
  - [ ] Filter shows/hides correct items
  - [ ] Clear button resets filter
  - [ ] Editor sync highlights correct item
  - [ ] Drag & drop reorders correctly
  - [ ] Invalid drops are rejected
  - [ ] Icons display correctly
  - [ ] Expansion state persists

## Documentation
- [ ] Update CHANGELOG.md
- [ ] Update ROADMAP.md section 1.4 (mark completed)
- [ ] Document context menu in user guide (if exists)

---

## Status Summary

**Current Phase:** Phase D completed
**Priority Order:**
1. Phase A (Context Menu) - Most requested, foundation for other features
2. Phase B (Search/Filter) - User-requested priority
3. Phase C (Editor Sync) - Quick win, high value
4. **Phase D (Drag & Drop) - COMPLETED (2024-12-14)**
5. Phase E (Icons) - Polish
6. Phase F (Persistence) - Nice-to-have

**Phase D Implementation Notes:**
- Enabled QTreeWidget drag & drop with InternalMove mode
- Used Qt::ItemIsDragEnabled/ItemIsDropEnabled flags for validation
- Chapters can be reordered within their parent Part only
- Parts can be reordered within the Body section only
- Front/back matter items and sections are not draggable
- Connected model's rowsMoved signal for drop detection
- Added chapterReordered/partReordered signals to NavigatorPanel
- Added reorderChapter/reorderPart methods to ProjectManager
- Added movePart method to Book class (moveChapter already existed in Part)
- MainWindow connects signals to ProjectManager and reverts on failure

**Dependencies:**
- Phase D requires ProjectManager changes for reordering - DONE
- Phase C requires EditorPanel signal for current tab
- Phase B is standalone, can start immediately

**Estimated:** 4-6 sessions total
