# Sub-Project C — Theme System Foundation (Revised Scope) — Design

**Date:** 2026-07-10
**Author:** Architect agent
**Status:** Design (analysis only — no production code written)
**Branch context:** Kalahari cleanup initiative, Sub-Project C

---

## 0. Purpose & how to read this doc

This document designs the "now" work for the Theme System Foundation cleanup. It is
split into workstreams by priority:

- **WS4 — Test infrastructure** (highest priority, lowest risk): a `ResetSingletons()`
  test hook + unit suites for `StyleResolver`, `FormatConverter`, `TrustedKeys`.
- **WS3 — `fprintf` classification** in `src/main.cpp`.
- **WS2 — genuine inline-color violations** (excluding the theme-definition layer).
- **WS1 — EditorColors ↔ ThemeManager unification** is explicitly **DEFERRED** to its own
  design doc; only characterized here.

The original 2026-04-11 audit that motivated this cleanup was **materially inaccurate**.
Section 1 corrects it against the current code before any design is proposed. Every number
and file:line reference below was re-verified directly (Grep/Read) in this session.

---

## 1. Verified-state summary (correcting the 2026-04-11 audit)

| Audit claim (2026-04-11) | Verified reality (2026-07-10) | Verdict |
|---|---|---|
| "322 hardcoded QColor to replace with ArtProvider" | `QColor\s*\(` matches **= 322 across 17 files** — but this count includes default ctors `QColor()`, copy/variable ctors `QColor(existingColor)`, and **function parameters** `const QColor& color`. Literal `(r,g,b)`/hex constructions are only **~167**, and most of *those* are in the theme-definition layer or are legitimate defaults with setters. | **False framing.** The audit ran a naive `QColor(` grep and reported the raw match count as "violations." |
| "editor_appearance.cpp:28-93 must be replaced" (58 occ.) | `src/editor/editor_appearance.cpp` has **66** `QColor(` occurrences, virtually all inside `EditorColors::lightTheme()/darkTheme()/sepiaTheme()` factory methods. **These DEFINE the editor palettes (source of truth).** Replacing them with ArtProvider getters would be circular and wrong. | **False positive. Exclude entirely.** |
| (implied) theme.cpp / theme_manager.cpp / fallback_theme.cpp are violations | `src/core/theme.cpp` (13), `src/core/theme_manager.cpp` (13), `src/core/fallback_theme.cpp` (16) are the **theme-definition layer**. Raw colors are legitimate — they are the fallback/default palette values. | **Exclude entirely.** |
| "settings_dialog.cpp:1079-1388 must be replaced" | `src/gui/settings_dialog.cpp` has **105** `QColor(` occurrences. The large blocks (`~769-812`, `~1079-1388`) are **default values fed into color-editor widgets** ("restore defaults"). They duplicate the theme JSON defaults (a real DRY issue) but the correct fix sources them from the theme/fallback layer, **not** `ArtProvider.getPrimaryColor()`. Only ~2-3 are cleanly-fixable-now violations. | **Mostly WS1-coupled, not ArtProvider swaps.** |
| "13 fprintf in main.cpp → Logger" | `src/main.cpp` has **36** `fprintf` calls. **All 36 are intentional CLI/benchmark stdout/stderr output** (`--cli --get-icon` UX and `--benchmark` machine-readable JSON). **Zero** should become Logger calls. | **False. 0 conversions, not 13.** |
| "test::ResetSingletons() exists" | Does **not** exist anywhere in the tree (confirmed). | **Confirmed absent — must be designed (WS4).** |
| `StyleResolver` / `FormatConverter` / `TrustedKeys` coverage | Source + headers exist and compile (`src/editor/style_resolver.cpp`, `src/editor/format_converter.cpp`, `src/core/trusted_keys.cpp`). **No dedicated test files** exist for any of them. (`FormatConverter` is merely referenced from `tests/editor/test_paragraph_layout.cpp`.) | **Confirmed — must be designed (WS4).** |

**Key insight:** the audit's "322" is literally the naive `QColor(` grep count, and its
"editor_appearance / theme files" targets are the palette *definitions*. The genuine,
cleanly-actionable surface is an order of magnitude smaller than reported.

---

## 2. WS4 — Test infrastructure (highest priority)

### 2.1 `ResetSingletons()` hook

**Goal:** a single call that returns every process-global singleton to a deterministic
baseline between tests, so test ordering never leaks state. Today only
`SettingsManager` is reset (via a Catch2 listener in `tests/test_main.cpp:51-61`).

**Placement:**
- New test-only header/source: `tests/test_support/reset_singletons.h` + `.cpp`
- Namespace: `kalahari::test`
- Signature: `void resetSingletons();`
- **Main-thread only** (documented). Catch2 tests run single-threaded on the QApplication
  thread created in `tests/test_main.cpp:64-67`, so this is safe.

**Wiring:** extend the existing `SettingsResetListener` in `tests/test_main.cpp` into a
`GlobalResetListener` whose `testCaseStarting()` calls `kalahari::test::resetSingletons()`.
This preserves the current pattern (one Catch2 `EventListenerBase`, `CATCH_REGISTER_LISTENER`).

**Per-singleton reset survey (all verified against current headers):**

| Singleton | Namespace | Existing reset path | Thread-safe? | Action needed |
|---|---|---|---|---|
| `SettingsManager` | `core` | `resetToDefaults()` (public, already used by the listener) | Yes (`std::mutex`) | **None** — call it. |
| `TrustedKeys` | `core` | `clear()` — explicitly documented "for testing" (`trusted_keys.h:128`) | Yes (`mutable std::mutex m_mutex`) | **None** — call `clear()`. |
| `CommandRegistry` | `gui` | `clear()` — "Primarily for testing" (`command_registry.h:197-199`); already used in `tests/gui/test_command_registry.cpp:53` | Yes (`mutable std::mutex m_mutex`) | **None** — call `clear()`. Note: `clear()` must also drop cached `QAction*` in `m_actions` (verify during impl). |
| `IconRegistry` | `core` | `resetAllCustomizations()`, `resetSizes()`, `resetTheme()`, `clearCache()` (`icon_registry.h:199-302`) | Main-thread UI object (no mutex) | **None** — compose `resetTheme()` + `resetAllCustomizations()` + `resetSizes()` + `clearCache()`. |
| `ThemeManager` | `core` | `resetColorOverrides()` (`theme_manager.h:76`) + `switchTheme("Light")` | Main-thread `QObject` | **None** — reset overrides, then switch to a known base theme for determinism. |
| `ArtProvider` | `core` | **No public reset.** Has `initialize()`, `setPrimaryColor/Secondary/IconTheme/IconSize` (`art_provider.h`). State lives in `m_iconTheme`, `m_managedActions`, `m_batchMode`, `m_pendingChanges`, `m_customGenreMappings`. | Main-thread `QObject` | **Add** a public `void reset()` documented "for testing" (mirrors the `CommandRegistry::clear()` / `TrustedKeys::clear()` precedent — a public test-only method, **no `friend` needed**). It clears `m_customGenreMappings`, `m_managedActions`, resets batch flags, then calls `initialize()`. |
| `Logger` | `core` | `init()`, `setLevel()`, `flush()` (`logger.h`) — no reset; `addSink()` accumulates sinks. | Yes (spdlog is thread-safe) | **Low priority.** Optionally add a test-only `resetForTesting()` that restores the default level and avoids sink accumulation. Not blocking — Logger is essentially append-only and harmless between tests. |

**Design of `resetSingletons()` (pseudocode, not production code):**

```
namespace kalahari::test {
void resetSingletons() {
    // Order: config first, then derived visual state.
    core::SettingsManager::getInstance().resetToDefaults();
    core::TrustedKeys::getInstance().clear();
    gui::CommandRegistry::getInstance().clear();
    core::IconRegistry::getInstance().resetTheme();
    core::IconRegistry::getInstance().resetAllCustomizations();
    core::IconRegistry::getInstance().resetSizes();
    core::IconRegistry::getInstance().clearCache();
    core::ThemeManager::getInstance().resetColorOverrides();
    core::ArtProvider::getInstance().reset();   // NEW public method
    // Logger: intentionally left as-is (append-only, harmless).
}
}
```

**The only production-code change required by WS4** is the new `ArtProvider::reset()`
method (and optionally `Logger::resetForTesting()`). Everything else is test-only code.
This makes WS4 genuinely low-risk.

### 2.2 Unit suite: `FormatConverter` (do this FIRST — pure, static, no DB)

- **File:** `tests/editor/test_format_converter.cpp`
- **Tags:** `[editor][format][converter]`
- **Deps:** `QApplication` (already provided by `tests/test_main.cpp`) for `QFont`/
  `QTextCharFormat`; construction of `KmlParagraph` from markup (verify `KmlParagraph`
  ctor accepts a markup string, per the header example `KmlParagraph("Hello <b>world</b>")`).
- **Public API under test** (`format_converter.h:37-83`): all static —
  `elementTypeToFormat`, `combineFormats`, `applyElementType`, `buildFormatRanges`.

| # | Case | Assertion |
|---|---|---|
| 1 | `elementTypeToFormat(Bold, base)` | `fontWeight() == QFont::Bold` |
| 2 | `elementTypeToFormat(Italic, base)` | `fontItalic() == true` |
| 3 | `elementTypeToFormat(Underline, base)` | `fontUnderline() == true` |
| 4 | `elementTypeToFormat(Strikethrough, base)` | `fontStrikeOut() == true` |
| 5 | `elementTypeToFormat(Subscript, base)` | `verticalAlignment() == AlignSubScript` and pointSize ≈ base × `SCRIPT_SIZE_FACTOR` (0.7) |
| 6 | `elementTypeToFormat(Superscript, base)` | `verticalAlignment() == AlignSuperScript` and reduced pointSize |
| 7 | base font family/size preserved for non-size attrs (e.g. Bold keeps family) | family/size unchanged |
| 8 | `combineFormats([Bold, Italic], base)` | both bold **and** italic set |
| 9 | `applyElementType(fmt, Bold, base)` then `applyElementType(fmt, Italic, base)` | stacking accumulates (bold+italic) |
| 10 | `buildFormatRanges(plainParagraph, base)` | empty or single default range (document actual) |
| 11 | `buildFormatRanges("a <b>bold</b> c")` | one `FormatRange` with correct `start`/`length` covering "bold", bold set |
| 12 | nested `<b><i>x</i></b>` | range for "x" has bold+italic |
| 13 | multiple siblings `<b>a</b><i>b</i>` | two ranges, correct offsets, no overlap |
| 14 | offsets are character-accurate against the paragraph plain text | `start + length` within text length |

Rationale: static + deterministic + no singleton/DB → highest ROI, validates the KML→Qt
formatting bridge which underpins editor rendering.

### 2.3 Unit suite: `TrustedKeys` (singleton with clean `clear()`)

- **File:** `tests/core/test_trusted_keys.cpp`
- **Tags:** `[core][security][trusted_keys]`
- **Deps:** none beyond Catch2. Each `TEST_CASE` starts with `TrustedKeys::getInstance().clear()`
  (belt-and-suspenders even once `resetSingletons()` lands).
- **Public API** (`trusted_keys.h:64-128`): `loadBuiltinKeys`, `loadUserKeys`, `getPublicKey`,
  `isTrusted`, `getPublisher`, `getAllPublishers`, `addUserKey`, `removeUserKey`,
  `saveUserKeys`, `clear`; free function `trustLevelToString`.

| # | Case | Assertion |
|---|---|---|
| 1 | `getInstance()` identity | same reference twice |
| 2 | `clear()` empties registry | `getAllPublishers().empty()`, `isTrusted(anything)==false` |
| 3 | `addUserKey(id, name, validBase64_32bytes)` | returns true; `isTrusted(id)`; `getPublicKey(id)` has 32 bytes; `getPublisher(id)->trustLevel == User` |
| 4 | `addUserKey` with invalid base64 | returns false; not trusted |
| 5 | `addUserKey` with wrong decoded length (≠32 bytes) | document actual (expected false) |
| 6 | `addUserKey` with empty base64 | returns false |
| 7 | `addUserKey` duplicate id | document actual (replace vs reject) |
| 8 | `removeUserKey(existingUserId)` | returns true; `isTrusted==false` afterward |
| 9 | `removeUserKey(unknown)` | returns false |
| 10 | `removeUserKey` on a **built-in** key (after `loadBuiltinKeys()`) | returns false (built-ins immutable) |
| 11 | `getPublicKey(unknown)` | `std::nullopt` |
| 12 | `getPublisher(unknown)` | `std::nullopt` |
| 13 | `getAllPublishers()` after N adds | size == N |
| 14 | `trustLevelToString(Full/Verified/User)` | distinct, non-empty strings |
| 15 | `loadBuiltinKeys()` | if `resources/keys/trusted_publishers.json` present → count > 0. **Resource-path dependency** — guard the assertion (skip/soft-check) if the file is not resolvable from the test CWD. |

Note the base64 edge cases (padding, embedded whitespace) exercise the private
`base64Decode` indirectly through `addUserKey`.

### 2.4 Unit suite: `StyleResolver` (most complex — needs a DB fixture)

- **File:** `tests/editor/test_style_resolver.cpp`
- **Tags:** `[editor][style][resolver]`
- **Deps:** `QApplication` (fonts/formats); `core::ProjectDatabase` for inheritance tests.
  Follow the DB setup already used by `tests/core/test_project_database.cpp` (in-memory or
  temp-file SQLite).
- **Public API** (`style_resolver.h:129-193`): `setDatabase`, `database`,
  `resolveParagraphStyle`, `resolveCharacterStyle`, `defaultParagraphStyle`,
  `defaultCharacterStyle`, `invalidateCache`, `reloadFromDatabase`, signal `stylesChanged()`.

**Tier A — no database (fast, always runnable):**

| # | Case | Assertion |
|---|---|---|
| 1 | `defaultParagraphStyle()` | family == "Segoe UI", size == 12, non-bold/italic |
| 2 | `defaultCharacterStyle()` | sensible defaults incl. `textColor` black, transparent background |
| 3 | `resolveParagraphStyle("does.not.exist")` with no DB | returns default (per header contract) |
| 4 | `resolveCharacterStyle("does.not.exist")` with no DB | returns default |
| 5 | `ResolvedParagraphStyle::toFont()` | QFont carries family/size/bold/italic/underline |
| 6 | `ResolvedParagraphStyle::toBlockFormat()` | alignment, indents, margins, line-height mapped |
| 7 | `ResolvedParagraphStyle::toCharFormat()` | foreground == `textColor`, font applied |
| 8 | `ResolvedCharacterStyle::toCharFormat()` | foreground + background applied |
| 9 | `setDatabase(nullptr)` then resolve | no crash; returns defaults; `database()==nullptr` |
| 10 | `invalidateCache()` / `reloadFromDatabase()` with null DB | no crash; `stylesChanged()` fires (QSignalSpy) |

**Tier B — with in-memory ProjectDatabase (integration):**

| # | Case | Assertion |
|---|---|---|
| 11 | insert style "base"; resolve it | properties match inserted values |
| 12 | insert "heading1" parent="base"; resolve "heading1" | inherited props from "base" present, child overrides win (`mergeStyles`) |
| 13 | multi-level chain A→B→C | grandparent props flatten into A |
| 14 | **circular** inheritance A→B→A | terminates (the `visited` `QSet` guards it); no hang |
| 15 | resolve twice (cache hit) | identical result; second call does not re-query (spy/behavioral) |
| 16 | modify DB + `invalidateCache()` | subsequent resolve reflects change |
| 17 | character-style inheritance path (`resolveCharacterStyle`) | analogous to paragraph |

Tier A is the low-risk quick win; Tier B validates the actual value of the class
(inheritance + caching + circular protection) and should not be skipped.

### 2.5 WS4 build integration

All three test files + `tests/test_support/reset_singletons.{h,cpp}` must be added to the
test target in `CMakeLists.txt` (follow the existing `tests/**` registration). Verify each
new class links against the already-built `kalahari_core` / editor libs.

---

## 3. WS3 — `fprintf` classification in `src/main.cpp`

**Verified count: 36 `fprintf` calls. Recommended conversions to Logger: ZERO.**

Every call is intentional command-line UX. There are two contexts, both of which
legitimately own stdout/stderr:

1. **`--cli --get-icon` icon downloader** — user-facing progress (stdout) and
   validation/errors (stderr).
2. **`--benchmark` mode** — machine-readable JSON results on **stdout** (explicitly
   *"Output results to stdout for parsing"*, `main.cpp:375`) and operator errors on stderr.
   Converting the JSON block to Logger would corrupt the parseable output.

Diagnostic logging **already exists** alongside most of these via paired `logger.*` calls.

**Full per-line classification (all verified):**

| Line(s) | Stream | Content | Class | Paired `logger.*`? |
|---|---|---|---|---|
| 58 | stderr | CLI download error | CLI-OUTPUT | yes (`error` @57) |
| 122 | stderr | "--get-icon requires a URL" | CLI-OUTPUT | no |
| 127-128 | stderr | "--icon-name is required" + usage | CLI-OUTPUT | no |
| 133 | stderr | "URL must start with http(s)" | CLI-OUTPUT | no |
| 138 | stdout | "Downloading: …" progress | CLI-OUTPUT | yes (`info` @137) |
| 170 | stderr | "Download failed" (timeout path) | CLI-OUTPUT | no |
| 178 | stderr | "Conversion failed: …" | CLI-OUTPUT | yes (`error` @177) |
| 188 | stderr | "Cannot create directory" | CLI-OUTPUT | yes (`error` @187) |
| 198 | stderr | "Cannot write to file" | CLI-OUTPUT | yes (`error` @197) |
| 206 | stdout | "✓ Saved: …" | CLI-OUTPUT | yes (`info` @205) |
| 213-216 | stderr | CLI usage/help block | CLI-OUTPUT | no |
| 267 | stderr | benchmark: no `.klh` manifest | CLI-OUTPUT | yes (`error` @266) |
| 277 | stderr | benchmark: failed to open project | CLI-OUTPUT | yes (`error` @276) |
| 356-357 | stderr | benchmark: no active editor + usage | CLI-OUTPUT | yes (`error` @355) |
| 376-397 | stdout | benchmark: JSON results block | CLI-OUTPUT (machine-readable) | — must NOT convert |

**Recommendation:**
- **Convert nothing.** This refutes the audit's "13 → Logger."
- **Optional, non-blocking enhancement:** add a paired `logger.warn/error(...)` for the six
  CLI validation/usage failures that currently log nothing (lines 122, 127-128, 133, 170,
  213-216) so the log file also records why a scripted CLI run failed. The `fprintf` stays;
  a Logger line is *added* beside it. This is a quality nicety, not a required change.

---

## 4. WS2 — genuine inline-color violations

**Exclusions (theme-definition layer — raw colors are the source of truth, do NOT touch):**
`src/editor/editor_appearance.cpp` (66), `src/core/theme.cpp` (13),
`src/core/theme_manager.cpp` (13), `src/core/fallback_theme.cpp` (16).

Per-file analysis of the remaining WS2 targets (all verified):

### 4.1 `src/gui/widgets/standalone_info_bar.cpp` — **GENUINE violation (highest-value fix)**
- Lines **118-123**: hardcoded cream/amber accent colors selected by a manual
  `theme.palette.window.lightness() > 128` luminance branch:
  `QColor(255,248,225)`, `QColor(255,213,79)` (light) / `QColor(62,50,30)`, `QColor(120,94,30)` (dark).
- These are semantic UI accent colors (info/warning banner) that are **not** in the theme
  system, so they don't follow user theme customization.
- **Recommendation:** add themed colors via `scripts/add_theme_color.py` (e.g.
  `infoBarBackground`, `infoBarBorder` with dark+light values), then read
  `theme.infoBarBackground` / `theme.infoBarBorder` and delete the luminance branch.
- **True violations: ~4 literals → 2 logical theme colors.** Clean to fix now.

### 4.2 `src/gui/settings_dialog.cpp` — **mostly WS1-coupled, ~2-3 fixable now**
- Lines **769-812** and **1079-1388**: default color values pushed into color-editor
  widgets (the "restore defaults" flow). These **duplicate** the Dark/Light theme JSON
  defaults. Real DRY problem, but the correct fix is to source defaults from the
  theme/fallback layer — coupled to **WS1**, not an `ArtProvider.getPrimaryColor()` swap.
- Line **230**: `QColor(Qt::gray)` for a disabled tree item foreground → should use a
  theme palette disabled/mid color. Minor **genuine** violation.
- Lines **1846-1847**: `QColor("#424242")`, `QColor("#757575")` fallback when the color
  widget is null → these are the ArtProvider primary/secondary defaults hardcoded; minor
  **genuine** violation (could read `ArtProvider::getPrimaryColor()`/`getSecondaryColor()`).
- **True cleanly-fixable-now violations: ~2-3** (line 230 + 1846-1847). The big default
  blocks (~40 literals) are **deferred with WS1**.

### 4.3 `src/core/icon_registry.cpp` — **NOT a violation (color-infrastructure layer)**
- Lines **78-82**: `QColor("#000000")`/`QColor(0,0,0)` are **diagnostic sentinels**
  (compare-against-black to warn "theme not loaded"), not UI colors.
- Lines **449-450, 715, 721**: `QColor("#333333")`/`QColor("#999999")` fallback defaults,
  explicitly commented "Match ThemeManager Light default." IconRegistry is the color engine
  that `ArtProvider` *delegates to* — routing it through `ArtProvider` would be circular.
- **True violations: 0. Exclude** (treat like the theme-definition layer).

### 4.4 `src/editor/tag_detector.cpp` — **semantic palette, defer**
- Lines **148-164**: `colorForType()` returns fixed category colors (Todo=orange,
  Fix=red, Check=blue, Note=green, Warning=yellow, default=gray).
- These are semantic markers (like syntax highlighting). Candidates for a future
  user-customizable/themeable "tag palette," but not a strict ArtProvider violation.
- **True violations now: 0.** Note as optional future themeable-tag-palette feature.

### 4.5 `src/editor/paragraph_layout.cpp` — **semantic decorations + minor selection default**
- Lines **14-15**: `DEFAULT_SELECTION_BG(0x30,0x8C,0xC6)` / `DEFAULT_SELECTION_FG(Qt::white)`
  — defaults overridden by `setSelectionColors()`. Ideally seeded from
  `theme.palette.highlight` / `highlightedText` at the **call site**.
- Line **724**: `QColor(Qt::red)` spell-error wavy underline — semantic (conventional red).
- Lines **742-751**: grammar-error colors by type (blue/green/gray) — semantic category
  colors, akin to syntax highlighting.
- **True violations now: ~0-2** (only the selection defaults, and only if we wire the
  caller to pass theme palette colors). Spell/grammar colors are semantic; optionally
  themeable later.

### 4.6 `src/editor/table_layout.cpp` — **defaults with setters; dark-theme correctness gap**
- Lines **35-40, 119-124, 205-210**: default border/background/header/text colors
  (light gray / white / black) in the constructor and move operations. The class exposes
  full setters (`setBorderColor`, `setBackgroundColor`, `setTextColor`, …), so real colors
  are injected by the caller.
- Hardcoded white/black defaults mean tables render wrong under a dark theme **if** the
  caller doesn't override. The real fix is ensuring the caller passes `theme.palette`
  colors, not editing these fallbacks.
- **True violations now: 0 strict** (fallback defaults). Flag the dark-theme correctness
  gap as a follow-up at the construction/caller layer.

### 4.7 WS2 realistic totals

| File | `QColor(` occ. | TRUE fixable-now violations | Disposition |
|---|---|---|---|
| standalone_info_bar.cpp | 4 | ~4 lit → 2 theme colors | **Fix now** (add_theme_color.py) |
| settings_dialog.cpp | 105 | ~2-3 | Fix the 2-3; defer default blocks to WS1 |
| icon_registry.cpp | 6 | 0 | Exclude (color infrastructure) |
| tag_detector.cpp | 6 | 0 | Defer (optional themeable tag palette) |
| paragraph_layout.cpp | 4 | 0-2 | Wire selection colors from theme (caller) |
| table_layout.cpp | 6 | 0 | Follow-up: caller passes theme palette |

**Bottom line:** the realistic count of TRUE, cleanly-actionable-now violations is
**~6-9 literals** (dominated by `standalone_info_bar.cpp`), *not* 322. The remainder is
either theme-definition source-of-truth, semantic/syntax-style colors, injected defaults
with setters, or WS1-coupled default duplication.

---

## 5. WS1 — EditorColors ↔ ThemeManager unification (DEFERRED)

Characterization only; **not designed here** — it gets its own design doc.

Kalahari currently runs **two parallel color systems**:

1. **Core `Theme` / `ThemeManager`** — JSON themes in `resources/themes/*.json` (Dark, Light),
   the `Theme` struct (`theme.colors`, `theme.palette`, `theme.log`, UI colors), user
   overrides via `setColorOverride()`/`applyColorOverrides()`, applied app-wide as `QPalette`
   + stylesheet. Source of truth: `resources/themes/*.json` + `theme.cpp` + `fallback_theme.cpp`.
2. **Editor `EditorColors`** — `editor_appearance.cpp` factory methods
   `lightTheme()/darkTheme()/sepiaTheme()` defining editor-specific colors (background, text,
   inactive text, cursor, selection, etc.) consumed by `BookEditor` rendering. **Independent
   of `ThemeManager`** and not user-driven through the same surface.

**Symptoms of the split** (visible in this doc): the settings_dialog "editor colors"
default blocks (§4.2, lines 1079-1388) and the table/paragraph decoration defaults (§4.5-4.6)
exist precisely because editor colors are a separate island.

**Deferred work (future doc):** make `EditorColors` derive from / register with
`ThemeManager` so there is a single source of truth, one settings surface, and consistent
dark/light behavior. **Do not attempt in Sub-Project C** — it is real architecture with
migration and settings-schema implications.

---

## 6. Recommended implementation sequence

1. **WS4.1 — `ResetSingletons()`** + `ArtProvider::reset()` (only production change) →
   wire into the Catch2 listener. Lowest risk, unblocks clean test isolation.
2. **WS4.2 — `FormatConverter` tests** (pure/static; no new prod code).
3. **WS4.3 — `TrustedKeys` tests** (uses `clear()`; watch the resource-path guard).
4. **WS4.4 — `StyleResolver` tests** (Tier A first, then Tier B with the DB fixture).
5. **WS2 — `standalone_info_bar.cpp`**: add `infoBarBackground`/`infoBarBorder` via
   `scripts/add_theme_color.py`, replace the luminance branch. Then the settings_dialog
   micro-fixes (line 230, 1846-1847).
6. **WS3 — optional:** add paired `logger.*` beside the six unlogged CLI failures. Convert
   no `fprintf`.
7. **WS1 — do NOT start here.** Open a separate design doc.

Each step is independently shippable; steps 1-4 (WS4) are the priority and carry almost no
regression risk.

---

## 7. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| `ArtProvider::reset()` re-`initialize()` has side effects (e.g. re-emits `resourcesChanged`) | Test flakiness | Keep `reset()` minimal; clear collections + flags, then `initialize()`; verify no dangling `QAction*` in `m_managedActions`. |
| `CommandRegistry::clear()` may not free cached `QAction*` (`m_actions`) | Leaks / stale actions across tests | Confirm `clear()` also empties `m_actions` during impl; extend if not. |
| `TrustedKeys::loadBuiltinKeys()` depends on `resources/keys/trusted_publishers.json` relative to test CWD | Test fails on some runners | Guard the count assertion; treat as soft/integration check. |
| `StyleResolver` Tier B needs SQLite/ProjectDatabase fixture | Setup complexity | Reuse the pattern from `tests/core/test_project_database.cpp`; keep Tier A independent so core coverage lands regardless. |
| `KmlParagraph` markup-string ctor assumption (FormatConverter tests) | Tests won't compile | Verify `KmlParagraph` construction API during impl; adjust fixture (build paragraph programmatically if needed). |
| Adding theme colors touches 5-9 files | Broad change surface | Use `scripts/add_theme_color.py` (mandated) — never hand-edit the theme files. |
| WS2 `settings_dialog` default blocks tempt over-reach into WS1 | Scope creep | Hard stop: only fix line 230 + 1846-1847 now; defaults stay until WS1. |

---

## 8. Test / verification strategy

- **Build:** `scripts/build_windows.bat Debug` (never cmake directly). Confirm the test
  target compiles the three new suites + `test_support`.
- **Run:** `./build-windows/bin/kalahari-tests.exe`. Baseline ~603 test cases (2026-04-11);
  the new suites must **add** cases with all green, and the existing suite must remain green
  (proves `ResetSingletons()` doesn't destabilize ordering).
- **Isolation check:** run the suite, then run it again with Catch2 `--order rand` to prove
  no inter-test state leakage now that `resetSingletons()` runs per-test.
- **WS2 verification:** after adding info-bar theme colors, `/verify`-drive the app —
  toggle Dark/Light and confirm the standalone info bar re-themes and no hardcoded cream/amber
  remains under dark theme.
- **WS3 verification:** run `kalahari --cli --get-icon …` and `kalahari --benchmark …` and
  confirm stdout/stderr output is unchanged (benchmark JSON still parseable). No behavioral
  change expected since nothing is converted.
- **Review gate:** `code-reviewer` / `/code-review` must pass before merge; enforce
  CLAUDE.md mandatory patterns (`tr()` for any new UI strings, `ArtProvider`/`ThemeManager`
  for colors, `add_theme_color.py` for theme colors, English-only code/comments).

---

## 9. Files touched (summary for the caller)

**New (test-only):**
- `tests/test_support/reset_singletons.h`
- `tests/test_support/reset_singletons.cpp`
- `tests/editor/test_format_converter.cpp`
- `tests/core/test_trusted_keys.cpp`
- `tests/editor/test_style_resolver.cpp`

**Modify (production):**
- `include/kalahari/core/art_provider.h` + `src/core/art_provider.cpp` — add test-only `reset()`
- (optional) `include/kalahari/core/logger.h` + `src/core/logger.cpp` — `resetForTesting()`
- `tests/test_main.cpp` — extend listener to call `kalahari::test::resetSingletons()`
- `CMakeLists.txt` — register new test sources
- `resources/themes/Dark.json`, `resources/themes/Light.json`, `theme.h`, `theme.cpp`,
  `theme_manager.cpp` — via `scripts/add_theme_color.py` for `infoBarBackground`/`infoBarBorder`
- `src/gui/widgets/standalone_info_bar.cpp` — use new theme colors, drop luminance branch
- `src/gui/settings_dialog.cpp` — micro-fixes at line 230 and 1846-1847 only
- (optional, WS3) `src/main.cpp` — add paired `logger.*` beside six unlogged CLI failures;
  convert no `fprintf`

**Explicitly NOT touched:** `editor_appearance.cpp`, `theme.cpp` (except the scripted color
add), `theme_manager.cpp` (except scripted add), `fallback_theme.cpp`, `icon_registry.cpp`,
`tag_detector.cpp`, `table_layout.cpp` (WS1/follow-up), and the settings_dialog default
blocks (WS1).
