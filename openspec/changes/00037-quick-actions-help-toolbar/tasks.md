# Tasks for #00037: Quick Actions & Help Toolbar

## Phase A: New Commands (45 min)
- [ ] Register `file.new` command (create new standalone file)
- [ ] Register `edit.settings` command (open Settings dialog)
- [ ] Register `tools.toolbarManager` command (open Toolbar Manager dialog)
- [ ] Register icon for `file.new` (file.svg or note_add.svg)
- [ ] Register icon for `tools.toolbarManager` (build.svg or tune.svg)

## Phase B: Update File Toolbar (15 min)
- [ ] Add `file.new` to File Toolbar config (first position)
- [ ] Verify File Toolbar displays correctly

## Phase C: Quick Actions Toolbar (30 min)
- [ ] Add `quickActions` config to `initializeConfigs()`
- [ ] Add to toolbar creation order after "file"
- [ ] Verify all 11 commands + 4 separators display correctly

## Phase D: Help Toolbar (30 min)
- [ ] Add `help` config to `initializeConfigs()`
- [ ] Add to toolbar creation order at end
- [ ] Verify all 4 help commands display correctly

## Phase E: Multi-Row Layout (30 min)
- [ ] Implement `addToolBarBreak()` between Row 1 and Row 2
- [ ] Row 1: File, Quick Actions, Edit, Book
- [ ] Row 2: Format, Insert, Styles, View, Tools, Help
- [ ] Test toolbar wrapping on window resize

## Phase F: Update Default Visibility (15 min)
- [ ] Set Book toolbar `defaultVisible = false`
- [ ] Set View toolbar `defaultVisible = false`
- [ ] Set Tools toolbar `defaultVisible = false`
- [ ] Verify File, Quick Actions, Edit, Format, Help remain visible

## Phase G: Integration & Polish (30 min)
- [ ] Quick Actions appears in VIEW/Toolbars submenu
- [ ] Help Toolbar appears in VIEW/Toolbars submenu
- [ ] Context menu works on Quick Actions toolbar
- [ ] Context menu works on Help toolbar
- [ ] Customization via Toolbar Manager dialog works

## Testing
- [ ] `file.new` creates new standalone file
- [ ] `edit.settings` opens Settings dialog
- [ ] `tools.toolbarManager` opens Toolbar Manager dialog
- [ ] All Quick Actions commands execute correctly
- [ ] All Help toolbar commands execute correctly
- [ ] Toolbar visibility toggles work for new toolbars
- [ ] Toolbar positions persist across sessions
- [ ] Multi-row layout persists across sessions
- [ ] Build passes without errors
- [ ] No regressions in existing toolbar functionality

## Documentation
- [ ] Update CHANGELOG.md with new feature
- [ ] Update ROADMAP.md if applicable
