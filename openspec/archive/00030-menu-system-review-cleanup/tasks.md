# Tasks for #00030 - Menu System Review & Cleanup

## Analysis (Phase 1 - Architect)
- [x] Audit all 9 menus - list items, shortcuts, phase markers
- [x] Identify duplicate menu entries (VIEW/Toolbars)
- [x] Identify shortcut conflicts (none found)
- [x] Identify commands with missing shortcuts

## Design (Phase 2 - Architect)
- [x] Propose clean menu structure (UX review with user)
- [x] Propose keyboard shortcuts (F1-F6, F11, Ctrl+B/I/U/F/H/W)
- [x] Create action plan for 5 phases

## Implementation (Phase 3 - Code Editor)

### Phase 1: Remove duplicates & fix shortcuts
- [x] Added REG_CMD_KEY macro for commands with shortcuts (no toolbar)
- [x] Changed panel shortcuts: Ctrl+1-5 → F2-F6
- [x] Added F11 for Full Screen
- [x] Added F1 for Help
- [x] Added Ctrl+F/H for Find/Replace
- [x] Added Ctrl+B/I/U for Bold/Italic/Underline
- [x] Added Ctrl+W for Close Book
- [x] Removed Reset Layout shortcut (Ctrl+0)

### Phase 2: Dynamic Toolbars submenu
- [x] Modified ToolbarManager::createViewMenuActions() to create "Toolbars" submenu
- [x] Removed static VIEW/Toolbars commands from CommandRegistry
- [x] Added "Toolbar Manager..." action dynamically

### Phase 3: Dynamic Perspectives submenu
- [x] Verified no duplicates in Perspectives (static commands OK for now)
- [x] Future: Will be replaced with PerspectiveManager

### Phase 4: Recent Books submenu
- [x] Created RecentBooksManager singleton class
- [x] Integrated with FILE menu (dynamic submenu)
- [x] Connected to MainWindow::onOpenRecentFile slot
- [x] Added addRecentFile() call in onOpenDocument()

### Phase 5: Verify icons
- [x] All icon themes (twotone, filled, outlined, rounded) have required icons

## Testing
- [x] Build passes without errors
- [x] All 73 tests pass (571 assertions)
- [x] Application runs correctly

## Documentation
- [x] Update CHANGELOG.md
- [x] Update session-state.json

## Completion
- **Date:** 2025-12-10
- **Status:** DEPLOYED
- **Files modified:**
  - `src/gui/register_commands.hpp` - Added REG_CMD_KEY macro
  - `src/gui/main_window.cpp` - Shortcuts, Recent Books, imports
  - `src/gui/toolbar_manager.cpp` - Dynamic Toolbars submenu
  - `include/kalahari/gui/main_window.h` - onOpenRecentFile slot
  - `include/kalahari/core/recent_books_manager.h` - NEW
  - `src/core/recent_books_manager.cpp` - NEW
  - `src/CMakeLists.txt` - Added RecentBooksManager
