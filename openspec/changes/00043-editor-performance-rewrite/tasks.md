# Tasks for #00043: Editor Performance Rewrite

## Phase 1: Research & Spike ✅ COMPLETE

### Proof of Concept
- [x] 1.1 Create minimal piece table prototype (standalone) → Pivoted to Fenwick tree
- [x] 1.2 Benchmark Fenwick tree vs QString for 150k words insert/delete
- [x] 1.3 Create minimal lazy layout prototype
- [x] 1.4 Benchmark viewport-only rendering vs full document
- [x] 1.5 Document findings and confirm architecture decision → Fenwick tree + lazy layout

---

## Phase 2: TextBuffer (Fenwick Tree) ✅ COMPLETE

### Core Structure (Pivoted from Piece Table to Fenwick Tree)
- [x] 2.1 Create `TextBuffer` class with Fenwick tree internals (HeightTree)
- [x] 2.2 Implement `HeightTree` for O(log N) height queries
- [x] 2.3 Implement paragraph storage with QString per paragraph
- [x] 2.4 Implement paragraph height tracking
- [x] 2.5 Implement content height prefix sums via Fenwick tree

### Text Operations
- [x] 2.6 Implement `setPlainText()` - bulk load
- [x] 2.7 Implement `insertParagraph()` / `removeParagraph()`
- [x] 2.8 Implement `setParagraphHeight()` with Fenwick tree update
- [x] 2.9 Implement `getParagraphHeight()` / `getParagraphY()`
- [x] 2.10 Implement `findParagraphAtY()` - O(log N) binary search

### Text Access
- [x] 2.11 Implement `plainText()` for full document
- [x] 2.12 Implement `paragraphText(index)` for single paragraph
- [x] 2.13 Implement `paragraphCount()` - O(1)
- [x] 2.14 Implement `totalHeight()` via Fenwick tree
- [x] 2.15 Implement modification signals

### Unit Tests
- [x] 2.16 Test paragraph operations
- [x] 2.17 Test height queries
- [x] 2.18 Test Fenwick tree correctness
- [x] 2.19 Test findParagraphAtY accuracy
- [x] 2.20 Test modification tracking

---

## Phase 3: FormatLayer (Phase 3.5 in implementation) ✅ COMPLETE

### Core Structure
- [x] 3.1 Create `FormatRange` struct (start, end, format)
- [x] 3.2 Create `TextFormat` struct (bold, italic, underline, strikethrough, etc.)
- [x] 3.3 Create `FormatLayer` class
- [x] 3.4 Implement interval tree (`IntervalTree<FormatRange>`)

### Format Operations
- [x] 3.5 Implement `addFormat(start, end, format)`
- [x] 3.6 Implement `removeFormat(start, end, type)`
- [x] 3.7 Implement `getFormatsAt(position)` - O(log N + k)
- [x] 3.8 Implement `getFormatsForRange(start, end)`

### Text Change Handling
- [x] 3.9 Implement `onTextInserted(position, length)` - shift ranges
- [x] 3.10 Implement `onTextDeleted(position, length)` - shrink/remove ranges
- [x] 3.11 Handle range splitting when text inserted in middle
- [x] 3.12 Handle range boundary adjustment

### Unit Tests
- [x] 3.13 Test format add/remove operations
- [x] 3.14 Test format query at position
- [x] 3.15 Test range adjustment on text insert/delete

---

## Phase 4: LazyLayoutManager ✅ COMPLETE

### Core Structure
- [x] 4.1 Create `LazyLayoutManager` class
- [x] 4.2 Create `LayoutEntry` struct (layout, valid flag, height)
- [x] 4.3 Create `LayoutState` enum (Invalid, Estimated, Valid)
- [x] 4.4 Maintain vector of LayoutEntry for paragraphs

### Height Estimation
- [x] 4.5 Calculate `m_estimatedLineHeight` from font metrics
- [x] 4.6 Estimate height based on text length / chars per line
- [x] 4.7 Implement `getEstimatedHeight(paraIndex)` for uncomputed layouts
- [x] 4.8 Update TextBuffer heights when layouts computed
- [x] 4.9 Implement adaptive estimation via `recalculateEstimatedHeight()`

### Lazy Layout Calculation
- [x] 4.10 Implement `getLayout(paraIndex)` - compute on demand
- [x] 4.11 Create QTextLayout for paragraph on demand
- [x] 4.12 Apply formats from FormatLayer to QTextLayout
- [x] 4.13 Cache computed layout in LayoutEntry
- [x] 4.14 Update TextBuffer height after layout

### Height Queries
- [x] 4.15 Implement `getHeight(paraIndex)` - real or estimated
- [x] 4.16 TextBuffer::totalHeight() for total (via Fenwick tree)
- [x] 4.17 TextBuffer::findParagraphAtY() using Fenwick tree
- [x] 4.18 Integration with TextBuffer's HeightTree

### Invalidation
- [x] 4.19 Implement `invalidate(paraIndex)`
- [x] 4.20 Implement `invalidateRange(startPara, endPara)`
- [x] 4.21 Implement `invalidateAll()`
- [x] 4.22 Handle buffer modifications via signals

### Unit Tests
- [x] 4.29 Test lazy layout creation
- [x] 4.30 Test height estimation accuracy
- [x] 4.31 Test invalidation behavior
- [x] 4.32 Test format application

NOTE: Background layout (4.23-4.28) deferred to Phase 8 integration

---

## Phase 5: ViewportManager ✅ COMPLETE

### Core Structure
- [x] 5.1 Create `ViewportManager` class with Q_OBJECT
- [x] 5.2 Store viewport size and scroll position
- [x] 5.3 Define BUFFER_SIZE constant (50 paragraphs)

### Visible Range
- [x] 5.4 Implement `firstVisibleParagraph()` using TextBuffer
- [x] 5.5 Implement `lastVisibleParagraph()` using TextBuffer
- [x] 5.6 Implement `setScrollPosition(y)` with range clamping

### Buffer Zone
- [x] 5.7 Implement `bufferStartParagraph()` - firstVisible - BUFFER_SIZE
- [x] 5.8 Implement `bufferEndParagraph()` - lastVisible + BUFFER_SIZE
- [x] 5.9 Signals for layout requests on buffer zone

### Scrollbar
- [x] 5.10 Implement `scrollbarPosition()` - 0.0 to 1.0
- [x] 5.11 Implement `scrollbarThumbSize()` - based on viewport/total
- [x] 5.12 Implement scroll position conversions
- [x] 5.13 Handle content < viewport (no scroll needed)

### Signals
- [x] 5.14 Emit `scrollPositionChanged()` on scroll
- [x] 5.15 Emit `viewportSizeChanged()` on resize
- [x] 5.16 Emit `visibleRangeChanged()` when visible paragraphs change

### Unit Tests
- [x] 5.17 Test visible range calculation
- [x] 5.18 Test buffer zone calculation
- [x] 5.19 Test scrollbar position accuracy
- [x] 5.20 Test edge cases (empty buffer, content < viewport)

---

## Phase 6: RenderEngine ✅ COMPLETE

### Core Structure
- [x] 6.1 Create `RenderEngine` class
- [x] 6.2 Store references to ViewportManager, LayoutEngine
- [x] 6.3 Implement dirty region tracking with QRegion

### Dirty Tracking
- [x] 6.4 Implement `markDirty(QRect region)`
- [x] 6.5 Implement `markParagraphDirty(paraIndex)`
- [x] 6.6 Coalesce adjacent dirty regions (via QRegion::united)
- [x] 6.7 Clear dirty regions after paint

### Paint Implementation
- [x] 6.8 Implement `paint(QPainter*, QRect clipRect)`
- [x] 6.9 Get visible paragraph range from ViewportManager
- [x] 6.10 Skip paragraphs outside viewport
- [x] 6.11 Skip clean paragraphs (retained mode via paintDirty)
- [x] 6.12 Paint only dirty paragraphs in viewport

### Paragraph Rendering
- [x] 6.13 Implement `paintParagraph(painter, paraIndex, y)`
- [x] 6.14 Get layout from LazyLayoutManager (QTextLayout)
- [x] 6.15 Draw QTextLayout at correct Y position
- [x] 6.16 Apply clipping for partial visibility

### Selection & Cursor
- [x] 6.17 Implement `setSelection(SelectionRange)` - uses editor_types.h
- [x] 6.18 Implement selection painting via QTextLayout::FormatRange
- [x] 6.19 Implement `paintCursor(painter)` - blinking line
- [x] 6.20 Cursor blink with QTimer (dirty region only)

### Unit Tests
- [x] 6.21 Test dirty region tracking (markDirty, markAllDirty, clear)
- [x] 6.22 Test viewport-only rendering (QImage + QPainter)
- [x] 6.23 Test selection/cursor/geometry/signals (74 assertions, 11 test cases)

---

## Phase 7: KML Conversion Layer ✅ COMPLETE

### Parser (KML → Internal)
- [x] 7.1 Create `KmlConverter` class
- [x] 7.2 Implement `parseKml(QString kml)` → TextBuffer + FormatLayer
- [x] 7.3 Parse `<p>` paragraph elements into TextBuffer
- [x] 7.4 Parse `<t>` text run elements
- [x] 7.5 Convert `<b>`, `<i>`, `<u>`, `<s>`, `<sub>`, `<sup>` to FormatRanges
- [x] 7.6 Parse nested formatting correctly (recursive parsing)
- [x] 7.7 Handle KML/doc/document root elements

### Metadata Parsing
- [x] 7.8 Create MetadataLayer class for comments/TODOs
- [x] 7.9 Parse `<comments>` section with `<comment>` elements
- [x] 7.10 Implement position adjustment on text changes

### Serializer (Internal → KML)
- [x] 7.11 Implement `toKml()` → QString
- [x] 7.12 Serialize paragraphs to `<p>` elements
- [x] 7.13 Convert FormatRanges to nested KML elements via FormatEvent system
- [x] 7.14 Handle overlapping formats with proper tag ordering
- [x] 7.15 Serialize comments from MetadataLayer
- [x] 7.16 Implement `paragraphToKml()` for single paragraph

### Round-Trip Validation
- [x] 7.17 Test: load → save produces equivalent text/formatting
- [x] 7.18 Test with complex nested formatting (bold+italic)
- [x] 7.19 Test edge cases (empty, whitespace, unicode, special chars)
- [x] 7.20 Unit tests: 118 assertions in 9 test cases pass

---

## Phase 8: BookEditor Integration ✅ COMPLETE

### Replace Internals
- [x] 8.1 Add TextBuffer member (alongside KmlDocument for gradual migration)
- [x] 8.2 Add FormatLayer member
- [x] 8.3 Add LazyLayoutManager member
- [x] 8.4 Add ViewportManager member
- [x] 8.5 Add RenderEngine member

### Event Handling
- [x] 8.6 Update text input methods (insertText, deleteBackward, deleteForward, insertNewline)
- [x] 8.7 Update `mousePressEvent` for cursor positioning (uses positionFromPointNewArch)
- [x] 8.8 Update `mouseMoveEvent` for selection (with RenderEngine.setSelection)
- [x] 8.9 Update `wheelEvent` to use ViewportManager (feature flag branch)

### Paint Integration
- [x] 8.10 Update `paintEvent` to use RenderEngine::paint (feature flag branch)
- [x] 8.11 Signal connections for repaint (viewportChanged -> update)
- [x] 8.12 Resize event notifies ViewportManager of size changes

### Cursor & Selection
- [x] 8.13 Update cursor movement (arrow keys, home, end, etc.)
- [x] 8.14 Update selection handling (shift+arrows, Ctrl+A, etc.)
- [x] 8.15 Implement `selectAll()` - O(1) with TextBuffer

### Clipboard
- [x] 8.16 Update `copy()` to use TextBuffer::plainText(range)
- [x] 8.17 Update `cut()` with TextBuffer::remove
- [x] 8.18 Update `paste()` with TextBuffer::insert
- [x] 8.19 Single-pass clipboard data generation

---

## Phase 9: Feature Parity

### Undo/Redo
- [ ] 9.1 Create undo commands for TextBuffer operations
- [ ] 9.2 Create undo commands for format changes
- [ ] 9.3 Test undo/redo with 100+ operations

### Find/Replace
- [ ] 9.4 Update Find to use TextBuffer::plainText()
- [ ] 9.5 Update Replace to use TextBuffer::remove/insert
- [ ] 9.6 Highlight found text in RenderEngine

### Comments System
- [ ] 9.7 Integrate comments with MetadataLayer
- [ ] 9.8 Render comment markers in RenderEngine
- [ ] 9.9 Handle comment position adjustment on text changes

### TODO Tags
- [ ] 9.10 Integrate TODOs with MetadataLayer
- [ ] 9.11 Render TODO markers in RenderEngine
- [ ] 9.12 Handle TODO position adjustment on text changes

### Snapshots
- [ ] 9.13 Update snapshot creation with new format
- [ ] 9.14 Update snapshot restore with new format
- [ ] 9.15 Test snapshot round-trip

### View Modes
- [ ] 9.16 Update page mode rendering
- [ ] 9.17 Update continuous scroll mode
- [ ] 9.18 Update distraction-free mode
- [ ] 9.19 Update focus mode (dim non-current paragraph)

---

## Phase 10: Performance Validation

### Benchmarks (150k words - matching Word)
- [ ] 10.1 Create 150k word test document
- [ ] 10.2 Benchmark scrolling FPS (target: 60fps)
- [ ] 10.3 Benchmark Select All time (target: < 50ms)
- [ ] 10.4 Benchmark Copy time (target: < 100ms)
- [ ] 10.5 Benchmark typing latency (target: < 16ms)
- [ ] 10.6 Benchmark load time (target: < 2 seconds)

### Stress Testing
- [ ] 10.7 Test with 200k+ word document
- [ ] 10.8 Test rapid scrolling (scroll through entire doc in 5 seconds)
- [ ] 10.9 Test rapid typing (100 chars/second)
- [ ] 10.10 Test large selection operations

### Memory Profiling
- [ ] 10.11 Memory usage comparison (old vs new)
- [ ] 10.12 Check for memory leaks with AddressSanitizer
- [ ] 10.13 Verify no heap fragmentation
- [ ] 10.14 Profile memory during long editing session

### Thread Safety
- [ ] 10.15 Test background layout thread safety
- [ ] 10.16 Stress test concurrent layout/render
- [ ] 10.17 Verify no race conditions with ThreadSanitizer

---

## Phase 11: Cleanup & Migration

### Deprecation
- [ ] 11.1 Mark KmlDocument as deprecated
- [ ] 11.2 Mark KmlParagraph as deprecated
- [ ] 11.3 Mark KmlElement hierarchy as deprecated
- [ ] 11.4 Remove feature flags (use new implementation only)
- [ ] 11.5 Remove dead code

### Final Validation
- [ ] 11.6 All existing unit tests pass
- [ ] 11.7 All integration tests pass
- [ ] 11.8 Manual testing of all features
- [ ] 11.9 Performance regression tests added to CI

---

## Documentation

- [ ] D.1 Update CHANGELOG.md with performance rewrite entry
- [ ] D.2 Document new architecture in code comments
- [ ] D.3 Create architecture diagram for developers
- [ ] D.4 Update API documentation for new classes
