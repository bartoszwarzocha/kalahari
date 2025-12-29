# OpenSpec #00045: BookEditor Architecture Cleanup

**Status:** PENDING
**Priority:** CRITICAL (blocking crashes)
**Created:** 2025-12-29

## Problem Statement

BookEditor currently has TWO architectures running simultaneously:

1. **Legacy Architecture (OpenSpec #00012-#00042):**
   - `KmlDocument* m_document`
   - `LayoutManager` (paragraph layout)
   - `VirtualScrollManager` (virtual scrolling)
   - `PageLayoutManager` (page mode)
   - `ParagraphLayout` objects

2. **New Architecture (OpenSpec #00043 Phase 8):**
   - `TextBuffer` (Fenwick Tree, O(log N))
   - `FormatLayer` (interval tree)
   - `LazyLayoutManager` (lazy layouts)
   - `ViewportManager` (viewport management)
   - `RenderEngine` (rendering)

This dual architecture causes:
- **CRASHES:** Use-after-free when observers point to destroyed components
- **State sync issues:** Data out of sync between old and new components
- **Complexity:** ~35 uses of `m_layoutManager`, ~23 uses of `m_scrollManager`
- **Performance:** Old O(N) code paths still active

## Root Cause of Current Crash

In `fromKml()`:
```cpp
// Line 4883: OLD TextBuffer DESTROYED
m_textBuffer = std::move(result.buffer);

// Line 4900: OLD LazyLayoutManager destroyed, its destructor calls:
//   m_buffer->removeObserver(this)  <-- USE-AFTER-FREE!
m_lazyLayoutManager = std::make_unique<LazyLayoutManager>(m_textBuffer.get());
```

## Solution

**Complete removal of legacy architecture.** Development mode - no backward compatibility needed.

### Components to REMOVE from BookEditor:

| Component | Lines of Code | Usage Count |
|-----------|--------------|-------------|
| `m_document` | ~50 | 0 direct |
| `m_layoutManager` | ~200 | 35 uses |
| `m_scrollManager` | ~150 | 23 uses |
| `m_pageLayoutManager` | ~100 | 11 uses |
| Legacy methods | ~500 | N/A |
| `syncDocumentToNewArchitecture()` | ~50 | Not needed |

**Total: ~1000 lines of dead/legacy code to remove**

### Methods to migrate or remove:

| Method | Current | Action |
|--------|---------|--------|
| `moveCursorUp()` | Uses m_layoutManager | Migrate to LazyLayoutManager |
| `moveCursorDown()` | Uses m_layoutManager | Migrate to LazyLayoutManager |
| `calculateCursorRect()` | Uses m_layoutManager | Use RenderEngine |
| `getMisspelledWordAt()` | Uses m_layoutManager | Use TextBuffer + FormatLayer |
| `selectWordAtCursor()` | Uses m_layoutManager | Use TextBuffer |
| `positionFromPoint()` | OLD version exists | Remove, keep `positionFromPointNewArch()` |
| `paintPageMode()` | OLD version | Remove, keep `paintPageModeNewArch()` |
| `getFocusedRange()` | OLD version | Remove, keep `getFocusedRangeNewArch()` |

### API Changes:

1. **Remove:** `setDocument(KmlDocument*)` - legacy API
2. **Keep:** `fromKml(QString)` / `toKml()` - works with new architecture
3. **Remove:** `document()` getter
4. **Remove:** `layoutManager()` / `scrollManager()` getters

### Files to Update:

1. `include/kalahari/editor/book_editor.h` - Remove legacy members
2. `src/editor/book_editor.cpp` - Remove legacy implementations
3. `src/gui/panels/editor_panel.cpp` - Remove `m_document` usage
4. `tests/editor/*.cpp` - Update tests to use new API
5. `examples/ExampleNovel/*.kchapter` - Verify KML format compatibility

## Implementation Plan

### Phase 1: Fix Immediate Crash (1 hour)
- Add `m_lazyLayoutManager.reset()` before buffer destruction in `fromKml()`

### Phase 2: Remove Legacy Components (4-6 hours)
1. Remove `m_document` member and `setDocument()` method
2. Remove `m_layoutManager` and migrate methods to `LazyLayoutManager`
3. Remove `m_scrollManager` and use `ViewportManager`
4. Remove `m_pageLayoutManager`
5. Remove `syncDocumentToNewArchitecture()`

### Phase 3: Cleanup Methods (2-3 hours)
1. Remove `*NewArch` suffix from methods (they become the only implementation)
2. Remove old method versions
3. Simplify observer pattern (only new architecture components)

### Phase 4: Documentation (1 hour)
1. Update `project_docs/` with new architecture description
2. Update CHANGELOG
3. Update ROADMAP

## Test Plan

1. Build passes without warnings
2. All 735 existing tests pass
3. Manual test: Open ExampleNovel chapters
4. Manual test: Edit text, formatting, cursor navigation
5. Manual test: Spell check, grammar check
6. Performance: No regression in typing latency

## Risks

- **Medium:** Some edge cases in cursor navigation may break
- **Mitigation:** Extensive testing with ExampleNovel

## Decision

Awaiting approval to proceed with architecture cleanup.
