# Tasks for #00044D: Text Color + Background Color

## 1. BookEditor Methods
- [ ] 1.1 Implement setSelectionTextColor(QColor) — apply to selection or set pending
- [ ] 1.2 Implement setSelectionBackgroundColor(QColor) — apply to selection or set pending
- [ ] 1.3 Implement currentTextColor() / currentBackgroundColor() queries
- [ ] 1.4 Add m_pendingTextColor, m_pendingBackgroundColor; apply in insertText()

## 2. Color Picker Widget
- [ ] 2.1 Create color picker popup (grid of common colors + "Custom..." → QColorDialog)
- [ ] 2.2 Add text color picker button to format toolbar
- [ ] 2.3 Add highlight color picker button to format toolbar

## 3. Integration
- [ ] 3.1 Connect `format.color` command callback in CommandRegistrar
- [ ] 3.2 Sync toolbar color indicator on formattingChanged()

## 4. Testing
- [ ] 4.1 Unit tests: color application, pending color, KML round-trip
