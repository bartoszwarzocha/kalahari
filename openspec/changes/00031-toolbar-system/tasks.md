# Tasks for #00031 - Toolbar System

## Analysis & Design
- [x] Audit current toolbar definitions against menu structure
- [x] Document all available commands from CommandRegistry (candidates for toolbar)
- [x] Design Toolbar Manager dialog UI mockup
- [x] Define data model for custom toolbar configuration persistence
- [x] Analyze Qt's drag-and-drop API for toolbar customization

## Implementation - Core Customization

### Phase A: Toolbar Configuration Model
- [x] Define toolbar configuration data structures (ToolbarConfig)
- [x] Define serialization format for custom configs (JSON)
- [ ] Add SettingsManager integration for toolbar configurations
- [ ] Implement "default vs custom" configuration detection

### Phase B: Toolbar Manager Dialog (UI Complete)
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

### Phase C: ToolbarManager API Integration (PENDING)
- [ ] Add singleton or accessor pattern to ToolbarManager
- [ ] Connect dialog saveToolbarConfigs() to ToolbarManager
- [ ] Connect dialog loadToolbarConfigs() to ToolbarManager
- [ ] Implement getToolbarIds(), getToolbarCommands(), setToolbarCommands()
- [ ] Implement createUserToolbar(), deleteUserToolbar(), renameUserToolbar()
- [ ] Implement isUserToolbar() check

### Phase D: Dynamic Toolbar Rebuilding (PENDING)
- [ ] Implement ToolbarManager::rebuildToolbar(id) method
- [ ] Clear existing actions from toolbar
- [ ] Repopulate from custom configuration
- [ ] Maintain action connections to CommandRegistry
- [ ] Handle theme/icon updates after rebuild
- [ ] Implement loadConfigurations() / saveConfigurations() with SettingsManager

### Phase E: Context Menu & Locking (PENDING)
- [ ] Implement right-click context menu on toolbar area
- [ ] Add toolbar visibility toggles to context menu
- [ ] Add "Toolbar Manager..." to context menu
- [ ] Add "Lock Toolbar Positions" toggle
- [ ] Implement toolbar locking (setMovable(false) for all)
- [ ] Persist lock state in settings

### Phase F: Enhanced Floating/Docking (PENDING)
- [ ] Test toolbar break behavior (new row when dragging)
- [ ] Add visual drag indicator if missing
- [ ] Ensure proper toolbar area constraints
- [ ] Overflow menu (chevron) for narrow toolbars

## Testing
- [ ] Test dialog opens and displays correctly
- [ ] Test category filter and search
- [ ] Test add/remove/reorder commands
- [ ] Test new/delete/rename user toolbars
- [ ] Test Apply/OK/Cancel behavior
- [ ] Test Reset to Defaults
- [ ] Test floating toolbar behavior on all platforms
- [ ] Test custom configuration persistence
- [ ] Verify no memory leaks (Qt ownership)

## Documentation
- [x] Update CHANGELOG.md with Toolbar System entry
- [ ] Update ROADMAP.md - mark 1.1 Toolbar System progress
- [ ] Document Toolbar Manager dialog usage (user guide)
- [ ] Update developer docs with customization API

## Final Verification
- [ ] All acceptance criteria from proposal.md met
- [x] Code review passed (Phase B)
- [ ] No regressions in existing functionality
- [ ] Application tested end-to-end with toolbar customizations

---

## Status Summary

**Completed (Phase A-B):**
- ToolbarManagerDialog UI fully functional
- 3-column Visual Studio-style layout
- Command browsing with category filter and search
- Add/remove/reorder commands in toolbars
- User toolbar creation/deletion/renaming
- Separator support
- Built-in toolbar configurations defined

**Pending (Phase C-F):**
- ToolbarManager API integration (actual toolbar modification)
- Configuration persistence to SettingsManager
- Context menu on toolbar area
- Toolbar locking
- Overflow menu (chevron)
