# Task #00034: MainWindow Integration with Command System

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 90 minutes
**Dependencies:** #00028 (File commands), #00029 (Edit commands), #00030 (ToolbarManager), #00033 (Edit toolbar)
**Phase:** Phase 0 - Architecture

---

## Goal

Integrate CommandRegistry and ToolbarManager into MainWindow, replacing hardcoded toolbar with command-based system.

---

## Requirements

### 1. Modify `src/gui/main_window.cpp` - Constructor

Replace toolbar creation with command system initialization:

```cpp
MainWindow::MainWindow()
    : wxFrame(nullptr, wxID_ANY, _("Kalahari - Writer's IDE"),
              wxDefaultPosition, wxSize(1024, 768))
{
    // ... existing initialization ...

    // Initialize Command Registry
    auto& registry = CommandRegistry::getInstance();
    registerFileCommands(registry, this);
    registerEditCommands(registry, this);

    // Initialize Toolbar Manager
    auto& toolbarMgr = ToolbarManager::getInstance();

    // Load toolbar configs from file or use defaults
    std::string toolbarConfigPath = /* get from SettingsManager */;
    if (wxFileExists(toolbarConfigPath)) {
        toolbarMgr.loadFromFile(toolbarConfigPath);
    } else {
        toolbarMgr.resetToDefaults();
    }

    // Create toolbars from configurations
    auto configs = toolbarMgr.getAllConfigs();
    for (const auto& config : configs) {
        if (config.visible) {
            wxAuiToolBar* toolbar = toolbarMgr.createToolbar(this, config);
            m_auiManager->AddPane(toolbar,
                wxAuiPaneInfo()
                    .Name(config.name)
                    .Caption(config.name)
                    .ToolbarPane()
                    .Top()
                    .Gripper(false));
        }
    }

    // ... rest of initialization ...
}
```

### 2. Remove Old Toolbar Code

**Remove from main_window.h:**
- `wxToolBar* m_toolBar` member variable
- `createToolBar()` method declaration
- `createToolBarContent()` method declaration

**Remove from main_window.cpp:**
- `createToolBar()` implementation
- `createToolBarContent()` implementation

### 3. Update `onSettingsApplied()`

Replace toolbar reload with ToolbarManager icon size update:

```cpp
void MainWindow::onSettingsApplied(SettingsAppliedEvent& event) {
    // ... existing code ...

    // Update toolbar icon sizes
    if (event.HasChange("icon_size")) {
        auto& toolbarMgr = ToolbarManager::getInstance();
        int newSize = IconRegistry::getInstance().getSizes().toolbar;

        auto configs = toolbarMgr.getAllConfigs();
        for (const auto& config : configs) {
            toolbarMgr.setToolbarIconSize(config.name, newSize);

            // Rebuild toolbar in wxAUI
            wxAuiToolBar* toolbar = toolbarMgr.getToolbar(config.name);
            if (toolbar) {
                m_auiManager->DetachPane(toolbar);
                toolbar->Destroy();

                wxAuiToolBar* newToolbar = toolbarMgr.createToolbar(this, config);
                m_auiManager->AddPane(newToolbar,
                    wxAuiPaneInfo()
                        .Name(config.name)
                        .Caption(config.name)
                        .ToolbarPane()
                        .Top()
                        .Gripper(false));
            }
        }

        m_auiManager->Update();
    }
}
```

### 4. Add View -> Toolbars Menu

Add submenu to View menu for toolbar visibility:

```cpp
wxMenu* toolbarsMenu = new wxMenu;
toolbarsMenu->AppendCheckItem(ID_VIEW_TOOLBAR_PROJECT, _("Project"));
toolbarsMenu->AppendCheckItem(ID_VIEW_TOOLBAR_EDIT, _("Edit"));
toolbarsMenu->AppendSeparator();
toolbarsMenu->Append(ID_VIEW_TOOLBAR_CUSTOMIZE, _("Customize..."));

viewMenu->AppendSubMenu(toolbarsMenu, _("Toolbars"));
```

### 5. Add Event Handlers

```cpp
void MainWindow::onViewToolbarProject(wxCommandEvent& event) {
    bool show = event.IsChecked();
    auto& toolbarMgr = ToolbarManager::getInstance();
    toolbarMgr.setToolbarVisible("Project", show);

    wxAuiPaneInfo& pane = m_auiManager->GetPane("Project");
    pane.Show(show);
    m_auiManager->Update();
}

void MainWindow::onViewToolbarEdit(wxCommandEvent& event) {
    bool show = event.IsChecked();
    auto& toolbarMgr = ToolbarManager::getInstance();
    toolbarMgr.setToolbarVisible("Edit", show);

    wxAuiPaneInfo& pane = m_auiManager->GetPane("Edit");
    pane.Show(show);
    m_auiManager->Update();
}
```

---

## Implementation Notes

**Backward Compatibility:**
- Remove old wxToolBar completely (replaced by wxAuiToolBar)
- wxAuiToolBar is dockable, more flexible

**Configuration Location:**
- Store toolbars.json in ~/.kalahari/toolbars.json
- Load at startup, save on exit

**Icon Size Sync:**
- When icon size changes in Settings, update all toolbars
- Rebuild toolbars with new icon size

**Phase 1+ Features (not implemented yet):**
- Toolbar customization dialog
- Drag-and-drop command addition
- Custom toolbar creation

---

## Acceptance Criteria

- [ ] CommandRegistry initialized in MainWindow constructor
- [ ] File and Edit commands registered at startup
- [ ] ToolbarManager loads configs from file or defaults
- [ ] Toolbars created and added to wxAuiManager
- [ ] Old wxToolBar code removed completely
- [ ] onSettingsApplied updates toolbar icon sizes
- [ ] View -> Toolbars menu added with visibility toggles
- [ ] Toolbar visibility can be toggled via View menu
- [ ] Code compiles, no warnings
- [ ] Application runs without crashes

---

## Testing

**Manual Test Plan:**
1. Launch application
2. Verify Project and Edit toolbars visible at top
3. Click toolbar buttons (New, Open, Save, Undo, etc.) - verify they work
4. Open Settings -> Appearance, change Icon Size to 32, click Apply
5. Verify toolbar icons resize immediately
6. Go to View -> Toolbars, uncheck "Project"
7. Verify Project toolbar disappears
8. Re-check "Project" in menu
9. Verify Project toolbar reappears
10. Close application
11. Relaunch application
12. Verify toolbar visibility matches previous session

---

## Related Files

- `src/gui/main_window.h` (remove old toolbar members)
- `src/gui/main_window.cpp` (major refactoring)
- `include/kalahari/gui/core_commands.h` (include)
- `src/CMakeLists.txt` (verify all new files linked)

---

## Next Tasks

After this task, existing tasks need to be renumbered:
- Old #00024 → New #00035 (Icon Size Persistence Verification)
- Old #00025 → New #00036 (Font Scaling Live Preview)
- Old #00026 → New #00037 (Font Scaling Apply Implementation)
- Old #00027 → New #00038 (Font Scaling Persistence Verification)
- Old #00028 → New #00039 (Dynamic Text Wrapping Verification)
- Old #00029 → New #00040 (Theme Restart Dialog Verification)
- Old #00030 → New #00041 (Navigator Panel Cleanup Verification)

---

**Created:** 2025-11-11
**Author:** Architecture Planning
