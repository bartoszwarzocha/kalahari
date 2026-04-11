# Tasks for #00044C: Toolbar State Sync + Font Size Dropdown

## 1. BookEditor Signal
- [ ] 1.1 Add formattingChanged() signal to BookEditor
- [ ] 1.2 Detect formatting at cursor position, emit formattingChanged() on cursor move/selection change

## 2. Toolbar Sync
- [ ] 2.1 ToolbarManager: connect formattingChanged() → sync font combo, size, B/I/U/S
- [ ] 2.2 B/I/U/S buttons: make checkable, sync checked state to isBold/Italic/etc.
- [ ] 2.3 Font combo → show currentFontFamily() at cursor
- [ ] 2.4 Font size → show currentFontSize() at cursor

## 3. Font Size Dropdown
- [ ] 3.1 Replace QSpinBox with editable QComboBox (presets: 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 72)
- [ ] 3.2 Validate custom input (min 6, max 144)
- [ ] 3.3 Connect font size combo → BookEditor::setSelectionFontSize()

## 4. Safety
- [ ] 4.1 Guard against feedback loops (block signals during sync)
