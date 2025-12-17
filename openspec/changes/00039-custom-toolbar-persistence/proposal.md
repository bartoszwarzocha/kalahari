# 00039: Custom Toolbar Persistence & Visibility

## Status
DEPLOYED

## Goal
Fix custom toolbar management issues: add View menu entries for custom toolbar visibility control and ensure custom toolbar visibility state persists correctly across application restarts.

## Context
Follow-up to OpenSpec #00031 (Toolbar System). User reported that:
1. Creating custom toolbars works correctly
2. Custom toolbars appear in Toolbar Configuration dialog
3. However, custom toolbars are missing from View menu
4. Custom toolbar visibility is not restored after restart

## Problem Analysis

### Issue 1: Missing View Menu Entries for Custom Toolbars
The View menu has a Toolbars submenu for built-in toolbars, but custom toolbars created via Toolbar Manager dialog are not added to this submenu. Users have no way to toggle custom toolbar visibility from the menu.

### Issue 2: Persistence of Custom Toolbar Visibility
Custom toolbars are present in the configuration (visible in Toolbar Manager dialog after restart) but their visibility state is not being restored properly. The toolbar exists but is hidden.

## Scope

### Included
1. **View Menu Integration for Custom Toolbars**
   - Add custom toolbars to View/Toolbars submenu
   - Update submenu dynamically when custom toolbars are created/deleted
   - Ensure checkmark state reflects actual visibility

2. **Visibility State Persistence**
   - Save custom toolbar visibility state to QSettings
   - Restore visibility state on application startup
   - Ensure state is saved when toolbar is shown/hidden

3. **State Restoration Order**
   - Ensure custom toolbars are created before visibility state is restored
   - Verify toolbar restoration happens in correct order during startup

### Excluded
- Custom toolbar creation/deletion logic (already working)
- Toolbar content customization (already working)
- Built-in toolbar visibility (already working)

## Acceptance Criteria
- [x] Custom toolbars appear in View/Toolbars submenu
- [x] Custom toolbar visibility can be toggled from View menu
- [x] Custom toolbar visibility state persists across restarts
- [x] Custom toolbars are visible after restart if they were visible before closing
- [x] View menu checkmarks correctly reflect custom toolbar visibility
- [x] No regressions in built-in toolbar functionality

## Implementation Summary

### Changes Made
1. Modified `createToolbars()` to create user toolbars at startup
2. Modified `createViewMenuActions()` to include user toolbars in View menu
3. Added `addViewMenuAction()` helper method for dynamic menu updates
4. Added `removeViewMenuAction()` helper method for dynamic menu updates
5. Updated `createUserToolbar()` to add View menu entry when toolbar created
6. Updated `deleteUserToolbar()` to remove View menu entry when toolbar deleted

### Files Modified
- `include/kalahari/gui/toolbar_manager.h` - Added helper method declarations
- `src/gui/toolbar_manager.cpp` - Implementation of persistence and menu integration

## Notes
- Custom toolbars are toolbars created by users via Toolbar Manager dialog
- Built-in toolbars are defined in ToolbarManager::initializeDefaultToolbars()
- All toolbar-related settings should use QSettings with consistent key naming
