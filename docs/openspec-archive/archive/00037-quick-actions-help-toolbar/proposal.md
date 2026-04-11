# 00037: Quick Actions & Help Toolbar

## Status
DEPLOYED

## Goal
Add two new specialized toolbars:
1. **Quick Actions Toolbar** - Consolidated toolbar with most frequently used actions for rapid access
2. **Help Toolbar** - Quick access to help resources, keyboard shortcuts, updates, and about info

Additionally, implement proper multi-row toolbar support so toolbars automatically wrap to new rows when they don't fit in a single line.

## Background Analysis

### Current Toolbar Structure (8 toolbars)
| Toolbar | Commands | Default Visible |
|---------|----------|-----------------|
| File | new.project, open, save, saveAs, close | Yes |
| Edit | undo, redo, cut, copy, paste, selectAll | Yes |
| Book | newChapter, newCharacter, newLocation, properties | Yes |
| View | dashboard, navigator, properties, search, assistant, log | Yes |
| Tools | spellcheck, wordCount, focus | Yes |
| Format | font, size, bold/italic/underline, alignment, lists | Yes |
| Insert | image, table, link, footnote, comment | No |
| Styles | heading1-3, body, quote, code | No |

### Problem
With 6 visible toolbars, users have many buttons but lack quick access to:
- Program configuration (Settings, Customize Toolbars)
- Help resources without using menu
- Common utility actions (Backup)

### Solution
Two new toolbars that consolidate essential quick-access actions.

## Scope

### Included

#### 1. Quick Actions Toolbar (13 buttons)
**Purpose:** One-click access to most important actions across all categories

**Final Commands (USER APPROVED):**
| # | Command ID | Label | Icon | Source |
|---|------------|-------|------|--------|
| 1 | file.new | New File | file.new | **NEW** |
| 2 | file.new.project | New Project | file.new.project | File |
| 3 | file.open | Open | file.open | File |
| 4 | file.save | Save | file.save | File |
| 5 | file.saveAll | Save All | file.saveAll | File |
| - | --- | separator | | |
| 6 | edit.undo | Undo | edit.undo | Edit |
| 7 | edit.redo | Redo | edit.redo | Edit |
| - | --- | separator | | |
| 8 | edit.find | Find | edit.find | Edit |
| - | --- | separator | | |
| 9 | tools.backupNow | Backup Now | tools.backupNow | Tools |
| - | --- | separator | | |
| 10 | edit.settings | Settings | edit.settings | **NEW** |
| 11 | tools.toolbarManager | Customize Toolbars | tools.toolbarManager | **NEW** |

**Visual Layout:**
```
[New File] [New Project] [Open] [Save] [Save All] | [Undo] [Redo] | [Find] | [Backup] | [Settings] [Customize]
```

**Default Position:** Row 1, after File toolbar
**Default Visible:** Yes

#### 2. File Toolbar Update
Add `file.new` (New File) command to existing File Toolbar.

**Updated File Toolbar Commands:**
```
[New File] [New Project] [Open] [Save] [Save As] [Close]
```

#### 3. Help Toolbar (4 buttons)
**Purpose:** Quick access to help, documentation, and updates

**Final Commands:**
| # | Command ID | Label | Icon | Source |
|---|------------|-------|------|--------|
| 1 | help.manual | Help | help.help | Help menu |
| 2 | help.shortcuts | Keyboard Shortcuts | help.shortcuts | Help menu |
| 3 | help.checkUpdates | Check for Updates | help.checkUpdates | Help menu |
| 4 | help.about | About | help.about | Help menu |

**Visual Layout:**
```
[Help] [Keyboard Shortcuts] [Check for Updates] [About]
```

**Default Position:** Row 2, right side
**Default Visible:** Yes

#### 4. Multi-Row Toolbar Layout (USER APPROVED)
- Use `QMainWindow::addToolBarBreak()` to create toolbar row breaks
- Qt automatically adds more rows when toolbars don't fit
- User can manually rearrange/collapse if desired

**Default 2-Row Layout:**
```
Row 1: [File] [Quick Actions] [Edit] [Book]
Row 2: [Format] [View] [Tools] [Help]
```

#### 5. Default Toolbar Visibility (USER APPROVED)
| Toolbar | Default Visible | Reason |
|---------|-----------------|--------|
| File | Yes | Essential file operations |
| **Quick Actions** | **Yes** | **NEW - Primary quick access** |
| Edit | Yes | Essential edit operations |
| Format | Yes | Essential for writing |
| **Help** | **Yes** | **NEW - Quick help access** |
| Book | No | Accessible via Quick Actions/menu |
| View | No | Accessible via menu |
| Tools | No | Accessible via Quick Actions/menu |
| Insert | No | Optional (was already hidden) |
| Styles | No | Optional (was already hidden) |

#### 6. New Commands to Register
| Command ID | Label | Icon | Menu Path | Callback |
|------------|-------|------|-----------|----------|
| file.new | New File | file.new | FILE/New File | Create new standalone file |
| edit.settings | Settings... | edit.settings | EDIT/Settings... | Open Settings dialog |
| tools.toolbarManager | Customize Toolbars... | tools.toolbarManager | TOOLS/Customize Toolbars... | Open Toolbar Manager |

### Excluded
- Drag & drop toolbar reorganization across rows (future enhancement)
- User-configurable row breaks in UI (use Toolbar Manager dialog)
- Toolbar overflow menu (chevron) - deferred per ROADMAP

## Acceptance Criteria
- [x] `file.new` command registered and functional
- [x] `edit.settings` command opens Settings dialog
- [x] `tools.toolbarManager` command opens Toolbar Manager dialog
- [x] File Toolbar updated with New File button
- [x] Quick Actions toolbar created with 13 commands (11 buttons + 4 separators)
- [x] Help toolbar created with 4 commands
- [x] Both new toolbars appear in VIEW/Toolbars submenu
- [x] Both new toolbars support customization via Toolbar Manager dialog
- [x] Multi-row layout implemented (default 2 rows)
- [x] Default visibility applied (Book, View, Tools hidden)
- [x] Toolbar positions persist across sessions
- [x] Context menu works on new toolbars (visibility, lock, customize)

## Design

### Files to Modify
- `src/gui/toolbar_manager.cpp` - Add Quick Actions and Help configs, update File config
- `src/gui/toolbar_manager.h` - Update if needed
- `src/gui/main_window.cpp` - Register new commands, add toolbar breaks, update visibility defaults

### Icons Required
| Icon ID | File | Status |
|---------|------|--------|
| file.new | file.svg or note_add.svg | Verify exists |
| edit.settings | settings.svg | Already registered |
| tools.toolbarManager | build.svg or tune.svg | Need to register |
| help.help | help.svg | Already registered |
| help.shortcuts | keyboard.svg | Already registered |
| help.checkUpdates | update.svg | Already registered |
| help.about | info.svg | Already registered |

### Implementation Approach

#### Phase A: New Commands (45 min)
1. Register `file.new` command with callback to create new standalone file
2. Register `edit.settings` command with callback to open Settings dialog
3. Register `tools.toolbarManager` command with callback to open Toolbar Manager
4. Register icon for `tools.toolbarManager`

#### Phase B: Update File Toolbar (15 min)
1. Add `file.new` to File Toolbar config (first position)

#### Phase C: Quick Actions Toolbar (30 min)
1. Add config to `initializeConfigs()`:
```cpp
m_configs["quickActions"] = {
    "quickActions",
    "Quick Actions",
    Qt::TopToolBarArea,
    true,  // visible by default
    {"file.new", "file.new.project", "file.open", "file.save", "file.saveAll", SEPARATOR_ID,
     "edit.undo", "edit.redo", SEPARATOR_ID,
     "edit.find", SEPARATOR_ID,
     "tools.backupNow", SEPARATOR_ID,
     "edit.settings", "tools.toolbarManager"}
};
```
2. Add "quickActions" to creation order after "file"

#### Phase D: Help Toolbar (30 min)
1. Add config to `initializeConfigs()`:
```cpp
m_configs["help"] = {
    "help",
    "Help Toolbar",
    Qt::TopToolBarArea,
    true,  // visible by default
    {"help.manual", "help.shortcuts", "help.checkUpdates", "help.about"}
};
```
2. Add "help" to creation order at end

#### Phase E: Multi-Row Layout (30 min)
1. Modify `createToolbars()` to use row-based creation:
```cpp
// Row 1: File, Quick Actions, Edit, Book
std::vector<std::string> row1 = {"file", "quickActions", "edit", "book"};
// Row 2: Format, Insert, Styles, View, Tools, Help
std::vector<std::string> row2 = {"format", "insert", "styles", "view", "tools", "help"};

for (const auto& id : row1) {
    auto it = m_configs.find(id);
    if (it != m_configs.end()) {
        QToolBar* toolbar = createToolbar(it->second, registry);
        m_toolbars[id] = toolbar;
    }
}

m_mainWindow->addToolBarBreak(Qt::TopToolBarArea);

for (const auto& id : row2) {
    auto it = m_configs.find(id);
    if (it != m_configs.end()) {
        QToolBar* toolbar = createToolbar(it->second, registry);
        m_toolbars[id] = toolbar;
    }
}
```

#### Phase F: Update Default Visibility (15 min)
1. Set Book, View, Tools toolbar `defaultVisible = false` in configs

#### Phase G: Testing & Polish (30 min)
1. Verify all commands execute correctly
2. Test persistence of toolbar state
3. Test customization via Toolbar Manager
4. Test multi-row behavior on window resize

## Estimated Time
- Phase A: 45 min
- Phase B: 15 min
- Phase C: 30 min
- Phase D: 30 min
- Phase E: 30 min
- Phase F: 15 min
- Phase G: 30 min
- **Total: ~3.5 hours**

## Notes
- Settings command uses existing `edit.settings` icon (settings.svg)
- Help toolbar commands already have icons registered
- Toolbar customization already supported via ToolbarManager::setToolbarCommands()
- User-created toolbars (via Toolbar Manager) will also respect multi-row layout
- Qt handles additional row wrapping automatically when window is narrow

## Deployment Notes
- Deployed: 2025-12-16
- Implementation includes configuration versioning (v5) to handle QSettings migration
- Quick Actions toolbar does NOT include undo/redo (removed per final specification)
- Final layout: Row 1 (Quick Actions, Edit, Format, Insert), Row 2 (Book, Styles, Tools, Help)
- Hidden by default: File, View toolbars
