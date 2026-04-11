# Tasks for #00040

## Phase 1: CommandRegistry Refactoring

### Core Changes
- [x] Add QAction storage (QHash<QString, QAction*>) to CommandRegistry
- [x] Add getAction(commandId) method returning QAction pointer
- [x] Modify registerCommand() to create QAction with all properties (lazy creation on getAction)
- [x] Add support for checkable actions in registration
- [x] Add updateActionState(commandId) method for refresh enabled/checked from callbacks
- [x] Ensure proper QAction parent (CommandRegistry owns actions)

### API Design
- [x] Design uses existing Command struct with all action properties
- [x] Add callback/slot connection mechanism for action triggers (via executeCommand)
- [x] Add getAllActions() for ToolbarManagerDialog
- [x] Add getActionsByCategory() for organized action listing
- [x] Add updateAllActionStates() for batch state refresh

## Phase 2: Menu Integration

### MenuBuilder Updates
- [x] Refactor MenuBuilder to use getAction() instead of creating new QAction
- [x] Remove duplicate QAction creation in menu building
- [x] Ensure menu shows action state (checked, enabled, etc.)
- [x] Test all existing menus work with shared actions

## Phase 3: Toolbar Integration

### Toolbar Updates
- [x] Update MainWindow toolbar creation to use shared actions
- [x] Remove duplicate QAction creation in toolbar setup
- [x] Ensure toolbar buttons reflect action state
- [x] Test toggle buttons work correctly (pressed state)

### ToolbarManagerDialog
- [x] Update ToolbarManagerDialog to query actions from CommandRegistry
- [x] Display available actions list from registry (uses getAction() for icons)
- [x] Allow drag-drop of shared actions to toolbars

## Phase 4: Fullscreen Implementation (Test Case)

### Fullscreen Feature
- [x] Register view.fullScreen as checkable action
- [x] Add toggleFullScreen() method to MainWindow
- [x] Connect action triggered signal to toggleFullScreen
- [x] Implement geometry save before entering fullscreen
- [x] Implement geometry restore when exiting fullscreen
- [x] Update action checked state when fullscreen changes
- [x] Persist geometry in SettingsManager for restart

## Testing

### Unit Tests
- [x] Test CommandRegistry action creation
- [x] Test getAction() returns same pointer for same command
- [x] Test checkable action state synchronization

### Integration Tests
- [x] Test menu and toolbar show same action state
- [x] Test action trigger works from both menu and toolbar
- [x] Test shortcut works regardless of focus

### Manual Tests
- [x] F11 toggles fullscreen
- [x] View menu toggles fullscreen
- [x] Menu checkmark reflects state
- [x] Toolbar button reflects state
- [x] Window returns to original size/position
- [x] All existing commands still work

## Documentation
- [x] Update CHANGELOG.md with refactoring and fullscreen feature
- [x] Add architecture notes to code comments
