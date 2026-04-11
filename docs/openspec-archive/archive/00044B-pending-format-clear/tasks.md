# Tasks for #00044B: Pending Format + Clear Formatting

## 1. Pending Format
- [ ] 1.1 BookEditor::insertText() — build QTextCharFormat from m_pendingBold/Italic/Underline/Strikethrough, apply to inserted text
- [ ] 1.2 Reset pending flags after applying
- [ ] 1.3 Add m_pendingFontFamily, m_pendingFontSize members
- [ ] 1.4 setSelectionFontFamily/Size with no selection → set pending instead of global
- [ ] 1.5 isBold/isItalic etc. — check pending state when no selection

## 2. Clear Formatting
- [ ] 2.1 Implement BookEditor::clearFormatting() — remove char format from selection
- [ ] 2.2 Connect `format.clearFormatting` command callback
- [ ] 2.3 Add keyboard shortcut (Ctrl+Space)

## 3. Testing
- [ ] 3.1 Unit tests: pending format application, clear formatting
