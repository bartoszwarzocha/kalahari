# Tasks for #00039

## Analysis
- [x] Analyze ToolbarManager::createViewMenuActions() - how custom toolbars are handled
- [x] Analyze ToolbarManager::saveState() - what is being saved
- [x] Analyze ToolbarManager::restoreState() - what is being restored
- [x] Identify startup sequence - when are custom toolbars created vs restored
- [x] Check how custom toolbars are stored vs built-in toolbars

## Implementation

### View Menu Integration
- [x] Modify createViewMenuActions() to include custom toolbars
- [x] Add mechanism to update View menu when custom toolbar is created
- [x] Add mechanism to update View menu when custom toolbar is deleted
- [x] Ensure toggle actions properly show/hide custom toolbars

### Persistence Fix
- [x] Fix saveState() to include custom toolbar visibility
- [x] Fix restoreState() to restore custom toolbar visibility
- [x] Ensure custom toolbars are created before restoreState() is called
- [x] Verify QSettings key naming consistency for custom toolbars

## Testing
- [x] Create custom toolbar, verify it appears in View menu
- [x] Toggle custom toolbar visibility from View menu
- [x] Close and reopen application, verify custom toolbar is visible
- [x] Create multiple custom toolbars, verify all persist correctly
- [x] Verify built-in toolbars still work correctly (no regression)

## Documentation
- [x] Update CHANGELOG.md with bug fix entry
