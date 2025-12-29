# 00044: Text Styling System

## Status
PENDING

## Summary

Comprehensive text styling system for the BookEditor that enables proper formatting of text at paragraph and character level. Currently, font controls in the toolbar change global appearance instead of styling selected text/paragraph. This OpenSpec introduces a complete styling architecture with themes, paragraph styles, inline formatting, and user-defined styles.

## Goal

Create a layered text styling system that:
1. Provides theme-based default styles for new documents
2. Enables paragraph-level formatting (alignment, indentation, margins, line spacing)
3. Enables character-level (inline) formatting that overrides paragraph styles
4. Allows users to create, save, and manage custom styles
5. Integrates with existing toolbar for intuitive UX

## Scope

### Included

**1. Theme System**
- Predefined style sets (Normal, Heading 1-3, Quote, Code, Dialog, etc.)
- Default values for new documents
- Theme = collection of styles for predefined set
- Theme switching capability

**2. Paragraph Styles**
- Text alignment (left, center, right, justify)
- Indentation (first line, left, right)
- Margins (before paragraph, after paragraph)
- Line spacing (line height, multiplier)
- Tab stops
- Inheritance from theme with local overrides

**3. Character/Inline Styles**
- Font family
- Font size
- Bold, italic, underline, strikethrough
- Text color (foreground)
- Background color (highlight)
- Override paragraph style for selection only
- Clear formatting (remove overrides, return to theme style)

**4. User Style Management**
- Create style from selection
- Style palette/picker
- Edit existing user styles
- Delete user styles
- Style organization

**5. Toolbar UX Changes**
- Font family: QFontComboBox dropdown
- Font size: QComboBox with presets (8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 72) + custom input
- Bold/Italic/Underline/Strikethrough: Toggle buttons (functional)
- Alignment buttons: Paragraph alignment (functional)
- Clear formatting button: format.clearFormatting action
- Style dropdown: Paragraph/character style selection

**6. KML Model Extensions**
- Extend KmlTextRun with inline overrides:
  - fontFamily, fontSize
  - bold, italic, underline, strikethrough
  - color, backgroundColor
- KML serialization/parsing for new attributes
- Backward compatibility (missing attributes = default/inherit)

**7. Integration**
- ToolbarManager: Replace font size spinner with dropdown
- BookEditor: Selection-based styling logic
- StyleResolver: Inline overrides with priority over paragraph/theme styles
- PropertiesPanel: Display current selection style

### Excluded

- Complex style hierarchies (beyond theme -> paragraph -> character)
- Import/export of style definitions
- Style templates from external sources
- Multi-column layouts
- Advanced typography (kerning, ligatures)

## Acceptance Criteria

### Core Functionality
- [ ] Theme defines default styles for all predefined style types
- [ ] Paragraph styles apply to entire paragraphs
- [ ] Character styles apply only to selected text range
- [ ] Inline formatting overrides paragraph/theme styles
- [ ] Clear formatting removes inline overrides

### KML Model
- [ ] KmlTextRun stores inline style overrides
- [ ] KML parser reads inline style attributes
- [ ] KML serializer writes inline style attributes
- [ ] Backward compatibility: old KML files load without errors

### Toolbar Integration
- [ ] Font family dropdown applies to selection
- [ ] Font size dropdown applies to selection
- [ ] B/I/U/S buttons toggle for selection
- [ ] Alignment buttons apply to paragraph(s)
- [ ] Clear formatting button works
- [ ] Style dropdown changes paragraph style

### User Styles
- [ ] Create new style from selection
- [ ] Apply user style to selection
- [ ] Edit user style properties
- [ ] Delete user style

### StyleResolver
- [ ] Resolves final style with inheritance chain
- [ ] Inline overrides have highest priority
- [ ] Cache invalidation on style changes

## Technical Design

### Architecture Overview

```
Theme (default styles)
    |
    v
Paragraph Style (can override theme)
    |
    v
Character Style (can override paragraph)
    |
    v
Inline Override (highest priority, per-run)
```

### Style Resolution Priority

1. **Inline Override** (KmlTextRun attributes) - HIGHEST
2. **Character Style** (if applied to selection)
3. **Paragraph Style** (block-level formatting)
4. **Theme Default** - LOWEST

### KmlTextRun Extensions

```cpp
struct InlineStyle {
    std::optional<QString> fontFamily;
    std::optional<int> fontSize;
    std::optional<bool> bold;
    std::optional<bool> italic;
    std::optional<bool> underline;
    std::optional<bool> strikethrough;
    std::optional<QColor> color;
    std::optional<QColor> backgroundColor;

    bool isEmpty() const;
    void merge(const InlineStyle& other);
    void clear();
};

class KmlTextRun {
    // existing...
    InlineStyle m_inlineOverrides;

public:
    void setInlineStyle(const InlineStyle& style);
    const InlineStyle& inlineStyle() const;
    bool hasInlineOverrides() const;
};
```

### KML Serialization Format

```xml
<paragraph style="normal">
  <run>Plain text without overrides</run>
  <run bold="true" italic="true">Bold and italic text</run>
  <run fontFamily="Courier New" fontSize="14" color="#FF0000">Colored code</run>
</paragraph>
```

### New Classes

- `InlineStyle` - Data structure for inline overrides
- `StylePalette` - Widget for style selection/management
- `CreateStyleDialog` - Dialog for creating new user styles
- `EditStyleDialog` - Dialog for editing style properties

### Files to Modify

**Model:**
- `include/kalahari/editor/kml_text_run.h` - Add InlineStyle
- `src/editor/kml_text_run.cpp` - Implement InlineStyle methods
- `src/editor/kml_parser.cpp` - Parse inline attributes
- `src/editor/kml_serializer.cpp` - Serialize inline attributes

**Styling:**
- `include/kalahari/editor/style_resolver.h` - Add inline override handling
- `src/editor/style_resolver.cpp` - Implement priority resolution

**UI:**
- `src/gui/toolbar_manager.cpp` - Change font size to dropdown
- `src/gui/editor_panel.cpp` - Style application logic
- `src/editor/book_editor.cpp` - Selection-based styling

**New Files:**
- `include/kalahari/editor/inline_style.h`
- `src/editor/inline_style.cpp`
- `include/kalahari/gui/style_palette.h`
- `src/gui/style_palette.cpp`
- `include/kalahari/gui/create_style_dialog.h`
- `src/gui/create_style_dialog.cpp`

## Dependencies

- **Depends on:**
  - **OpenSpec #00043: Editor Performance Rewrite** - MUST be completed first
  - OpenSpec #00041: SQLite Project Database (for storing user styles)

- **Required by:**
  - Export functionality (needs to read styles for formatting)
  - Document templates (will use style definitions)

## Estimated Scope

- **Total: ~45-55 atomic tasks**
- Model + Parser: ~12 tasks
- StyleResolver updates: ~8 tasks
- UI/Toolbar: ~12 tasks
- BookEditor styling: ~15 tasks
- User style management: ~8 tasks

## Notes

- This is a prerequisite for proper word processor functionality
- Current toolbar controls are non-functional for text styling
- Design follows standard word processor patterns (Word, LibreOffice)
- Inline overrides stored in KML ensure document portability
