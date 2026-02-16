# Analysis: Phase 15 Post-Regression Issues

**Date:** 2026-02-16
**OpenSpec:** #00043 - Editor Performance Rewrite
**Status:** Issues found after Phase 15 manual testing

---

## ROOT CAUSE 1: HUGE LAG — Missing Viewport Culling in Scroll Mode (P0)

**Symptom:** Huge lag when scrolling, especially on large documents (150K words)
**Location:** `src/editor/editor_render_pipeline.cpp`, lines ~706-737, ~1688-1728

### Problem

In Scroll Mode, `rebuildPaginationCache()` creates a **single virtual page** containing **ALL** paragraph slices for the entire document. The render loop then iterates ALL slices — there is **no per-slice visibility check**.

For a 150K word document (~3000+ paragraphs), this means:
- ~3000 painter save/restore cycles per frame
- ~3000 transform setups per frame
- Drawing ALL text on every paint event (only ~20-30 paragraphs visible)

The page-level visibility check never triggers because the single virtual page covers everything.

### Fix

**Recommended:** Use existing `renderParagraphs()` (line 819) for Scroll Mode — it already has proper viewport culling via `firstVisibleParagraph`/`lastVisibleParagraph`. Only use pagination/slice approach for Page Mode.

```cpp
if (m_context.viewMode == ViewMode::Page) {
    rebuildPaginationCache();
    renderPageBackgrounds(painter, clipRect);
    for (const PageContent& page : pages()) { ... renderSlice ... }
} else {
    renderParagraphs(painter, clipRect);  // Has first/last culling
    renderSelection(painter, clipRect);
    renderCursor(painter);
}
```

---

## ROOT CAUSE 2: BROKEN FONTS — documentChanged() Only Re-layouts 3 Blocks (P0)

**Symptom:** All text same size, no paragraph spacing variation, fonts look wrong
**Location:** `src/editor/kalahari_text_document_layout.cpp`, lines ~88-109

### Problem

`KalahariTextDocumentLayout::documentChanged()` has a hard-coded limit of **3 blocks**:

```cpp
int blocksToLayout = 3;  // HARD-CODED LIMIT!
```

When `setFont()` calls `m_document->markContentsDirty(0, characterCount())`, this triggers `documentChanged(0, 0, N)` which only re-layouts blocks 0, 1, 2 — the rest keep the old font.

Additionally, `QTextDocumentSource::setFont()` updates `m_document->setDefaultFont()` but does NOT update `KalahariTextDocumentLayout::m_font`, so blocks re-laid out later may use the wrong font size.

### Fix

**Option A (quick):** In `QTextDocumentSource::setFont()`, also call `customLayout->setFont(font)` which re-layouts all blocks.

**Option B (proper):** Fix `documentChanged()` to detect full-document changes and re-layout all blocks:
```cpp
bool isFullDocumentChange = (from == 0 && charsRemoved == 0 &&
                              charsAdded >= document()->characterCount() - 1);
if (isFullDocumentChange) {
    layoutAllBlocks();
    return;
}
```

---

## ROOT CAUSE 3: SLOW HIT-TESTING — Consequence of Root Cause 1 (P1)

Hit-testing itself is efficient (HeightTree binary search), but click triggers `update()` → `paintEvent` → `render()` which has the O(n) rendering problem. Fixing Root Cause 1 fixes this.

---

## Additional Findings

### Finding 4: Redundant Font Applications in syncPipelineState()
`setZoom()` → applies effectiveFont, `setFont()` → overwrites with base font, `applyInitialConfig()` → re-applies effectiveFont. Three unnecessary relayout cycles.

### Finding 5: markAllDirty() Called Excessively
20+ setter methods call `markAllDirty()`, each emitting `repaintRequested`. Called sequentially in `syncPipelineState()`.

### Finding 6: Over-Aggressive Cache Invalidation
Color-only setters (e.g., `setTextColor`) invalidate pagination cache, triggering full rebuild for changes that don't affect layout.

---

## Files to Modify

| File | Issue | Priority |
|------|-------|----------|
| `src/editor/editor_render_pipeline.cpp` | Branch Scroll/Page rendering, add culling | **P0** |
| `src/editor/kalahari_text_document_layout.cpp` | Fix `documentChanged()` 3-block limit | **P0** |
| `src/editor/text_source_adapter.cpp` | `setFont()` should update custom layout font | **P1** |
| `src/editor/book_editor.cpp` | Refactor `syncPipelineState()` redundant fonts | **P2** |
| `src/editor/editor_render_pipeline.cpp` | Reduce unnecessary cache invalidation | **P2** |

## Recommended Fix Order

1. **Fix Scroll Mode rendering** (Root Cause 1) — viewport culling
2. **Fix font propagation** (Root Cause 2) — documentChanged + setFont chain
3. **Cleanup** — redundant font apps, cache invalidation
