# 00040: Central Action Management (Command Registry Refactoring)

## Status
DEPLOYED

## Goal
Refactor CommandRegistry to be the central source of truth for all application actions (QAction instances). Currently, CommandRegistry only stores command DEFINITIONS, while menus, toolbars, and dialogs create SEPARATE QAction instances for each command. This leads to synchronization issues (e.g., toggle states not updating across UI elements) and prevents centralized shortcut management.

## Problem Statement
Current architecture problems discovered during fullscreen implementation:
1. CommandRegistry stores only DEFINITIONS (name, shortcut, icon, description)
2. Menu, toolbar, and dialogs each create their OWN QAction instances
3. No synchronization between these instances (e.g., toggle fullscreen checked state)
4. No central shortcut management - shortcuts duplicated across QActions
5. ToolbarManagerDialog needs list of available actions but has no central source

## New Architecture
- **CommandRegistry** = Central API for actions, owns single QAction per command
- **Menu/Toolbar/Dialogs** = Renderers that display "execution points" for actions
- **One QAction per command**, shared by all renderers
- **Centralized shortcut management** - shortcuts defined once, applied once
- **ToolbarManagerDialog** integration - can query available actions from registry

## Scope
### Included
- Refactor CommandRegistry to create and own QAction instances
- Implement getAction(commandId) returning shared QAction pointer
- Update MenuBuilder to use shared actions from registry
- Update Toolbar creation to use shared actions from registry
- Update ToolbarManagerDialog to get action list from registry
- Implement fullscreen toggle as test case for new architecture
- Support checkable actions with synchronized state
- Centralized shortcut management

### Excluded
- Command serialization/persistence (separate feature)
- Undo/redo command history
- Macro recording
- Remote command execution
- Plugin command registration (can be added later)

## Acceptance Criteria
- [x] CommandRegistry owns single QAction per registered command
- [x] getAction(commandId) returns pointer to shared QAction
- [x] Menu items use shared actions (no duplicate QAction creation)
- [x] Toolbar buttons use shared actions (no duplicate QAction creation)
- [x] ToolbarManagerDialog can query available actions from registry
- [x] Checkable actions (like fullscreen) sync state across all UI elements
- [x] Shortcuts are defined once in CommandRegistry, work everywhere
- [x] Fullscreen toggle works correctly as proof of new architecture
- [x] F11 toggles fullscreen mode on/off
- [x] View > Full Screen menu item toggles fullscreen
- [x] Menu item shows checkmark when in fullscreen
- [x] Toolbar button shows pressed state when in fullscreen

## Design
CommandRegistry refactored to central action ownership:
- `m_commands` stores Command structs (definition)
- `m_actions` stores QAction* (one per command, lazy-created)
- `getAction(commandId)` returns shared QAction pointer
- `updateActionState(commandId)` synchronizes checked/enabled state
- Menu/Toolbar/Dialog use getAction() instead of creating own QAction
- Checkable actions (fullscreen) auto-sync state across all UI elements

### Files to Modify
- `include/kalahari/core/command_registry.h` - Add QAction storage, getAction() method
- `src/core/command_registry.cpp` - Implement QAction creation and management
- `src/gui/command_registrar.cpp` - Update to use new API
- `src/gui/menu_builder.cpp` - Use shared actions from registry
- `src/gui/main_window.cpp` - Update toolbar creation to use shared actions
- `src/gui/dialogs/toolbar_manager_dialog.cpp` - Use registry for action list
- `include/kalahari/gui/main_window.h` - Add fullscreen support

### New Files
- None expected (refactoring existing code)

## Notes
- This is a significant architectural change that will affect how actions are created throughout the app
- Fullscreen implementation serves as test case and proof of concept
- Consider backward compatibility - existing code should continue to work during transition
- QAction ownership: CommandRegistry will own actions, other classes get non-owning pointers
- Thread safety: Consider if actions will be accessed from multiple threads (unlikely in Qt UI)
- Memory management: Actions live for application lifetime, no early cleanup needed
