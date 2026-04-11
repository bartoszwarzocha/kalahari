# 00044A: Formatting Round-Trip Serialization

## Status
DEPLOYED

## Goal
Fix the core bug: text formatting (font, size, color, alignment) applied in the editor is lost on save/reload. After this OpenSpec, all formatting survives a KML save/reload cycle.

## Approach
Keep existing `<b>`, `<i>`, `<u>`, `<s>` tags. Add attributes (`font`, `size`, `color`, `bg`) on formatting tags. New `<span>` wrapper for text with font/color but no B/I/U/S.

## Scope
### Included
- KML serializer: emit font/size/color/bg/align attributes
- KML parser: read font/size/color/bg/align attributes
- `<span>` tag as attribute carrier
- Backward compatibility with old KML files

### Excluded
- UI changes (toolbar, color picker)
- Pending format
- Paragraph styles

## Acceptance Criteria
- [ ] Paragraph alignment survives save/reload
- [ ] Font family survives save/reload
- [ ] Font size survives save/reload
- [ ] Text color survives save/reload
- [ ] Background color survives save/reload
- [ ] Old KML files (without attributes) load without errors

## Dependencies
- None — first in chain

## Files to Modify
- `src/editor/kml_serializer.cpp`
- `src/editor/kml_format_registry.cpp` / `.h`
- `src/editor/kml_parser.cpp`
