# Editor – Stage 1 (quick fixes): results

Date: 2026-10-03 · branch `claude/project-thread-mol17d` (PR #5) · base: stage 0 (`a0698e6`), on top
of the user's local `main` (`b311c1b`) · plan: `/mnt/project-files/przeglad/edytor.md`, §6 Stage 1

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

- **Cloud** (Linux, Qt 6.4.2, Debug, `QT_QPA_PLATFORM=offscreen`): 683 test cases, all pass. Under
  ASan + UBSan they pass as well, apart from the 9 `[event-bus]` cases left out for the test bug the
  audit found (`tests/core/test_event_bus.cpp:305`).
- **Windows** (the user's machine, MSVC 14.51, Qt from vcpkg), at `7ef93a3`: Debug and Release – 683
  test cases, 682 pass and 1 is skipped (the focused-cursor test, which needs the offscreen platform);
  no debug assertion in Debug.
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
- MSVC Debug found what Linux and macOS did not: `relayoutRange()` computed `begin() + oldLast + 1`
  with `oldLast == -1` (first layout of a document, blocks inserted at the start), an iterator before
  `begin()`. Fixed in `7ef93a3`. libstdc++ debug mode (`-D_GLIBCXX_DEBUG`) reproduces it; with the fix
  the editor and core tests run clean in that mode, except 31 that hand a `QVariantMap` to the system
  Qt and crash on the ABI alone (`QMap` wraps `std::map`, whose layout differs in debug mode).

## Benchmark (150k words, 1568 paragraphs)

Hidden test `"[benchmark][stage0]"`, updated for stage 1: full relayouts are counted with
`blocksLaidOut()`, Ctrl+wheel and search rows were added, and the edit-block replica rows are gone (the
real load path now uses an edit block).

### Cloud, Debug – stage 0 (`a0698e6`) and stage 1 run back to back on the same machine

| Operation | Stage 0 [ms] | Stage 1 [ms] | Notes |
|---|---:|---:|---|
| fromKml – 25k / 50k / 100k words | 530 / 1091 / 2364 | 54 / 82 / 177 | |
| fromKml – 150k words | 4177 | 253 | 16× faster |
| Width change 1000→1200 px | 600 | 201 | full relayouts: 3 → 1 |
| First paint after the width change | 207 | 7 | |
| Zoom 100→125% (Continuous) | 407 | 219 | full relayouts: 2 → 1 |
| Ctrl+wheel, 5 notches | – | 0.0 | 0 relayouts during, 1 after |
| Typing 100 chars – Continuous, no paint / paint after each | 90 / 500 | 48 / 493 | ≈5 ms/char with paint |
| Typing 100 chars – Page, no paint / paint after each | 142 / 675 | 55 / 706 | ≈7 ms/char with paint |
| paintEvent – Continuous / DistractionFree (avg of 10) | 2.9 / 64.8 | 3.3 / 4.3 | word count from the cache |
| Search "a" (all matches) / paint with highlights | – | 268 / 4.5 | 58 979 matches |

### Windows (the user's machine), screen: physical DPI 142.4, logical 96, scaling 125%

Debug: stage 0 is one run (`a0698e6`), stage 1 the median of 5 runs (`7ef93a3`; the CPU is a hybrid
Intel Core Ultra 9 275HX, single runs vary up to 2×). Release: stage 1 only (one run at `f2e1254`; a run
at `7ef93a3` gave the same load and typing times).

| Operation | Debug stage 0 [ms] | Debug stage 1 [ms] | Release stage 1 [ms] |
|---|---:|---:|---:|
| fromKml – 25k / 50k / 100k words | 907 / 1765 / 4030 | 89 / 171 / 339 | 18 / 32 / 64 |
| fromKml – 150k words | 7273 | 510 | 98 |
| Width change 1000→1200 px | 824 (3 relayouts) | 259 (1) | 62 (1) |
| First paint after the width change | 298 | 37 | 3.7 |
| Zoom 100→125% | 559 (2 relayouts) | 272 (1) | 62 (1) |
| First paint after the zoom | 41 | 64 | 5.6 |
| Ctrl+wheel, 5 notches | – | 0.3 (0 during, 1 after) | 0.1 |
| Typing 100 chars – Continuous / Page, no paint | 205 / 199 | 133 / 196 | 13 / 15 |
| Typing – Continuous, paint after each [ms/char] | 30.5 | 55.2 | 3.5 |
| Typing – Page, paint after each [ms/char] | 24.8 | 57.5 | 3.7 |
| Switch to Page + first paint | 24 | 57 | 4.8 |
| Scroll to the end / Ctrl+End | 30 / 31 | 48 / 46 | 3.4 / 3.1 |
| paintEvent – Continuous | 25.2 | 29.6 | 2.9 |
| paintEvent – whole document selected | 28.6 | 50.9 | 3.3 |
| paintEvent – DistractionFree | 310 | 39 | 3.1 |
| Search "a" (58 979 matches) | – | 2576 | 103 |

In Debug on Windows the first paint after any change (typing, zoom, switching to Page, scrolling, a
selection) costs 15–35 ms more than in stage 0, while typing without a paint is not slower and a
repeated paint costs about the same; Linux shows no such difference. Most likely cause (inferred, not
measured): with the logical DPI the text is 1/1.48 of its former size on this screen, so the 1000×800
viewport holds about 2.2× more text, and glyph rendering through the debug Qt libraries dominates the
paint. Release types at 3.5 ms per character with a full paint. To be measured in stage 4 (paint cost
against the amount of visible text).

## Notes for later stages

- **Stage 2 (viewport)**: `ViewportManager::paragraphY()`/`paragraphAtY()` scan linearly (0.1 ms per
  call at 1568 paragraphs); `rebuildPaginationCache()` is O(n) in the scroll modes.
- **Stage 3 (page mode)**: the page size from the appearance settings is never passed to the pipeline
  (always the A4 default); page margins are scaled by zoom but the page is not; search, comment and
  marker overlays and the cursor rectangle use scroll-mode coordinates in Page mode;
  `calculateEffectiveMargins()` returns the pipeline's margins once it is configured; typing with a
  paint is slower in Page mode in the cloud (7.1 vs 4.9 ms/char; on Windows about the same).
- **Stage 4 (rendering)**: the Block/Underline cursor width is measured with the base font (ignores
  zoom and character formats); comment/marker rendering; paint cost against the amount of visible text
  (the first paint after a change in Windows Debug, see the benchmark).
- **Search**: `SearchEngine` does not follow document edits (stale matches); `FindReplaceBar` searches
  on every keystroke and copies all matches (search for "a" in 150k words: 268 ms in the cloud,
  2.6 s in Windows Debug).
- **Leftovers**: `KmlParser` is used only by tests (a second KML parser next to `KmlDocumentModel`);
  `KmlDocumentModel`'s layout and height code (the old view mode) no longer renders anything; the
  `Marker*Command` classes are used only by tests; `EditorRenderPipeline::setConfigDpi()` is unused.
- `ChapterDocument::fromJson()` keeps the statistics stored in the file until the chapter is saved
  again, so old word counts can differ from the editor until then.
- Outside the editor: `main_window.cpp` adds a `statisticsChanged` connection on every
  `documentOpened`; the startup log shows 11 "Icon … not registered" warnings (e.g. `view.mode.*`,
  `file.export.icml`).
- Building on Windows: `scripts\build_windows.bat Release` after a Debug build in the same
  `build-windows` leaves the debug vcpkg DLLs in `bin\` (`zip.dll`, `double-conversion.dll`,
  `hunspell-1.7-0.dll`), because only newer files are copied, and the Release tests stop on a CRT
  error. Switching configurations needs `clean` (or separate build directories).
