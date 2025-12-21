# 00042: Custom Text Editor - Tasks

> **Philosophy:** Pisarz, szklanka whisky, zanurzenie w procesie twórczym.
> Zero dystraktorów. Zero wersji mobilnych. Tylko słowa.

**CRITICAL:** Each task = BUILD + USER TEST before next task.
**Estimated:** ~120 atomic tasks across 7 phases.

---

## Phase 1: KML Model Layer (Foundation)

### 1.1 Project Setup ✅
- [x] Create `include/kalahari/editor/` directory
- [x] Create `src/editor/` directory
- [x] Create `tests/editor/` directory
- [x] Add editor module to `src/CMakeLists.txt`
- [x] Add editor tests to `tests/CMakeLists.txt`
- [x] **BUILD + TEST** (867 assertions, 90 test cases)

### 1.2 Basic Types ✅
- [x] Create `editor_types.h` - CursorPosition + SelectionRange (merged)
- [x] CursorPosition: comparison operators (==, !=, <, <=, >, >=)
- [x] SelectionRange: isEmpty(), isMultiParagraph(), normalized()
- [x] Unit tests for CursorPosition (15 assertions)
- [x] Unit tests for SelectionRange (15 assertions)
- [x] **BUILD + TEST** (867 assertions, 90 test cases)

### 1.3 KML Element Base
- [x] Create `kml_element.h` - KmlElement base class
- [x] Create `kml_element.cpp` - implementation
- [x] Define ElementType enum (Text, Bold, Italic, Underline, etc.)
- [x] Implement toKml() virtual method
- [x] Implement clone() virtual method
- [x] Unit tests for KmlElement
- [x] **BUILD + TEST** (909 assertions, 99 test cases)

### 1.4 KML Text Run
- [x] Create `kml_text_run.h` - KmlTextRun class (inline text with style)
- [x] Create `kml_text_run.cpp` - implementation
- [x] Implement text getter/setter
- [x] Implement styleId getter/setter
- [x] Implement toKml() for text run
- [x] Implement fromKml() static parser
- [x] Unit tests for KmlTextRun
- [x] **BUILD + TEST** (1029 assertions, 122 test cases)

### 1.5 KML Inline Elements
- [x] Create `kml_inline_elements.h` - Bold, Italic, Underline, Strike classes
- [x] Create `kml_inline_elements.cpp` - implementations
- [x] Implement nested element support (bold inside italic)
- [x] Implement toKml() for each inline type
- [x] Unit tests for inline elements
- [x] **BUILD + TEST** (1185 assertions, 161 test cases)

### 1.6 KML Paragraph (Basic) ✅
- [x] Create `kml_paragraph.h` - KmlParagraph class
- [x] Create `kml_paragraph.cpp` - implementation
- [x] Implement element container (vector<unique_ptr<KmlElement>>)
- [x] Implement addElement(), removeElement()
- [x] Implement styleId for paragraph style
- [x] Implement plainText() extraction
- [x] Implement toKml() for paragraph
- [x] Unit tests for KmlParagraph basic operations
- [x] **BUILD + TEST** (487 assertions, 100 test cases)

### 1.7 KML Paragraph (Advanced) ✅
- [x] Implement insertText(offset, text)
- [x] Implement deleteText(start, end)
- [x] Implement splitAt(offset) -> returns new paragraph
- [x] Implement mergeWith(otherParagraph)
- [x] Implement characterCount()
- [x] Unit tests for text manipulation (136 assertions, 18 test cases)
- [x] **BUILD + TEST** (775 assertions, 142 editor test cases)

### 1.8 KML Document (Basic) ✅
- [x] Create `kml_document.h` - KmlDocument class (observer pattern, not QObject - DLL export issues)
- [x] Create `kml_document.cpp` - implementation
- [x] Implement paragraph container
- [x] Implement paragraph(index), paragraphCount()
- [x] Implement addParagraph(), removeParagraph()
- [x] Implement insertParagraph(index, paragraph)
- [x] Define IDocumentObserver: onContentChanged, onParagraphModified, onParagraphInserted, onParagraphRemoved
- [x] Implement clone(), copy/move constructors
- [x] Implement plainText(), length(), isEmpty()
- [x] Implement toKml() serialization
- [x] Unit tests for KmlDocument basic operations (152 assertions, 24 test cases)
- [x] **BUILD + TEST** (639 assertions, 124 editor test cases)

### 1.9 KML Document (Text Operations) ✅
- [x] Implement insertText(CursorPosition, text)
- [x] Implement deleteText(CursorPosition start, CursorPosition end)
- [x] Implement applyStyle(SelectionRange, styleId)
- [x] Handle paragraph splitting on Enter (splitParagraph)
- [x] Handle paragraph merging on Backspace at start (mergeParagraphWithPrevious)
- [x] Unit tests for document text operations (120+ assertions, 14 test cases)
- [x] **BUILD + TEST** (895 assertions, 154 editor test cases)

### 1.10 KML Parser (Read) ✅
- [x] Create `kml_parser.h` - KmlParser class
- [x] Create `kml_parser.cpp` - implementation using QXmlStreamReader
- [x] Parse <p> paragraph elements
- [x] Parse <b>, <i>, <u>, <s> inline elements
- [x] Parse text content
- [x] Parse style attributes
- [x] Handle nested inline elements
- [x] Error handling for malformed KML
- [x] Unit tests for KML parsing (166 assertions, 20 test cases)
- [x] **BUILD + TEST** (1061 assertions, 174 editor test cases)

### 1.11 KML Serializer (Write) ✅
- [x] Implement KmlDocument::toKml() (already exists, verified)
- [x] Proper XML escaping (fixed KmlParagraph style attribute escaping)
- [x] Preserve whitespace correctly (QXmlStreamWriter handles this)
- [x] Unit tests for round-trip (parse -> serialize -> parse) - 120 assertions, 6 test cases
- [x] **BUILD + TEST** (1181 assertions, 180 editor test cases)

### 1.12 KML Table Elements ✅
- [x] Create `kml_table.h` - KmlTable, KmlTableRow, KmlTableCell classes
- [x] Create `kml_table.cpp` - implementations
- [x] Implement cell content (nested KmlParagraph)
- [x] Implement colspan, rowspan attributes
- [x] Implement toKml() for tables
- [x] Extend KmlParser to handle <table>, <tr>, <td>, <th>
- [x] Unit tests for table structures (225 assertions, 28 test cases)
- [x] **BUILD + TEST** (1406 assertions, 208 editor test cases)

---

## Phase 2: Layout Engine

### 2.1 Paragraph Layout (Basic) [COMPLETE]
- [x] Create `paragraph_layout.h` - ParagraphLayout class
- [x] Create `paragraph_layout.cpp` - implementation
- [x] Wrap QTextLayout
- [x] Implement setText(), setFont()
- [x] Implement doLayout(width) -> returns height
- [x] Implement isDirty(), invalidate()
- [x] Unit tests for basic layout (119 assertions, 26 test cases)
- [x] **BUILD + TEST** (2243 assertions, 296 test cases)

### 2.2 Paragraph Layout (Formatting) [COMPLETE]
- [x] Implement setFormats(QList<FormatRange>)
- [x] Map KmlElement styles to QTextCharFormat (FormatConverter class)
- [x] Handle bold, italic, underline, strikethrough
- [x] Handle subscript/superscript (with size scaling)
- [x] Unit tests for formatted layout (71 assertions, 10 test cases)
- [x] **BUILD + TEST** (2433 assertions, 332 test cases)

### 2.3 Paragraph Layout (Geometry) [COMPLETE - already in 2.1]
- [x] Implement height() getter
- [x] Implement lineCount()
- [x] Implement lineRect(lineIndex)
- [x] Implement boundingRect()
- [x] Unit tests for geometry (included in 2.1 tests)
- [x] **BUILD + TEST**

### 2.4 Paragraph Layout (Hit Testing) [COMPLETE]
- [x] Implement positionAt(QPointF) -> character offset
- [x] Implement cursorRect(position) -> QRectF
- [x] Handle click between characters
- [x] Unit tests for hit testing (100 assertions, 10 test cases)
- [x] **BUILD + TEST** (2533 assertions, 342 test cases)

### 2.5 Paragraph Layout (Drawing) [COMPLETE]
- [x] Implement draw(QPainter*, QPointF)
- [x] Handle selection highlighting
- [x] Handle spell error underlines (wavy red)
- [x] Unit tests for draw (visual inspection)
- [x] **BUILD + TEST** (all tests passed)

### 2.6 Table Layout (Basic) [COMPLETE]
- [x] Create `table_layout.h` - TableLayout class
- [x] Create `table_layout.cpp` - implementation
- [x] Implement cell size calculation
- [x] Implement column width distribution
- [x] Implement row height calculation
- [x] Unit tests for table layout
- [x] **BUILD + TEST** (206 assertions, 34 test cases)

### 2.7 Table Layout (Drawing) [COMPLETE]
- [x] Implement draw(QPainter*, QPointF)
- [x] Draw cell borders
- [x] Draw cell backgrounds
- [x] Handle cell padding
- [x] Unit tests for table drawing (45 assertions, 11 test cases)
- [x] **BUILD + TEST** (all 251 table_layout assertions passed)

### 2.8 Virtual Scroll Manager (Basic) [COMPLETE]
- [x] Create `virtual_scroll_manager.h`
- [x] Create `virtual_scroll_manager.cpp`
- [x] Implement setDocument(KmlDocument*)
- [x] Implement setViewport(top, height)
- [x] Implement visibleRange() -> QPair<int,int>
- [x] Define BUFFER_PARAGRAPHS constant (10)
- [x] Unit tests for visible range calculation (181 assertions, 23 test cases)
- [x] **BUILD + TEST** (3036 assertions, 424 test cases)

### 2.9 Virtual Scroll Manager (Heights) [COMPLETE]
- [x] Implement ParagraphInfo struct (y, height, heightKnown)
- [x] Implement updateParagraphHeight(index, height)
- [x] Implement totalHeight()
- [x] Implement paragraphY(index)
- [x] Use ESTIMATED_LINE_HEIGHT for unknown paragraphs
- [x] Unit tests for height estimation (91 new assertions, 13 new test cases)
- [x] **BUILD + TEST** (3127 assertions, 437 test cases)

### 2.10 Virtual Scroll Manager (Scrolling) [COMPLETE]
- [x] Implement scrollOffset getter/setter
- [x] Implement paragraphAtY(y) -> index
- [x] Implement ensureParagraphVisible(index)
- [x] Implement ensurePositionVisible(CursorPosition)
- [x] Implement maxScrollOffset()
- [x] Handle scroll offset changes (with clamping)
- [x] Unit tests for scrolling (48 new assertions, 6 new test cases)
- [x] **BUILD + TEST** (3175 assertions, 443 test cases)

### 2.11 Layout Manager [COMPLETE]
- [x] Create `layout_manager.h` - LayoutManager class
- [x] Create `layout_manager.cpp`
- [x] Manage ParagraphLayout instances (lazy creation)
- [x] Connect to VirtualScrollManager
- [x] Implement layoutVisibleParagraphs()
- [x] Invalidate layouts on content change
- [x] Implements IDocumentObserver for document change notifications
- [x] Unit tests for layout manager (155 assertions, 24 test cases)
- [x] **BUILD + TEST** (3330 assertions, 467 test cases)

---

## Phase 3: BookEditor Widget (Basic)

### 3.1 BookEditor Shell [COMPLETE]
- [x] Create `book_editor.h` - BookEditor : public QWidget
- [x] Create `book_editor.cpp`
- [x] Implement constructor/destructor
- [x] Implement setDocument(KmlDocument*)
- [x] Implement minimumSizeHint(), sizeHint()
- [x] Basic paintEvent() - just fill background
- [x] Unit tests for widget creation (44 assertions, 12 test cases)
- [x] **BUILD + TEST** (3374 assertions, 479 test cases - all pass)

### 3.2 BookEditor Scrolling [COMPLETE]
- [x] Add QScrollBar member (vertical)
- [x] Connect scrollbar to VirtualScrollManager
- [x] Implement wheelEvent() for mouse wheel
- [x] Implement smooth scrolling animation (QPropertyAnimation with OutCubic easing)
- [x] Update scrollbar range on content change
- [x] **BUILD + TEST** (all scrolling tests pass)

### 3.3 BookEditor Rendering (Basic) [COMPLETE]
- [x] Implement paintEvent() with paragraph rendering
- [x] Use LayoutManager for visible paragraphs
- [x] Render paragraph backgrounds (optional, commented for future use)
- [x] Render text using ParagraphLayout::draw()
- [x] **BUILD + TEST** (all 487 tests pass)

### 3.4 BookEditor Cursor (Position) [COMPLETE]
- [x] Add CursorPosition m_cursor member
- [x] Implement cursorPosition() getter
- [x] Implement setCursorPosition(CursorPosition)
- [x] Emit cursorPositionChanged signal
- [x] **BUILD + TEST** (105 assertions in 27 test cases)

### 3.5 BookEditor Cursor (Rendering) [COMPLETE]
- [x] Implement cursor blinking timer (500ms)
- [x] Render cursor line in paintEvent()
- [x] Calculate cursor rect from layout
- [x] Handle cursor at end of paragraph
- [x] **BUILD + TEST** (all 2598 editor assertions pass)

### 3.6 BookEditor Cursor (Navigation - Arrows) [COMPLETE]
- [x] Implement moveCursorLeft()
- [x] Implement moveCursorRight()
- [x] Implement moveCursorUp()
- [x] Implement moveCursorDown()
- [x] Handle paragraph boundaries
- [x] Handle keyPressEvent for arrow keys
- [x] Unit tests for arrow navigation (66 assertions, 15 test cases)
- [x] **BUILD + TEST** (2664 assertions, 421 editor test cases)

### 3.7 BookEditor Cursor (Navigation - Word) [COMPLETE]
- [x] Implement moveCursorWordLeft() (Ctrl+Left)
- [x] Implement moveCursorWordRight() (Ctrl+Right)
- [x] Implement moveCursorToLineStart() (Home)
- [x] Implement moveCursorToLineEnd() (End)
- [x] Handle keyPressEvent for Ctrl+Arrow and Home/End
- [x] **BUILD + TEST** (all tests pass)

### 3.8 BookEditor Cursor (Navigation - Document) [COMPLETE]
- [x] Implement moveCursorToDocStart() (Ctrl+Home)
- [x] Implement moveCursorToDocEnd() (Ctrl+End)
- [x] Implement moveCursorPageUp() (Page Up)
- [x] Implement moveCursorPageDown() (Page Down)
- [x] Handle keyPressEvent for Ctrl+Home/End and Page Up/Down
- [x] Preferred X position maintained during vertical navigation
- [x] **BUILD + TEST** (171 assertions, 42 book_editor test cases)

### 3.9 BookEditor Mouse (Click) [COMPLETE]
- [x] Implement mousePressEvent()
- [x] Convert click position to CursorPosition
- [x] Use ParagraphLayout::positionAt()
- [x] Set cursor on click
- [x] **BUILD + TEST** (all tests pass)

### 3.10 BookEditor Mouse (Selection) [COMPLETE]
- [x] Add SelectionRange m_selection member
- [x] Implement mouseMoveEvent() for drag selection
- [x] Update selection during drag
- [x] Render selection highlight in paintEvent()
- [x] **BUILD + TEST** (all tests pass)

### 3.11 BookEditor Mouse (Double/Triple Click) [COMPLETE]
- [x] Implement double-click to select word
- [x] Implement triple-click to select paragraph
- [x] Track click count with timer
- [x] **BUILD + TEST** (all tests pass)

### 3.12 BookEditor Selection (Keyboard) [COMPLETE]
- [x] Implement Shift+Arrow for selection extension
- [x] Implement Ctrl+Shift+Arrow for word selection
- [x] Implement Shift+Home/End for line selection
- [x] Implement Ctrl+A for select all
- [x] **BUILD + TEST** (232 assertions, 57 test cases)

---

## Phase 4: Text Input & Editing

### 4.1 Basic Text Input [COMPLETE]
- [x] Implement keyPressEvent() for printable characters
- [x] Insert character at cursor position
- [x] Move cursor after insertion
- [x] Clear selection before insert (replace)
- [x] **BUILD + TEST** (73 assertions, 11 test cases)

### 4.2 Enter Key [COMPLETE]
- [x] Handle Enter key in keyPressEvent()
- [x] Split paragraph at cursor
- [x] Create new paragraph
- [x] Move cursor to start of new paragraph
- [x] **BUILD + TEST** (all tests pass)

### 4.3 Backspace [COMPLETE]
- [x] Handle Backspace in keyPressEvent()
- [x] Delete character before cursor
- [x] Handle Backspace at paragraph start (merge)
- [x] Handle Backspace with selection (delete selection)
- [x] **BUILD + TEST** (all tests pass)

### 4.4 Delete Key [COMPLETE]
- [x] Handle Delete in keyPressEvent()
- [x] Delete character after cursor
- [x] Handle Delete at paragraph end (merge)
- [x] Handle Delete with selection
- [x] **BUILD + TEST** (all tests pass)

### 4.5 IME Support (Basic) [COMPLETE]
- [x] Implement inputMethodEvent()
- [x] Handle preedit string (composition)
- [x] Store composition state
- [x] Render composition underline (via preedit insertion)
- [x] **BUILD + TEST** (all tests pass)

### 4.6 IME Support (Commit) [COMPLETE]
- [x] Handle commit string in inputMethodEvent()
- [x] Clear composition on commit
- [x] Insert committed text
- [x] **BUILD + TEST** (all tests pass)

### 4.7 IME Support (Query) [COMPLETE]
- [x] Implement inputMethodQuery()
- [x] Return ImEnabled, ImCursorRectangle
- [x] Return ImFont, ImCursorPosition
- [x] Return ImSurroundingText, ImCurrentSelection
- [x] Test with CJK input (Chinese/Japanese/Korean)
- [x] **BUILD + TEST** (14 assertions, 3 test cases)

### 4.8 Undo/Redo Framework [COMPLETE]
- [x] Create `kml_commands.h` - KmlCommand base class
- [x] Create `kml_commands.cpp` - InsertText, DeleteText, ApplyStyle, SplitParagraph, MergeParagraphs commands
- [x] Add QUndoStack to BookEditor
- [x] Define CommandId enum
- [x] Connect Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z shortcuts
- [x] **BUILD + TEST** (7 assertions, 3 test cases)

### 4.9 Insert Text Command [COMPLETE]
- [x] Create InsertTextCommand class
- [x] Implement redo() - insert text
- [x] Implement undo() - delete inserted text
- [x] Store cursor positions (before/after)
- [x] Integrate with BookEditor::insertText()
- [x] **BUILD + TEST** (all tests pass)

### 4.10 Insert Text Command (Merging) [COMPLETE]
- [x] Implement mergeWith() for consecutive typing
- [x] Return same id() for mergeable commands
- [x] Limit merge to same paragraph
- [x] Limit merge time window (1 second)
- [x] Fixed dangling pointer bug after command merge
- [x] **BUILD + TEST** (2819 assertions, 453 test cases)

### 4.11 Delete Text Command [COMPLETE]
- [x] Create DeleteTextCommand class
- [x] Implement redo() - delete text range
- [x] Implement undo() - restore deleted text
- [x] Store deleted content for restoration
- [x] Integrate with deleteSelectedText, deleteBackward, deleteForward
- [x] **BUILD + TEST** (all tests pass)

### 4.12 Split/Merge Paragraph Commands [COMPLETE]
- [x] Create SplitParagraphCommand class (Enter key)
- [x] Create MergeParagraphsCommand class (Backspace/Delete at boundary)
- [x] Integrate with insertNewline()
- [x] Integrate with deleteBackward/deleteForward
- [x] **BUILD + TEST** (all tests pass)

### 4.13 Clipboard Handler (Copy) [COMPLETE]
- [x] Create `clipboard_handler.h`
- [x] Create `clipboard_handler.cpp`
- [x] Implement copy(doc, selection)
- [x] Set KML to clipboard (custom MIME type)
- [x] Set HTML to clipboard (converted)
- [x] Set plain text to clipboard
- [x] **BUILD + TEST** (61 assertions, 8 test cases)

### 4.14 Clipboard Handler (Paste) [COMPLETE]
- [x] Implement paste() -> returns KML
- [x] Check for KML format first
- [x] Convert HTML to KML if no KML
- [x] Convert plain text to KML if no HTML
- [x] **BUILD + TEST** (all tests pass)

### 4.15 Clipboard Handler (Conversion) [COMPLETE]
- [x] Implement kmlToHtml()
- [x] Implement htmlToKml()
- [x] Implement textToKml()
- [x] Implement kmlToText()
- [x] Handle common HTML tags (b, i, u, p, strong, em)
- [x] Unit tests for all conversions
- [x] **BUILD + TEST** (2880 assertions, 461 test cases)

### 4.16 Cut/Copy/Paste Integration [COMPLETE]
- [x] Implement Ctrl+X shortcut (cut)
- [x] Implement Ctrl+C shortcut (copy)
- [x] Implement Ctrl+V shortcut (paste)
- [x] Cut uses undo command from deleteSelectedText
- [x] **BUILD + TEST** (all tests pass)

---

## Phase 5: View Modes

### 5.1 View Mode Framework [COMPLETE]
- [x] Create `view_modes.h` - ViewMode enum
- [x] Add m_viewMode to BookEditor
- [x] Implement setViewMode(ViewMode)
- [x] Emit viewModeChanged signal
- [x] **BUILD + TEST** (3972 assertions, 579 test cases)

### 5.2 Continuous Mode [COMPLETE]
- [x] Implement continuous mode rendering
- [x] No page breaks
- [x] Simple vertical layout
- [x] (Default mode - already working via VirtualScrollManager)
- [x] **BUILD + TEST** (87 assertions, 12 test cases for view modes)

### 5.3 Page Mode (Basic) [COMPLETE]
- [x] Calculate page dimensions (A4/Letter)
- [x] Render page boundaries
- [x] Render page shadows
- [x] Handle page margins
- [x] **BUILD + TEST** (all tests pass)

### 5.4 Page Mode (Pagination) [COMPLETE]
- [x] Calculate page breaks
- [x] Handle widow/orphan control
- [x] Render page numbers
- [x] **BUILD + TEST** (all tests pass)

### 5.5 Page Mode (Navigation) [COMPLETE]
- [x] Implement Page Up/Down for page navigation
- [x] Jump to specific page
- [x] Show current page in status bar
- [x] **BUILD + TEST** (all tests pass)

### 5.6 Typewriter Mode (Basic) [COMPLETE]
- [x] Keep current line at vertical center
- [x] Smooth scroll animation on typing
- [x] Adjust scroll on cursor movement
- [x] **BUILD + TEST** (all tests pass)

### 5.7 Focus Mode (Basic) [COMPLETE]
- [x] Add m_focusMode flag
- [x] Implement setFocusMode(bool)
- [x] Dim non-active paragraphs (50% opacity)
- [x] **BUILD + TEST** (all tests pass)

### 5.8 Focus Mode (Sentence) [COMPLETE]
- [x] Option to focus on sentence (not paragraph)
- [x] Detect sentence boundaries
- [x] Highlight current sentence
- [x] **BUILD + TEST** (all tests pass)

### 5.9 Distraction-Free Mode (Basic) [COMPLETE]
- [x] Create fullscreen mode (F11)
- [x] Hide all UI except editor
- [x] Dark/light theme for distraction-free
- [x] Esc to exit
- [x] **BUILD + TEST** (all tests pass)

### 5.10 Distraction-Free Mode (Fade UI) [COMPLETE]
- [x] Show UI elements on mouse move to edges
- [x] Fade out after timeout (2 seconds)
- [x] Show word count at bottom
- [x] **BUILD + TEST** (all tests pass)

### 5.11 Split View (Framework) [COMPLETE]
- [x] Add split view support to EditorPanel
- [x] Horizontal split option (Ctrl+\)
- [x] Vertical split option (Ctrl+Shift+\)
- [x] Sync document between splits
- [x] **BUILD + TEST** (all tests pass)

### 5.12 Split View (Independent Scroll) [COMPLETE]
- [x] Each split has own scroll position
- [x] Each split has own cursor
- [x] Highlight active split
- [x] **BUILD + TEST** (all tests pass)

### 5.13 Split View (Close) [COMPLETE]
- [x] Implement Ctrl+W to close split
- [x] Revert to single view when one closed
- [x] Remember split positions
- [x] **BUILD + TEST** (all tests pass)

---

## Phase 6: Services

### 6.1 Style Resolver (Basic)
- [ ] Create `style_resolver.h`
- [ ] Create `style_resolver.cpp`
- [ ] Implement setDatabase(ProjectDatabase*)
- [ ] Implement resolve(styleId) -> ResolvedStyle
- [ ] Create ResolvedStyle struct
- [ ] **BUILD + TEST**

### 6.2 Style Resolver (Inheritance)
- [ ] Implement style inheritance (baseStyle)
- [ ] Prevent circular inheritance
- [ ] Cache resolved styles
- [ ] Implement invalidateCache()
- [ ] Unit tests for inheritance
- [ ] **BUILD + TEST**

### 6.3 Style Resolver (Application)
- [ ] Implement toFont() conversion
- [ ] Implement toCharFormat() conversion
- [ ] Implement toBlockFormat() conversion
- [ ] Connect to ParagraphLayout
- [ ] **BUILD + TEST**

### 6.4 Spell Check Service (Setup)
- [ ] Create `spell_check_service.h`
- [ ] Create `spell_check_service.cpp`
- [ ] Add Hunspell to vcpkg.json
- [ ] Add Hunspell to CMakeLists.txt
- [ ] Initialize Hunspell with dictionary
- [ ] **BUILD + TEST**

### 6.5 Spell Check Service (Dictionary)
- [ ] Load system dictionaries
- [ ] Support multiple languages
- [ ] Implement setLanguage(lang)
- [ ] Find dictionary files (platform-specific paths)
- [ ] **BUILD + TEST**

### 6.6 Spell Check Service (Checking)
- [ ] Implement markDirty(paragraphIndex)
- [ ] Implement background checking
- [ ] Use QTimer for debounce (300ms)
- [ ] Store SpellError results per paragraph
- [ ] **BUILD + TEST**

### 6.7 Spell Check Service (Results)
- [ ] Implement errorsForParagraph(index)
- [ ] Implement suggestions(word)
- [ ] Cache word check results
- [ ] Emit paragraphChecked signal
- [ ] **BUILD + TEST**

### 6.8 Spell Check Service (User Dictionary)
- [ ] Implement addToUserDictionary(word)
- [ ] Implement ignoreWord(word) (session only)
- [ ] Save user dictionary to file
- [ ] Load user dictionary on start
- [ ] **BUILD + TEST**

### 6.9 Spell Check UI
- [ ] Render wavy underline for errors
- [ ] Show context menu on right-click
- [ ] Show suggestions in menu
- [ ] Add to dictionary option
- [ ] Ignore option
- [ ] **BUILD + TEST**

### 6.10 Statistics Collector (Basic)
- [ ] Create `statistics_collector.h`
- [ ] Create `statistics_collector.cpp`
- [ ] Implement wordCount(), characterCount()
- [ ] Implement paragraphCount()
- [ ] Connect to document changes
- [ ] **BUILD + TEST**

### 6.11 Statistics Collector (Session)
- [ ] Track words written/deleted per session
- [ ] Track active time
- [ ] Implement startSession(), endSession()
- [ ] Store SessionStats
- [ ] **BUILD + TEST**

### 6.12 Statistics Collector (Database)
- [ ] Connect to ProjectDatabase
- [ ] Implement flush() - save to database
- [ ] Auto-flush every 5 minutes
- [ ] Flush on session end
- [ ] **BUILD + TEST**

### 6.13 Statistics UI
- [ ] Show word count in status bar
- [ ] Show character count
- [ ] Show estimated reading time
- [ ] Update in real-time
- [ ] **BUILD + TEST**

### 6.14 Grammar Check Service (Setup)
- [ ] Create `grammar_check_service.h`
- [ ] Create `grammar_check_service.cpp`
- [ ] Research LanguageTool integration (REST API vs library)
- [ ] Define GrammarError struct
- [ ] **BUILD + TEST**

### 6.15 Grammar Check Service (API)
- [ ] Implement LanguageTool API client
- [ ] Handle API authentication (if needed)
- [ ] Implement checkText(text) -> errors
- [ ] Handle network errors gracefully
- [ ] **BUILD + TEST**

### 6.16 Grammar Check Service (Integration)
- [ ] Connect to document changes
- [ ] Debounce API calls (1 second)
- [ ] Store grammar errors per paragraph
- [ ] Emit grammarChecked signal
- [ ] **BUILD + TEST**

### 6.17 Grammar Check UI
- [ ] Render grammar errors (different underline color - blue)
- [ ] Show context menu with suggestions
- [ ] Show error explanation
- [ ] Apply correction option
- [ ] **BUILD + TEST**

### 6.18 Word Frequency Analyzer (Basic)
- [ ] Create `word_frequency_analyzer.h`
- [ ] Create `word_frequency_analyzer.cpp`
- [ ] Implement analyze(document) -> word counts
- [ ] Ignore common words (stop words)
- [ ] Case-insensitive counting
- [ ] **BUILD + TEST**

### 6.19 Word Frequency Analyzer (Detection)
- [ ] Define "overused" threshold (configurable)
- [ ] Detect repetitive words
- [ ] Detect close repetition (within N paragraphs)
- [ ] **BUILD + TEST**

### 6.20 Word Frequency UI
- [ ] Create Word Frequency Panel (dockable)
- [ ] List words by frequency
- [ ] Click to highlight in document
- [ ] Option to highlight overused words
- [ ] **BUILD + TEST**

### 6.21 Text-to-Speech Service (Setup)
- [ ] Create `text_to_speech_service.h`
- [ ] Create `text_to_speech_service.cpp`
- [ ] Initialize QTextToSpeech
- [ ] List available voices
- [ ] Implement setVoice(voice)
- [ ] **BUILD + TEST**

### 6.22 Text-to-Speech Service (Playback)
- [ ] Implement speak(text)
- [ ] Implement pause(), resume(), stop()
- [ ] Track current position
- [ ] Emit positionChanged signal
- [ ] **BUILD + TEST**

### 6.23 Text-to-Speech UI
- [ ] Add TTS button to toolbar
- [ ] Read selected text (or from cursor)
- [ ] Highlight current word during playback
- [ ] Playback controls (pause/stop)
- [ ] **BUILD + TEST**

### 6.24 Text-to-Speech Settings
- [ ] Voice selection in preferences
- [ ] Speed control
- [ ] Volume control
- [ ] **BUILD + TEST**

---

## Phase 7: Integration & Polish

### 7.1 EditorPanel Integration (Basic)
- [ ] Replace QPlainTextEdit with BookEditor
- [ ] Connect document loading
- [ ] Connect document saving
- [ ] Handle .kchapter file format
- [ ] **BUILD + TEST**

### 7.2 EditorPanel Integration (Toolbar)
- [ ] Connect formatting buttons (bold, italic, etc.)
- [ ] Connect paragraph style dropdown
- [ ] Connect view mode selector
- [ ] Update toolbar state on cursor change
- [ ] **BUILD + TEST**

### 7.3 EditorPanel Integration (Menus)
- [ ] Connect Edit menu (undo, redo, cut, copy, paste)
- [ ] Connect Format menu (styles)
- [ ] Connect View menu (view modes, focus mode)
- [ ] Update menu state on selection change
- [ ] **BUILD + TEST**

### 7.4 PropertiesPanel Integration
- [ ] Show paragraph style in properties
- [ ] Show character count for selection
- [ ] Show word count for selection
- [ ] Allow style change from properties
- [ ] **BUILD + TEST**

### 7.5 NavigatorPanel Integration
- [ ] Update chapter modified indicator
- [ ] Handle chapter switching
- [ ] Save chapter on switch
- [ ] Confirm save on unsaved changes
- [ ] **BUILD + TEST**

### 7.6 ProjectDatabase Integration (Styles)
- [ ] Load paragraph styles from database
- [ ] Load character styles from database
- [ ] Save custom styles to database
- [ ] Sync style changes across editors
- [ ] **BUILD + TEST**

### 7.7 ProjectDatabase Integration (Statistics)
- [ ] Save session statistics to database
- [ ] Update chapter word count in database
- [ ] Track writing time per chapter
- [ ] **BUILD + TEST**

### 7.8 Comments Feature (Basic)
- [ ] Define KmlComment element
- [ ] Parse/serialize comments in KML
- [ ] Render collapsed comment marker
- [ ] Expand comment on click
- [ ] **BUILD + TEST**

### 7.9 Comments Feature (UI)
- [ ] Insert comment (Ctrl+Alt+C)
- [ ] Delete comment
- [ ] Edit comment text
- [ ] Show comment in side panel
- [ ] **BUILD + TEST**

### 7.10 TODO/FIX/CHECK Tags
- [ ] Detect TODO, FIX, CHECK in text
- [ ] Highlight with distinct colors
- [ ] List all tags in panel
- [ ] Navigate to tag on click
- [ ] **BUILD + TEST**

### 7.11 Quick Insert (@character)
- [ ] Detect @ prefix while typing
- [ ] Show character autocomplete dropdown
- [ ] Insert character reference
- [ ] Highlight character references
- [ ] **BUILD + TEST**

### 7.12 Quick Insert (#location)
- [ ] Detect # prefix while typing
- [ ] Show location autocomplete dropdown
- [ ] Insert location reference
- [ ] Highlight location references
- [ ] **BUILD + TEST**

### 7.13 Snapshots (Restore Points)
- [ ] Create snapshot on demand
- [ ] Store snapshot with timestamp
- [ ] List snapshots for chapter
- [ ] Restore from snapshot
- [ ] **BUILD + TEST**

### 7.14 Performance Optimization (Layout)
- [ ] Profile layout performance
- [ ] Optimize visible paragraph detection
- [ ] Lazy load paragraph layouts
- [ ] Cache layout results
- [ ] Target: 60 FPS with 100k words
- [ ] **BUILD + TEST**

### 7.15 Performance Optimization (Memory)
- [ ] Profile memory usage
- [ ] Release layout for off-screen paragraphs
- [ ] Optimize KML in-memory representation
- [ ] Target: < 100MB for 100k word document
- [ ] **BUILD + TEST**

### 7.16 Accessibility
- [ ] Implement QAccessibleInterface
- [ ] Support screen readers
- [ ] Full keyboard navigation
- [ ] High contrast mode support
- [ ] **BUILD + TEST**

### 7.17 Unit Tests (Complete)
- [ ] Test all edge cases
- [ ] Test error handling paths
- [ ] Achieve >80% code coverage
- [ ] **BUILD + TEST**

### 7.18 Integration Tests
- [ ] Test full document workflow (load, edit, save)
- [ ] Test undo/redo chain
- [ ] Test clipboard round-trip
- [ ] Test view mode switching
- [ ] **BUILD + TEST**

### 7.19 Manual Testing
- [ ] Test with long document (100k words)
- [ ] Test with CJK input (Chinese, Japanese, Korean)
- [ ] Test with RTL text (Arabic, Hebrew)
- [ ] Test all keyboard shortcuts
- [ ] Test all view modes
- [ ] **BUILD + TEST**

### 7.20 Documentation
- [ ] Update CHANGELOG.md
- [ ] Update ROADMAP.md
- [ ] Add user documentation for editor
- [ ] Add developer documentation (architecture)
- [ ] **BUILD + TEST**

### 7.21 Final Review
- [ ] Code review all new files
- [ ] Check for memory leaks
- [ ] Check for thread safety issues
- [ ] Verify all acceptance criteria met
- [ ] **BUILD + TEST - OPENSPEC COMPLETE**

---

## Summary

| Phase | Tasks | Description |
|-------|-------|-------------|
| 1 | 12 | KML Model Layer (Foundation) |
| 2 | 11 | Layout Engine |
| 3 | 12 | BookEditor Widget (Basic) |
| 4 | 16 | Text Input & Editing |
| 5 | 13 | View Modes |
| 6 | 24 | Services (Spell, Grammar, TTS, Stats) |
| 7 | 21 | Integration & Polish |
| **Total** | **109** | |

**Workflow:** Each numbered section (1.1, 1.2, etc.) = one atomic work unit.
Complete all checkboxes in section → BUILD + TEST → next section.

**Estimated time per section:** 30-90 minutes (implementation + build + test)
**Total estimated:** ~80-160 hours of implementation.

---

## Dependencies

```
Phase 1 (Model) ─────┬────► Phase 2 (Layout)
                     │
                     └────► Phase 3 (Widget) ───► Phase 4 (Editing)
                                    │
Phase 2 (Layout) ───────────────────┘
                                    │
                                    ▼
                            Phase 5 (View Modes)
                                    │
                                    ▼
                            Phase 6 (Services) ───► Phase 7 (Integration)
```

**Critical path:** Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 7
**Parallel options:** Phase 5 and Phase 6 can start after Phase 4 basics.
