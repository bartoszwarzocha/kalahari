# Tasks for #00044: Text Styling System

## Phase 1: Model Extensions

### InlineStyle Structure
- [ ] 1.1 Create `InlineStyle` struct with optional fields (fontFamily, fontSize, bold, italic, underline, strikethrough, color, backgroundColor)
- [ ] 1.2 Implement `InlineStyle::isEmpty()` method
- [ ] 1.3 Implement `InlineStyle::merge(other)` method for combining styles
- [ ] 1.4 Implement `InlineStyle::clear()` method

### KmlTextRun Extensions
- [ ] 1.5 Add `m_inlineOverrides` member to KmlTextRun
- [ ] 1.6 Add `setInlineStyle()` method
- [ ] 1.7 Add `inlineStyle()` getter
- [ ] 1.8 Add `hasInlineOverrides()` method
- [ ] 1.9 Update KmlTextRun copy/move constructors for InlineStyle

## Phase 2: KML Parser/Serializer

### Parser
- [ ] 2.1 Parse `bold` attribute on `<run>` elements
- [ ] 2.2 Parse `italic` attribute on `<run>` elements
- [ ] 2.3 Parse `underline` attribute on `<run>` elements
- [ ] 2.4 Parse `strikethrough` attribute on `<run>` elements
- [ ] 2.5 Parse `fontFamily` attribute on `<run>` elements
- [ ] 2.6 Parse `fontSize` attribute on `<run>` elements
- [ ] 2.7 Parse `color` attribute on `<run>` elements (hex format)
- [ ] 2.8 Parse `backgroundColor` attribute on `<run>` elements
- [ ] 2.9 Handle missing attributes gracefully (backward compatibility)

### Serializer
- [ ] 2.10 Serialize `bold` attribute when set
- [ ] 2.11 Serialize `italic` attribute when set
- [ ] 2.12 Serialize `underline` attribute when set
- [ ] 2.13 Serialize `strikethrough` attribute when set
- [ ] 2.14 Serialize `fontFamily` attribute when set
- [ ] 2.15 Serialize `fontSize` attribute when set
- [ ] 2.16 Serialize `color` attribute when set (hex format)
- [ ] 2.17 Serialize `backgroundColor` attribute when set
- [ ] 2.18 Skip serialization of unset optional values (minimal output)

## Phase 3: StyleResolver Updates

### Priority Resolution
- [ ] 3.1 Update `ResolvedStyle` to track override sources
- [ ] 3.2 Implement inline override priority (highest)
- [ ] 3.3 Implement character style priority (second)
- [ ] 3.4 Implement paragraph style priority (third)
- [ ] 3.5 Implement theme default priority (lowest)

### Resolution Methods
- [ ] 3.6 Add `resolveForTextRun(run, paragraphStyle)` method
- [ ] 3.7 Merge InlineStyle into ResolvedStyle
- [ ] 3.8 Cache resolved styles per text run
- [ ] 3.9 Invalidate cache on style changes
- [ ] 3.10 Invalidate cache on inline override changes

## Phase 4: Toolbar UI Changes

### Font Size Dropdown
- [ ] 4.1 Replace font size QSpinBox with QComboBox
- [ ] 4.2 Add preset sizes: 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 72
- [ ] 4.3 Make font size combo editable (allow custom values)
- [ ] 4.4 Validate custom font size input (min 6, max 144)
- [ ] 4.5 Connect font size change to applyFontSize action

### Font Family Dropdown
- [ ] 4.6 Ensure QFontComboBox is properly connected
- [ ] 4.7 Connect font family change to applyFontFamily action
- [ ] 4.8 Update font family display when selection changes

### Toggle Buttons
- [ ] 4.9 Connect Bold button to toggleBold action
- [ ] 4.10 Connect Italic button to toggleItalic action
- [ ] 4.11 Connect Underline button to toggleUnderline action
- [ ] 4.12 Connect Strikethrough button to toggleStrikethrough action
- [ ] 4.13 Update toggle button states based on selection

### Clear Formatting
- [ ] 4.14 Add format.clearFormatting action to ArtProvider
- [ ] 4.15 Create Clear Formatting toolbar button
- [ ] 4.16 Connect Clear Formatting to clearInlineFormatting action

### Style Dropdown
- [ ] 4.17 Add paragraph style dropdown (QComboBox)
- [ ] 4.18 Populate with theme styles (Normal, Heading 1-3, Quote, Code, etc.)
- [ ] 4.19 Connect style change to applyParagraphStyle action
- [ ] 4.20 Update style dropdown when cursor moves

## Phase 5: BookEditor Styling Logic

### Selection-Based Formatting
- [ ] 5.1 Implement `applyInlineStyle(InlineStyle)` for selection
- [ ] 5.2 Handle empty selection (apply to word under cursor)
- [ ] 5.3 Handle multi-paragraph selection
- [ ] 5.4 Split text runs at selection boundaries

### Individual Style Actions
- [ ] 5.5 Implement `toggleBold()` for selection
- [ ] 5.6 Implement `toggleItalic()` for selection
- [ ] 5.7 Implement `toggleUnderline()` for selection
- [ ] 5.8 Implement `toggleStrikethrough()` for selection
- [ ] 5.9 Implement `applyFontFamily(family)` for selection
- [ ] 5.10 Implement `applyFontSize(size)` for selection
- [ ] 5.11 Implement `applyTextColor(color)` for selection
- [ ] 5.12 Implement `applyBackgroundColor(color)` for selection

### Paragraph Formatting
- [ ] 5.13 Implement `applyAlignment(alignment)` for paragraph(s)
- [ ] 5.14 Implement `applyParagraphStyle(styleId)` for paragraph(s)
- [ ] 5.15 Handle multi-paragraph alignment change

### Clear Formatting
- [ ] 5.16 Implement `clearInlineFormatting()` for selection
- [ ] 5.17 Remove all inline overrides from selected runs
- [ ] 5.18 Merge adjacent runs after clearing (if same style)

### Style Query
- [ ] 5.19 Implement `currentInlineStyle()` for cursor/selection
- [ ] 5.20 Return mixed state indicator for heterogeneous selection
- [ ] 5.21 Emit signal when selection style changes

## Phase 6: Undo/Redo Integration

### Inline Style Commands
- [ ] 6.1 Create `ApplyInlineStyleCommand` for QUndoStack
- [ ] 6.2 Store old InlineStyle for undo
- [ ] 6.3 Handle run splitting in undo
- [ ] 6.4 Merge consecutive style commands when possible

### Clear Formatting Command
- [ ] 6.5 Create `ClearInlineFormattingCommand`
- [ ] 6.6 Store old styles for all affected runs

## Phase 7: User Style Management

### Style Storage
- [ ] 7.1 Design user_styles table schema in ProjectDatabase
- [ ] 7.2 Implement `saveUserStyle(name, style)` in StyleManager
- [ ] 7.3 Implement `loadUserStyles()` from database
- [ ] 7.4 Implement `deleteUserStyle(name)` from database
- [ ] 7.5 Implement `updateUserStyle(name, style)` in database

### Create Style Dialog
- [ ] 7.6 Create `CreateStyleDialog` UI
- [ ] 7.7 Capture current selection style as base
- [ ] 7.8 Allow editing style properties before save
- [ ] 7.9 Validate unique style name
- [ ] 7.10 Save new style to database

### Style Palette Widget
- [ ] 7.11 Create `StylePalette` widget
- [ ] 7.12 Display theme styles
- [ ] 7.13 Display user-defined styles
- [ ] 7.14 Apply style on click
- [ ] 7.15 Right-click menu for edit/delete user styles

## Phase 8: Rendering Integration

### Text Run Rendering
- [ ] 8.1 Apply resolved inline styles in ParagraphLayout
- [ ] 8.2 Create QTextLayout::FormatRange from InlineStyle
- [ ] 8.3 Handle color rendering
- [ ] 8.4 Handle background color rendering

### Selection Highlight
- [ ] 8.5 Preserve inline formatting in selection rendering
- [ ] 8.6 Show current format indicators in status bar

## Phase 9: Testing

### Unit Tests
- [ ] 9.1 Test InlineStyle merge behavior
- [ ] 9.2 Test KML parsing with inline attributes
- [ ] 9.3 Test KML serialization with inline attributes
- [ ] 9.4 Test backward compatibility (old KML files)
- [ ] 9.5 Test StyleResolver priority chain
- [ ] 9.6 Test run splitting on partial selection

### Integration Tests
- [ ] 9.7 Test toolbar -> BookEditor style flow
- [ ] 9.8 Test undo/redo for style changes
- [ ] 9.9 Test user style save/load cycle

### Manual Testing
- [ ] 9.10 Test all toolbar controls with real text
- [ ] 9.11 Test clear formatting behavior
- [ ] 9.12 Test style persistence after save/reload

## Documentation

- [ ] D.1 Update CHANGELOG.md with styling system entry
- [ ] D.2 Update ROADMAP.md to mark styling feature complete
