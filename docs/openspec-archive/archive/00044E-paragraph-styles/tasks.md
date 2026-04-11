# Tasks for #00044E: Paragraph Styles

## 1. Style Presets
- [ ] 1.1 Define built-in paragraph style presets in StyleResolver (H1, H2, H3, body, quote, code)
- [ ] 1.2 StyleResolver: resolve built-in styles without database (fallback defaults)

## 2. BookEditor
- [ ] 2.1 Implement applyParagraphStyle(styleId) — apply block + char format to current paragraph(s)
- [ ] 2.2 Add format.style.heading1 → code command callbacks in CommandRegistrar
- [ ] 2.3 Connect callbacks via MainWindow

## 3. Toolbar
- [ ] 3.1 Add paragraph style QComboBox to format toolbar (Normal, H1-3, Quote, Code)
- [ ] 3.2 Connect combo → applyParagraphStyle()
- [ ] 3.3 Sync combo to current paragraph's style on cursor move

## 4. Serialization
- [ ] 4.1 KML: serialize `<p style="heading1">` / parse style attribute

## 5. Testing
- [ ] 5.1 Unit tests: style application, KML round-trip
