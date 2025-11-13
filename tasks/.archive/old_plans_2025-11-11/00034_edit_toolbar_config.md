# Task #00034: Initial Edit Toolbar Configuration

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 45 minutes
**Dependencies:** #00029 (Edit commands), #00033 (Project toolbar)
**Phase:** Phase 0 - Architecture

---

## Goal

Define and implement default "Edit" toolbar configuration with editing commands.

---

## Requirements

### 1. Extend `src/gui/toolbar_manager.cpp`

Add Edit toolbar to resetToDefaults():

```cpp
void ToolbarManager::resetToDefaults() {
    m_configs.clear();

    // Project Toolbar (from Task #00032)
    // ...

    // Edit Toolbar
    ToolbarConfig editToolbar;
    editToolbar.name = "Edit";
    editToolbar.iconSize = 24;
    editToolbar.visible = true;

    // Undo/Redo
    editToolbar.items.push_back(ToolbarItem("edit.undo"));
    editToolbar.items.push_back(ToolbarItem("edit.redo"));
    editToolbar.items.push_back(ToolbarItem());  // Separator

    // Clipboard operations
    editToolbar.items.push_back(ToolbarItem("edit.cut"));
    editToolbar.items.push_back(ToolbarItem("edit.copy"));
    editToolbar.items.push_back(ToolbarItem("edit.paste"));

    m_configs[editToolbar.name] = editToolbar;
}
```

### 2. Edit Toolbar Command List

**Phase 0 (Current):**
- `edit.undo` - Undo last action
- `edit.redo` - Redo last undone action
- Separator
- `edit.cut` - Cut selection
- `edit.copy` - Copy selection
- `edit.paste` - Paste from clipboard

**Phase 1+ (Potential additions):**
- `edit.find` - Find text (Ctrl+F)
- `edit.replace` - Find and replace (Ctrl+H)
- `format.bold` - Bold formatting
- `format.italic` - Italic formatting

---

## Implementation Notes

**Icon Size:**
- Default: 24x24 (same as Project toolbar)
- Consistent sizing across all toolbars

**Visibility:**
- Edit toolbar visible by default
- User can hide via View -> Toolbars menu

**Clipboard Operations:**
- Cut/Copy/Paste in toolbar for quick access
- Duplicates Edit menu (intentional - common workflow)

**Toolbar Position (wxAUI):**
- Default position: Top-left, below Project toolbar
- User can dock anywhere via wxAuiManager

---

## Acceptance Criteria

- [ ] resetToDefaults() creates "Edit" toolbar config
- [ ] Edit toolbar contains 6 commands + 1 separator
- [ ] Icon size set to 24
- [ ] Toolbar visible by default
- [ ] Both Project and Edit toolbars present after reset
- [ ] Code compiles, no warnings
- [ ] Config can be saved/loaded via ToolbarManager

---

## Testing

Manual verification:
```cpp
auto& tm = ToolbarManager::getInstance();
tm.resetToDefaults();

auto configs = tm.getAllConfigs();
assert(configs.size() == 2);  // Project + Edit

auto* editConfig = tm.getConfig("Edit");
assert(editConfig != nullptr);
assert(editConfig->iconSize == 24);
assert(editConfig->visible == true);
assert(editConfig->items.size() == 7);  // 6 commands + 1 separator

assert(editConfig->items[0].commandId == "edit.undo");
assert(editConfig->items[1].commandId == "edit.redo");
assert(editConfig->items[2].type == ToolbarItemType::Separator);
assert(editConfig->items[3].commandId == "edit.cut");
assert(editConfig->items[4].commandId == "edit.copy");
assert(editConfig->items[5].commandId == "edit.paste");
```

---

## Related Files

- `src/gui/toolbar_manager.cpp` (extend resetToDefaults)

---

## Next Task

Task #00034 - MainWindow Integration with Command System

---

**Created:** 2025-11-11
**Author:** Architecture Planning
