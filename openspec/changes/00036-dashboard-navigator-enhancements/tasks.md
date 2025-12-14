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

## Documentation
- [x] Update CHANGELOG.md with new features
- [x] Update ROADMAP.md if applicable
