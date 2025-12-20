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

### 3.1 BookEditor Shell
- [ ] Create `book_editor.h` - BookEditor : public QWidget
- [ ] Create `book_editor.cpp`
- [ ] Implement constructor/destructor
- [ ] Implement setDocument(KmlDocument*)
- [ ] Implement minimumSizeHint(), sizeHint()
- [ ] Basic paintEvent() - just fill background
- [ ] Unit tests for widget creation
- [ ] **BUILD + TEST**

### 3.2 BookEditor Scrolling
- [ ] Add QScrollBar member (vertical)
- [ ] Connect scrollbar to VirtualScrollManager
- [ ] Implement wheelEvent() for mouse wheel
- [ ] Implement smooth scrolling animation
- [ ] Update scrollbar range on content change
- [ ] **BUILD + TEST**

### 3.3 BookEditor Rendering (Basic)
- [ ] Implement paintEvent() with paragraph rendering
- [ ] Use LayoutManager for visible paragraphs
- [ ] Render paragraph backgrounds
- [ ] Render text using ParagraphLayout::draw()
- [ ] **BUILD + TEST**

### 3.4 BookEditor Cursor (Position)
- [ ] Add CursorPosition m_cursor member
- [ ] Implement cursorPosition() getter
- [ ] Implement setCursorPosition(CursorPosition)
- [ ] Emit cursorPositionChanged signal
- [ ] **BUILD + TEST**

### 3.5 BookEditor Cursor (Rendering)
- [ ] Implement cursor blinking timer (500ms)
- [ ] Render cursor line in paintEvent()
- [ ] Calculate cursor rect from layout
- [ ] Handle cursor at end of paragraph
- [ ] **BUILD + TEST**

### 3.6 BookEditor Cursor (Navigation - Arrows)
- [ ] Implement moveCursorLeft()
- [ ] Implement moveCursorRight()
- [ ] Implement moveCursorUp()
- [ ] Implement moveCursorDown()
- [ ] Handle paragraph boundaries
- [ ] **BUILD + TEST**

### 3.7 BookEditor Cursor (Navigation - Word)
- [ ] Implement moveCursorWordLeft() (Ctrl+Left)
- [ ] Implement moveCursorWordRight() (Ctrl+Right)
- [ ] Implement moveCursorToLineStart() (Home)
- [ ] Implement moveCursorToLineEnd() (End)
- [ ] **BUILD + TEST**

### 3.8 BookEditor Cursor (Navigation - Document)
- [ ] Implement moveCursorToDocStart() (Ctrl+Home)
- [ ] Implement moveCursorToDocEnd() (Ctrl+End)
- [ ] Implement moveCursorPageUp() (Page Up)
- [ ] Implement moveCursorPageDown() (Page Down)
- [ ] **BUILD + TEST**

### 3.9 BookEditor Mouse (Click)
- [ ] Implement mousePressEvent()
- [ ] Convert click position to CursorPosition
- [ ] Use ParagraphLayout::positionAt()
- [ ] Set cursor on click
- [ ] **BUILD + TEST**

### 3.10 BookEditor Mouse (Selection)
- [ ] Add SelectionRange m_selection member
- [ ] Implement mouseMoveEvent() for drag selection
- [ ] Update selection during drag
- [ ] Render selection highlight in paintEvent()
- [ ] **BUILD + TEST**

### 3.11 BookEditor Mouse (Double/Triple Click)
- [ ] Implement double-click to select word
- [ ] Implement triple-click to select paragraph
- [ ] Track click count with timer
- [ ] **BUILD + TEST**

### 3.12 BookEditor Selection (Keyboard)
- [ ] Implement Shift+Arrow for selection extension
- [ ] Implement Ctrl+Shift+Arrow for word selection
- [ ] Implement Shift+Home/End for line selection
- [ ] Implement Ctrl+A for select all
- [ ] **BUILD + TEST**

---

## Phase 4: Text Input & Editing

### 4.1 Basic Text Input
- [ ] Implement keyPressEvent() for printable characters
- [ ] Insert character at cursor position
- [ ] Move cursor after insertion
- [ ] Clear selection before insert (replace)
- [ ] **BUILD + TEST**

### 4.2 Enter Key
- [ ] Handle Enter key in keyPressEvent()
- [ ] Split paragraph at cursor
- [ ] Create new paragraph
- [ ] Move cursor to start of new paragraph
- [ ] **BUILD + TEST**

### 4.3 Backspace
- [ ] Handle Backspace in keyPressEvent()
- [ ] Delete character before cursor
- [ ] Handle Backspace at paragraph start (merge)
- [ ] Handle Backspace with selection (delete selection)
- [ ] **BUILD + TEST**

### 4.4 Delete Key
- [ ] Handle Delete in keyPressEvent()
- [ ] Delete character after cursor
- [ ] Handle Delete at paragraph end (merge)
- [ ] Handle Delete with selection
- [ ] **BUILD + TEST**

### 4.5 IME Support (Basic)
- [ ] Implement inputMethodEvent()
- [ ] Handle preedit string (composition)
- [ ] Store composition state
- [ ] Render composition underline
- [ ] **BUILD + TEST**

### 4.6 IME Support (Commit)
- [ ] Handle commit string in inputMethodEvent()
- [ ] Clear composition on commit
- [ ] Insert committed text
- [ ] **BUILD + TEST**

### 4.7 IME Support (Query)
- [ ] Implement inputMethodQuery()
- [ ] Return ImEnabled, ImCursorRectangle
- [ ] Return ImFont, ImCursorPosition
- [ ] Return ImSurroundingText, ImCurrentSelection
- [ ] Test with CJK input (Chinese/Japanese/Korean)
- [ ] **BUILD + TEST**

### 4.8 Undo/Redo Framework
- [ ] Create `kml_commands.h` - KmlCommand base class
- [ ] Create `kml_commands.cpp`
- [ ] Add QUndoStack to BookEditor
- [ ] Define CommandId enum
- [ ] Connect Ctrl+Z, Ctrl+Y shortcuts
- [ ] **BUILD + TEST**

### 4.9 Insert Text Command
- [ ] Create InsertTextCommand class
- [ ] Implement redo() - insert text
- [ ] Implement undo() - delete inserted text
- [ ] Store cursor positions (before/after)
- [ ] Unit tests for insert command
- [ ] **BUILD + TEST**

### 4.10 Insert Text Command (Merging)
- [ ] Implement mergeWith() for consecutive typing
- [ ] Return same id() for mergeable commands
- [ ] Limit merge to same paragraph
- [ ] Limit merge time window (1 second)
- [ ] Unit tests for command merging
- [ ] **BUILD + TEST**

### 4.11 Delete Text Command
- [ ] Create DeleteTextCommand class
- [ ] Implement redo() - delete text range
- [ ] Implement undo() - restore deleted KML
- [ ] Store deleted content as KML
- [ ] Handle multi-paragraph deletion
- [ ] Unit tests for delete command
- [ ] **BUILD + TEST**

### 4.12 Apply Style Command
- [ ] Create ApplyStyleCommand class
- [ ] Implement redo() - apply style to range
- [ ] Implement undo() - restore old styles
- [ ] Store old style per affected element
- [ ] Unit tests for style command
- [ ] **BUILD + TEST**

### 4.13 Clipboard Handler (Copy)
- [ ] Create `clipboard_handler.h`
- [ ] Create `clipboard_handler.cpp`
- [ ] Implement copy(doc, selection)
- [ ] Set KML to clipboard (custom MIME type)
- [ ] Set HTML to clipboard (converted)
- [ ] Set plain text to clipboard
- [ ] **BUILD + TEST**

### 4.14 Clipboard Handler (Paste)
- [ ] Implement paste() -> returns KML
- [ ] Check for KML format first
- [ ] Convert HTML to KML if no KML
- [ ] Convert plain text to KML if no HTML
- [ ] **BUILD + TEST**

### 4.15 Clipboard Handler (Conversion)
- [ ] Implement kmlToHtml()
- [ ] Implement htmlToKml()
- [ ] Implement textToKml()
- [ ] Handle common HTML tags (b, i, u, p)
- [ ] Unit tests for clipboard conversion
- [ ] **BUILD + TEST**

### 4.16 Cut/Copy/Paste Integration
- [ ] Implement Ctrl+X shortcut (cut)
- [ ] Implement Ctrl+C shortcut (copy)
- [ ] Implement Ctrl+V shortcut (paste)
- [ ] Create compound undo for cut
- [ ] **BUILD + TEST**

---

## Phase 5: View Modes

### 5.1 View Mode Framework
- [ ] Create `view_modes.h` - ViewMode enum
- [ ] Add m_viewMode to BookEditor
- [ ] Implement setViewMode(ViewMode)
- [ ] Emit viewModeChanged signal
- [ ] **BUILD + TEST**

### 5.2 Continuous Mode
- [ ] Implement continuous mode rendering
- [ ] No page breaks
- [ ] Simple vertical layout
- [ ] (Default mode - already working)
- [ ] **BUILD + TEST**

### 5.3 Page Mode (Basic)
- [ ] Calculate page dimensions (A4/Letter)
- [ ] Render page boundaries
- [ ] Render page shadows
- [ ] Handle page margins
- [ ] **BUILD + TEST**

### 5.4 Page Mode (Pagination)
- [ ] Calculate page breaks
- [ ] Handle widow/orphan control
- [ ] Render page numbers
- [ ] **BUILD + TEST**

### 5.5 Page Mode (Navigation)
- [ ] Implement Page Up/Down for page navigation
- [ ] Jump to specific page
- [ ] Show current page in status bar
- [ ] **BUILD + TEST**

### 5.6 Typewriter Mode (Basic)
- [ ] Keep current line at vertical center
- [ ] Smooth scroll animation on typing
- [ ] Adjust scroll on cursor movement
- [ ] **BUILD + TEST**

### 5.7 Focus Mode (Basic)
- [ ] Add m_focusMode flag
- [ ] Implement setFocusMode(bool)
- [ ] Dim non-active paragraphs (50% opacity)
- [ ] **BUILD + TEST**

### 5.8 Focus Mode (Sentence)
- [ ] Option to focus on sentence (not paragraph)
- [ ] Detect sentence boundaries
- [ ] Highlight current sentence
- [ ] **BUILD + TEST**

### 5.9 Distraction-Free Mode (Basic)
- [ ] Create fullscreen mode (F11)
- [ ] Hide all UI except editor
- [ ] Dark/light theme for distraction-free
- [ ] Esc to exit
- [ ] **BUILD + TEST**

### 5.10 Distraction-Free Mode (Fade UI)
- [ ] Show UI elements on mouse move to edges
- [ ] Fade out after timeout (2 seconds)
- [ ] Show word count at bottom
- [ ] **BUILD + TEST**

### 5.11 Split View (Framework)
- [ ] Add split view support to EditorPanel
- [ ] Horizontal split option (Ctrl+\)
- [ ] Vertical split option (Ctrl+Shift+\)
- [ ] Sync document between splits
- [ ] **BUILD + TEST**

### 5.12 Split View (Independent Scroll)
- [ ] Each split has own scroll position
- [ ] Each split has own cursor
- [ ] Highlight active split
- [ ] **BUILD + TEST**

### 5.13 Split View (Close)
- [ ] Implement Ctrl+W to close split
- [ ] Revert to single view when one closed
- [ ] Remember split positions
- [ ] **BUILD + TEST**

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
