# 00031: Toolbar System

## Status
PENDING

## Goal
Complete implementation of the toolbar system with full customization capabilities, floating/dockable behavior, and a professional Toolbar Manager dialog (Visual Studio-style) allowing users to configure toolbar contents from a palette of available commands.

## Context
Phase 1.1 from ROADMAP.md. Builds upon the existing toolbar infrastructure established in OpenSpec #00019 (ToolbarManager class) and refined in OpenSpec #00030 (Menu System Review).

## Current State
- ToolbarManager class exists (`src/gui/toolbar_manager.cpp`)
- 5 toolbars defined: File, Edit, Book, View, Tools
- Toolbars are already movable and floatable (basic Qt functionality)
- VIEW/Toolbars submenu dynamically created
- State persistence via QSettings (visibility + Qt windowState)
- Icons managed via ArtProvider (self-updating on theme change)
- Toolbar Manager... action exists but opens no dialog

## Scope

### Included
1. **Toolbar Completeness Audit**
   - Verify all 5 toolbars have appropriate commands
   - Synchronize toolbar names with menu structure
   - Evaluate need for additional toolbars (Format Toolbar)

2. **Floating/Dockable Behavior Enhancement**
   - Ensure all toolbars can be detached (floating)
   - Ensure all toolbars can dock to any edge (Top, Left, Right, Bottom)
   - Add visual feedback during drag operations
   - Verify toolbar break behavior (new row/column when dragging)

3. **Toolbar Customization Dialog (Toolbar Manager)**
   - Visual Studio-style command palette
   - List of available commands (from CommandRegistry)
   - Drag-and-drop or Add/Remove buttons
   - Per-toolbar configuration
   - Preview of changes before applying
   - Reset to defaults option

4. **User Toolbar Configuration Persistence**
   - Save custom toolbar layouts to settings
   - Restore custom layouts on startup
   - Export/Import toolbar configuration (optional)

5. **Right-Click Context Menu**
   - Show/hide individual toolbars
   - Quick access to Toolbar Manager
   - Lock/Unlock toolbar positions

### Excluded
- Icon design/creation (handled by ArtProvider system)
- New command implementation (commands must already exist in CommandRegistry)
- Menu system changes (completed in OpenSpec #00030)

## Existing Infrastructure Reference

### ToolbarManager Class
Location: `src/gui/toolbar_manager.cpp`, `include/kalahari/gui/toolbar_manager.h`

Key features:
- ToolbarConfig struct: id, label, defaultArea, defaultVisible, commandIds
- createToolbars(CommandRegistry&) - creates toolbars from config
- getToolbar(id), showToolbar(id, visible), isToolbarVisible(id)
- saveState(), restoreState() - persistence via QSettings
- createViewMenuActions(QMenu*) - VIEW/Toolbars submenu
- updateIconSizes() - responds to ArtProvider changes

### Current Toolbar Definitions
```cpp
m_configs["file"] = {"file", "File Toolbar", Qt::TopToolBarArea, true,
    {"file.new", "file.open", "file.save", "file.saveAs", "file.close"}};

m_configs["edit"] = {"edit", "Edit Toolbar", Qt::TopToolBarArea, true,
    {"edit.undo", "edit.redo", "_SEPARATOR_",
     "edit.cut", "edit.copy", "edit.paste", "edit.selectAll"}};

m_configs["book"] = {"book", "Book Toolbar", Qt::TopToolBarArea, true,
    {"book.newChapter", "book.newCharacter", "book.newLocation", "book.properties"}};

m_configs["view"] = {"view", "View Toolbar", Qt::TopToolBarArea, true,
    {"view.navigator", "view.properties", "view.search", "view.assistant", "view.log"}};

m_configs["tools"] = {"tools", "Tools Toolbar", Qt::TopToolBarArea, true,
    {"tools.spellcheck", "tools.stats.wordCount", "tools.focus.normal"}};
```

## Acceptance Criteria
- [ ] All toolbars can be freely moved, floated, and docked to any edge
- [ ] Toolbar Manager dialog implemented with command palette
- [ ] User can add/remove commands from any toolbar
- [ ] User can reorder commands within a toolbar (drag-and-drop)
- [ ] Custom toolbar configurations persist across sessions
- [ ] Right-click context menu on toolbar area works correctly
- [ ] Lock/Unlock toolbar positions feature works
- [ ] Reset to default toolbar configuration works
- [ ] No regressions in existing toolbar functionality
- [ ] All toolbars synchronized with current menu structure

## Design
(To be added by architect agent)

### Proposed New Files
- `src/gui/dialogs/toolbar_manager_dialog.h`
- `src/gui/dialogs/toolbar_manager_dialog.cpp`

### Files to Modify
- `src/gui/toolbar_manager.cpp` - extend for customization support
- `include/kalahari/gui/toolbar_manager.h` - new methods for customization
- `src/gui/main_window.cpp` - connect Toolbar Manager dialog

## Notes
- The Toolbar Manager dialog should follow the application's theme system
- All toolbar-related strings should use tr() for internationalization
- Consider QListWidget with drag-and-drop for command palette
- Use QDialogButtonBox for standard Apply/OK/Cancel/Reset buttons
- Reference Visual Studio's "Customize Toolbar" dialog for UX inspiration
