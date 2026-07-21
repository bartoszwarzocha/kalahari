# Editor — MASTER Plan (Performance & Correctness)

**Created:** 2026-07-21 · supersedes the interim Phase-1 plan (folded in below).
**Basis:** four full-subsystem audits (rendering/layout, editing/formatting, document
model/persistence, view modes/appearance) — all file:line grounded.
**Status legend:** ⬜ todo · 🔵 in progress · ✅ done (built + user-verified + committed)

---

## 0. ROOT CAUSE — why we keep patching (answers "why so many fix-iterations")
All four audits converged on ONE structural cause: **the editor carries duplicated
representations, and features are half-built on one side and dropped on the other.**

| Duplication | "Good" side | "Lossy/latent" side | Consequence |
|---|---|---|---|
| **Two layout engines** | edit: `QTextDocument` + `KalahariTextDocumentLayout` (what the user actually sees — edit mode is always on after load) | view: `KmlDocumentModel` + `HeightTree` (mostly dead at runtime) | every typography feature must be built twice; they diverge (justify source, per-run colour/font) |
| **Two KML parsers** | `KmlParser` (reads font/size/colour/justify) — **DEAD** | `KmlDocumentModel::parseInlineContent` — **LIVE, lossy** | reload silently drops font/size/colour/justify → re-save makes the loss permanent |
| **Two undo stacks** | custom `m_undoStack` (text edits) | `QTextDocument` native (never disabled) | formatting is not undoable; caret desyncs after undo |
| **Two cursor/selection models** | `m_cursorPosition`/`m_selection` | `QTextDocument` cursor | stale caret/selection after undo/redo |
| **Read-but-dropped settings** | settings dialog writes them | `setAppearance` never forwards them | line spacing, paragraph spacing, indent, cursor style/width, text-frame border all do nothing |

**The strategy is therefore CONVERGENCE, not symptom-patching:**
1. Commit to the **edit-mode `QTextDocument` path as the single canonical engine** (it already
   renders styles correctly and is what the user sees).
2. Make the **load path lossless** into that engine (fix/replace the lossy parser).
3. Make it the **single undo + cursor authority**.
4. Retire the dead duplicates (view parser, stale model, dead `ParagraphLayout`/`TableLayout`) as cleanup.
5. Feature gaps then become **single-implementation** fixes.
Performance stays **measurement-gated** (real files are ~15k words; fix quadratics when felt, no speculative rewrite — the treap/virtualization program stays parked until a profile demands it).

---

## 1. WHAT IS ALREADY DONE / WORKING  (the "done" inventory)
- **This session:** font no longer shrinks on settings save (`bbaa986`); `hasFormat` O(N²) →
  fragments, style toggles instant (`d27d2a0`); Phase-1 sweep confirmed no other O(N²) in the edit path (`d3b727a`).
- **Rendering:** plain text, wrapping, block layout; alignment L/C/R (edit mode); caret + selection
  highlight; DPI+zoom font scaling; margins; scrollbar/height.
- **Editing:** caret movement (arrows/word/doc/page); selection (mouse/keyboard/select-all); insert/
  delete/newline/split-merge; bold/italic/underline/strike APPLY + render + persist (edit mode);
  font family/size on selection; markers (TODO/Note); plain clipboard cut/copy/paste; correct counts.
- **View/appearance:** Continuous mode (reference, solid); Page-mode *rendering* (real pagination);
  zoom via Ctrl+wheel + API + zoom modes; appearance: font, text/bg colours, light/dark, selection
  colour, cursor blink, margins.
- **Persistence:** `.kchapter`/`.klh` disk I/O is lossless (KML stored verbatim); serializer (SAVE
  side) writes everything faithfully.

---

## 2. ROADMAP — ordered stages (dependency-driven)
Each item: analyze → implement → I build → **you test the specific check I give you** → commit.

### Stage A — Lossless persistence (do FIRST: silent, permanent DATA LOSS)
- ⬜ **A1** Fix the load parser so **font family / size / text colour / background** survive load.
  (`kml_document_model.cpp:557-638`; also widen the run-emit predicate `:582-595`, or route load
  through the complete `KmlParser` + retire the duplicate.)
- ⬜ **A2** Apply **paragraph alignment (incl. justify)** on materialization — add a model accessor
  and set the block alignment in `ensureEditMode` (`book_editor.cpp:5386-5411`). Fixes justify-lost-on-reload.
- ⬜ **A3** Fix **spurious "unsaved changes" on chapter open** — the navigator path connects
  `contentChanged` before `setContent` with no reset (`navigator_coordinator.cpp:140` vs `:168`).
- ⬜ **A4 (cleanup)** retire the dead second parser + stale `KmlDocumentModel` two-representation hop.

### Stage B — One undo + one cursor authority (foundation for coherent editing)
- ⬜ **B1** Disable `QTextDocument` native undo (`m_textBuffer->setUndoRedoEnabled(false)` in
  `ensureEditMode`); route formatting/alignment/font through the **already-existing**
  `FormatApplyCommand`/`FormatRemoveCommand` (+ a new alignment command) on `m_undoStack`.
- ⬜ **B2** Restore `m_cursorPosition`/`m_selection` (and `syncPipelineCursor()`) on undo/redo.
- ⬜ **B3** Make **pending format** real — `insertText` consumes `m_pendingBold/Italic/...` then clears.

### Stage C — Style & paragraph-format rendering correctness (the visible complaints)
- ⬜ **C1** **Justify** live: runtime-verify the width-plumbing hazard (`QTextDocumentSource::setTextWidth`
  vs `updateLayoutWidth` → if `m_textWidth==0`, `effectiveWidth=10000` ⇒ no wrap, no justify), fix so
  multi-line justified paragraphs actually justify.
- ⬜ **C2** **Line spacing / paragraph spacing / first-line indent** — currently ignored by the layout
  math; wire into `layoutBlock` line-height + block formats.
- ⬜ **C3** **Appearance plumbing** — forward the read-but-dropped settings from `setAppearance` to the
  pipeline: line spacing, paragraph spacing, indent, cursor style, cursor width, text-frame border.
- ⬜ **C4** **Text colour** operation (`setSelectionTextColor`, undoable via Stage B) + recolour
  selected-text foreground.

### Stage D — View modes (build on the now-correct base)
- ⬜ **D0** Wire the dead **zoom menu/toolbar commands** (`command_registrar.cpp:504-506`) — quick win.
- ⬜ **D1** Confirm **Continuous** as exact reference.
- ⬜ **D2** **Page** mode navigation from the real pagination cache (not viewport height); page-size
  selection (A4/A5/Letter); page numbers; mirror margins.
- ⬜ **D3** **Focus** mode: actually enable dimming/current-line highlight on `setViewMode(Focus)`.
- ⬜ **D4** **Typewriter**: line-lock in all modes (not edit-mode-only).
- ⬜ **D5** **Distraction-Free**: connect fullscreen/hide-UI + real text centering/narrowing.

### Stage E — Rich content (larger, later)
- ⬜ **E1** Inline **images** (no infrastructure exists today).
- ⬜ **E2** **Tables** — wire the fully-built-but-dead `TableLayout` into the pipeline, or reimplement.

### Stage F — Performance (measurement-gated)
- ⬜ **F1** Measure typing/scroll/click on the **real** book. If the edit-path O(N)
  (`updateBlockPositions` per keystroke, linear `hitTest`, `positionFromPoint`) is felt, replace with
  the O(log N) `HeightTree` path (design doc `2026-07-14-editor-performance-fix-design.md`). Only then.

### Stage G — Un-stub or clearly disable misleading features (fold in as we pass each area)
- ⬜ Comments (stubbed), spell-check underline/suggestions (stubbed), grammar (stubbed), formatted
  clipboard (plain-only). Either implement or disable the UI so it doesn't mislead.

---

## 3. SYSTEMATIC TEST SEQUENCE ("place by place", bottom-up)
We walk this together; I tell you the exact action + what to look for each time.
- **T1 Foundation:** type, move caret everywhere, select, **undo/redo of text AND of formatting**.
- **T2 Character styles:** bold/italic/underline/strike/colour → visible, undoable, and **survive save→reopen**.
- **T3 Paragraph format:** align L/C/R/**justify** visible + survive reopen; line spacing, paragraph spacing, indent take effect.
- **T4 Round-trip:** apply everything → save → close → reopen → all survives; **no spurious "unsaved" prompt on open**.
- **T5 View modes:** Continuous → Page (nav/size/numbers) → Focus → Typewriter → Distraction-Free → zoom buttons.
- **T6 Performance:** on the real book — typing, scrolling, clicking, select-all ops.
- **T7 Rich content:** images, tables (later).

---

## 4. CADENCE (how we work — no jumping)
One stage/item at a time. For each: I analyze deeply first → implement → build & report ready →
**I tell you precisely what to test and what to look for** → you verify → we commit → next item.
I keep this file as the single source of progress.

## Progress log
- 2026-07-21: Priority-0 font-shrink fixed (`bbaa986`). hasFormat O(N²) fixed (`d27d2a0`), Phase-1
  closed (`d3b727a`). Four-subsystem audit completed → this master plan. Root cause = duplicated
  representations; strategy = convergence on the edit-mode QTextDocument engine.
