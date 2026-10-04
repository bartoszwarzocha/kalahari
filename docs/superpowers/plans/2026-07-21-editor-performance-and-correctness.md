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
- ✅ **A1 — font/size/colour/background survive reload — DONE & user-verified (2026-07-26, `860277f`).**
  `KmlDocumentModel::parseInlineContent` now reads inline style attrs (font/size/color/bg) via
  `applyInlineStyleAttributes` on formatting tags + `<span>`, and the run-emit predicate was widened
  (Foreground/Background/FontFamilies/FontPointSize). Round-trip symmetric with the serializer.
  Related fixes shipped alongside: **format-bleed-on-reload** (`2b0ad20` — `ensureEditMode` inserted
  paragraph text without an explicit char format, so a bold title / italic paragraph bled into all
  following paragraphs; now inserts with a clean `QTextCharFormat()`), and **toolbar font/size
  controls follow the caret** (`169ac25`).
- ✅ **A2 — alignment survives reload — DONE & user-verified (2026-07-24).** Added
  `KmlDocumentModel::paragraphAlignment(index)` and apply it to each block format in `buildDocument`
  (per-para block format = zero margins + alignment). Save side already serialised align
  (`kml_serializer.cpp:98-107`), so the round-trip now works: **center/right survive reopen**
  (previously everything reverted to left). Justify's *value* also survives, but justify does not
  render live — see C1.
- ✅ **A3 — Dirty/Save COHERENCE — DONE & user-verified (2026-07-22).** Final design differs from
  the first cut: content truth is the **per-open-tab** signal (`m_dirtyChapters` + standalone
  per-tab flag), NOT model `BookElement` dirty — the model flag gets set by tree-build/selection/
  metadata with no user edit and can't be cleared, which caused a spurious "save on project open"
  prompt; that layer was removed. Structure/metadata = `pm.isDirty()`. One predicate
  `MainWindow::hasUnsavedChanges()` drives every prompt path (tab-close, closeEvent, maybeSave,
  New, Open-Recent, project-switch). Also delivered: **#6** chapter-switch no longer prompts (tab
  keeps "*" + app-close prompt, per user UX); **#8** rename reads the CLEAN model title so the "*"
  dirty indicator and " [Status]" suffix never leak into the saved title; the "*" modified indicator
  is now produced inside `getDisplayTitle()` so it survives every tree rebuild/refresh; dead
  `confirmSaveOrDiscard`/`saveCurrentChapter` removed. Verified matrix incl. project-switch. Original analysis below.
- 🔵 **A3 — Dirty/Save COHERENCE (single source of truth).** Audit (2026-07-21) found **4 live
  dirty flags, no single truth**: `MainWindow::m_isDirty`(#1, only phase-0/standalone),
  `ProjectManager::m_isDirty`(#2, chapter edit + structural, **eagerly reset by `saveManifest()`
  pm.cpp:370**), `NavigatorCoordinator::m_dirtyChapters`(#3, per-chapter), `BookElement::m_isDirty`(#5,
  only during the save routine). Prompt paths read DIFFERENT flags → the observed bugs:
  - **Bug#1 (fixed):** spurious prompt on chapter open — connect-before-setContent; reordered
    (navigator_coordinator.cpp:145/150). ✅ user-confirmed.
  - **Bug#2:** edit chapter → close TAB → no prompt. Tab-close (main_window.cpp:919) checks #1
    (never set by chapter edits) AND its body is a `// TODO: just close` no-op stub.
  - **Bug#3:** edit → close PROGRAM → prompt only *sometimes*. closeEvent ORs #1∨#3∨#2 with
    divergent lifecycles; #2 cleared by every structural `saveManifest()`; #3 can be stale.
  **Target (minimal, concrete):** dirtiness per-element on `BookElement`, aggregated by
  `ProjectManager`; one predicate `MainWindow::hasUnsavedChanges()` = `#1 (non-project docs)` OR
  `!pm.getDirtyElements().empty()` (content) OR `pm.isStructureDirty()` (manifest). Steps:
    1. Chapter-edit lambda (navigator_coordinator.cpp:155-157) also marks the model
       (`pm.markElementDirty(id)`); `m_dirtyChapters` becomes a derived `"*"` cache, not a rival truth.
    2. De-conflate `ProjectManager`: split `m_structureDirty` (manifest) vs content(=`getDirtyElements()`);
       `saveManifest()` resets ONLY `m_structureDirty` (fixes Bug#3's eager clear).
    3. Rewrite tab-close (main_window.cpp:913-929): resolve tab's `elementId`, prompt Save/Discard/
       Cancel via the predicate, clear that element's dirty on close (fixes Bug#2 + stale flag).
    4. Standalone editor: real per-tab dirty flag (document_coordinator.cpp:772-782), seeded false,
       included in the predicate.
    5. Route ALL prompt paths through `hasUnsavedChanges()`: `maybeSave`(doc_coord:75), `onCloseDocument`
       (:642), `onNewDocument`(:165), `onOpenRecentFile`(:330), project-switch confirms (:275,:352).
    6. Simplify `closeEvent` (main_window.cpp:1052-1107) to the single predicate (both the check and the
       Save-branch recheck).
  - 🟡 **A4 (cleanup) — ASSESSED & real landmine handled (2026-07-27, `d2a833f`).** The lossy
    unreachable `toKml()` view-mode fallback (silently dropped inline formatting) now logs a loud
    error + is documented — no silent data loss. Deliberately NOT done: `KmlParser` is NOT dead
    (3 test files use it as a round-trip parser — removing it drops coverage); the dead
    `documentModified` signal is harmless (removal = pure churn). Retiring the stale
    `KmlDocumentModel` two-representation hop is a big rework, not cleanup — leave for a future
    convergence pass. Net: no forgotten runtime problem remains.

  **A3 test matrix (user runs after implement):**
  | # | Action | Expected |
  |---|--------|----------|
  | 1 | open chapter A, no edit, open B | no prompt |
  | 2 | open A, no edit, close tab / close app | no prompt |
  | 3 | edit A, close TAB | **prompt Save/Discard/Cancel** (Bug#2) |
  | 4 | edit A, close APP | **prompt** (Bug#3) |
  | 5 | edit A, close app, repeat 5× | **prompt every time** (no randomness) |
  | 6 | edit A, switch to B | prompt (already worked) |
  | 7 | edit A, Save via any prompt, then close app | **no prompt** (all flags cleared) |
  | 8 | edit A, structural op (rename/add), close app | **prompt** (structural save must not clear content dirty) |
  | 9 | edit standalone file, close | **prompt** |

### Stage B — One undo + one cursor authority (foundation for coherent editing)
- ⬜ **B1** Disable `QTextDocument` native undo (`m_textBuffer->setUndoRedoEnabled(false)` in
  `ensureEditMode`); route formatting/alignment/font through the **already-existing**
  `FormatApplyCommand`/`FormatRemoveCommand` (+ a new alignment command) on `m_undoStack`.
- ⬜ **B2** Restore `m_cursorPosition`/`m_selection` (and `syncPipelineCursor()`) on undo/redo.
- ⬜ **B3** Make **pending format** real — `insertText` consumes `m_pendingBold/Italic/...` then clears.

### Stage C — Style & paragraph-format rendering correctness (the visible complaints)
- ⏸️ **C1 — Justify live: DEFERRED (2026-07-24, user-agreed "not at all costs").** Root-caused via a
  `[DIAG-JUSTIFY]` instrument: the layout is configured 100% correctly (alignment=0x8 AlignJustify,
  effWidth==m_textWidth==1255, 6 lines, ~30-50px room to stretch on non-last lines). Center/right
  render (draw-time offset) but justify does not (Qt's engine-level space-expansion isn't applied in
  our custom-layout + pipeline `QTextLayout::draw()` path). Ruled out: width, last-line, alignment
  value, document default text option (no `setDefaultTextOption` anywhere). One bounded fix attempt
  (`layout->setCacheEnabled(true)`, per Qt's manual-layout example) did NOT fix it — kept anyway as a
  small perf win. Per the editor's "turbo-typewriter" philosophy justify is view-comfort, so it's
  parked. **Revisit during Stage-C view-comfort layout work (with hyphenation C5)** — likely needs
  drawing lines with explicit space-distribution or routing these blocks through Qt's own layout.
- ⬜ **C2** **Line spacing / paragraph spacing / first-line indent** — currently ignored by the layout
  math; wire into `layoutBlock` line-height + block formats.
- ⬜ **C3** **Appearance plumbing** — forward the read-but-dropped settings from `setAppearance` to the
  pipeline: line spacing, paragraph spacing, indent, cursor style, cursor width, text-frame border.
- ⬜ **C4** **Text colour** operation (`setSelectionTextColor`, undoable via Stage B) + recolour
  selected-text foreground.
- ⬜ **C6** **Per-run font size not DPI-scaled** (deferred here 2026-07-26, user-agreed). In FontScaling
  modes (Continuous/Focus/DF) the pipeline scales only the base font; an explicit per-fragment point
  size (set via the toolbar spinner) renders at its RAW value, inconsistent with the DPI-scaled base.
  Proper fix = render-time scaling of per-fragment sizes (`layoutBlock` builds `QTextLayout::FormatRange`s
  with each fragment size × the DPI/zoom factor) — the SAME rework as C2.1 per-run formatting visibility,
  so do them together. Also reconsider whether the font-shrink fix's "bake scaled size onto every char"
  shortcut should be replaced by pure render-time scaling. (Per-run sizes are rare in prose → low urgency.)
- ⬜ **C5** **Automatic hyphenation — VIEW-ONLY** (author-comfort). Qt has NO built-in hyphenation:
  needs a hyphenation lib + Polish (and per-language) dictionaries; insert soft hyphens (U+00AD)
  at break points during `layoutBlock`, and NEVER persist them to KML. Pairs with justify (C1).
  **Design principle behind this:** the editor's job is comfortable WRITING; all formatting/justify/
  hyphenation are author-comfort for the on-screen view only. The MANUSCRIPT export is clean and
  standardised (fixed-width font, no hyphenation) because typesetting handles composition later.
  Implication: keep a clean "manuscript/typescript export" separate from the rich editing view.

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
