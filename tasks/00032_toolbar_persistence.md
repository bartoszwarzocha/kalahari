# Task #00032: Toolbar Configuration & Persistence

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 60 minutes
**Dependencies:** #00031 (ToolbarManager)
**Phase:** Phase 0 - Architecture

---

## Goal

Implement JSON serialization/deserialization for toolbar configurations and integrate with SettingsManager.

---

## Requirements

### 1. Extend `src/gui/toolbar_manager.cpp`

Add persistence methods:

```cpp
void ToolbarManager::saveToFile(const std::string& path) const {
    nlohmann::json j;
    j["toolbars"] = nlohmann::json::array();

    for (const auto& [name, config] : m_configs) {
        nlohmann::json toolbarJson;
        toolbarJson["name"] = config.name;
        toolbarJson["iconSize"] = config.iconSize;
        toolbarJson["visible"] = config.visible;
        toolbarJson["items"] = nlohmann::json::array();

        for (const auto& item : config.items) {
            nlohmann::json itemJson;
            if (item.type == ToolbarItemType::Separator) {
                itemJson["type"] = "separator";
            } else {
                itemJson["type"] = "command";
                itemJson["commandId"] = item.commandId;
            }
            toolbarJson["items"].push_back(itemJson);
        }

        j["toolbars"].push_back(toolbarJson);
    }

    std::ofstream file(path);
    file << j.dump(2);  // Pretty print with 2-space indent
}

void ToolbarManager::loadFromFile(const std::string& path) {
    // Read JSON, parse toolbars array, populate m_configs
    // Do NOT create wxAuiToolBar instances yet (only configs)
}
```

### 2. Extend `include/kalahari/gui/toolbar_manager.h`

```cpp
/// Save toolbar configurations to JSON file
void saveToFile(const std::string& path) const;

/// Load toolbar configurations from JSON file
void loadFromFile(const std::string& path);

/// Reset to default toolbar configurations
void resetToDefaults();
```

### 3. JSON Format

```json
{
    "toolbars": [
        {
            "name": "Project",
            "iconSize": 24,
            "visible": true,
            "items": [
                {"type": "command", "commandId": "file.new"},
                {"type": "command", "commandId": "file.open"},
                {"type": "command", "commandId": "file.save"},
                {"type": "separator"},
                {"type": "command", "commandId": "project.build"}
            ]
        },
        {
            "name": "Edit",
            "iconSize": 24,
            "visible": true,
            "items": [
                {"type": "command", "commandId": "edit.undo"},
                {"type": "command", "commandId": "edit.redo"},
                {"type": "separator"},
                {"type": "command", "commandId": "edit.cut"},
                {"type": "command", "commandId": "edit.copy"},
                {"type": "command", "commandId": "edit.paste"}
            ]
        }
    ]
}
```

---

## Implementation Notes

**File Location:**
- Store in `~/.kalahari/toolbars.json` (user config directory)
- Use SettingsManager::getConfigDir() for path

**Loading Order:**
1. Load toolbar configs from JSON (startup)
2. Create wxAuiToolBar instances when MainWindow initializes
3. Toolbars managed by wxAuiManager for docking

**Error Handling:**
- If toolbars.json doesn't exist: use resetToDefaults()
- If JSON parsing fails: log error, use defaults
- If command not found: skip item, log warning

**resetToDefaults():**
- Create 2 default toolbar configs (Project, Edit)
- See Task #00032 and #00033 for command lists

---

## Acceptance Criteria

- [ ] saveToFile writes JSON with proper structure
- [ ] loadFromFile reads JSON and populates m_configs
- [ ] JSON format matches specification
- [ ] resetToDefaults creates 2 default toolbars
- [ ] Error handling for missing file
- [ ] Error handling for invalid JSON
- [ ] Code compiles, no warnings

---

## Testing

Manual verification:
```cpp
auto& tm = ToolbarManager::getInstance();

// Create test config
ToolbarConfig config;
config.name = "Test";
config.iconSize = 24;
config.items.push_back(ToolbarItem("file.save"));
config.items.push_back(ToolbarItem());  // Separator

tm.updateConfig(config);
tm.saveToFile("test_toolbars.json");

// Load and verify
ToolbarManager& tm2 = ToolbarManager::getInstance();
tm2.loadFromFile("test_toolbars.json");

auto configs = tm2.getAllConfigs();
assert(configs.size() == 1);
assert(configs[0].name == "Test");
assert(configs[0].items.size() == 2);
assert(configs[0].items[0].commandId == "file.save");
assert(configs[0].items[1].type == ToolbarItemType::Separator);
```

---

## Related Files

- `include/kalahari/gui/toolbar_manager.h` (extend)
- `src/gui/toolbar_manager.cpp` (extend)

---

## Next Task

Task #00032 - Initial Project Toolbar Configuration

---

**Created:** 2025-11-11
**Author:** Architecture Planning
