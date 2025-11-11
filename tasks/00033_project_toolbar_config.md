# Task #00033: Initial Project Toolbar Configuration

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 45 minutes
**Dependencies:** #00028 (File commands), #00032 (Toolbar persistence)
**Phase:** Phase 0 - Architecture

---

## Goal

Define and implement default "Project" toolbar configuration with file management commands.

---

## Requirements

### 1. Extend `src/gui/toolbar_manager.cpp`

Implement default Project toolbar in resetToDefaults():

```cpp
void ToolbarManager::resetToDefaults() {
    m_configs.clear();

    // Project Toolbar
    ToolbarConfig projectToolbar;
    projectToolbar.name = "Project";
    projectToolbar.iconSize = 24;
    projectToolbar.visible = true;

    // File operations
    projectToolbar.items.push_back(ToolbarItem("file.new"));
    projectToolbar.items.push_back(ToolbarItem("file.open"));
    projectToolbar.items.push_back(ToolbarItem("file.save"));
    projectToolbar.items.push_back(ToolbarItem());  // Separator

    // Phase 1: Project-specific commands (placeholders for now)
    // projectToolbar.items.push_back(ToolbarItem("project.build"));
    // projectToolbar.items.push_back(ToolbarItem("project.export"));

    m_configs[projectToolbar.name] = projectToolbar;

    // Edit toolbar will be added in Task #00033
}
```

### 2. Project Toolbar Command List

**Phase 0 (Current):**
- `file.new` - New document
- `file.open` - Open document
- `file.save` - Save document
- Separator

**Phase 1+ (Placeholder notes):**
- `project.build` - Compile project
- `project.export` - Export to DOCX/PDF
- `project.settings` - Project settings

---

## Implementation Notes

**Icon Size:**
- Default: 24x24 (balanced visibility)
- User can change via View -> Toolbars -> Customize

**Visibility:**
- Project toolbar visible by default
- User can hide via View -> Toolbars menu

**Command Availability:**
- Only include commands that are already registered
- Phase 1 commands commented out until implemented

**Toolbar Position (wxAUI):**
- Default position: Top-left (first toolbar)
- User can dock anywhere via wxAuiManager

---

## Acceptance Criteria

- [ ] resetToDefaults() creates "Project" toolbar config
- [ ] Project toolbar contains 3 file commands + separator
- [ ] Icon size set to 24
- [ ] Toolbar visible by default
- [ ] Code compiles, no warnings
- [ ] Config can be saved/loaded via ToolbarManager

---

## Testing

Manual verification:
```cpp
auto& tm = ToolbarManager::getInstance();
tm.resetToDefaults();

auto configs = tm.getAllConfigs();
assert(configs.size() >= 1);  // At least Project toolbar

auto* projectConfig = tm.getConfig("Project");
assert(projectConfig != nullptr);
assert(projectConfig->iconSize == 24);
assert(projectConfig->visible == true);
assert(projectConfig->items.size() == 4);  // 3 commands + separator

assert(projectConfig->items[0].commandId == "file.new");
assert(projectConfig->items[1].commandId == "file.open");
assert(projectConfig->items[2].commandId == "file.save");
assert(projectConfig->items[3].type == ToolbarItemType::Separator);
```

---

## Related Files

- `src/gui/toolbar_manager.cpp` (extend resetToDefaults)

---

## Next Task

Task #00033 - Initial Edit Toolbar Configuration

---

**Created:** 2025-11-11
**Author:** Architecture Planning
