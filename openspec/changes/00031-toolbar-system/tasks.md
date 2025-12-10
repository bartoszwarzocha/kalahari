# Tasks for #00031 - Toolbar System

## Analysis & Design
- [x] Audit current toolbar definitions against menu structure
- [x] Document all available commands from CommandRegistry (candidates for toolbar)
- [x] Design Toolbar Manager dialog UI mockup
- [x] Define data model for custom toolbar configuration persistence
- [x] Analyze Qt's drag-and-drop API for toolbar customization

## Implementation - Core Customization

### Phase A: Toolbar Configuration Model ✅
- [x] Define toolbar configuration data structures (ToolbarConfig)
- [x] Define serialization format for custom configs (JSON)
- [x] Add SettingsManager integration for toolbar configurations
- [x] Implement "default vs custom" configuration detection

### Phase B: Toolbar Manager Dialog (UI) ✅
- [x] Create ToolbarManagerDialog class (QDialog)
- [x] Implement toolbar selector (QListWidget for toolbar selection)
- [x] Implement available commands list (QTreeWidget with category filter)
- [x] Implement current toolbar commands list (QListWidget with reorder)
- [x] Add command button (double-click and explicit button)
- [x] Move Up/Move Down buttons for reordering
- [x] Remove button for commands
- [x] Separator insertion button
- [x] Apply/OK/Cancel/Reset buttons (QDialogButtonBox)
- [x] Connect dialog to VIEW/Toolbars/Toolbar Manager... action
- [x] Category filter and search functionality
- [x] New user toolbar creation dialog
- [x] Delete/Rename user toolbar support

### Phase C: ToolbarManager API Integration ✅
- [x] Pass ToolbarManager pointer to dialog constructor
- [x] Connect dialog saveToolbarConfigs() to ToolbarManager
- [x] Connect dialog loadToolbarConfigs() to ToolbarManager
- [x] Implement getToolbarIds(), getToolbarCommands(), setToolbarCommands()
- [x] Implement createUserToolbar(), deleteUserToolbar(), renameUserToolbar()
- [x] Implement isUserToolbar() check

### Phase D: Dynamic Toolbar Rebuilding ✅
- [x] Implement ToolbarManager::rebuildToolbar(id) method
- [x] Clear existing actions from toolbar
- [x] Repopulate from custom configuration
- [x] Maintain action connections to CommandRegistry
- [x] Handle theme/icon updates after rebuild
- [x] Implement loadConfigurations() / saveConfigurations() with SettingsManager

### Phase E: Context Menu & Locking ✅
- [x] Implement right-click context menu on toolbar area
- [x] Add toolbar visibility toggles to context menu
- [x] Add "Toolbar Manager..." to context menu
- [x] Add "Lock Toolbar Positions" toggle
- [x] Implement toolbar locking (setMovable(false) for all)
- [x] Persist lock state in settings

### Phase F: Overflow Menu (Chevron) ⏳ DEFERRED
- [ ] ~~Test toolbar break behavior (new row when dragging)~~
- [ ] ~~Add visual drag indicator if missing~~
- [ ] ~~Ensure proper toolbar area constraints~~
- [ ] ~~Overflow menu (chevron) for narrow toolbars~~

**Note:** Qt6 QToolBar does NOT have built-in overflow/chevron support.
Implementing this would require a custom toolbar layout class with resize
event handling and popup menu. Deferred to future enhancement.

## Testing
- [x] Build passes
- [x] All tests pass (73/73)
- [ ] Manual testing: Dialog opens and displays correctly
- [ ] Manual testing: Category filter and search
- [ ] Manual testing: Add/remove/reorder commands
- [ ] Manual testing: Create/delete/rename user toolbars
- [ ] Manual testing: Apply/OK/Cancel behavior
- [ ] Manual testing: Reset to Defaults
- [ ] Manual testing: Context menu on toolbars
- [ ] Manual testing: Toolbar locking
- [ ] Manual testing: Configuration persistence

## Documentation
- [x] Update CHANGELOG.md with Toolbar System entry
- [ ] Update ROADMAP.md - mark 1.1 Toolbar System progress
- [ ] Document Toolbar Manager dialog usage (user guide)
- [ ] Update developer docs with customization API

## Final Verification
- [ ] All acceptance criteria from proposal.md met (Phase F deferred)
- [x] Code review passed
- [ ] No regressions in existing functionality
- [ ] Application tested end-to-end with toolbar customizations

---

## Status Summary

**DEPLOYED (Phase A-E):**
- ToolbarManagerDialog: 3-column Visual Studio-style UI
- ToolbarManager API: Full customization support
- Context Menu: Right-click on toolbars
- Toolbar Locking: Lock/unlock positions
- Persistence: JSON via SettingsManager

**DEFERRED (Phase F):**
- Overflow menu (chevron) - requires custom implementation
- Qt6 does not provide built-in overflow support

**Commits:**
- `bbff89d` - Phase A-B (UI)
- `598dbc3` - Phase C (API Integration)
- `45f67f8` - Phase E (Context Menu & Locking)
