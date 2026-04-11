# 00044C: Toolbar State Sync + Font Size Dropdown

## Status
MIGRATED — see docs/superpowers/specs/2026-04-10-text-styling-system-design.md

## Goal
Toolbar reflects actual formatting at cursor position. Font size uses dropdown with presets instead of spinner.

## Scope
### Included
- formattingChanged() signal from BookEditor
- Toolbar sync: font combo, size, B/I/U/S toggle states
- Font size QComboBox with presets (8-72) replacing QSpinBox
- Feedback loop prevention

### Excluded
- Color picker (that's #00044D)
- Paragraph style dropdown (that's #00044E)

## Acceptance Criteria
- [ ] Moving cursor to bold text → Bold button shows checked
- [ ] Moving cursor to 16pt text → Size dropdown shows 16
- [ ] Font combo shows font at cursor, not global
- [ ] Font size dropdown has presets + custom input
- [ ] No feedback loops (toolbar change → editor → toolbar → ...)

## Dependencies
- #00044B (pending format)

## Files to Modify
- `src/editor/book_editor.cpp` / `.h`
- `src/gui/toolbar_manager.cpp` / `.h`
