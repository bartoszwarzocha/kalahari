# 00044D: Text Color + Background Color

## Status
MIGRATED — see docs/superpowers/specs/2026-04-10-text-styling-system-design.md

## Goal
User can set text color and highlight (background) color from toolbar. Colors serialize to KML.

## Scope
### Included
- setSelectionTextColor / setSelectionBackgroundColor
- Pending color (type colored text)
- Color picker popup widget
- Toolbar color buttons
- KML round-trip (via #00044A attributes)

### Excluded
- Style management (that's #00044F)

## Acceptance Criteria
- [ ] Select text → pick color → text changes color
- [ ] Select text → pick highlight → background changes
- [ ] No selection → pick color → type → text has that color
- [ ] Color survives save/reload
- [ ] Toolbar shows current color at cursor

## Dependencies
- #00044A (color serialization), #00044B (pending format), #00044C (formattingChanged signal for toolbar sync)

## Files to Modify
- `src/editor/book_editor.cpp` / `.h`
- `src/gui/toolbar_manager.cpp`
- New: color picker widget
