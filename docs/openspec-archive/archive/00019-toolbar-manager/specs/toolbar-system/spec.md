# Specification: Toolbar Manager System

**Spec ID:** `toolbar-system`
**Change ID:** `00019-toolbar-manager`
**Version:** 1.0
**Status:** Approved
**Created:** 2025-11-22

---

## Requirements

### Requirement 1: Multiple Toolbars Creation

**ID:** `gui/toolbars/creation`

The ToolbarManager SHALL create 6 distinct toolbars on initialization: File, Edit, Book, View, and Tools toolbars.

**GIVEN** MainWindow is initializing
**WHEN** ToolbarManager::createToolbars() is called
**THEN** 6 QToolBar instances MUST be created
**AND** each toolbar MUST have a unique objectName ("file", "edit", "book", "view", "tools")
**AND** each toolbar MUST be added to MainWindow's Top toolbar area
**AND** all toolbars MUST be visible by default

---

### Requirement 2: Toolbar Command Assignment

**ID:** `gui/toolbars/commands`

Each toolbar SHALL contain specific commands retrieved from CommandRegistry.

**GIVEN** CommandRegistry has registered commands
**WHEN** ToolbarManager creates a toolbar
**THEN** File toolbar MUST contain: file.new, file.open, file.save, file.saveAs, file.close
**AND** Edit toolbar MUST contain: edit.undo, edit.redo, SEPARATOR, edit.cut, edit.copy, edit.paste, edit.selectAll
**AND** Book toolbar MUST contain: book.newChapter, book.newCharacter, book.newLocation, book.properties
**AND** View toolbar MUST contain: panel toggle actions for Navigator, Properties, Search, Assistant, Log
**AND** Tools toolbar MUST contain: tools.spellcheck, tools.stats.wordCount, tools.focus.normal

---

### Requirement 3: Icon System - Standard Icons

**ID:** `gui/toolbars/icons-standard`

IconSet SHALL support creation from Qt standard icons.

**GIVEN** a Qt Standard Pixmap enum value
**WHEN** IconSet::fromStandardIcon() is called
**THEN** an IconSet MUST be returned with 3 sizes (16x16, 24x24, 32x32)
**AND** all pixmaps MUST be valid (not null)
**AND** pixmaps MUST be rendered from QStyle::standardIcon()

---

### Requirement 4: Icon System - Placeholder Icons

**ID:** `gui/toolbars/icons-placeholder`

IconSet SHALL support creation of placeholder icons with colored backgrounds and letters.

**GIVEN** a letter string and color
**WHEN** IconSet::createPlaceholder() is called
**THEN** an IconSet MUST be returned with 3 sizes (16x16, 24x24, 32x32)
**AND** each pixmap MUST have the specified background color
**AND** each pixmap MUST display the letter in white, bold, centered
**AND** letter font size MUST be 60% of pixmap height

---

### Requirement 5: Command Icon Assignment

**ID:** `gui/toolbars/command-icons`

All toolbar commands SHALL have icons assigned during registration.

**GIVEN** a command is registered with REG_CMD_TOOL_ICON
**WHEN** the command is retrieved from CommandRegistry
**THEN** the command.icons field MUST NOT be empty
**AND** command.icons.icon24 MUST be valid for toolbar display
**AND** toQAction() MUST include the icon in the returned QAction

---

### Requirement 6: Toolbar Visibility Control

**ID:** `gui/toolbars/visibility`

ToolbarManager SHALL provide methods to show and hide toolbars.

**GIVEN** a toolbar with ID "edit"
**WHEN** ToolbarManager::showToolbar("edit", false) is called
**THEN** the Edit toolbar MUST become invisible
**AND** isToolbarVisible("edit") MUST return false
**WHEN** ToolbarManager::showToolbar("edit", true) is called
**THEN** the Edit toolbar MUST become visible
**AND** isToolbarVisible("edit") MUST return true

---

### Requirement 7: View Menu Integration

**ID:** `gui/toolbars/view-menu`

ToolbarManager SHALL create toggle actions in the View menu for toolbar visibility.

**GIVEN** View menu exists
**WHEN** ToolbarManager::createViewMenuActions() is called
**THEN** a "Toolbars" submenu MUST be added to View menu
**AND** 6 checkable QActions MUST be created (one per toolbar)
**AND** each action label MUST be "{Toolbar Name} Toolbar"
**AND** each action MUST be checked if toolbar is visible
**AND** clicking an action MUST toggle the corresponding toolbar's visibility

---

### Requirement 8: Action-Visibility Synchronization

**ID:** `gui/toolbars/view-sync`

View menu toolbar actions SHALL stay synchronized with actual toolbar visibility.

**GIVEN** a toolbar is visible
**WHEN** the toolbar is manually hidden (via close button or context menu)
**THEN** the corresponding View menu action MUST become unchecked
**GIVEN** a toolbar is hidden
**WHEN** the toolbar is shown via View menu action
**THEN** the View menu action MUST become checked

---

### Requirement 9: Toolbar Movability

**ID:** `gui/toolbars/movable`

All toolbars SHALL be movable and floatable.

**GIVEN** a toolbar is docked in the Top area
**WHEN** user drags the toolbar handle
**THEN** the toolbar MUST be movable to Left, Right, or Bottom areas
**AND** the toolbar MUST be floatable (detachable as separate window)
**AND** double-clicking the toolbar title MUST toggle float/dock state

---

### Requirement 10: State Persistence - Visibility

**ID:** `gui/toolbars/persistence-visibility`

Toolbar visibility state SHALL be saved and restored across sessions.

**GIVEN** Edit toolbar is hidden
**WHEN** ToolbarManager::saveState() is called
**THEN** QSettings key "Toolbars/edit/visible" MUST be set to false
**WHEN** application restarts and ToolbarManager::restoreState() is called
**THEN** Edit toolbar MUST be hidden
**AND** View menu action for Edit toolbar MUST be unchecked

---

### Requirement 11: State Persistence - Position

**ID:** `gui/toolbars/persistence-position`

Toolbar positions and floating state SHALL be saved and restored via Qt's saveState mechanism.

**GIVEN** File toolbar is moved to Left area
**WHEN** application is closed (triggering QMainWindow::saveState)
**THEN** toolbar position MUST be saved in QSettings "windowState" key
**WHEN** application restarts and QMainWindow::restoreState() is called
**THEN** File toolbar MUST appear in Left area

---

### Requirement 12: Toolbar Appearance

**ID:** `gui/toolbars/appearance`

Toolbars SHALL have consistent appearance and behavior.

**GIVEN** a toolbar is created
**WHEN** the toolbar is displayed
**THEN** toolbar icon size MUST be 24x24 pixels
**AND** toolbar button style MUST be IconOnly (no text labels)
**AND** toolbar MUST have a title matching its configuration label
**AND** toolbar MUST show tooltips on button hover

---

### Requirement 13: Command Execution from Toolbar

**ID:** `gui/toolbars/command-execution`

Clicking a toolbar button SHALL execute the corresponding command.

**GIVEN** a toolbar with a "Save" button
**WHEN** user clicks the "Save" button
**THEN** CommandRegistry::executeCommand("file.save") MUST be called
**AND** the command's execute callback MUST run
**AND** status bar MUST show command feedback (if applicable)

---

### Requirement 14: Separator Handling

**ID:** `gui/toolbars/separators`

Toolbars SHALL support separators between command groups.

**GIVEN** a toolbar configuration includes "_SEPARATOR_"
**WHEN** the toolbar is built
**THEN** a visual separator MUST be inserted at that position
**AND** the separator MUST create visual spacing between command groups

---

### Requirement 15: Default Toolbar State

**ID:** `gui/toolbars/defaults`

All toolbars SHALL be visible and positioned in Top area on first launch.

**GIVEN** fresh installation (no saved state)
**WHEN** MainWindow is first shown
**THEN** all 6 toolbars MUST be visible
**AND** all toolbars MUST be docked in Top toolbar area
**AND** all View menu toolbar actions MUST be checked

---

## Test Scenarios

### Scenario 1: Fresh Installation

**GIVEN** application is launched for the first time
**WHEN** MainWindow appears
**THEN** I see 6 toolbars in Top area: File, Edit, Book, View, Tools
**AND** all toolbars show icons (24x24)
**AND** all toolbars are movable
**AND** View > Toolbars shows 6 checked actions

### Scenario 2: Hide and Restore Toolbar

**GIVEN** Edit toolbar is visible
**WHEN** I click View > Toolbars > Edit Toolbar
**THEN** Edit toolbar disappears
**AND** View > Toolbars > Edit Toolbar becomes unchecked
**WHEN** I restart the application
**THEN** Edit toolbar is still hidden
**WHEN** I click View > Toolbars > Edit Toolbar again
**THEN** Edit toolbar reappears
**AND** checkmark is restored

### Scenario 3: Move and Float Toolbar

**GIVEN** File toolbar is docked in Top area
**WHEN** I drag File toolbar to Left area
**THEN** File toolbar docks on the left side
**WHEN** I double-click File toolbar title
**THEN** File toolbar becomes a floating window
**WHEN** I close and reopen the application
**THEN** File toolbar is floating at the same position

### Scenario 4: Command Execution

**GIVEN** File toolbar is visible
**WHEN** I click the "Save" button (disk icon)
**THEN** file.save command executes
**AND** status bar shows "Document saved" or equivalent
**AND** document is saved to disk

### Scenario 5: Icon Display

**GIVEN** toolbars are visible
**WHEN** I inspect toolbar buttons
**THEN** File toolbar shows: file icon (New), folder icon (Open), disk icon (Save), etc.
**AND** Edit toolbar shows: arrow left (Undo), arrow right (Redo), colored placeholders (Cut/Copy/Paste)
**AND** all icons are 24x24 pixels
**AND** no broken/missing icons

---

## Acceptance Criteria Summary

1. ✅ 6 toolbars created (File, Edit, Book, View, Tools)
2. ✅ Each toolbar contains correct commands
3. ✅ All toolbar commands have icons (Standard Qt or Placeholder)
4. ✅ Icons display correctly (24x24, valid pixmaps)
5. ✅ View > Toolbars submenu with 6 toggle actions
6. ✅ Toolbar visibility synced with View menu checkmarks
7. ✅ Toolbars movable, floatable, dockable
8. ✅ Toolbar state (position + visibility) persists across sessions
9. ✅ Clicking toolbar button executes command
10. ✅ All toolbars visible by default on first launch

---

**Version:** 1.0
**Status:** Approved
**Generated with Claude Code** | 2025-11-22
