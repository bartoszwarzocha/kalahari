# Sub-Project A: Repo Hygiene Cleanup — Design Document

## Revision History

- **2026-04-11 r2 (current)**: Corrected scope after direct `git ls-files` verification. The initial 2026-04-11 audit (which seeded this spec) claimed that `.vs/`, `build-*/`, `compile_commands.json`, `lib/x64-*/bwx_*.{lib,a}`, `src/presenters/`, `src/services/`, `external/`, and other on-disk artifacts were tracked in git. Direct verification showed that **only 6 files** from the original cleanup scope are actually tracked (4 under `concept_files/wxFormBuilder/`, 2 under `tests/gui/test_bwx_*.cpp`). Everything else was already correctly ignored by `.gitignore` and never tracked — physically present on the developer's local disk but invisible to git. Scope narrowed from 7 commits to 4. Non-git local disk cleanup moved to an optional, user-run post-merge section. See section "Post-Merge Local Disk Cleanup" for the omitted items and user-runnable commands.
- **2026-04-11 r1 (superseded)**: Initial spec based on audit claims; 7 commits planned.

## Goal

Eliminate the last tracked wxWidgets-era artifacts in the Kalahari repository and port the final piece of active wxWidgets code (`GuiLogSink`) to Qt. After this sub-project merges, no active wxWidgets API calls or `#include <wx/...>` directives remain in `src/` or `include/`, and three categories of dead files are gone from git: legacy `wxFormBuilder` concept files, disabled `test_bwx_*.cpp` tests, and the related stale comment block in `tests/CMakeLists.txt`.

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

Rationale for order: each sub-project increases the safety net for the next. A produces a clean working tree; D produces CI enforcement; C produces the theme foundation; B is the risky refactor executed under maximum safety.

## Goals / Non-Goals

### Goals
- Remove the 4 tracked `concept_files/wxFormBuilder/*` files
- Eliminate all active wxWidgets API calls and `#include <wx/...>` from `src/` (currently only `gui_log_sink.cpp` / `gui_log_sink.h`)
- Remove the 2 disabled `tests/gui/test_bwx_*.cpp` test files
- Remove the corresponding stale comment block in `tests/CMakeLists.txt` (lines 58-62)
- Align `.clang-format` include priorities with the Qt reality (replace `^<wx/` with `^<(Q|qt)/`)
- Update or delete the stale `src/README.md` (describes pre-Qt6 MVP architecture that never materialized)
- Document all changes under `CHANGELOG.md [Unreleased]`
- Provide optional, documented post-merge bash commands for local disk cleanup that the user can run at their discretion

### Non-Goals
- **Git history rewrite** — even if reductive value were desired, none of the "bulky" items (`.vs/`, `build-*/`) are tracked, so `git filter-repo` / BFG would have no files to remove from history. Out of scope.
- **Physical deletion of on-disk but gitignored artifacts** — these are committed to post-merge optional commands, not part of Sub-Project A's commits
- **Linux / macOS build verification** — primary verification is Windows; Linux/macOS are addressed in Sub-Project D
- **Fixing PCH activation, CI linting, sanitizers** — all Sub-Project D scope
- **Replacing hardcoded `QColor(r,g,b)` values** — Sub-Project C scope
- **`BookEditor` / `DocumentCoordinator` refactor** — Sub-Project B scope
- **Renaming or restructuring `docs/` vs `project_docs/`** — documentation cleanup deferred to a future cycle

## Scope — Four Groups of Changes

### Group 1: Remove `concept_files/wxFormBuilder/` (tracked)
`git rm -r concept_files/wxFormBuilder/` — removes the 4 tracked files (~392 KB):
- `Kalahari.fbp` (wxFormBuilder project file, 85 KB)
- `GUI koncepcja 1.jpg`, `GUI koncepcja 2.jpg` (mockup screenshots)
- `GUI koncepcja.txt` (design notes)

These files describe wxWidgets-era UI layouts superseded by the Qt6 `QDockWidget` architecture.

### Group 2: Port `GuiLogSink` from wxWidgets to Qt (code change)
The last active wxWidgets code in `src/`. See **Section: GuiLogSink Qt Port Design** below for the before/after diff and safety analysis. Files touched:
- `src/core/gui_log_sink.cpp`
- `include/kalahari/core/gui_log_sink.h`

### Group 3: Remove dead wxWidgets test files
Two disabled test files plus a stale CMakeLists comment block:
- `git rm tests/gui/test_bwx_text_document.cpp`
- `git rm tests/gui/test_bwx_text_renderer.cpp`
- Edit `tests/CMakeLists.txt` — remove the comment block at lines 58-62 referencing the removed files (also references `test_bwx_text_editor.cpp` which no longer exists on disk)

### Group 4: Config housekeeping and documentation
- `.clang-format`: replace include priority block for wxWidgets (lines 98-100) with Qt:
  ```yaml
  # Before (lines 98-100):
  # wxWidgets headers
  - Regex: '^<wx/'
    Priority: 2
  ```
  ```yaml
  # After:
  # Qt headers
  - Regex: '^<(Q|qt)/'
    Priority: 2
  ```
- `src/README.md`: **delete** the file (see decision below)
- `CHANGELOG.md`: add entry under `## [Unreleased]` documenting the sub-project

#### Decision: `src/README.md` — delete, not update

The file is extensively stale:
- Documents "MVP (Model-View-Presenter) architecture" with a `presenters/` layer — the project actually uses the Coordinator pattern, and the empty `presenters/` and `services/` directories are out-of-band physical state (not tracked) that the audit flagged
- States "View layer - wxWidgets GUI" and "wxWidgets-free" dependencies description — all wxWidgets references are false after the Qt6 migration
- "Precompiled headers for wxWidgets (Phase 1+)" — incorrect; PCH is for Qt (and isn't even activated, a Sub-Project D concern)
- "Last Updated: 2025-10-26, Phase: 0 (Foundation)" — the project is in Phase 1 now

Fixing this requires a rewrite, not an update, and the current architecture is already documented in `project_docs/03_architecture.md`. A stale pointer README is worse than no README — it actively misinforms new readers. Decision: delete the file and let `project_docs/` remain the single source of architectural truth.

## Approach: 4 Atomic Commits on Feature Branch

### Branch Strategy
- Safety net branches created before work starts:
  - `backup/pre-cleanup-hygiene` — pointer to main HEAD before starting
  - `wip/framework-migration` — snapshot of current uncommitted changes (the OpenSpec→Superpowers framework-migration WIP currently present in working directory; out of scope for A but must not bleed into the feature branch)
- Working branch: `cleanup/repo-hygiene` (feature branch, not pushed until all commits complete and verified)
- No `git rm --cached` anywhere — the previous r1 plan was built on the incorrect audit claim that `.vs/`, `build-*/`, etc. were tracked. They are not. Everything in this spec is either `git rm` (removes tracked files + disk) or straight file edits.

### Commit Sequence

| # | Commit | Type | Build Verify |
|---|---|---|---|
| 1 | `chore(repo): remove wxFormBuilder legacy concept files` | `git rm -r` | No |
| 2 | `refactor(core): port GuiLogSink from wxWidgets to Qt` | Code change | **Yes + smoke test** |
| 3 | `chore(tests): remove dead wxWidgets test files` | `git rm` + edit | **Yes (test build)** |
| 4 | `chore: finalize repo hygiene cleanup` | Edits + docs | No (clang-format validate only) |

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
| 1 | Remove wxFormBuilder | (a) `git ls-files concept_files/wxFormBuilder/` → empty output; (b) `git status` shows clean working tree | No |
| 2 | Port GuiLogSink | (a) full build `scripts/build_windows.bat Debug`; (b) `./build-windows/bin/kalahari-tests.exe` passing at same count as baseline; (c) **manual smoke test**: run `kalahari.exe`; ensure LogPanel dock is visible (open via View menu if hidden); perform log-generating actions in this order — open an example project from `examples/` (generates project-load logs), switch theme in settings (generates theme-change logs), close/reopen document; after each action, **visually confirm new log lines appear in LogPanel** with timestamps matching the action | Yes + smoke |
| 3 | Remove dead tests | (a) `grep -rn "test_bwx" tests/` returns empty output (files gone, comment block gone); (b) test build rebuild via `scripts/build_windows.bat Debug` | Yes (test build) |
| 4 | Config + docs | (a) YAML parse `.clang-format` (e.g., `python -c "import yaml; yaml.safe_load(open('.clang-format'))"`); (b) clang-format dry-run on a sample file (`clang-format --dry-run src/main.cpp`); (c) visual check CHANGELOG.md rendering; (d) `ls src/README.md` → does not exist | No |

**Build time estimate**: ~10 min per full build × 2 full builds (commits 2, 3) = ~20 min build time. Test run time ~2-5 min. Manual smoke test ~5 min. Total verification overhead: ~30 min on top of implementation time.

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

- Static verification fails (commits 1, 4): fix inline before `git commit` — never commit broken state
- Build fail in commit 2 or 3: `git commit --amend` after fix (if no subsequent commits exist) — merges fix into the same commit
- Build OK but problem discovered later: `git revert <sha>` on feature branch — adds a reverting commit instead of rewriting history
- Want to discard commit 2 from feature branch history (before any push): `git reset --hard HEAD~1` — destructive but safe on unpushed branch

**Rule**: on feature branch before first push, `reset --hard` is allowed. After first push, only `revert`.

### Full Sub-Project A rollback

- Abort during work: `git checkout main && git branch -D cleanup/repo-hygiene`
- Restore post-merge regression: `git reset --hard backup/pre-cleanup-hygiene`

### Commit 2 smoke test failure (E1 escalation)

Manual smoke test is the only non-statically-verifiable check. If log messages do not appear in LogPanel after the port, escalation process:

1. **First fix attempt** (30 min budget): typical causes — `qApp` unexpectedly null in spdlog background thread context; missing include; wrong connection type. Use `git commit --amend` if fix found.
2. **Second attempt**: instrumented debugging — temporary `qDebug()` around `invokeMethod`, `QT_LOGGING_RULES` verbose, verify lambda actually posts.
3. **Third failure → escalation decision**:
   - **E1 (preferred)**: `git reset --hard HEAD~1` to drop commit 2, continue commits 3 and 4. Sub-Project A ships with 3 commits instead of 4. `GuiLogSink` port becomes a standalone follow-up task with its own brainstorm cycle (~1 day).
   - **E2 (nuclear)**: abort all of A, full rollback to `backup/pre-cleanup-hygiene`.

Pre-decision: **E1**. The rest of Sub-Project A has independent value and should not suffer for one difficult port. Final decision made in the moment if failure occurs.

## Commit Message Templates

### Commit 1
```
chore(repo): remove wxFormBuilder legacy concept files

Delete concept_files/wxFormBuilder/ (392 KB, 4 tracked files):
- Kalahari.fbp (wxFormBuilder project file)
- GUI koncepcja 1.jpg, GUI koncepcja 2.jpg (mockup screenshots)
- GUI koncepcja.txt (design notes)

These files describe wxWidgets-era UI layouts that were superseded
by Qt6 QDockWidget-based architecture during the Nov 2025 migration.
```

### Commit 2
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

### Commit 3
```
chore(tests): remove dead wxWidgets test files

Delete orphaned test files from the wxWidgets era:
- tests/gui/test_bwx_text_document.cpp
- tests/gui/test_bwx_text_renderer.cpp

Also remove the stale comment block in tests/CMakeLists.txt
(lines 58-62) that referenced these files plus test_bwx_text_editor.cpp
(already gone from disk).

These tests were disabled with a comment noting GTK/X11 display
dependency. The bwxTextEditor code they tested was replaced by
the KML-based Qt editor (OpenSpec #00042, deployed 2026-02-18).
```

### Commit 4
```
chore: finalize repo hygiene cleanup

- .clang-format: replace wxWidgets include priority with Qt:
    '^<wx/' → '^<(Q|qt)/' in IncludeCategories
- src/README.md: delete (extensively stale; described pre-Qt6
  MVP architecture that never materialized; real architecture
  docs live in project_docs/03_architecture.md)
- CHANGELOG.md: add entry under [Unreleased] documenting
  the cleanup sub-project
```

## CHANGELOG Entry

Added under `## [Unreleased]`:

```markdown
### Changed
- **Repo Hygiene:** Sub-Project A — wxWidgets Finalization (2026-04-11)
  - Removed `concept_files/wxFormBuilder/` (wxWidgets-era UI mockups, 4 files)
  - Ported `GuiLogSink` from `wxTheApp->CallAfter()` to
    `QMetaObject::invokeMethod(panel, ..., Qt::QueuedConnection)` —
    **last active wxWidgets code in src/ eliminated**
  - Removed dead `tests/gui/test_bwx_text_{document,renderer}.cpp`
    and the corresponding stale comment block in `tests/CMakeLists.txt`
  - Updated `.clang-format` include priority from wxWidgets to Qt
  - Deleted stale `src/README.md` (described pre-Qt6 MVP architecture
    that never materialized; current architecture docs live in
    `project_docs/03_architecture.md`)

### Removed
- wxWidgets legacy concept files and test files (see "Repo Hygiene" above)
- Stale `src/README.md`
```

## Success Criteria

Sub-Project A is **done** when all of the following are ✅:

1. All 4 commits land on `cleanup/repo-hygiene` (or 3 if E1 escalation — commit 2 dropped)
2. `git ls-files | grep -E "concept_files/wxFormBuilder|tests/gui/test_bwx_|src/README\.md"` returns empty output
3. `grep -rn "wxTheApp\|wx/app\.h\|#include <wx" src/ include/kalahari/` returns empty — no active wxWidgets includes or API calls remain. Occurrences of the literal string "wxWidgets" in rationale/history comments are permitted (e.g., "ported from wxWidgets in 2026-04-11")
4. `scripts/build_windows.bat Debug` passes without errors and without cleanup-related warnings
5. `./build-windows/bin/kalahari-tests.exe` — 100% pass rate (same count as pre-A baseline, not fewer)
6. Manual smoke test of `kalahari.exe`: log messages appear in LogPanel after the port (or documented as known issue if E1 escalation)
7. `.clang-format` validates (YAML parseable, dry-run passes); no `^<wx/` regex remains
8. `CHANGELOG.md` has entry under `[Unreleased]`
9. `wip/framework-migration` branch exists with pre-work snapshot
10. `backup/pre-cleanup-hygiene` branch exists as safety net

After 1-10 are green, the user is asked for explicit approval to merge the feature branch into main. **Merge is not part of Sub-Project A** — it is a separate conscious decision after review of the full diff.

## Post-Merge Local Disk Cleanup (Optional, User-Run)

These items were flagged by the original audit as "repo clutter" but are **not tracked in git** — they exist only on the developer's local disk and are already correctly ignored by `.gitignore`. Deleting them has no effect on the repository and is entirely optional. Provided here as a convenience for the user to run manually after Sub-Project A merges, if a cleaner working tree is desired.

**⚠️ Warning**: `.vs/`, `build-windows/`, `build-vs2026/`, `build-linux-vmware/` are active IDE / build state. Close Visual Studio and any running build before deleting them, otherwise the IDE may corrupt its own state or regenerate them immediately.

```bash
# --- Small root clutter (safe to delete any time) ---
rm -f nul temp_build.bat ninja_output.txt kalahari.log
rm -rf .vscode-ai-assistant/

# --- Orphan empty architecture directories (MVP-pattern leftovers) ---
rm -rf src/presenters/ src/services/
rm -rf include/kalahari/presenters/ include/kalahari/services/
rm -rf external/
rm -rf bin/x64-windows/ bin/x64-linux/

# --- Dead wxWidgets libraries on disk (~50 MB) ---
rm -f lib/x64-windows/bwx_core.lib lib/x64-windows/bwx_gui.lib
rm -f lib/x64-linux/Debug/libbwx_core.a lib/x64-linux/Debug/libbwx_gui.a
rm -f lib/x64-linux/Release/libbwx_core.a lib/x64-linux/Release/libbwx_gui.a

# --- IDE / build caches (ONLY after closing VS and stopping any build!) ---
# rm -rf .vs/                 # ~4.1 GB VS cache
# rm -rf build-windows/       # ~3.5 GB CMake/Ninja output
# rm -rf build-vs2026/
# rm -rf build-linux-vmware/
# rm -f compile_commands.json # regenerated on next CMake configure
```

None of the above produces a git commit; `git status` will be unchanged before and after running these commands (with the exception of newly-empty parent directories, which git tracks via their contents, not as entries).

## Out of Scope

- Git history rewrite (no tracked bulk items exist to remove from history; moot)
- Linux / macOS build verification (Sub-Project D)
- PCH activation (Sub-Project D)
- CI linting (clang-format/clang-tidy/ASAN/UBSAN) enforcement (Sub-Project D)
- Hardcoded `QColor(r,g,b)` replacement (Sub-Project C)
- `BookEditor` / `DocumentCoordinator` refactor (Sub-Project B)
- `docs/` vs `project_docs/` consolidation (future cycle)
- Replacing `fprintf` with Logger in `main.cpp` (fits better in Sub-Project C or D)
- `plugins/hello_plugin.kplugin` evaluation (future cycle)
- Physical deletion of untracked on-disk artifacts (moved to optional Post-Merge Local Disk Cleanup section)

## Deliverables

- This design document: `docs/superpowers/specs/2026-04-11-sub-project-a-repo-hygiene-cleanup-design.md` (r2)
- After user approval of this revised spec: invocation of `superpowers:writing-plans` to produce the implementation plan (separate document under `docs/superpowers/plans/`)

## Next Steps After Sub-Project A

After Sub-Project A merges, start a fresh brainstorming cycle for **Sub-Project D: CI Hardening & Build**. Do not chain sub-projects within a single spec — each gets its own spec → plan → implementation cycle to preserve the safety of staged reviews and bounded scope.
