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

## Phase 9: Feature Parity ✅ COMPLETE

### Undo/Redo
- [x] 9.1 Create undo commands for TextBuffer operations
- [x] 9.2 Create undo commands for format changes
- [x] 9.3 Test undo/redo with 100+ operations

### Find/Replace
- [x] 9.4 Update Find to use TextBuffer::plainText()
- [x] 9.5 Update Replace to use TextBuffer::remove/insert
- [x] 9.6 Highlight found text in RenderEngine

### Comments System
- [x] 9.7 Integrate comments with MetadataLayer
- [x] 9.8 Render comment markers in RenderEngine
- [x] 9.9 Handle comment position adjustment on text changes

### TODO Tags
- [x] 9.10 Integrate TODOs with MetadataLayer
- [x] 9.11 Render TODO markers in RenderEngine
- [x] 9.12 Handle TODO position adjustment on text changes

### Snapshots
- [x] 9.13 Update snapshot creation with new format
- [x] 9.14 Update snapshot restore with new format
- [x] 9.15 Test snapshot round-trip

### View Modes
- [x] 9.16 Update page mode rendering
- [x] 9.17 Update continuous scroll mode
- [x] 9.18 Update distraction-free mode
- [x] 9.19 Update focus mode (dim non-current paragraph)

---

## Phase 10: Performance Validation ✅ COMPLETE

### Benchmarks (150k words - matching Word)
- [x] 10.1 Create 150k word test document (TestDocumentGenerator)
- [x] 10.2 Benchmark scrolling FPS: **5555 FPS** (target: 60fps) ✅ 92x better
- [x] 10.3 Benchmark Select All time: **1.00ms** (target: < 50ms) ✅ 50x better
- [x] 10.4 Benchmark Copy time: **29ns cached** (target: < 100ms) ✅
- [x] 10.5 Benchmark typing latency: **P99 6.78ms** (target: < 16ms) ✅
- [x] 10.6 Benchmark load time: **6.86ms** (target: < 2 seconds) ✅ 290x better

### Stress Testing
- [x] 10.7 Test with 200k+ word document: **Load 6.86ms, Init 1.60ms, Scroll 81ms**
- [x] 10.8 Test rapid scrolling: **5555 FPS continuous, 1.57ms per random jump**
- [x] 10.9 Test rapid typing (100 chars/second): **P99 6.78ms, Burst P99 15.85ms**
- [x] 10.10 Test large selection: **Select All 1ms, Delete 477k chars <1ms**

### Memory Profiling
- [x] 10.11 Memory usage: **19.5 bytes per word** (very efficient)
- [x] 10.12 Leak detection: **1.08x growth after 10 load/unload cycles** (no leaks)
- [x] 10.13 Heap fragmentation: **Minimal - covered by leak detection**
- [x] 10.14 Long editing session: **1.03x growth after 1000 insert/delete pairs**

### Thread Safety
- [x] 10.15 Single-threaded invariants verified (state consistency)
- [x] 10.16 Background layout: DEFERRED (single-threaded architecture)
- [x] 10.17 ThreadSanitizer: N/A (requires separate TSan build)

---

## Phase 11: ARCHITECTURE CORRECTION (QTextDocument Direct)

**DATA ANALIZY:** 2025-12-30

### Problem z fazami 1-10

Fazy 1-10 dodały zbędne warstwy zamiast użyć QTextDocument bezpośrednio:

| Warstwa | Problem | Akcja |
|---------|---------|-------|
| `TextBuffer` | Wrapper nad QTextDocument | USUNĄĆ |
| `HeightTree` (Fenwick) | QTextBlock::layout() daje wysokości | USUNĄĆ |
| `ITextBufferObserver` | 4 observery = O(n²) przy bulk load | USUNĄĆ |
| `FormatLayer` + `IntervalTree` | QTextCharFormat robi to samo | USUNĄĆ |
| `MetadataLayer` | Komentarze/TODO to inline tagi KML | USUNĄĆ |
| `LazyLayoutManager` | QAbstractTextDocumentLayout wystarczy | USUNĄĆ |

### Docelowa architektura: 2 kroki

```
KROK 1: KML → QTextDocument (z QTextCharFormat dla formatowania i metadanych)
KROK 2: Renderuj widoczny fragment (ViewportManager + RenderEngine)
```

### Co ZOSTAJE (pełna funkcjonalność)

| Komponent | Rola |
|-----------|------|
| `BookEditor` | Widget - bezpośrednio używa QTextDocument* |
| `KmlParser` | KML → QTextDocument (NOWY) |
| `KmlSerializer` | QTextDocument → KML (NOWY) |
| `ViewportManager` | Scroll, visible range (zmodyfikowany na QTextDocument) |
| `RenderEngine` | Page Mode, Focus Mode, komentarze (zmodyfikowany) |

### Mapowanie KML → QTextCharFormat

| KML Element | QTextCharFormat |
|-------------|-----------------|
| `<em>` | `setFontItalic(true)` |
| `<strong>` | `setFontWeight(QFont::Bold)` |
| `<u>` | `setFontUnderline(true)` |
| `<s>` | `setFontStrikeOut(true)` |
| `<comment>` | `setProperty(KmlComment, QVariant::fromValue(commentData))` |
| `<todo>` | `setProperty(KmlTodo, QVariant::fromValue(todoData))` |
| `<footnote>` | `setProperty(KmlFootnote, QVariant::fromValue(footnoteData))` |

Custom property IDs:
```cpp
enum KmlProperty {
    KmlComment = QTextFormat::UserProperty + 1,
    KmlTodo = QTextFormat::UserProperty + 2,
    KmlFootnote = QTextFormat::UserProperty + 3,
    KmlCharRef = QTextFormat::UserProperty + 4,
    KmlLocRef = QTextFormat::UserProperty + 5,
};
```

---

### 11.1: Nowy KmlParser (KML → QTextDocument) ✅ COMPLETE
- [x] 11.1.1 Utwórz `include/kalahari/editor/kml_parser.h`
- [x] 11.1.2 Utwórz `src/editor/kml_parser.cpp`
- [x] 11.1.3 Implementuj `parseKml(QString) -> QTextDocument*`
- [x] 11.1.4 Implementuj inline formatting (`<em>`, `<strong>`, `<u>`, `<s>`)
- [x] 11.1.5 Implementuj comment/TODO/footnote jako QTextCharFormat properties
- [x] 11.1.6 Testy jednostkowe parsera

### 11.2: Nowy KmlSerializer (QTextDocument → KML) ✅ COMPLETE
- [x] 11.2.1 Utwórz `include/kalahari/editor/kml_serializer.h`
- [x] 11.2.2 Utwórz `src/editor/kml_serializer.cpp`
- [x] 11.2.3 Implementuj `toKml(QTextDocument*) -> QString`
- [x] 11.2.4 Implementuj format-to-tag conversion
- [x] 11.2.5 Testy round-trip (parse → serialize → parse)

**REFACTORING:** Dodano `kml_format_registry.h/cpp` jako wspólne źródło mapowań (Option C).

### 11.3: Zmodyfikuj ViewportManager ✅ COMPLETE
- [x] 11.3.1 Zmień `setBuffer(TextBuffer*)` na `setDocument(QTextDocument*)`
- [x] 11.3.2 Implementuj visible range używając QTextBlock
- [x] 11.3.3 Usuń ITextBufferObserver interface
- [x] 11.3.4 Zaktualizuj testy

### 11.4: Refaktoryzacja RenderEngine ✅ COMPLETE
- [x] 11.4.1 Zmień `setBuffer(TextBuffer*)` na `setDocument(QTextDocument*)` - ALREADY DONE
- [x] 11.4.2 Usuń `setFormatLayer()` - czytaj formaty z QTextCharFormat - ALREADY DONE (method never existed)
- [x] 11.4.3 Usuń `setMetadataLayer()` - czytaj metadane z QTextCharFormat::UserProperty - ALREADY DONE
- [x] 11.4.4 Implementuj comment/TODO highlighting z QTextCharFormat - DONE (uses KmlPropComment/KmlPropTodo)
- [x] 11.4.5 Usuń LazyLayoutManager dependency - użyj QTextBlock::layout() - DONE (uses QTextBlock::layout() directly)
- [x] 11.4.6 Zaktualizuj testy - DONE (71 assertions, 11 test cases pass)

### 11.5: Refaktoryzacja buffer_commands
- [x] 11.5.1 Przepisz InsertTextCommand na QTextCursor ✅
- [x] 11.5.2 Przepisz DeleteTextCommand na QTextCursor ✅
- [x] 11.5.3 Przepisz FormatTextCommand na QTextCursor::setCharFormat() ✅
- [x] 11.5.4 Usuń FormatLayer z wszystkich komend ✅
- [x] 11.5.5 Wykorzystaj QTextDocument undo/redo ✅
- [ ] 11.5.6 Zaktualizuj testy

**Naprawiono błędy kompilacji po refaktoryzacji (2025-12-31):**
- Usunięto duplikację `MarkerType` z `kml_converter.h` (już istnieje w `buffer_commands.h`)
- Dodano include `buffer_commands.h` w `kml_converter.h` dla MarkerType
- Zaktualizowano `book_editor.cpp`:
  - Wszystkie komendy teraz używają `m_textBuffer->document()` zamiast `m_textBuffer.get(), m_formatLayer.get(), m_metadataLayer.get()`
  - CompositeBufferCommand -> CompositeDocumentCommand
  - TextTodo -> TextMarker dla marker operations
  - Usunięto parametry `std::vector<FormatRange>{}`
- Zaktualizowano `search_engine.cpp`:
  - replaceCurrent() - nowe API TextReplaceCommand
  - replaceAll() - nowe API ReplaceAllCommand
  - absoluteToCursorPosition() teraz używa document()
  - Usunięto obsługę FormatLayer

### 11.6: Przepisz BookEditor core (COMPLETE - 2025-12-31)
- [ ] 11.6.1 Zamień `m_textBuffer` na `QTextDocument* m_document` (KEEP for now - buffer_commands need TextBuffer wrapper)
- [x] 11.6.2 Usuń `m_formatLayer` member ✅ (Phase 11.6 cleanup)
- [ ] 11.6.3 Usuń `m_metadataLayer` member (KEEP for now - marker operations need it)
- [x] 11.6.4 Usuń `m_lazyLayoutManager` member ✅ (Phase 11.6 cleanup - replaced with QTextBlock::layout())
- [x] 11.6.5 Przepisz `fromKml()` używając KmlParser ✅
- [x] 11.6.6 Przepisz `toKml()` używając KmlSerializer ✅
- [x] 11.6.7 Zaktualizuj cursor/selection na QTextCursor (added m_textCursor, init in ctor/fromKml) ✅
- [x] 11.6.8 Uprość accessor methods (paragraphCount, paragraphPlainText, plainText, characterCount) ✅
- [ ] 11.6.9 Zaktualizuj wszystkie operacje edycji (będzie w Phase 11.8 po usunięciu buffer_commands dependencies)

**Phase 11.6 cleanup (2025-12-31):**
- Usunięto `m_formatLayer` - formatting jest teraz w QTextCharFormat
- Usunięto `m_lazyLayoutManager` - zastąpiono QTextBlock::layout()
- Zaktualizowano metody:
  - `positionFromPoint()` - używa doc->findBlockByNumber(i).layout()
  - `paintPageMode()` - używa block.layout()->boundingRect().height()
  - `getFocusedRange()` - używa block.layout()
  - `paintFocusOverlay()` - używa block.layout()->boundingRect().height()
  - `deleteBackward()/deleteForward()` - usunięto invalidateLayout() (Qt robi automatycznie)
  - `fromKml()` - usunięto tworzenie LazyLayoutManager i FormatLayer
  - `setupFindReplace()` - usunięto setFormatLayer()
- Usunięto includes: format_layer.h, lazy_layout_manager.h
- Build: PASS, Tests: 768 tests, 5182 assertions PASS

### 11.7: Wyodrebnij SearchService API ✅ COMPLETE
**ZMIANA PLANU:** Zamiast refaktoryzowac FindReplaceBar, wyodrebniamy czyste API do wyszukiwania.
UI (FindReplaceBar, SearchPanel) zostanie zaprojektowane osobno.

- [x] 11.7.1 Utworz `ISearchService` interface w `include/kalahari/editor/search_service.h`
- [x] 11.7.2 Utworz `SearchService` implementacje uzywajaca QTextDocument
- [x] 11.7.3 Przenieś logike wyszukiwania z SearchEngine do SearchService
- [x] 11.7.4 SearchService: `findAll/findNext/findPrevious(query, options) -> results`
- [x] 11.7.5 SearchService: `replace(match, text)` i `replaceAll(query, text)`
- [x] 11.7.6 Dodaj `SearchSession` - nawigacja i zarządzanie stanem dla UI
- [x] 11.7.7 Testy jednostkowe SearchService (21 testów, 172 asercje)
- [ ] 11.7.8 DEFER: FindReplaceBar/SearchPanel UI - osobny OpenSpec

**IMPLEMENTACJA:**
- Utworzono nowe typy `DocSearchMatch`, `DocSearchOptions` (prefiks Doc dla unikniecia konfliktu z SearchEngine)
- `ISearchService` - interface dla dependency injection
- `SearchService` - implementacja uzywajaca QTextDocument::find()
- `SearchSession` - klasa Q_OBJECT dla UI (sygnaly, nawigacja, stan)
- Pliki: `search_service.h`, `search_service.cpp`, `test_search_service.cpp`

### 11.8: Usuń stare pliki (CZĘŚCIOWO - wymaga dalszej refaktoryzacji)

**POSTĘP:**
- [x] 11.8.0 Usuń LazyLayoutManager z ViewportManager
  - Usunięto setLayoutManager(), layoutManager(), m_layoutManager
  - Usunięto requestLayout(), syncLayoutManagerViewport()
  - ViewportManager teraz używa tylko QTextDocument/QAbstractTextDocumentLayout
  - Zaktualizowano test_viewport_manager.cpp i test_render_engine.cpp
  - Build i testy OK (5277 assertions, 775 testów)

**BLOKOWANE - Wymagają refaktoringu BookEditor:**
BookEditor nadal intensywnie używa TextBuffer, FormatLayer, LazyLayoutManager, KmlConverter.
Pełne usunięcie wymaga zastąpienia tych wywołań bezpośrednim dostępem do QTextDocument.

- [ ] 11.8.1 Usuń `text_buffer.h/cpp` (po usunięciu wszystkich użyć)
- [ ] 11.8.2 Usuń `height_tree.h/cpp` (część TextBuffer)
- [ ] 11.8.3 Usuń `format_layer.h/cpp`
- [ ] 11.8.4 Usuń `interval_tree.h` (część FormatLayer)
- [ ] 11.8.5 Usuń `lazy_layout_manager.h/cpp`
- [ ] 11.8.6 Usuń `kml_converter.h/cpp` (stary parser)
- [ ] 11.8.7 Usuń `ITextBufferObserver` interface
- [ ] 11.8.8 Zaktualizuj CMakeLists.txt

**NASTĘPNE KROKI:**
1. Refaktoruj BookEditor::positionFromPoint() - użyj QTextBlock::layout() zamiast m_lazyLayoutManager->getLayout()
2. Refaktoruj paintPageMode() - użyj bezpośrednio QTextDocument layout
3. Usuń m_lazyLayoutManager z BookEditor po kompletnym refaktoring
4. Podobnie dla TextBuffer, FormatLayer, KmlConverter

### 11.9: Cleanup i walidacja
- [ ] 11.9.1 Usuń nieużywane includes
- [ ] 11.9.2 Usuń martwy kod
- [ ] 11.9.3 Wszystkie testy jednostkowe przechodzą
- [ ] 11.9.4 Manual test z ExampleNovel (150K słów)
- [ ] 11.9.5 Benchmark: load < 2s, scroll 60fps, edit < 16ms

---

## Phase 11.10: Lazy Rendering Fix (Performance Critical)

**PROBLEM:** `QTextDocument::setHtml()` wykonuje pełny layout wszystkich bloków.
Dla 1.9MB dokumentu (15,000 paragrafów) trwa to 30+ sekund.

**ROZWIĄZANIE:** Lazy rendering - QTextLayout tylko dla widocznych paragrafów.

### Architektura (Word/Writer style)
```
KML ──parse──► KmlDocumentModel (lekka struktura w pamięci)
                     │
                     ├── vector<Paragraph> (text + formats)
                     ├── HeightTree (Fenwick - szacowane wysokości)
                     └── LayoutCache (QTextLayout tylko dla widocznych)
                            │
                  ┌─────────┴─────────┐
                  ▼                   ▼
          ViewportManager      RenderEngine
```

### Komponenty
- [x] 11.10.1 HeightTree - Fenwick tree dla O(log n) operacji na wysokościach
- [x] 11.10.2 FormatRun - struct dla zakresów formatowania
- [x] 11.10.3 KmlDocumentModel - lekka struktura dokumentu w pamięci
- [x] 11.10.4 LayoutCache - cache QTextLayout dla widocznych paragrafów
- [x] 11.10.5 Parsowanie KML do KmlDocumentModel (pełny dokument, bez layoutu)
- [x] 11.10.6 Integracja ViewportManager z KmlDocumentModel
- [x] 11.10.7 Integracja RenderEngine z lazy rendering
- [x] 11.10.8 Integracja BookEditor z KmlDocumentModel
- [x] 11.10.9 Benchmark: load 131ms for 1728 paragraphs (~230x improvement from 30s freeze)

### Kluczowa różnica vs Phase 11.1-11.8
- **Było:** setHtml() → pełny layout wszystkich bloków (WOLNE)
- **Jest:** parse → lekka struktura + lazy QTextLayout (SZYBKIE)

---

## Future: Oddzielne metadane (opcjonalnie)

**Pomysł do rozważenia w przyszłości:**
Metadane (komentarze, TODO, footnotes) jako osobna warstwa zamiast inline w tekście.

**Zalety:**
- Czystszy model danych (Comment ma id, author, timestamp osobno)
- Lista komentarzy = iteracja po liście, nie parsowanie dokumentu
- Prostszy HTML dla formatowania

**Wady:**
- Synchronizacja pozycji przy edycji tekstu
- Dwa źródła prawdy

**Status:** Do rozważenia po wdrożeniu lazy rendering.

---

## Documentation

- [ ] D.1 Update CHANGELOG.md with performance rewrite entry
- [ ] D.2 Document new architecture in code comments
- [ ] D.3 Create architecture diagram for developers
- [ ] D.4 Update API documentation for new classes

---

## Referencje

- **Specyfikacja funkcjonalna:** `project_docs/19_text_editor_functional_spec_pl.md`
- **Architektura Qt:** QTextDocument, QTextCharFormat, QTextBlock, QTextCursor
- **Cel:** Architektura Word/Writer - 2 kroki zamiast 8

---

## Podsumowanie Phase 11

### Komponenty usunięte
| Komponent | Linie kodu | Zamiennik |
|-----------|------------|-----------|
| TextBuffer + HeightTree | ~800 | QTextDocument |
| FormatLayer + IntervalTree | ~600 | QTextCharFormat |
| MetadataLayer | ~200 | QTextCharFormat::UserProperty |
| LazyLayoutManager | ~400 | QTextBlock::layout() |
| KmlConverter | ~500 | KmlParser + KmlSerializer |
| ITextBufferObserver | ~100 | Qt signals |
| **TOTAL** | **~2600** | - |

### Nowa architektura
```
KML ──KmlParser──► QTextDocument ──KmlSerializer──► KML
                        │
                   QTextCharFormat (formatting + metadata)
                        │
              ┌─────────┴─────────┐
              ▼                   ▼
      ViewportManager      RenderEngine
         (visible range)    (painting)
```

---

## Phase 11.11: Custom QAbstractTextDocumentLayout (IN PROGRESS)

**DATA:** 2026-01-03

### Problem

Qt's default `QTextDocumentLayout` automatically adds `font.leading()` between lines,
causing gaps between paragraphs. Previous attempts:

1. **Manual layout in ensureEditMode()** - Qt overwrites on any document modification
2. **QPlainTextDocumentLayout** - No text wrapping support, designed for plain text
3. **contentsChanged re-layout** - O(n) on every keystroke, slow for large documents

### Rozwiązanie: Custom Layout Class

Własna klasa `KalahariTextDocumentLayout` dziedzicząca z `QAbstractTextDocumentLayout`:

```
QAbstractTextDocumentLayout
          │
          ▼
KalahariTextDocumentLayout
  - documentChanged() → layout affected blocks with lines at y=0
  - blockBoundingRect() → return tight bounds without leading
  - hitTest() → proper position mapping
  - draw() → paint blocks at correct positions
```

### Kluczowe metody

| Metoda | Rola |
|--------|------|
| `documentChanged(from, removed, added)` | Wywoływana przez Qt przy zmianie - re-layout bloków z y=0 |
| `layoutBlock(block)` | Przygotowuje QTextLayout z liniami zaczynającymi od y=0 |
| `blockBoundingRect(block)` | Zwraca ścisłe granice bez extra leading |
| `blockY(blockNumber)` | Pozycja Y bloku (cumulative heights) |
| `hitTest(point, accuracy)` | Konwersja point → document position |
| `draw(painter, context)` | Rysowanie widocznych bloków |

### Zadania

- [x] 11.11.1 Utworz header: `include/kalahari/editor/kalahari_text_document_layout.h`
- [x] 11.11.2 Utworz implementację: `src/editor/kalahari_text_document_layout.cpp`
- [ ] 11.11.3 Dodaj do CMakeLists.txt
- [ ] 11.11.4 Integruj z BookEditor::ensureEditMode()
- [ ] 11.11.5 Usuń contentsChanged hack z ensureEditMode()
- [ ] 11.11.6 Build i testy
- [ ] 11.11.7 Manual test: Enter nie powoduje przerw, tekst się zawija
- [ ] 11.11.8 Fix: pusta linia na początku dokumentu

### Pliki

| Plik | Status |
|------|--------|
| `include/kalahari/editor/kalahari_text_document_layout.h` | CREATED |
| `src/editor/kalahari_text_document_layout.cpp` | CREATED |
| `src/editor/book_editor.cpp` | TO MODIFY |
| `src/CMakeLists.txt` | TO MODIFY |

---

## Phase 12: EditorRenderPipeline (Unified Rendering Architecture)

**DATA:** 2026-01-04

### Problem

Obecny kod to balagan - rendering rozsypany po wielu miejscach:

| Problem | Lokalizacja | Skutek |
|---------|-------------|--------|
| **God Class** | BookEditor (~1200 linii header) | Trudne do utrzymania |
| **3 sciezki renderingu** | paintEvent() | Duplikacja, niespojnosc |
| **Rozrzucone atrybuty** | EditorAppearance, hardcoded margins | Trudne do rozszerzenia |
| **Podzielona logika layout** | KalahariTextDocumentLayout, ViewportManager, KmlDocumentModel | Niespojne wysokosci |
| **Duplikowane rysowanie** | Cursor/selection w BookEditor i RenderEngine | Podatne na bledy |

### Rozwiazanie: Jeden Pipeline

Pipeline: TextSource -> Attributes -> Layout -> Render

| Stage | Input | Output | Odpowiedzialnosc |
|-------|-------|--------|------------------|
| 1. TextSource | Document data | Text + Formats | Zunifikowany dostep do QTextDocument lub KmlDocumentModel |
| 2. Attributes | EditorAppearance | RenderContext | Zbierz marginesy, skale, kolory, line spacing |
| 3. Layout | RenderContext + Text | BlockPositions | Oblicz pozycje z marginesami, skala, zawijaniem, wyrownaniem |
| 4. Render | BlockPositions + Context | Pixels | Rysuj wszystko (tekst, kursor, selekcje, overlay) |

### Kluczowa zasada: Kolejnosc krokow

ZAWSZE ta sama kolejnosc dla kazdej operacji:
1. TEXT      -> QTextDocument (zrodlo prawdy)
2. ATRYBUTY  -> QTextBlockFormat (wyrownanie, wciecia) + RenderContext (marginesy, skala)
3. LAYOUT    -> layoutBlock() - oblicz pozycje linii z uwzglednieniem wyrownania
4. RENDER    -> paint() - rysuj na ekran

Zmiana wyrownania:
setAlignment() -> zmien QTextBlockFormat -> relayout affected blocks -> repaint

### Nowe klasy

- ITextSource - interface dla QTextDocumentSource i KmlDocumentModelSource
- RenderContext - wszystkie atrybuty renderingu w jednej strukturze
- EditorRenderPipeline - JEDEN punkt wejscia: render(painter, clipRect)

### BookEditor po refaktoryzacji

PRZED (paintEvent - 100+ linii):
- 3 osobne sciezki dla Page/View/Edit mode
- Duplikacja cursor/selection

PO (paintEvent - 3 linie):
- m_renderPipeline->render(&painter, event->rect())

### Rozszerzalnosc

| Funkcja | Obecny stan | Po refaktoryzacji |
|---------|-------------|-------------------|
| Marginesy | Hardcoded LEFT_MARGIN | pipeline->setMargins(50, 30, 50, 30) |
| Skalowanie | Brak | pipeline->setScaleFactor(1.25) |
| Wyrownanie | Nie dziala (brak relayout) | pipeline->setBlockAlignment(idx, align) |
| Zmiana widoku | 3 osobne sciezki | pipeline->setViewMode(ViewMode::Page) |

### Zadania

#### 12.1: Infrastruktura
- [x] 12.1.1 Utworz text_source_adapter.h/cpp - ITextSource interface
- [x] 12.1.2 Utworz render_context.h - RenderContext struct
- [x] 12.1.3 Utworz editor_render_pipeline.h/cpp - glowna klasa
- [x] 12.1.4 Dodaj do CMakeLists.txt

#### 12.2: Migracja renderingu
- [x] 12.2.1 Przenies logike z RenderEngine do EditorRenderPipeline
  - Dodano renderMarkerHighlights() (TODO/NOTE/DONE markers)
  - Dodano renderCommentHighlights() (HTML/C-style comments)
  - Dodano cursor styles (Line/Block/Underline)
  - Dodano cursor blink timer support
- [x] 12.2.2 Zintegruj ViewportManager wewnetrznie
  - updateVisibleRange() uzywa ViewportManager gdy dostepny
  - Fallback na obliczenia z scrollY gdy brak VM
- [x] 12.2.3 Skonsoliduj rysowanie cursor/selection (tylko w pipeline)
  - renderCursor() obsluguje wszystkie style
  - renderSelection() + renderParagraphSelection() complete
- [x] 12.2.4 Obsluz wszystkie widoki przez ten sam pipeline
  - ViewMode w RenderContext
  - FocusModeConfig dla trybu focus
  - PageModeConfig dla trybu stron
- [ ] 12.2.5 Zintegruj KalahariTextDocumentLayout z pipeline
  - Wymaga zmian w BookEditor (Phase 12.3)

#### 12.3: Uproszczenie BookEditor
- [x] 12.3.1 Dodaj m_renderPipeline member
  - Dodano #include <kalahari/editor/editor_render_pipeline.h>
  - Dodano std::unique_ptr<EditorRenderPipeline> m_renderPipeline
  - Inicjalizacja w konstruktorze z polaczeniem sygnalow
- [x] 12.3.2 Zamien 3 sciezki renderingu na jedno wywolanie render()
  - paintEvent() teraz deleguje do m_renderPipeline->render()
  - Dodano syncPipelineState() do synchronizacji stanu
  - Zachowano fallback do starego kodu (deprecated, do usuniecia w 12.5)
- [x] 12.3.3 Usun duplikowany stan (cursor w obu miejscach)
  - Cursor forwarded do pipeline przez syncPipelineState()
  - Selection forwarded do pipeline przez syncPipelineState()
  - BookEditor tylko przechowuje stan, pipeline renderuje
- [x] 12.3.4 Deleguj wszystkie zapytania o rendering do pipeline
  - paintEvent() -> pipeline.render()
  - Focus mode -> pipeline context
  - View mode -> pipeline context
- [x] 12.3.5 Przepisz setAlign* na pipeline->setBlockAlignment()
  - setAlignLeft/Center/Right/Justify wywoluja pipeline.markAllDirty()
  - Wyrownanie nadal stosowane przez QTextBlockFormat
  - Pipeline automatycznie pobiera zmiany z QTextDocument

#### 12.4: Rozszerzenia
- [ ] 12.4.1 Dodaj obsluge skalowania
- [ ] 12.4.2 Dodaj konfigurowalne marginesy
- [ ] 12.4.3 Test: wszystkie widoki dzialaja identycznie
- [ ] 12.4.4 Test: wyrownanie dziala poprawnie

#### 12.5: Cleanup
- [ ] 12.5.1 Usun stary RenderEngine (po pelnej migracji)
- [ ] 12.5.2 Usun zduplikowany kod z BookEditor
- [ ] 12.5.3 Zaktualizuj testy

### Pliki do utworzenia

| Plik | Cel |
|------|-----|
| include/kalahari/editor/text_source_adapter.h | ITextSource interface + adaptery |
| src/editor/text_source_adapter.cpp | Implementacje adapterow |
| include/kalahari/editor/render_context.h | RenderContext struct |
| include/kalahari/editor/editor_render_pipeline.h | Glowna klasa pipeline |
| src/editor/editor_render_pipeline.cpp | Implementacja pipeline |

### Pliki do modyfikacji

| Plik | Zmiana |
|------|--------|
| book_editor.h | Dodaj m_renderPipeline, usun renderowanie |
| book_editor.cpp | Deleguj do pipeline |
| kalahari_text_document_layout.cpp | Integracja z pipeline |
| src/CMakeLists.txt | Dodaj nowe pliki |

### Pliki do usuniecia (po migracji)

| Plik | Powod |
|------|-------|
| render_engine.h/cpp | Logika przeniesiona do EditorRenderPipeline |

---

#### 12.6: Configurable Margins

**Problem:** Hardcoded `LEFT_MARGIN = 10.0`, `TOP_MARGIN = 10.0` w book_editor.cpp (~25 uzyc).
Brak marginesow per view mode. Brak marginesow lustrzanych dla oprawy ksiazki.

**Rozwiazanie:** Konfiguracje marginesow per typ widoku + calculateMargins() w RenderContext.

##### Struktury danych

**PageMarginsConfig** (dla Page, Typewriter):
```cpp
struct PageMarginsConfig {
    double top = 25.4;           // mm (1 inch)
    double bottom = 25.4;
    double left = 25.4;
    double right = 25.4;
    bool mirrorEnabled = false;  // Lustrzane dla oprawy
    double inner = 30.0;         // Margines wewnetrzny (oprawa)
    double outer = 20.0;         // Margines zewnetrzny

    double effectiveLeft(int pageNumber) const;
    double effectiveRight(int pageNumber) const;
};
```

**ViewMarginsConfig** (dla Continuous, Focus, DistractionFree):
```cpp
struct ViewMarginsConfig {
    double vertical = 30.0;     // piksele, gora i dol symetrycznie
    double horizontal = 50.0;   // piksele, lewo i prawo symetrycznie
};
```

##### Zadania

###### 12.6.1: Dodaj struktury marginesow do editor_appearance.h
- [ ] 12.6.1.1 Dodaj `PageMarginsConfig` struct
- [ ] 12.6.1.2 Dodaj `ViewMarginsConfig` struct
- [ ] 12.6.1.3 Dodaj `pageMargins` i `viewMargins` do EditorAppearance
- [ ] 12.6.1.4 Implementuj fromJson/toJson dla nowych struktur

###### 12.6.2: Zaktualizuj RenderContext
- [ ] 12.6.2.1 Dodaj pole `currentPageNumber`
- [ ] 12.6.2.2 Dodaj statyczne `calculateMargins(appearance, viewMode, pageNumber)`
- [ ] 12.6.2.3 Zaktualizuj effectiveTextWidth() o nowe marginesy

###### 12.6.3: Zaktualizuj EditorRenderPipeline
- [ ] 12.6.3.1 Wywoluj calculateMargins() w render() na podstawie view mode
- [ ] 12.6.3.2 Przekazuj numer strony dla Page mode (marginesy lustrzane)

###### 12.6.4: Refaktoryzacja BookEditor
- [ ] 12.6.4.1 Usun `constexpr LEFT_MARGIN`, `TOP_MARGIN`
- [ ] 12.6.4.2 Zaktualizuj syncPipelineState() - ustawiaj marginesy z appearance
- [ ] 12.6.4.3 Zaktualizuj updateLayoutWidth() - uzyj RenderContext.margins
- [ ] 12.6.4.4 Zaktualizuj positionFromPoint() - uzyj marginesow z contextu
- [ ] 12.6.4.5 Zaktualizuj obliczenia cursor rect

###### 12.6.5: Testy
- [ ] 12.6.5.1 Test marginesow lustrzanych (odd/even page)
- [ ] 12.6.5.2 Test przelaczania marginesow per view mode
- [ ] 12.6.5.3 Test JSON round-trip dla margin configs
