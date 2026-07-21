# Editor: Performance & Correctness — Step-by-Step Plan

**Created:** 2026-07-21
**Owner topic:** "Make the editor a first-class, performant control."
**Status legend:** ⬜ todo · 🔵 in progress · ✅ done (built + user-verified + committed)

## Goal (user's words)
A performant editor that is:
- **a) fast**
- **b) correctly displays all view modes and styles**
- **c) reacts fast to style changes** (today: select-all + bold takes ~10 s)

## Guiding principle (decided this session, 2026-07-21)
At **real** document sizes this project targets (~15k words ≈ ~100k chars, a few
hundred paragraphs), performance problems are **targeted O(N²) bugs, not scaling walls.**
→ **Measure, fix the quadratic, verify. No speculative rewrite.**

The earlier "Tier 2 / order-statistic treap / viewport virtualization" program is
**PARKED**: its ceiling benchmark measured **150k *paragraphs*** — 100–500× larger than
any real file here. It solved a problem we don't have. Revisit only if a *measured*
profile on a real file demands it (see Phase 4).

Working rules: one topic end-to-end (build + verify + commit before the next);
I build & signal, user runs the app; measure before rewriting.

---

## Phase 1 — Style operations must be instant  (attacks "c")
- 🔵 **1.1 Fix `hasFormat()` O(N²)** (built 2026-07-21, tests green — awaiting user verify + commit)
  — it built one `QTextCursor` per character to decide
  the bold/italic toggle direction; on select-all that is ~100k cursor constructions =
  the ~10 s freeze. Replace with **`QTextFragment` iteration** (runs of uniform format).
  Covers bold/italic/underline/strikethrough (all route through `toggleFormat` → `hasFormat`).
- ⬜ **1.2 Sweep sibling inefficiencies** in the edit/stat path:
  - `wordCount()` / `characterCountNoSpaces()` call `toPlainText()` + full scan on **every**
    `contentChanged` (3× per edit via `StatisticsCollector::recalculateStats`, whose "O(1)
    cached" comment is false). Make counts genuinely incremental/cached, or debounce.
  - Audit alignment / other selection ops for per-char loops. (`setSelectionFontFamily/Size`
    already verified clean.)
- ⬜ **1.3 Verify**: bold / italic / underline / font / size / align on select-all are all
  instant on the real chapter.

## Phase 2 — Styles must actually render  (attacks "b")
- ⬜ **2.1** `KalahariTextDocumentLayout::layoutBlock` never calls `QTextLayout::setFormats()`,
  so per-run bold/italic/underline/font are stored but **invisible** (only block-level
  alignment + one uniform block font render). Build a `QList<QTextLayout::FormatRange>` from
  the block's fragment char-formats and apply it. (Same `layoutBlock` we already touched for
  the font-shrink fix — must stay consistent with the pipeline's scaled effective font.)
- ⬜ **2.2 Verify**: all inline styles render correctly and match what the toolbar reports.

## Phase 3 — View modes solid  (attacks "b")
Order the user set: get the **two basic modes flawless first** (Continuous + the "base"
mode — confirm whether base = Page), reusing shared mechanisms, THEN the two view modes
(**Typewriter**, **Focus / Distraction-free**).
- ⬜ 3.1 Continuous mode: correct + fast.
- ⬜ 3.2 Base/Page mode: correct pagination + fast.
- ⬜ 3.3 Typewriter mode.
- ⬜ 3.4 Focus / Distraction-free mode.

## Phase 4 — Load / scroll performance — ONLY IF measured to be a problem
The OpenOffice-style "load a big book, then work fast" concern. Do **not** pre-build for it.
- ⬜ 4.1 If opening real files or scrolling shows lag: **measure first** (instrument load +
  scroll on a real large file, capture paragraph/char counts).
- ⬜ 4.2 If the profile points at full-document layout: lazy/viewport layout (shape only
  visible blocks ± margin; estimated heights for the rest). Order-statistic accounting
  (Fenwick/treap) only if the profile shows the linear position/height walk is itself the cost.

---

## Progress log
- 2026-07-21: Plan created. Priority 0 (font shrink on settings save) already fixed &
  committed `bbaa986` before this plan. Root-caused the ~10 s bold to `hasFormat` O(N²).
  Parked the treap/virtualization program (mis-scoped for 150k paragraphs).
