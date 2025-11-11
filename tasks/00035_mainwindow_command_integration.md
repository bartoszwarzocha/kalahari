# Task #00035: MainWindow Full Integration (Menu + Toolbars)

**Status:** 📋 Planned
**Priority:** P1 (HIGH - Architecture Foundation)
**Estimated:** 120 minutes
**Dependencies:** #00030 (Menu System), #00034 (Edit toolbar)
**Phase:** Phase 0 - Architecture

---

## Goal

Complete integration of CommandRegistry into MainWindow: replace hardcoded menu and toolbar with command-based system. This is the final task that unifies menu, toolbar, and keyboard shortcut handling through CommandRegistry.

---

## Requirements

### 1. Remove Old Menu and Toolbar Code

**Remove from main_window.h:**
- `wxToolBar* m_toolBar` member variable
- `createToolBar()` method declaration
- `createToolBarContent()` method declaration

**Remove from main_window.cpp:**
- Old `createMenuBar()` implementation (will be replaced)
- `createToolBar()` implementation
- `createToolBarContent()` implementation
- All old event handlers (onFileNew, onFileSave, etc.) - replaced by commands

### 2. Modify `src/gui/main_window.cpp` - Constructor

Replace menu and toolbar creation with command system initialization:

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

### 3. Rewrite `createMenuBar()` with MenuBuilder

Use MenuBuilder from Task #00030:

```cpp
void MainWindow::createMenuBar() {
    MenuBuilder builder(this);
    wxMenuBar* menuBar = new wxMenuBar;

    // File menu (command-based)
    wxMenu* fileMenu = new wxMenu;
    builder.addCommandToMenu(fileMenu, "file.new", false);
    builder.addCommandToMenu(fileMenu, "file.open", false);
    builder.addCommandToMenu(fileMenu, "file.save", false);
    builder.addCommandToMenu(fileMenu, "file.saveas", true);  // separator after
    builder.addCommandToMenu(fileMenu, "file.settings", true);
    builder.addCommandToMenu(fileMenu, "file.exit", false);
    menuBar->Append(fileMenu, _("&File"));

    // Edit menu (command-based)
    wxMenu* editMenu = new wxMenu;
    builder.addCommandToMenu(editMenu, "edit.undo", false);
    builder.addCommandToMenu(editMenu, "edit.redo", true);
    builder.addCommandToMenu(editMenu, "edit.cut", false);
    builder.addCommandToMenu(editMenu, "edit.copy", false);
    builder.addCommandToMenu(editMenu, "edit.paste", true);
    builder.addCommandToMenu(editMenu, "edit.selectall", false);
    menuBar->Append(editMenu, _("&Edit"));

    // View, Help menus - keep existing for now
    wxMenu* viewMenu = createViewMenu();
    menuBar->Append(viewMenu, _("&View"));

    wxMenu* helpMenu = createHelpMenu();
    menuBar->Append(helpMenu, _("&Help"));

    SetMenuBar(menuBar);
}
```

### 4. Update `onSettingsApplied()`

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

### 5. Add View -> Toolbars Menu

Add submenu to View menu for toolbar visibility:

```cpp
wxMenu* toolbarsMenu = new wxMenu;
toolbarsMenu->AppendCheckItem(ID_VIEW_TOOLBAR_PROJECT, _("Project"));
toolbarsMenu->AppendCheckItem(ID_VIEW_TOOLBAR_EDIT, _("Edit"));
toolbarsMenu->AppendSeparator();
toolbarsMenu->Append(ID_VIEW_TOOLBAR_CUSTOMIZE, _("Customize..."));

viewMenu->AppendSubMenu(toolbarsMenu, _("Toolbars"));
```

### 6. Add Event Handlers for Toolbar Visibility

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
- [ ] **Menu system uses MenuBuilder** (from Task #00030)
- [ ] **File and Edit menus execute commands via CommandRegistry**
- [ ] ToolbarManager loads configs from file or defaults
- [ ] Toolbars created and added to wxAuiManager
- [ ] Old menu event handlers removed (replaced by commands)
- [ ] Old wxToolBar code removed completely
- [ ] onSettingsApplied updates toolbar icon sizes
- [ ] View -> Toolbars menu added with visibility toggles
- [ ] Toolbar visibility can be toggled via View menu
- [ ] **Unified execution: Menu, toolbar, keyboard all use CommandRegistry**
- [ ] Code compiles, no warnings
- [ ] Application runs without crashes

---

## Testing

**Manual Test Plan:**

**Menu Testing:**
1. Launch application
2. Click File -> New (should create new document)
3. Click File -> Save (should save)
4. Click Edit -> Undo (should undo)
5. Verify keyboard shortcuts shown in menu (Ctrl+S, Ctrl+Z, etc.)

**Toolbar Testing:**
6. Verify Project and Edit toolbars visible at top
7. Click toolbar buttons (New, Open, Save, Undo, etc.) - verify they work
8. Open Settings -> Appearance, change Icon Size to 32, click Apply
9. Verify toolbar icons resize immediately

**Unified Execution Testing:**
10. Try File -> Save from menu
11. Try Save button from toolbar
12. Try Ctrl+S keyboard shortcut
13. **All three should execute identical code path via CommandRegistry**

**Toolbar Visibility:**
14. Go to View -> Toolbars, uncheck "Project"
15. Verify Project toolbar disappears
16. Re-check "Project" in menu
17. Verify Project toolbar reappears

**Persistence:**
18. Close application
19. Relaunch application
20. Verify toolbar visibility matches previous session

---

## Related Files

- `src/gui/main_window.h` (remove old toolbar members)
- `src/gui/main_window.cpp` (major refactoring)
- `include/kalahari/gui/core_commands.h` (include)
- `src/CMakeLists.txt` (verify all new files linked)

---

## Next Tasks

After this task completes, Command Registry architecture is fully integrated.

Remaining tasks (Settings system):
- Task #00036 - Icon Size Persistence Verification
- Task #00037 - Font Scaling Live Preview
- Task #00038 - Font Scaling Apply Implementation
- Task #00039 - Font Scaling Persistence Verification
- Task #00040 - Dynamic Text Wrapping Verification
- Task #00041 - Theme Restart Dialog Verification
- Task #00042 - Navigator Panel Cleanup Verification

---

**Created:** 2025-11-11
**Author:** Architecture Planning
