# Text Styling System — Design Document

## Goal

Complete the text styling experience in Kalahari's editor. After #00044A established KML formatting round-trip, this design covers the remaining five features needed for a full styling system: pending format, toolbar sync, text color, paragraph styles, and user style management.

## Architecture

The system builds incrementally — each phase depends on the previous:

```
00044B: Pending Format + Clear     (core editing mechanics)
   ↓
00044C: Toolbar Sync + Font Size   (UI reflects editor state)
   ↓
00044D: Text Color + Background    (color support)
   ↓
00044E: Paragraph Styles           (built-in style presets)
   ↓
00044F: User Style Management      (custom styles, persistence)
```

## Phase B: Pending Format + Clear Formatting

**Goal:** Toggle bold → type → text is bold. Clear all formatting from selection.

**Scope:**
- Pending format application in `BookEditor::insertText()`
- Pending font family and font size
- Clear formatting (remove all char format from selection)
- Keyboard shortcut: Ctrl+Space

**Files:**
- `src/editor/book_editor.cpp` / `.h`
- `src/gui/command_registrar.cpp`

**Acceptance Criteria:**
- Toggle bold with no selection → type text → text is bold
- Toggle italic with no selection → type text → text is italic
- Set font with no selection → type text → text has that font
- Clear formatting removes all inline formatting from selection
- Ctrl+Space clears formatting

## Phase C: Toolbar State Sync + Font Size Dropdown

**Goal:** Toolbar reflects actual formatting at cursor position. Font size uses dropdown with presets.

**Scope:**
- `formattingChanged()` signal from BookEditor
- Toolbar sync: font combo, size, B/I/U/S toggle states
- Font size QComboBox with presets (8-72) replacing QSpinBox
- Feedback loop prevention (block signals during sync)

**Files:**
- `src/editor/book_editor.cpp` / `.h`
- `src/gui/toolbar_manager.cpp` / `.h`

**Acceptance Criteria:**
- Moving cursor to bold text → Bold button shows checked
- Moving cursor to 16pt text → Size dropdown shows 16
- Font combo shows font at cursor, not global
- Font size dropdown has presets + custom input
- No feedback loops

## Phase D: Text Color + Background Color

**Goal:** User can set text color and highlight color from toolbar. Colors serialize to KML.

**Scope:**
- `setSelectionTextColor()` / `setSelectionBackgroundColor()`
- Pending color (type colored text)
- Color picker popup widget (grid of common colors + Custom → QColorDialog)
- Toolbar color buttons with indicator
- KML round-trip via #00044A attributes

**Files:**
- `src/editor/book_editor.cpp` / `.h`
- `src/gui/toolbar_manager.cpp`
- New: color picker widget

**Acceptance Criteria:**
- Select text → pick color → text changes color
- Select text → pick highlight → background changes
- No selection → pick color → type → text has that color
- Color survives save/reload
- Toolbar shows current color at cursor

## Phase E: Paragraph Styles

**Goal:** Paragraph style dropdown works. Built-in presets: Heading 1-3, Body, Quote, Code.

**Style Definitions:**
- H1: 24pt bold
- H2: 20pt bold
- H3: 16pt bold italic
- Body: 12pt
- Quote: 12pt italic with indent
- Code: 11pt monospace

**Scope:**
- Built-in style presets in StyleResolver
- `applyParagraphStyle()` in BookEditor
- Style dropdown in toolbar
- KML `<p style="heading1">` serialization
- Command callbacks for `format.style.*`

**Files:**
- `src/editor/style_resolver.cpp` / `.h`
- `src/editor/book_editor.cpp` / `.h`
- `src/gui/toolbar_manager.cpp`
- `src/gui/command_registrar.cpp`
- `src/editor/kml_serializer.cpp`, `src/editor/kml_parser.cpp`

**Acceptance Criteria:**
- Select paragraph → choose Heading 1 → paragraph styled as 24pt bold
- Style dropdown shows current paragraph's style
- Paragraph style survives save/reload
- Built-in styles resolve without database

## Phase F: User Style Management

**Goal:** Users can create, edit, delete custom paragraph/character styles. Styles persist in project database.

**Scope:**
- CreateStyleDialog: capture selection formatting, name, save
- EditStyleDialog: edit style properties
- StylePalette widget: list built-in + user styles
- CRUD via ProjectDatabase API
- `format.style.manage` command

**Files:**
- `src/editor/style_resolver.cpp` / `.h`
- New: `src/gui/style_palette.cpp` / `.h`
- New: `src/gui/dialogs/create_style_dialog.cpp` / `.h`
- New: `src/gui/dialogs/edit_style_dialog.cpp` / `.h`

**Acceptance Criteria:**
- Create style from current selection
- Apply user style to selection
- Edit style properties
- Delete user style
- Styles persist after project close/reopen

## Dependencies Summary

| Phase | Depends On |
|-------|-----------|
| B | #00044A (KML round-trip) — DEPLOYED |
| C | B (pending format) |
| D | B (pending format), C (formattingChanged signal) |
| E | #00044A (alignment), C (toolbar sync) |
| F | E (paragraph styles) |

## Out of Scope

- Import/export styles
- Style templates
- Document themes (global style sets)
