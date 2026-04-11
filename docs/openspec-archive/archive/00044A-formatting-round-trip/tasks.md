# Tasks for #00044A: Formatting Round-Trip

## 1. Serializer
- [x] 1.1 KmlSerializer: emit `align="..."` on `<p>` when alignment ≠ left
- [x] 1.2 KmlSerializer::serializeBlockContent(): after formatting tags, emit `font="..."` attribute when fontFamily ≠ default (NOTE: formatToOpenTags() only returns tag strings — attribute emission must be added to the serializer itself, not to the registry)
- [x] 1.3 KmlSerializer: emit `size="..."` attribute when fontSize ≠ default
- [x] 1.4 KmlSerializer: emit `color="..."` attribute when foreground ≠ black
- [x] 1.5 KmlSerializer: emit `bg="..."` attribute when background ≠ transparent
- [x] 1.6 KmlSerializer: emit `<span font="..." size="..." color="...">` wrapper when text has font/color/size overrides but no B/I/U/S tags

## 2. Parser
- [x] 2.1 KmlParser: parse `font`, `size`, `color`, `bg` attributes from formatting tags and `<span>`
- [x] 2.2 Add `<span>` to KmlFormatRegistry as recognized inline tag

## 3. Testing
- [x] 3.1 Unit tests: round-trip (apply format → serialize → parse → verify preserved)
- [x] 3.2 Backward compatibility test: old KML files load without errors
