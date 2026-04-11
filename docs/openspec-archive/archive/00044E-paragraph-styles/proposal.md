# 00044E: Paragraph Styles (Heading, Body, Quote, Code)

## Status
MIGRATED — see docs/superpowers/specs/2026-04-10-text-styling-system-design.md

## Goal
Paragraph style dropdown works. User can apply Heading 1-3, Body, Quote, Code. Styles defined as built-in presets.

## Scope
### Included
- Built-in style presets (H1: 24pt bold, H2: 20pt bold, H3: 16pt bold italic, body: 12pt, quote: 12pt italic indent, code: 11pt monospace)
- applyParagraphStyle() in BookEditor
- Style dropdown in toolbar
- KML `<p style="heading1">` serialization
- Command callbacks for format.style.*

### Excluded
- User-defined styles (that's #00044F)

## Acceptance Criteria
- [ ] Select paragraph → choose Heading 1 → paragraph styled as 24pt bold
- [ ] Style dropdown shows current paragraph's style
- [ ] Paragraph style survives save/reload
- [ ] Built-in styles resolve without database

## Dependencies
- #00044A (alignment serialization), #00044C (toolbar sync)

## Files to Modify
- `src/editor/style_resolver.cpp` / `.h`
- `src/editor/book_editor.cpp` / `.h`
- `src/gui/toolbar_manager.cpp`
- `src/gui/command_registrar.cpp`
- `src/editor/kml_serializer.cpp`, `src/editor/kml_parser.cpp`
