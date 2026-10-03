# Editor – Stage 1 (quick fixes): results

Date: 2026-10-03 · branch `claude/project-thread-mol17d` · base: stage 0 (`a0698e6`), on top of the
user's local `main` (`b311c1b`) · plan: `/mnt/project-files/przeglad/edytor.md`, §6 Stage 1

## What changed

| Plan item | Change |
|---|---|
| Layout (prerequisite) | `KalahariTextDocumentLayout` re-lays out exactly the blocks an edit touched (it used to lay out a fixed window of 3 blocks, so a paste or an undo left blocks without lines). Wrap width and default font are owned by `QTextDocument`; the render pipeline is the only code that sets them. |
| W1 – loading | `ensureEditMode()` builds the document in one edit block with undo disabled; the pipeline applies font and width while the document is still empty, so the content is laid out once. `KmlDocumentModel` parses each paragraph straight from the document reader instead of re-serialising and re-parsing it. |
| W2 – resize | A visible editor applies a new width 80 ms after the last resize event: one full relayout per window drag (it was three per resize event). |
| W3 – zoom | Ctrl+wheel in the font-scaling modes is applied 80 ms after the last notch; `zoomFactor()` reports the pending value at once. The scroll range now follows every height change (`ViewportManager` listens to `documentSizeChanged`), so it is right after a zoom or a delayed width change. |
| W5, W9 – word count | One definition for the whole application, `core::countText()`: a word is a run of non-whitespace characters containing a letter or a digit (a dialogue dash or `***` is not a word). `BookEditor` caches the counts per paragraph (`QTextBlockUserData`), so a statistics query recounts only edited paragraphs. Used by `ChapterDocument`, `KmlDocumentModel`, `SnapshotManager`, `PropertiesPanel` and the distraction-free overlay. `ChapterDocument::kmlToPlainText()` gives the editor's text (inline tags no longer split words, entities are decoded); the chapter paragraph count was always 1 and now counts paragraphs with text. |
| W8 – search highlights | Only matches in the visible paragraphs are measured (binary search in the sorted matches). |
| 4.1 – alignment | Already fixed in the base (`05b3179`); confirmed by the stage 0 tests. |
| 4.2 – DPI | The logical DPI is used everywhere. The font is scaled by zoom only (it was also scaled by physical/logical DPI, 1.48× on the user's 125% screen); page sizes (points) and margins (mm) are converted with the same logical DPI, and pagination no longer scales line heights a second time. |

Also fixed, in the same code paths:

- **KML**: every attribute of metadata elements survives load and save (comment author/created/resolved,
  todo completed/priority, footnote number, unknown attributes); `<charref>`/`<locref>` are no longer
  dropped on load; a TODO inside a comment keeps both elements on save; an unknown inline element no
  longer cuts off the rest of the paragraph (`KmlDocumentModel` and `KmlParser`); attribute values with
  `& < > "` load correctly.
- **TODO markers** are stored as the same attribute map as the KML `<todo>` element (it was a JSON
  string), so a marker added in the editor survives save and reload; removing a marker clears its
  whole anchor.
- **Fonts**: `setAppearance()` no longer merges the font family and size into every character (an
  undoable edit that was saved as `font="…" size="…"` on every run).
- **Cursor**: one blink timer (there were two, with different intervals – an irregular blink and two
  full repaints per cycle); it runs only while the editor has focus, the cursor is hidden without
  focus, a blink repaints only the cursor, and the cursor stays drawn with blinking turned off. The IME
  cursor rectangle comes from the pipeline (a duplicate calculation was removed).
- **Properties panel**: selection changes are debounced like cursor moves; whole-document statistics
  come from the editor's cache.

## Tests

- **Cloud** (Linux, Qt 6.4.2, Debug, `QT_QPA_PLATFORM=offscreen`): 683 test cases, all pass.
- The 11 stage 0 tests tagged `[known-bug][!mayfail]` pass and are regular tests now.
- New: `tests/core/test_text_statistics.cpp`, `tests/editor/test_editor_stage1.cpp` (13 cases: word
  count cache, resize and Ctrl+wheel debounce, scroll range, fonts not saved into KML, unknown
  elements, charref/locref, nested metadata, cursor blink, DPI). Helpers shared by the editor tests and
  the benchmark: `tests/editor/editor_test_utils.h`.
- Mutation check: putting each fixed defect back makes its test fail (resize debounce, wheel zoom,
  scroll range, unknown element in both parsers, nested metadata, blinking without focus, full repaint
  per blink).
- The focused-cursor test needs the offscreen platform and skips elsewhere, so the suite never shows
  a window.
- **Windows**: see below (filled in after the build on the user's machine).

## Benchmark (cloud, Debug, 150k words, 1568 paragraphs)

Hidden test `"[benchmark][stage0]"`, updated for stage 1: full relayouts are counted with
`blocksLaidOut()`, and Ctrl+wheel and search-highlight rows were added. The edit-block replica rows
are gone – the real load path now uses an edit block.

| Operation | Time [ms] | Notes |
|---|---:|---|
| fromKml – 25k words | 75.5 | |
| fromKml – 50k words | 92.4 | |
| fromKml – 100k words | 170.8 | |
| fromKml – 150k words | 253.1 | 2409 ms before stage 1 (same cloud environment) |
| paintEvent – Continuous (avg of 10) | 3.3 | |
| Width change 1000→1200 px | 196.8 | 1 full relayout |
| Zoom 100→125% (Continuous) | 200.4 | 1 full relayout |
| Ctrl+wheel, 5 notches | 0.0 | 0 relayouts during, 1 after |
| Typing 100 chars – Continuous, paint after each | 548.5 | 5.5 ms/char |
| Typing 100 chars – Page, paint after each | 873.9 | 8.7 ms/char |
| paintEvent – DistractionFree (avg of 10) | 4.9 | word count from the cache |
| Search "a" (all matches) | 279.5 | 58 979 matches |
| paintEvent with search highlights | 5.7 | |

## Notes for later stages

- **Stage 2 (viewport)**: `ViewportManager::paragraphY()`/`paragraphAtY()` scan linearly (0.1 ms per
  call at 1568 paragraphs); `rebuildPaginationCache()` is O(n) in the scroll modes.
- **Stage 3 (page mode)**: the page size from the appearance settings is never passed to the pipeline
  (always the A4 default); page margins are scaled by zoom but the page is not; search, comment and
  marker overlays and the cursor rectangle use scroll-mode coordinates in Page mode;
  `calculateEffectiveMargins()` returns the pipeline's margins once it is configured; typing is slower
  in Page mode (8.7 vs 5.5 ms/char).
- **Stage 4 (rendering)**: the Block/Underline cursor width is measured with the base font (ignores
  zoom and character formats); comment/marker rendering.
- **Search**: `SearchEngine` does not follow document edits (stale matches); `FindReplaceBar` searches
  on every keystroke and copies all matches (280 ms for "a" in 150k words).
- **Leftovers**: `KmlParser` is used only by tests (a second KML parser next to `KmlDocumentModel`);
  `KmlDocumentModel`'s layout and height code (the old view mode) no longer renders anything; the
  `Marker*Command` classes are used only by tests; `EditorRenderPipeline::setConfigDpi()` is unused.
- `ChapterDocument::fromJson()` keeps the statistics stored in the file until the chapter is saved
  again, so old word counts can differ from the editor until then.
- Outside the editor: `main_window.cpp` adds a `statisticsChanged` connection on every
  `documentOpened`.
