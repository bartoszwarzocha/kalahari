# 00036: Dashboard & Navigator Enhancements

## Status
DEPLOYED

## Goal
Improve UX with quick status changes in Navigator context menu and enhanced Dashboard with recent books list and auto-load functionality.

## Scope

### Included
- Navigator context menu status submenu for chapters
- Dashboard recent books section with card-style display
- Auto-load last project on startup option
- Synchronization between Recent Files and Dashboard recent books

### Excluded
- Changes to file format (uses existing .kchapter)
- New project types or templates
- Advanced filtering/sorting of recent books

## Acceptance Criteria
- [x] Navigator context menu shows "Set Status" submenu for chapter files
- [x] Status submenu has radio items: Draft, Revision, Final
- [x] Current chapter status is checked in submenu
- [x] Changing status updates .kchapter file immediately
- [x] Dashboard displays "Recent Books" section with up to 5 items
- [x] Each recent book shows as card with icon, title, stats, date
- [x] Clicking a recent book card opens the project
- [x] Dashboard has "Open last project on startup" checkbox
- [x] Settings dialog has matching checkbox in appropriate section
- [x] "Clear Recent" action clears both Recent Files menu and Dashboard list
- [x] Auto-load setting persists and works on application startup

## Design
(To be added by architect agent)

### Files to Modify
- `src/ui/NavigatorPanel.cpp` / `.h` - Add status submenu
- `src/ui/DashboardPanel.cpp` / `.h` - Add recent books section
- `src/ui/SettingsDialog.cpp` / `.h` - Add auto-load checkbox
- `src/ui/MainWindow.cpp` / `.h` - Auto-load logic on startup

### New Files
- `src/ui/BookCard.cpp` / `.h` - Card widget for recent books display

### Class Diagram
(To be added by architect agent)

## Implementation Phases

### Phase A: Navigator Status Submenu
- Add "Set Status" submenu to NavigatorPanel context menu
- Use QActionGroup for radio behavior (Draft, Revision, Final)
- Connect to saveChapterMetadata() for persistence
- Read current status from .kchapter to check correct item

### Phase B: Dashboard Recent Books
- Create BookCard widget with horizontal layout
- Update DashboardPanel to include "Recent Books" section
- Load recent books from RecentFiles or SettingsManager
- Display: icon (64x64), title (bold), description/stats, last modified
- Cards stacked vertically with scroll if needed
- Click opens the project

### Phase C: Auto-load Last Project
- Add setting key: "general.autoLoadLastProject" (bool)
- Add checkbox to Dashboard below recent books section
- Add checkbox to Settings dialog (General or Startup section)
- MainWindow::showEvent() checks setting and opens last project
- "Clear Recent" action clears both Recent Files menu and Dashboard list
- Both lists stay synchronized

## Notes
- Status values must match existing .kchapter format
- Book type icon should reflect the project type if available
- Consider empty state for Dashboard when no recent books exist
- Keyboard shortcuts section fix should be included in Dashboard work
