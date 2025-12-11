# 00032: Theme & Icons Optimization

## Status
PENDING

## Goal
Optimize theme and icon management to achieve:
1. Professional application startup (no visible loading steps)
2. Consistent icon color updates across all UI components
3. Clean, centralized architecture for theme/icon management

## Problem Statement

### Issue 1: Unprofessional Theme Loading
During application startup, users can see the loading steps:
- First the colors change
- Then icons appear in the correct color
This creates an unprofessional impression - users should see the fully themed UI immediately.

### Issue 2: Panel Header Icons Don't Update
Icons in panel headers (dock widgets) do not change color when primary/secondary colors are modified. This indicates inconsistent icon management - some components update correctly, others don't.

### Issue 3: Fragmented Icon Color Management
There's no unified, elegant, central mechanism for icon color management across all application components. Different parts of the UI handle icon colors differently.

## Scope

### Included
- Analyze current theme loading flow and identify optimization points
- Analyze icon color update mechanism and find gaps
- Fix panel header icons to respond to color changes
- Implement centralized icon color management
- Optimize startup sequence for professional appearance
- Full code review of theme and icon related components

### Excluded
- New theme features (beyond fixing current issues)
- UI redesign
- New icon sets

## Acceptance Criteria
- [ ] Application starts with fully themed UI (no visible loading steps)
- [ ] All icons (including panel headers) update when primary/secondary colors change
- [ ] Single, centralized mechanism for icon color management
- [ ] Code review completed for all theme/icon components
- [ ] No performance regression

## Components to Review
- `core::ArtProvider` - icon management
- `core::ThemeManager` - theme management
- Panel/dock widget headers
- Toolbar icons
- Menu icons
- Any other icon-using components

## Design
(To be added by architect agent)

### Files to Modify
(To be determined during analysis)

### New Files
(To be determined during analysis)

## Notes
- This is a quality/polish task focused on professional UX
- May require changes to application initialization order
- Signal/slot connections for color changes need audit
