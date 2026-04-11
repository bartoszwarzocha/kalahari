# 00044F: User Style Management

## Status
MIGRATED — see docs/superpowers/specs/2026-04-10-text-styling-system-design.md

## Goal
Users can create, edit, and delete custom paragraph/character styles. Styles persist in project database.

## Scope
### Included
- CreateStyleDialog: capture selection formatting, name, save
- EditStyleDialog: edit style properties
- StylePalette widget: list built-in + user styles
- CRUD via ProjectDatabase API
- format.style.manage command

### Excluded
- Import/export styles
- Style templates

## Acceptance Criteria
- [ ] Create style from current selection
- [ ] Apply user style to selection
- [ ] Edit style properties
- [ ] Delete user style
- [ ] Styles persist after project close/reopen

## Dependencies
- #00044E (paragraph styles)

## Files to Modify
- `src/editor/style_resolver.cpp` / `.h`
- New: `src/gui/style_palette.cpp` / `.h`
- New: `src/gui/dialogs/create_style_dialog.cpp` / `.h`
- New: `src/gui/dialogs/edit_style_dialog.cpp` / `.h`
