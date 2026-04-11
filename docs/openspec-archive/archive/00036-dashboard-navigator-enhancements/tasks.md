# Tasks for #00036

## Phase A: Navigator Status Submenu

### Implementation
- [x] Add "Set Status" submenu to NavigatorPanel context menu
- [x] Create QActionGroup with radio actions (Draft, Revision, Final)
- [x] Read current chapter status from .kchapter file
- [x] Set checked state on correct radio action based on current status
- [x] Connect action triggered signals to status change handler
- [x] Implement status change handler that calls saveChapterMetadata()
- [x] Verify .kchapter file is updated immediately after status change

### Testing
- [x] Test context menu appears with status submenu for chapter files
- [x] Test correct status is checked based on file content
- [x] Test changing status updates .kchapter file

## Phase B: Dashboard Recent Books

### Implementation
- [x] Create BookCard widget class (RecentBookCard.h / RecentBookCard.cpp)
- [x] Implement BookCard layout: icon (48x48), title, stats, date
- [x] Add signals for card click (open project)
- [x] Update DashboardPanel to include "Recent Books" section header
- [x] Add vertical layout container for BookCard widgets
- [x] Load recent books data from RecentBooksManager
- [x] Populate BookCard widgets with project data
- [x] Connect card click to project open action (via openRecentBookRequested signal)
- [x] Handle empty state (no recent books message)
- [x] Fix/update keyboard shortcuts display section

### Testing
- [x] Test recent books section displays correctly
- [x] Test book cards show correct information
- [x] Test clicking card opens the project
- [x] Test empty state message when no recent books

## Phase C: Auto-load Last Project

### Implementation
- [x] Add setting key "general.autoLoadLastProject" to SettingsManager
- [x] Add checkbox to Dashboard below recent books section
- [x] Add checkbox to Settings dialog (General section)
- [x] Connect checkboxes to setting value bidirectionally
- [x] Implement auto-load logic in MainWindow::showEvent()
- [x] Modify "Clear Recent" action to clear Dashboard list
- [x] Ensure Recent Files menu and Dashboard list stay synchronized

### Testing
- [x] Test checkbox state persists across application restarts
- [x] Test auto-load opens last project on startup when enabled
- [x] Test auto-load does nothing when disabled
- [x] Test "Clear Recent" clears both lists

## Phase D: Polish & Fixes

### Critical Issues
- [x] #4: INVESTIGATE: Program very slow on first startup (root cause: 183 icons × 7 sizes = 1281 SVG renders; fix deferred to splashscreen implementation)
- [x] #11: Dashboard settings not working - fixed toggle + divider visibility

### Missing Features
- [x] #2: No way to re-show Dashboard - added View menu item and toolbar button
- [x] #1: Dashboard tab has no icon - added home icon
- [x] #10: Add Dashboard setting: number of items (3-9, default 5)

### Visual Polish
- [x] #3: Navigator treeview icons - default size changed to 20px
- [x] #5: Add settings for Dashboard icon size (24-64px, default 48px)
- [x] #8: Dashboard center divider - now uses themed palette.mid color
- [x] #9: Add subtle borders to clickable areas (1px, themed, border-radius 6px)
- [x] #6: Add dashboardPrimary/dashboardSecondary colors (Dark: #36BBA7/#075F5A, Light: #18786F/#36BBA7)
- [x] #7: Change infoPrimary/infoSecondary colors (Dark: #34A6F4/#10598A, Light: #1C69A8/#34A6F4)

## Phase E: Manual Testing Fixes

### Settings Synchronization
- [x] #E1: Dashboard checkbox "Open last project" not synced with Settings dialog checkbox after Apply

### UI Layout Consistency
- [x] #E2: DECISION: Icon size stays in Dashboard tab (user choice)
- [x] #E3: Dashboard spinboxes stretched (QGridLayout with column stretch)

### Dashboard Colors
- [x] #E4: Recent Files icons use dashboardPrimary/dashboardSecondary

### Visual Polish
- [x] #E5: Card borders more contrast (blend 70% mid + 30% text)
- [x] #E6: Center divider 50% opacity (alpha=127)

### Critical Bugs
- [x] #E7: Navigator double-click fixed (removed duplicate signal connection)

### Settings Dialog Styling
- [x] #E8: Settings text readable (palette.placeholderText instead of palette.mid)

## Phase F: Final Testing Fixes

### Dashboard Sync
- [x] #F1: Dashboard checkbox synced on startup (added sync in applyThemeColors() via QTimer)

### Visual Polish
- [x] #F2: Center divider reduced to 15% opacity (alpha=40)

### Performance
- [x] #F3: Settings dialog now only updates what changed (conditional theme/icon refresh)

## Documentation
- [x] Update CHANGELOG.md with new features
- [x] Update ROADMAP.md if applicable
