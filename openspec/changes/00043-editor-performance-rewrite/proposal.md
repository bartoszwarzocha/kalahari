# 00043: Editor Performance Rewrite

## Status
IN_PROGRESS

## Summary

Complete rewrite of BookEditor using Word/Writer-like architecture. The current implementation uses a document model (KmlDocument with nested KmlParagraph/KmlElement trees) that causes severe performance degradation on large documents. On high-end hardware (Intel Ultra 9, 128GB RAM), the editor:
- Has unacceptable scrolling performance
- Takes seconds to Select All + Copy
- Shows noticeable lag during text editing

Microsoft Word handles the same content instantly, indicating a 100-1000x performance gap. This OpenSpec implements the same architectural patterns used by Word and LibreOffice Writer.

---

## ⚠️ ARCHITECTURE CORRECTION (2025-12-30)

**Absorbs:** OpenSpec #00045 (Architecture Cleanup)

### Problem z dotychczasową implementacją

Fazy 1-8 tego OpenSpec dodały NOWE warstwy złożoności zamiast uprościć architekturę:

| Dodana warstwa | Problem |
|----------------|---------|
| `TextBuffer` wrapper | Niepotrzebna warstwa nad QTextDocument |
| `HeightTree` (Fenwick) | QTextDocument ma wbudowane layouty |
| `FormatLayer` + `IntervalTree` | QTextCharFormat w Qt robi to samo |
| `ITextBufferObserver` (4 observery) | **ŹRÓDŁO O(n²)** - każdy observer = O(n) na insert |
| `LazyLayoutManager` | QAbstractTextDocumentLayout robi to lepiej |

**Wynik:** Program zawiesza się przy ładowaniu dokumentu 150K słów.

### Nowa architektura: 2 kroki zamiast 8

```
KROK 1: KML → QTextDocument (wbudowany piece-table Qt)
KROK 2: Renderuj widoczny fragment (lazy, na żądanie)
```

### Co USUNĄĆ (faza korekty):

| Komponent | Plik | Akcja |
|-----------|------|-------|
| `TextBuffer` wrapper | `text_buffer.h/cpp` | USUNĄĆ - używać QTextDocument bezpośrednio |
| `HeightTree` (Fenwick) | `text_buffer.h/cpp` | USUNĄĆ - QTextDocument ma wbudowane layouty |
| `ITextBufferObserver` | `text_buffer.h` | USUNĄĆ - źródło O(n²) przy ładowaniu |
| Stara architektura | `book_editor.cpp` | USUNĄĆ (`m_document`, `m_layoutManager`, `m_scrollManager`) |

### Co ZOSTAJE (pełna funkcjonalność):

| Komponent | Rola |
|-----------|------|
| `BookEditor` | Widget - bezpośrednio używa QTextDocument |
| `KmlParser/Serializer` | KML ↔ QTextDocument (z QTextCharFormat) |
| `MetadataLayer` | Komentarze, TODO, footnotes - POTRZEBNY (część KML) |
| `FormatLayer` | Do analizy - może być potrzebny dla formatowania KML |
| `ViewportManager` | Pełna funkcjonalność (scroll, visible range) |
| `RenderEngine` | Pełna funkcjonalność (Page Mode, Focus Mode, itp.) |
| `LazyLayoutManager` | Do analizy - może być potrzebny dla lazy layout |

### UWAGA: Wymagana dalsza analiza

Przed implementacją należy dokładnie przeanalizować:
1. Jak BookEditor będzie działał po zmianach
2. Jak KML będzie parsowany do QTextDocument z QTextCharFormat
3. Które komponenty (FormatLayer, LazyLayoutManager) są faktycznie potrzebne
4. Referencja: `project_docs/19_text_editor_functional_spec_pl.md`

### Dlaczego QTextDocument wystarczy:

| Funkcja | QTextDocument | Nasza implementacja |
|---------|---------------|---------------------|
| Piece-table | ✅ Wbudowany | ❌ Wrapper niepotrzebny |
| Formatowanie | ✅ QTextCharFormat | ❌ IntervalTree zbędny |
| Layout | ✅ QTextLayout | ❌ LazyLayoutManager zbędny |
| Wysokości | ✅ QTextBlock::layout() | ❌ HeightTree zbędny |
| 50K słów w akapicie | ✅ Obsługuje natywnie | ❌ Nasza arch. opiera się na paragraphIndex |

---

## Root Cause Analysis

### Current Architecture Problems

| Issue | Location | Complexity | Impact |
|-------|----------|------------|--------|
| No `plainText()` cache | `kml_paragraph.cpp:178` | O(M) per call | HIGH - called everywhere |
| Layout during paint | `book_editor.cpp:1764` | O(visible) per frame | MEDIUM |
| `selectedText()` loop | `book_editor.cpp:944-959` | O(N * M) | HIGH - Select All |
| Triple clipboard traversal | `clipboard_handler.cpp` | O(N) * 3 | HIGH - Copy |
| String concatenation | Multiple locations | O(N^2) potential | MEDIUM |
| Per-paragraph heap allocation | `kml_paragraph.h` | Fragmentation | LOW-MEDIUM |
| Full document layout | Always | O(N) | CRITICAL |
| No viewport-only rendering | Always | O(N) | CRITICAL |

### Current Document Model

```
KmlDocument
  └── vector<unique_ptr<KmlParagraph>>     ← separate heap alloc per paragraph
        └── vector<unique_ptr<KmlElement>>  ← more allocations
              └── recursive tree (Bold, Italic, TextRun...)
```

Every `plainText()` call traverses the entire element tree and concatenates strings.

### Why Word Processors Are Fast

| Aspect | BookEditor | Word/LibreOffice |
|--------|-----------|------------------|
| Text storage | Nested heap objects | Piece table / contiguous buffer |
| `plainText()` | O(N) - recalculated | O(1) - cached |
| Rendering | Full document | Viewport only (1-2 pages) |
| Layout | All paragraphs | Visible + buffer (~50 paragraphs) |
| Off-screen | Calculated | Estimated (average height) |
| Memory | Fragmented | Contiguous |
| Insert/Delete | O(N) copy | O(log N) piece table |
| Pagination | Synchronous | Background thread |

## Goal

Replace the current BookEditor with a Word/Writer-like architecture that:
1. Handles 150k+ words without noticeable lag
2. Provides instant scrolling, selection, and copy operations
3. Renders only the visible viewport
4. Uses lazy layout with estimation for off-screen content
5. Maintains KML format compatibility for document storage
6. Supports all existing features (comments, TODO tags, snapshots)

---

## Target Architecture (Word/Writer Model)

### How Word Renders 1000 Pages Instantly

Word does NOT render all pages. It uses a layered approach:

```
Document: 1000 pages, 150k words
├── Loaded to memory: entire text (piece table) - FAST
├── Layout calculated: visible + buffer (~10 pages) - ON DEMAND
├── Rendered: visible viewport only (1-2 pages) - MINIMAL
└── Rest: estimated heights (average line height × paragraph count)
```

### Virtual Scrolling with Estimation

```
Pages 1-5:      [Layout CALCULATED] ← user was here
Pages 6-10:     [Layout CALCULATED] ← buffer zone
Pages 11-990:   [ESTIMATED: avg_height × paragraph_count]
Pages 991-1000: [Not yet known]
```

The scrollbar position is based on **estimated** heights. When user scrolls to page 500:
1. Layout engine quickly calculates pages 495-510
2. Renderer draws only page 500
3. Rest remains estimation
4. Scrollbar adjusts slightly as real heights replace estimates

### Background Pagination

Word calculates pagination in a background thread:
- While you type on page 1, background thread calculates pages 2, 3, 4...
- Status bar: "Page 1 of 5" → "Page 1 of 47" → "Page 1 of 312" → "Page 1 of 1000"
- Full pagination completes asynchronously

### Piece Table - Key to Performance

```
Traditional approach (our KmlDocument):
┌─────────────────────────────────────────────┐
│ "Hello World" stored in QString             │
│ Insert "Beautiful " at pos 6                │
│ → Copy entire string, insert, O(N)          │
│ → "Hello Beautiful World"                   │
└─────────────────────────────────────────────┘

Piece Table approach (Word):
┌─────────────────────────────────────────────┐
│ Original buffer (readonly): "Hello World"  │
│ Add buffer: "Beautiful "                    │
│ Piece table:                                │
│   [original, 0-5] → "Hello "                │
│   [add, 0-10]     → "Beautiful "            │
│   [original, 6-11] → "World"                │
│ Insert = O(log N) - just add table entry    │
│ Text is NEVER copied                        │
└─────────────────────────────────────────────┘
```

---

## Technical Design

### New Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         BookEditor                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │ TextBuffer  │  │ FormatLayer │  │ MetadataLayer           │  │
│  │ (PieceTable)│  │ (Ranges)    │  │ (Comments, TODOs, etc.) │  │
│  └──────┬──────┘  └──────┬──────┘  └────────────┬────────────┘  │
│         │                │                      │                │
│         └────────────────┼──────────────────────┘                │
│                          ▼                                       │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    LayoutEngine                            │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌───────────────────┐  │  │
│  │  │ Visible     │  │ Buffer Zone │  │ Estimation Zone   │  │  │
│  │  │ (calculated)│  │ (background)│  │ (avg height only) │  │  │
│  │  └─────────────┘  └─────────────┘  └───────────────────┘  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                          │                                       │
│                          ▼                                       │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                   ViewportManager                          │  │
│  │  • Determines visible paragraph range                      │  │
│  │  • Requests layout for buffer zone                         │  │
│  │  • Calculates scrollbar from estimates + real heights      │  │
│  └───────────────────────────────────────────────────────────┘  │
│                          │                                       │
│                          ▼                                       │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    RenderEngine                            │  │
│  │  • Renders ONLY visible viewport                           │  │
│  │  • Dirty region tracking                                   │  │
│  │  • Retained mode (cache unchanged regions)                 │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### Component Details

#### 1. TextBuffer (Piece Table)

```cpp
class TextBuffer {
public:
    // O(log N) operations
    void insert(size_t position, const QString& text);
    void remove(size_t position, size_t length);

    // O(1) cached access
    QString plainText() const;                    // Full document
    QString plainText(size_t start, size_t end) const;  // Range

    // O(1) paragraph access
    size_t paragraphCount() const;
    QString paragraphText(size_t index) const;    // Cached per paragraph

    // Dirty tracking
    void markParagraphDirty(size_t index);
    bool isParagraphDirty(size_t index) const;

private:
    struct Piece {
        enum class Source { Original, Add };
        Source source;
        size_t start;
        size_t length;
    };

    QString m_originalBuffer;           // Readonly, loaded from file
    QString m_addBuffer;                // Append-only, new text goes here
    std::vector<Piece> m_pieces;        // Piece table

    // Caches
    mutable QString m_plainTextCache;
    mutable bool m_plainTextDirty = true;
    mutable std::vector<QString> m_paragraphCache;
    mutable std::vector<bool> m_paragraphDirty;
};
```

#### 2. FormatLayer (Separate from Text)

```cpp
struct FormatRange {
    size_t start;           // Character offset
    size_t end;             // Character offset
    TextFormat format;      // Bold, italic, font, color, etc.
};

class FormatLayer {
public:
    void addFormat(size_t start, size_t end, const TextFormat& format);
    void removeFormat(size_t start, size_t end, FormatType type);

    // Get all formats at position (for rendering)
    std::vector<FormatRange> getFormatsAt(size_t position) const;

    // Get formats for paragraph (for layout)
    std::vector<FormatRange> getFormatsForParagraph(size_t paraIndex) const;

    // Adjust ranges when text is inserted/deleted
    void onTextInserted(size_t position, size_t length);
    void onTextDeleted(size_t position, size_t length);

private:
    // Interval tree for O(log N) range queries
    IntervalTree<FormatRange> m_formats;
};
```

#### 3. LayoutEngine (Lazy + Estimation)

```cpp
class LayoutEngine {
public:
    // Get layout for paragraph (calculates if needed)
    ParagraphLayout* getLayout(size_t paraIndex);

    // Get height (real if calculated, estimated if not)
    qreal getParagraphHeight(size_t paraIndex) const;

    // Get total document height (mix of real + estimated)
    qreal getTotalHeight() const;

    // Find paragraph at Y coordinate
    size_t findParagraphAtY(qreal y) const;

    // Invalidation
    void invalidateParagraph(size_t paraIndex);
    void invalidateRange(size_t startPara, size_t endPara);

    // Background layout
    void requestLayoutForRange(size_t startPara, size_t endPara);
    void processBackgroundLayout();  // Called from timer/thread

private:
    enum class LayoutState { Estimated, Calculated };

    struct ParagraphInfo {
        LayoutState state = LayoutState::Estimated;
        qreal height = 0;                    // Real or estimated
        std::unique_ptr<QTextLayout> layout; // Only if Calculated
    };

    std::vector<ParagraphInfo> m_paragraphs;
    qreal m_averageLineHeight = 20.0;        // For estimation
    qreal m_averageLinesPerParagraph = 2.0;  // For estimation

    // Fenwick tree for O(log N) prefix sums
    FenwickTree m_heightPrefixSums;

    // Background layout queue
    std::queue<size_t> m_layoutQueue;
};
```

#### 4. ViewportManager

```cpp
class ViewportManager {
public:
    void setViewportSize(const QSize& size);
    void setScrollPosition(qreal y);

    // What to render
    size_t getFirstVisibleParagraph() const;
    size_t getLastVisibleParagraph() const;

    // What to pre-layout (buffer zone)
    size_t getBufferStart() const;  // firstVisible - BUFFER_SIZE
    size_t getBufferEnd() const;    // lastVisible + BUFFER_SIZE

    // Scrollbar
    qreal getScrollbarPosition() const;  // 0.0 - 1.0
    qreal getScrollbarThumbSize() const;
    void setScrollbarPosition(qreal pos);

signals:
    void viewportChanged();
    void requestLayout(size_t startPara, size_t endPara);

private:
    static constexpr size_t BUFFER_SIZE = 50;  // Paragraphs to pre-layout

    QSize m_viewportSize;
    qreal m_scrollY = 0;
    LayoutEngine* m_layoutEngine;
};
```

#### 5. RenderEngine (Viewport Only)

```cpp
class RenderEngine {
public:
    void paint(QPainter* painter, const QRect& clipRect);

    // Dirty tracking
    void markDirty(const QRect& region);
    void markParagraphDirty(size_t paraIndex);

    // Selection
    void setSelection(const TextRange& selection);

private:
    void paintParagraph(QPainter* painter, size_t paraIndex, qreal y);
    void paintSelection(QPainter* painter);
    void paintCursor(QPainter* painter);

    // Only paint paragraphs in viewport
    ViewportManager* m_viewportManager;
    LayoutEngine* m_layoutEngine;

    // Dirty regions (coalesced)
    QRegion m_dirtyRegion;
};
```

### Data Flow

```
User types 'A'
    │
    ▼
TextBuffer::insert(cursorPos, "A")      ← O(log N) piece table insert
    │
    ▼
FormatLayer::onTextInserted(...)        ← O(log N) range adjustment
    │
    ▼
LayoutEngine::invalidateParagraph(N)    ← O(1) mark dirty
    │
    ▼
RenderEngine::markParagraphDirty(N)     ← O(1) add to dirty region
    │
    ▼
Next paintEvent:
    │
    ├─ ViewportManager::getFirstVisible()
    ├─ ViewportManager::getLastVisible()
    │
    ▼
RenderEngine::paint()
    │
    ├─ Skip paragraphs outside viewport
    ├─ Skip clean paragraphs (retained mode)
    └─ Paint only dirty paragraphs in viewport
```

### Background Layout Thread

```
Main Thread                          Background Thread
    │                                      │
    ├─ User scrolls to page 500            │
    │                                      │
    ▼                                      │
ViewportManager detects new range          │
    │                                      │
    ▼                                      │
LayoutEngine.requestLayoutForRange(495-510)│
    │                                      │
    ├──────── Queue paragraphs ───────────►│
    │                                      │
    ▼                                      ▼
Render with current                   Calculate layouts
layouts (some estimated)              for paragraphs 495-510
    │                                      │
    │                                      ▼
    │◄─────── Signal: layoutReady ─────────┤
    │                                      │
    ▼                                      │
Update heights, repaint                    │
(now with real layouts)                    │
```

### KML Compatibility Layer

KML remains the file format. Internal representation is different:

```
┌─────────────────────────────────────────────────────────────┐
│                    File: chapter.kchapter                    │
│  <kml>                                                       │
│    <paragraph>                                               │
│      <run>Hello </run>                                       │
│      <bold><run>World</run></bold>                           │
│    </paragraph>                                              │
│  </kml>                                                      │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼ LOAD
┌─────────────────────────────────────────────────────────────┐
│                   Internal Representation                    │
│                                                              │
│  TextBuffer:                                                 │
│    m_originalBuffer = "Hello World"                          │
│    m_pieces = [(original, 0, 11)]                            │
│                                                              │
│  FormatLayer:                                                │
│    m_formats = [FormatRange(6, 11, Bold)]                    │
│                                                              │
│  MetadataLayer:                                              │
│    (comments, TODOs, etc.)                                   │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼ SAVE
┌─────────────────────────────────────────────────────────────┐
│                    File: chapter.kchapter                    │
│  (identical KML output)                                      │
└─────────────────────────────────────────────────────────────┘
```

---

## Scope

### Included

**1. TextBuffer (Piece Table)**
- O(log N) insert/delete operations
- O(1) cached plainText() access
- Per-paragraph text caching
- Dirty flag tracking

**2. FormatLayer**
- Format ranges separate from text
- Interval tree for O(log N) queries
- Automatic range adjustment on text changes

**3. LayoutEngine**
- Lazy layout calculation
- Height estimation for off-screen paragraphs
- Background layout thread
- Fenwick tree for prefix sums (keep existing)

**4. ViewportManager**
- Visible range calculation
- Buffer zone management
- Scrollbar position from mixed real/estimated heights

**5. RenderEngine**
- Viewport-only rendering
- Dirty region tracking
- Retained mode (skip unchanged regions)

**6. KML Compatibility**
- KML → Internal format parser
- Internal format → KML serializer
- Round-trip identical output

**7. Feature Parity**
- Comments, TODO tags, snapshots
- Find/Replace
- Undo/Redo
- All view modes (page, continuous, distraction-free)

### Excluded

- New features (belong in subsequent OpenSpecs)
- UI changes beyond performance needs
- Text styling system (OpenSpec #00044)

---

## Acceptance Criteria

### Performance Requirements (150k words - matching Word)
- [ ] 150k word document: scrolling at 60fps
- [ ] 150k word document: Select All < 50ms
- [ ] 150k word document: Copy < 100ms
- [ ] 150k word document: typing latency < 16ms
- [ ] 150k word document: no perceptible lag in any operation
- [ ] 150k word document: load time < 2 seconds
- [ ] Background pagination: non-blocking UI during pagination

### Functional Requirements
- [ ] All existing unit tests pass
- [ ] KML round-trip: load → save produces identical output
- [ ] Comments system works correctly
- [ ] TODO tags system works correctly
- [ ] Snapshot system works correctly
- [ ] Find/Replace works correctly
- [ ] Undo/Redo works correctly with 100+ operations
- [ ] All view modes work (page, continuous, distraction-free)

### Regression Testing
- [ ] No memory leaks (verified with sanitizers)
- [ ] Thread safety maintained
- [ ] No visual regressions in text rendering

---

## Dependencies

- **Depends on:**
  - OpenSpec #00042: Custom Text Editor (provides baseline to rewrite)

- **Required by:**
  - OpenSpec #00044: Text Styling System

---

## Estimated Scope

- **Total: ~80-100 atomic tasks**
- Phase 1: Research & Spike (~5 tasks)
- Phase 2: TextBuffer / Piece Table (~15 tasks)
- Phase 3: FormatLayer (~10 tasks)
- Phase 4: LayoutEngine with estimation (~20 tasks)
- Phase 5: ViewportManager (~10 tasks)
- Phase 6: RenderEngine (~15 tasks)
- Phase 7: KML conversion layer (~10 tasks)
- Phase 8: Feature parity (~10 tasks)
- Phase 9: Performance validation (~5 tasks)

---

## Notes

- This is a critical infrastructure change following Word/Writer architecture
- Must not introduce regressions in existing functionality
- Performance benchmarks should be automated and run in CI
- Consider feature flags to switch between old/new implementation during development
- Background thread requires careful synchronization
