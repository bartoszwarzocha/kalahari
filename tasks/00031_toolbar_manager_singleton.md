# Task #00031: ToolbarManager Singleton

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 90 minutes
**Dependencies:** #00025 (CommandRegistry)
**Phase:** Phase 0 - Architecture

---

## Goal

Implement ToolbarManager singleton for creating, managing, and customizing wxAuiToolBars based on registered commands.

---

## Requirements

### 1. Create `include/kalahari/gui/toolbar_manager.h`

```cpp
#include <wx/aui/aui.h>
#include <string>
#include <vector>
#include <map>

namespace kalahari {
namespace gui {

class CommandRegistry;

/// Toolbar item type
enum class ToolbarItemType {
    Command,    // Regular command button
    Separator   // Visual separator
};

/// Single toolbar item (command or separator)
struct ToolbarItem {
    ToolbarItemType type;
    std::string commandId;  // Empty for separators

    ToolbarItem() : type(ToolbarItemType::Separator) {}
    explicit ToolbarItem(const std::string& cmdId)
        : type(ToolbarItemType::Command), commandId(cmdId) {}
};

/// Toolbar configuration
struct ToolbarConfig {
    std::string name;              // "Project", "Edit"
    std::vector<ToolbarItem> items;
    int iconSize = 24;             // 16, 24, or 32
    bool visible = true;
};

class ToolbarManager {
public:
    static ToolbarManager& getInstance();

    /// Create toolbar from configuration
    /// @param parent Parent window (MainWindow)
    /// @param config Toolbar configuration
    /// @return Created wxAuiToolBar instance
    wxAuiToolBar* createToolbar(wxWindow* parent, const ToolbarConfig& config);

    /// Get existing toolbar by name
    wxAuiToolBar* getToolbar(const std::string& name) const;

    /// Update toolbar icon size (rebuild required)
    void setToolbarIconSize(const std::string& name, int size);

    /// Show/hide toolbar
    void setToolbarVisible(const std::string& name, bool visible);

    /// Get all toolbar configurations
    std::vector<ToolbarConfig> getAllConfigs() const;

    /// Update toolbar configuration
    void updateConfig(const ToolbarConfig& config);

private:
    ToolbarManager() = default;
    ~ToolbarManager() = default;
    ToolbarManager(const ToolbarManager&) = delete;
    ToolbarManager& operator=(const ToolbarManager&) = delete;

    std::map<std::string, wxAuiToolBar*> m_toolbars;
    std::map<std::string, ToolbarConfig> m_configs;
};

} // namespace gui
} // namespace kalahari
```

### 2. Implement in `src/gui/toolbar_manager.cpp`

**createToolbar:**
- Create wxAuiToolBar with wxAUI_TB_DEFAULT_STYLE
- For each item in config:
  - If Command: Get command from CommandRegistry, add tool with icon
  - If Separator: Add separator
- Store toolbar in m_toolbars map
- Store config in m_configs map
- Return created toolbar

**getToolbar:**
- Lookup in m_toolbars map
- Return nullptr if not found

**setToolbarIconSize:**
- Update config in m_configs
- Rebuild toolbar if it exists (destroy + recreate)

**setToolbarVisible:**
- Update config visibility flag
- If toolbar exists, call Show(visible)

**getAllConfigs/updateConfig:**
- Manage m_configs map

---

## Implementation Notes

**Toolbar Creation Pattern:**
```cpp
wxAuiToolBar* toolbar = new wxAuiToolBar(parent, wxID_ANY,
    wxDefaultPosition, wxDefaultSize, wxAUI_TB_DEFAULT_STYLE);

wxSize iconSize(config.iconSize, config.iconSize);

for (const auto& item : config.items) {
    if (item.type == ToolbarItemType::Separator) {
        toolbar->AddSeparator();
    } else {
        auto* cmd = CommandRegistry::getInstance().getCommand(item.commandId);
        if (cmd) {
            toolbar->AddTool(wxID_ANY, cmd->label,
                cmd->icons.GetBitmap(config.iconSize),
                cmd->tooltip);
        }
    }
}

toolbar->Realize();
```

**Icon Size Reload:**
- When setToolbarIconSize called, destroy old toolbar
- Create new toolbar with updated icon size
- Notify wxAuiManager to update layout

---

## Acceptance Criteria

- [ ] `toolbar_manager.h` created with singleton interface
- [ ] `toolbar_manager.cpp` implements all methods
- [ ] createToolbar creates wxAuiToolBar from config
- [ ] Toolbar items populated from CommandRegistry
- [ ] Separators handled correctly
- [ ] getToolbar returns existing toolbar or nullptr
- [ ] setToolbarIconSize rebuilds toolbar
- [ ] Code compiles, no warnings

---

## Testing

Manual verification:
```cpp
auto& tm = ToolbarManager::getInstance();
auto& registry = CommandRegistry::getInstance();

// Register test command
Command testCmd;
testCmd.id = "test.command";
testCmd.label = "Test";
testCmd.icons = IconSet(wxART_INFORMATION);
registry.registerCommand(testCmd);

// Create toolbar
ToolbarConfig config;
config.name = "Test";
config.items.push_back(ToolbarItem("test.command"));
config.items.push_back(ToolbarItem());  // Separator
config.iconSize = 24;

wxAuiToolBar* toolbar = tm.createToolbar(mainWindow, config);
assert(toolbar != nullptr);
assert(tm.getToolbar("Test") == toolbar);

// Change icon size
tm.setToolbarIconSize("Test", 32);
assert(tm.getAllConfigs()[0].iconSize == 32);
```

---

## Related Files

- `include/kalahari/gui/toolbar_manager.h` (new)
- `src/gui/toolbar_manager.cpp` (new)
- `src/CMakeLists.txt` (add toolbar_manager.cpp)

---

## Next Task

Task #00031 - Toolbar Configuration & Persistence

---

**Created:** 2025-11-11
**Author:** Architecture Planning
