# Sub-Project A: Repo Hygiene Cleanup — Design Document

## Goal

Mechanical cleanup of tracked git artifacts, dead wxWidgets-era files, and orphan architecture directories — plus porting the last remaining active wxWidgets code (`GuiLogSink`) to Qt. After this sub-project, no active wxWidgets code exists in `src/`, the repo root is free of stale build outputs, and empty architectural placeholders from an incomplete MVP migration are gone.

This is **Sub-Project A** of a four-part cleanup initiative identified in the 2026-04-11 professional audit. The audit surfaced ~32 action items grouped into four independent sub-projects; A is the safety-first starting point because it is mostly mechanical (file/git operations) with only one real code change.

## Position in Overall Initiative

```
A: Repo Hygiene Cleanup         ← THIS SPEC
   ↓
D: CI Hardening & Build         (enables safety net for B)
   ↓
C: Theme System Foundation      (unblocks Phase F of Text Styling)
   ↓
B: BookEditor Refactor          (largest risk, best safety net)
```

Rationale for order: each sub-project increases the safety net for the next. A produces a clean working directory; D produces CI enforcement; C produces the theme foundation; B is the risky refactor executed under maximum safety.

## Goals / Non-Goals

### Goals
- Remove all tracked build artifacts and VS cache from the git index
- Eliminate all remaining wxWidgets traces in `src/` (code, includes, doc comments)
- Remove empty architectural placeholders that no longer represent active intent
- Eliminate wxWidgets-era dead test files
- Align `.clang-format` include priorities with the Qt reality
- Document all changes under `CHANGELOG.md [Unreleased]`

### Non-Goals
- **Git history rewrite** — `git rm --cached` only; repo physical size reduction (via `git filter-repo` / BFG) is a deferred decision requiring force-push, explicitly out of scope here
- **Linux / macOS build verification** — primary verification is Windows; Linux/macOS are addressed in Sub-Project D
- **Fixing PCH activation, CI linting, sanitizers** — all Sub-Project D scope
- **Replacing hardcoded `QColor(r,g,b)` values** — Sub-Project C scope
- **`BookEditor` / `DocumentCoordinator` refactor** — Sub-Project B scope
- **Renaming or restructuring `docs/` vs `project_docs/`** — documentation cleanup deferred to a future cycle

## Scope — Six Groups of Changes

### Group 1: Untrack build/cache artifacts (zero risk)
Remove from git index (files remain on disk per `.gitignore`):
- `.vs/` (4.1 GB — `slnx.sqlite` 261 MB, `cmake.db` 119 MB)
- `build-windows/`, `build-vs2026/`, `build-linux-vmware/`
- `compile_commands.json` (537 KB, CMake generated)
- `kalahari.log`, `ninja_output.txt`
- `nul` (Windows `> NUL` redirect artifact)
- `temp_build.bat`
- `.vscode-ai-assistant/`

### Group 2: Remove wxWidgets legacy libraries (~48 MB)
Physical deletion of stale vcpkg-era `bwx_sdk` binaries:
- `lib/x64-windows/bwx_core.lib`, `bwx_gui.lib`
- `lib/x64-linux/Debug/libbwx_core.a`, `libbwx_gui.a`
- `lib/x64-linux/Release/libbwx_core.a`, `libbwx_gui.a`

Verified (during brainstorming) not referenced by any `CMakeLists.txt` — these are pure vcpkg artifacts from the pre-Qt6 era.

### Group 3: Remove wxFormBuilder legacy concept files (~392 KB)
Physical deletion of `concept_files/wxFormBuilder/`:
- `Kalahari.fbp` (wxFormBuilder project file, 85 KB)
- 3 GUI concept JPG screenshots
- GUI concepts text notes

These describe wxWidgets-era UI layouts superseded by Qt6 `QDockWidget` architecture.

### Group 4: Remove orphan architecture directories (decision-driven)
Empty placeholders from an incomplete MVP migration:
- `src/presenters/`, `src/services/`
- `include/kalahari/presenters/`, `include/kalahari/services/`
- `external/` (never populated — vcpkg submodule lives at `/vcpkg/`)
- `bin/x64-windows/`, `bin/x64-linux/{Debug,Release}/` (empty output dirs)

**Decision**: Delete now. The project uses the Coordinator pattern (`DockCoordinator`, `DocumentCoordinator`, etc.) rather than MVP. If a service layer is needed later, it will be reintroduced as a deliberate design decision rather than carried forward as ghost intent.

### Group 5: Port `GuiLogSink` from wxWidgets to Qt (real code change)
The last active wxWidgets code in `src/`. See **Section: GuiLogSink Qt Port Design** below.

### Group 6: Config housekeeping and documentation
- `.clang-format`: replace `'^<wx/'` include priority with `'^<(Q|qt)/'`
- Remove dead test files:
  - `tests/gui/test_bwx_text_document.cpp`
  - `tests/gui/test_bwx_text_renderer.cpp`
  - Clean up any matching references in the test `CMakeLists.txt` (exact path — `tests/CMakeLists.txt` or `tests/gui/CMakeLists.txt` — verified during implementation via `grep -rn "test_bwx" tests/`)
- Update `src/README.md` if it references wxWidgets
- Add CHANGELOG entry under `[Unreleased]`

## Approach: 7 Atomic Commits on Feature Branch

### Branch Strategy
- Safety net branches created before work starts:
  - `backup/pre-cleanup-hygiene` — pointer to main HEAD before starting
  - `wip/framework-migration` — snapshot of current uncommitted changes (OpenSpec→Superpowers WIP in working directory)
- Working branch: `cleanup/repo-hygiene` (feature branch, not pushed until all commits complete and verified)
- `git rm --cached` only for Group 1 — no history rewrite, no force-push

### Commit Sequence

| # | Commit | Type | Build Verify |
|---|---|---|---|
| 1 | `chore(repo): untrack VS cache and build artifacts` | Git ops only | No |
| 2 | `chore(repo): remove wxWidgets legacy libraries` | Physical delete | No |
| 3 | `chore(repo): remove wxFormBuilder legacy files` | Physical delete | No |
| 4 | `refactor(arch): remove orphan presenters/services directories` | Physical delete | **Yes** |
| 5 | `refactor(core): port GuiLogSink from wxWidgets to Qt` | Code change | **Yes + smoke test** |
| 6 | `chore(tests): remove dead wxWidgets test files` | Delete + CMake | **Yes (test build)** |
| 7 | `chore: finalize repo hygiene cleanup` | Config + docs | No |

## GuiLogSink Qt Port Design

### Current State (`src/core/gui_log_sink.cpp`)

`GuiLogSink` is a template class (`GuiLogSinkImpl<Mutex>`) that forwards spdlog messages to a `LogPanel` widget. Template methods are defined in the `.cpp` with explicit instantiation at the bottom (for `std::mutex` and `spdlog::details::null_mutex`). `LogPanel` is a `QWidget` (verified: `include/kalahari/gui/panels/log_panel.h:46` — `class LogPanel : public QWidget`).

Currently uses `wxTheApp->CallAfter([panel, log_message]() { ... })` at two call sites (`sink_it_()` for forwarding, `setPanel()` for backfill). Each lambda checks `panel && !panel->IsBeingDeleted()` before calling `panel->appendLog()`.

### Qt Replacement

**Core idea**: use `QMetaObject::invokeMethod(panel, lambda, Qt::QueuedConnection)` with `panel` (a `QObject`-derived type) as the target. When `Qt::QueuedConnection` is specified, Qt posts the lambda to the target's event loop. If the target is destroyed before event delivery, Qt silently drops the invocation — semantically equivalent to `wxTheApp`'s shutdown-safe behavior plus `IsBeingDeleted` check, in one atomic operation.

### `src/core/gui_log_sink.cpp` changes

**Includes (lines 6-8)**:
```diff
 #include <kalahari/core/gui_log_sink.h>
 #include <kalahari/gui/panels/log_panel.h>
-#include <wx/app.h>
 #include <spdlog/details/fmt_helper.h>
 #include <spdlog/pattern_formatter.h>
+#include <QMetaObject>
+#include <QCoreApplication>
```

**`sink_it_()` (lines 44-57)**:
```diff
-    // Marshal to main GUI thread using wxTheApp->CallAfter()
-    // This is thread-safe - wxWidgets queues the lambda for execution on main thread
-    if (wxTheApp) {
-        // Capture raw pointer (safe: CallAfter won't execute if app is shutting down)
-        gui::LogPanel* panel = m_panel;
-
-        wxTheApp->CallAfter([panel, log_message]() {
-            // On main thread: check if panel is still valid
-            // wxWidgets handles deleted windows gracefully
-            if (panel && !panel->IsBeingDeleted()) {
-                panel->appendLog(log_message);
-            }
-        });
-    }
+    // Marshal to main GUI thread via Qt event loop.
+    // QMetaObject::invokeMethod with panel as target + Qt::QueuedConnection
+    // posts the lambda to the panel's event loop (main thread). If the panel
+    // is destroyed before the event is processed, Qt drops the invocation
+    // silently (no IsBeingDeleted equivalent needed).
+    if (qApp) {
+        gui::LogPanel* panel = m_panel;
+        QMetaObject::invokeMethod(
+            panel,
+            [panel, log_message]() {
+                panel->appendLog(log_message);
+            },
+            Qt::QueuedConnection);
+    }
```

**`setPanel()` backfill path (lines 86-92)**: analogous change. Replace `wxTheApp->CallAfter([panel, history]() { ... })` with `QMetaObject::invokeMethod(panel, [panel, history]() { ... }, Qt::QueuedConnection)`. Drop the `!panel->IsBeingDeleted()` check. Keep the `qApp` null check for defensive shutdown handling.

### `include/kalahari/core/gui_log_sink.h` changes

Doc comment updates only — no include changes needed (the header contains no wx/Qt API calls, only declarations):

- Line 4-5: replace "Uses `wxTheApp->CallAfter()` to marshal GUI calls to main thread" with "Uses `QMetaObject::invokeMethod` with `Qt::QueuedConnection` to marshal GUI calls to main thread"
- Lines 38-45 (Thread Safety / Lifetime Safety blocks): replace `wxTheApp->CallAfter()` references with `QMetaObject::invokeMethod`; replace `wxAUI/MainWindow` with `QMainWindow / QDockWidget parent`; drop the `IsBeingDeleted` lifetime explanation
- Line 93: `"Raw pointer to LogPanel (owned by wxWidgets, not ref-counted)"` → `"Raw pointer to LogPanel (owned by Qt parent, not ref-counted)"`

### Safety Analysis

1. **`m_panel == nullptr` case**: both call sites (`sink_it_()` line 32, `setPanel()` line 70) already have early-return guards before reaching any marshalling code. `QMetaObject::invokeMethod` will therefore never be called with a null target. Safe.

2. **`qApp` null during logging**: kept as defensive guard mirroring the original `wxTheApp` null check — handles the edge case of very-early startup or late-shutdown logging.

3. **Template instantiation unchanged**: explicit instantiations (`template class GuiLogSinkImpl<std::mutex>;` and `<null_mutex>;`) remain at the bottom of `gui_log_sink.cpp`. Port is fully contained in `.cpp`.

4. **Qt version**: `QMetaObject::invokeMethod` with functor and explicit connection type requires Qt 5.10+. Kalahari requires Qt 6.5+. Safe.

5. **Include reach**: `QMetaObject` is transitively available via `<QWidget>` (included through `log_panel.h`), but we add explicit `#include <QMetaObject>` for clarity of intent.

## Verification Plan

| # | Commit | Verification | Build |
|---|---|---|---|
| 1 | Untrack artifacts | `git ls-files \| grep -E "\.vs/\|build-\|compile_commands\|kalahari\.log\|nul$\|temp_build\|ninja_output\|\.vscode-ai-assistant"` → empty | No |
| 2 | Remove bwx libs | `grep -rn "bwx_core\|bwx_gui" CMakeLists.txt src/CMakeLists.txt cmake/ vcpkg.json` → empty | No |
| 3 | Remove wxFormBuilder | `grep -rn "wxFormBuilder\|Kalahari\.fbp" .` → empty (excluding `.git`) | No |
| 4 | Remove orphan dirs | (a) grep `presenters\|services\|external` in `CMakeLists.txt` family → no `add_subdirectory` references; (b) full build `scripts/build_windows.bat Debug` | Yes |
| 5 | Port GuiLogSink | (a) full build; (b) `./build-windows/bin/kalahari-tests.exe` passing; (c) **manual smoke test**: run `kalahari.exe`; ensure LogPanel dock is visible (open via View menu if hidden); perform log-generating actions in this order — open an example project from `examples/` (generates project-load logs), switch theme in settings (generates theme-change logs), close/reopen document; after each action, **visually confirm new log lines appear in LogPanel** with timestamps matching the action | Yes + smoke |
| 6 | Remove dead tests | (a) `grep -rn "test_bwx" tests/` → empty; (b) test build rebuild kalahari-tests | Yes (test build) |
| 7 | Config + docs | (a) YAML parse `.clang-format`; (b) clang-format dry-run on sample file; (c) visual check CHANGELOG | No |

**Build time estimate**: ~10 min per full build × 3 full builds (commits 4, 5, 6) = ~30 min build time. Test run time ~2-5 min. Manual smoke test ~5 min. Total verification overhead: ~40-45 min on top of implementation time.

## Rollback Strategy

### Pre-work: uncommitted changes handling

The working directory at the start of work contains extensive uncommitted changes (framework migration WIP from OpenSpec→Superpowers). These are out of scope for Sub-Project A but must not be mixed into the feature branch.

**Resolution**: Snapshot to `wip/framework-migration` branch before starting:

```bash
git checkout -b wip/framework-migration
git add -A
git commit -m "WIP: snapshot of framework migration pre-cleanup state"
git checkout main                       # main is now clean
git branch backup/pre-cleanup-hygiene   # safety net
git checkout -b cleanup/repo-hygiene    # start the real work
```

After Sub-Project A merges (or is abandoned), the user decides what to do with `wip/framework-migration`: rebase onto the updated main, cherry-pick selectively, or reset.

### Per-commit rollback (during A)

- Static verification fails (commits 1-3, 7): fix inline before `git commit` — never commit broken state
- Build fail in commit 4 or 5: `git commit --amend` after fix (if no subsequent commits exist) — merges fix into the same commit
- Build OK but problem discovered later: `git revert <sha>` on feature branch — adds a reverting commit instead of rewriting history
- Want to discard commit 5 from feature branch history (before any push): `git reset --hard HEAD~1` — destructive but safe on unpushed branch

**Rule**: on feature branch before first push, `reset --hard` is allowed. After first push, only `revert`.

### Full Sub-Project A rollback

- Abort during work: `git checkout main && git branch -D cleanup/repo-hygiene`
- Restore post-merge regression: `git reset --hard backup/pre-cleanup-hygiene`

### Commit 5 smoke test failure (E1 escalation)

Manual smoke test is the only non-statically-verifiable check. If log messages do not appear in LogPanel after the port, escalation process:

1. **First fix attempt** (30 min budget): typical causes — `qApp` unexpectedly null in spdlog background thread context; missing include; wrong connection type. Use `git commit --amend` if fix found.
2. **Second attempt**: instrumented debugging — temporary `qDebug()` around `invokeMethod`, `QT_LOGGING_RULES` verbose, verify lambda actually posts.
3. **Third failure → escalation decision**:
   - **E1 (preferred)**: `git reset --hard HEAD~1` to drop commit 5, continue commits 6 and 7. Sub-Project A ships with 6 commits. `GuiLogSink` port becomes a standalone follow-up task with its own brainstorm cycle (~1 day).
   - **E2 (nuclear)**: abort all of A, full rollback to `backup/pre-cleanup-hygiene`.

Pre-decision: **E1**. The rest of Sub-Project A has independent value and should not suffer for one difficult port. Final decision made in the moment if failure occurs.

## Commit Message Templates

### Commit 1
```
chore(repo): untrack VS cache and build artifacts

Remove tracked files that should have been ignored per .gitignore:
- .vs/ (VS cache, 4.1 GB with slnx.sqlite + cmake.db)
- build-windows/, build-vs2026/, build-linux-vmware/ (build outputs)
- compile_commands.json (CMake generated, 537 KB)
- kalahari.log, ninja_output.txt (build/runtime logs)
- nul (Windows NUL redirect artifact)
- temp_build.bat (stale helper script)
- .vscode-ai-assistant/ (deprecated editor integration)

Uses git rm --cached only — files remain on disk.
Git history not rewritten; repo size reduction via filter-repo/BFG
is a deferred decision requiring force-push.
```

### Commit 2
```
chore(repo): remove wxWidgets legacy libraries

Delete stale vcpkg-era bwx_sdk binaries (48 MB total):
- lib/x64-windows/bwx_core.lib, bwx_gui.lib
- lib/x64-linux/Debug/libbwx_core.a, libbwx_gui.a
- lib/x64-linux/Release/libbwx_core.a, libbwx_gui.a

These libraries were part of the wxWidgets/bwx_sdk stack replaced
by Qt6 in November 2025. Verified not referenced by any CMakeLists.txt.
```

### Commit 3
```
chore(repo): remove wxFormBuilder legacy concept files

Delete concept_files/wxFormBuilder/ (392 KB):
- Kalahari.fbp (wxFormBuilder project file)
- 3 GUI concept JPG screenshots
- GUI concepts text notes

These files describe wxWidgets-era UI layouts that were superseded
by Qt6 QDockWidget-based architecture.
```

### Commit 4
```
refactor(arch): remove orphan presenters/services directories

Delete empty architectural placeholders left from an incomplete
MVP pattern migration:
- src/presenters/, src/services/
- include/kalahari/presenters/, include/kalahari/services/
- external/ (never populated; vcpkg submodule lives at /vcpkg/)
- bin/x64-windows/, bin/x64-linux/{Debug,Release}/ (empty output dirs)

The project uses the Coordinator pattern (DockCoordinator,
DocumentCoordinator, etc.) rather than MVP. If a service layer
is needed later, it will be reintroduced as a deliberate decision.
```

### Commit 5
```
refactor(core): port GuiLogSink from wxWidgets to Qt

Replace wxTheApp->CallAfter() with QMetaObject::invokeMethod and
Qt::QueuedConnection for marshalling log messages to the main GUI
thread.

- Remove #include <wx/app.h>
- Add #include <QMetaObject>, #include <QCoreApplication>
- sink_it_(): post lambda to LogPanel event loop
- setPanel(): same change for backfill path
- Drop panel->IsBeingDeleted() check (Qt handles deleted targets)
- Update header doc comments (wxTheApp → QMetaObject::invokeMethod,
  wxAUI → QMainWindow/QDockWidget)

This is the last active wxWidgets code in src/. After this commit,
the Qt6 migration is complete in all production code.

Verified via manual smoke test: log messages appear in LogPanel
after the port.
```

### Commit 6
```
chore(tests): remove dead wxWidgets test files

Delete orphaned test files from the wxWidgets era:
- tests/gui/test_bwx_text_document.cpp
- tests/gui/test_bwx_text_renderer.cpp

These tests were disabled in tests/gui/CMakeLists.txt with a comment
noting GTK/X11 display dependency. The bwxTextEditor code they
tested was replaced by the KML-based Qt editor (OpenSpec #00042,
deployed 2026-02-18).

Clean up any matching references in the test CMakeLists.txt (exact
path verified during implementation via grep).
```

### Commit 7
```
chore: finalize repo hygiene cleanup

- .clang-format: replace wxWidgets include priority with Qt:
    '^<wx/' → '^<(Q|qt)/' in IncludeCategories
- src/README.md: update legacy wxWidgets references to Qt6
- CHANGELOG.md: add entry under [Unreleased] documenting
  the cleanup sub-project
```

## CHANGELOG Entry

Added under `## [Unreleased]`:

```markdown
### Changed
- **Repo Hygiene:** Sub-Project A — Cleanup & wxWidgets Finalization (2026-04-11)
  - Untracked `.vs/`, `build-*/`, `compile_commands.json`, and other build
    artifacts from git index (files remain on disk per .gitignore; history
    rewrite deferred)
  - Removed wxWidgets legacy libraries (`lib/x64-*/bwx_*.{lib,a}`, 48 MB total)
  - Removed `concept_files/wxFormBuilder/` (wxWidgets-era UI concepts)
  - Removed orphan `src/presenters/`, `src/services/`, `include/kalahari/presenters/`,
    `include/kalahari/services/`, `external/` directories (empty architectural
    placeholders from incomplete MVP migration)
  - Ported `GuiLogSink` from `wxTheApp->CallAfter()` to
    `QMetaObject::invokeMethod(panel, ..., Qt::QueuedConnection)` —
    **last active wxWidgets code in src/ eliminated**
  - Removed dead `tests/gui/test_bwx_text_*.cpp` files
  - Updated `.clang-format` include priority from wxWidgets to Qt
  - Updated `src/README.md` wxWidgets references to Qt6

### Removed
- wxWidgets legacy libraries and artifacts (see "Repo Hygiene" above for full list)
```

## Success Criteria

Sub-Project A is **done** when all of the following are ✅:

1. All 7 commits land on `cleanup/repo-hygiene` (or 6 if E1 escalation — commit 5 dropped)
2. `git ls-files | grep -E "\.vs/|build-windows/|build-vs2026/|build-linux-vmware/|compile_commands\.json|kalahari\.log|nul$|temp_build\.bat|ninja_output\.txt|\.vscode-ai-assistant/|lib/.*bwx_|concept_files/wxFormBuilder|src/presenters|src/services|include/kalahari/presenters|include/kalahari/services|external/|test_bwx_"` returns empty output
3. `grep -rn "wxTheApp\|wx/app\.h\|#include <wx" src/ include/kalahari/` returns empty — no active wxWidgets includes or API calls remain. Occurrences of the literal string "wxWidgets" in rationale/history comments are permitted (e.g., "ported from wxWidgets in 2026-04-11")
4. `scripts/build_windows.bat Debug` passes without errors and without cleanup-related warnings
5. `./build-windows/bin/kalahari-tests.exe` — 100% pass rate (same count as pre-A baseline, not fewer)
6. Manual smoke test of `kalahari.exe`: log messages appear in LogPanel after the port (or documented as known issue if E1 escalation)
7. `.clang-format` validates (YAML parseable, dry-run passes)
8. `CHANGELOG.md` has entry under `[Unreleased]`
9. `wip/framework-migration` branch exists with pre-work snapshot
10. `backup/pre-cleanup-hygiene` branch exists as safety net

After 1-10 are green, the user is asked for explicit approval to merge the feature branch into main. **Merge is not part of Sub-Project A** — it is a separate conscious decision after review of the full diff.

## Out of Scope

- Git history rewrite (deferred; separate cycle with force-push implications)
- Linux / macOS build verification (Sub-Project D)
- PCH activation (Sub-Project D)
- CI linting (clang-format/clang-tidy/ASAN/UBSAN) enforcement (Sub-Project D)
- Hardcoded `QColor(r,g,b)` replacement (Sub-Project C)
- `BookEditor` / `DocumentCoordinator` refactor (Sub-Project B)
- `docs/` vs `project_docs/` consolidation (future cycle)
- Replacing `fprintf` with Logger in `main.cpp` (fits better in Sub-Project C or D)
- `plugins/hello_plugin.kplugin` evaluation (future cycle)

## Deliverables

- This design document: `docs/superpowers/specs/2026-04-11-sub-project-a-repo-hygiene-cleanup-design.md`
- After user approval of this spec: invocation of `superpowers:writing-plans` to produce the implementation plan (separate document under `docs/superpowers/plans/`)

## Next Steps After Sub-Project A

After Sub-Project A merges, start a fresh brainstorming cycle for **Sub-Project D: CI Hardening & Build**. Do not chain sub-projects within a single spec — each gets its own spec → plan → implementation cycle to preserve the safety of staged reviews and bounded scope.
