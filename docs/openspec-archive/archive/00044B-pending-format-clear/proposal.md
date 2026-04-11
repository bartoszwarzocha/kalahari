# 00044B: Pending Format + Clear Formatting

## Status
MIGRATED — see docs/superpowers/specs/2026-04-10-text-styling-system-design.md

## Goal
Make pending format work (toggle bold → type → text is bold) and implement clear formatting. Core editing experience complete.

## Scope
### Included
- Pending format application in insertText()
- Pending font family and font size
- Clear formatting (remove all char format from selection)
- Keyboard shortcut for clear formatting

### Excluded
- Toolbar sync (that's #00044C)
- Color (that's #00044D)

## Acceptance Criteria
- [ ] Toggle bold with no selection → type text → text is bold
- [ ] Toggle italic with no selection → type text → text is italic
- [ ] Set font with no selection → type text → text has that font
- [ ] Clear formatting removes all inline formatting from selection
- [ ] Ctrl+Space clears formatting

## Dependencies
- #00044A (formatting must survive save/reload)

## Files to Modify
- `src/editor/book_editor.cpp` / `.h`
- `src/gui/command_registrar.cpp`
