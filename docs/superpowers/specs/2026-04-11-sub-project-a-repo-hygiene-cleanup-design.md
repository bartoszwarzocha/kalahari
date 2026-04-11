# Sub-Project A: Repo Hygiene Cleanup — Design Document

## Revision History

- **2026-04-11 r3 (current)**: Corrected Group 2 scope after direct verification during Task 3 execution. The r1 and r2 audits both claimed `gui_log_sink.cpp` was "the last active wxWidgets code in `src/`" needing a port to Qt. Direct verification showed `gui_log_sink.{cpp,h}` is **orphan dead code** — not in `src/CMakeLists.txt`, not `#included` by any other source file, and its only method calls `LogPanel::appendLog(log_message)` with one argument against a two-argument signature `appendLog(int level, const QString& message)` that would not even compile if the file were added to the build. A proper Qt replacement (`LogPanelSink`, OpenSpec #00024) already exists at `include/kalahari/core/log_panel_sink.h` and is the actual sink used by `LogPanel`, `DockCoordinator`, and `MainWindow`. Group 2 changed from **"port GuiLogSink to Qt"** (risky code change with smoke test gate) to **"delete orphan GuiLogSink dead code"** (simple `git rm` of two tracked files). Smoke test removed from verification plan — deleting a file not in the build cannot break the build. E1 escalation removed. Final shipping goal unchanged: no active wxWidgets API calls remain in `src/` or `include/kalahari/` after Sub-Project A merges.
- **2026-04-11 r2 (superseded)**: Corrected scope after direct `git ls-files` verification showed the r1 audit had overstated tracked files. Only 6 files from the original cleanup scope were actually tracked (4 under `concept_files/wxFormBuilder/`, 2 under `tests/gui/test_bwx_*.cpp`). Everything else was already correctly ignored by `.gitignore` and never tracked. Scope narrowed 7 commits → 4 commits. Non-git local disk cleanup moved to an optional, user-run post-merge section.
- **2026-04-11 r1 (superseded)**: Initial spec based on audit claims; 7 commits planned.

## Goal

Eliminate the last tracked wxWidgets-era artifacts in the Kalahari repository. After this sub-project merges, no active wxWidgets API calls or `#include <wx/...>` directives remain in `src/` or `include/`, and four categories of dead files are gone from git: legacy `wxFormBuilder` concept files, disabled `test_bwx_*.cpp` tests, the related stale comment block in `tests/CMakeLists.txt`, and the orphan template class `GuiLogSink` / `GuiLogSinkImpl` that has been unused since the Qt migration.

This is **Sub-Project A** of a four-part cleanup initiative identified in the 2026-04-11 professional audit. The audit surfaced ~32 action items grouped into four independent sub-projects; A is the safety-first starting point because it is mostly mechanical (file/git operations) with no code changes — only deletions of tracked but unused files plus a few doc/config edits.

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
- Delete `src/core/gui_log_sink.cpp` and `include/kalahari/core/gui_log_sink.h` as orphan dead code (not in build, not used, not even compilable against current `LogPanel::appendLog` signature)
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

**Status**: already executed as commit `419a4c3` on `cleanup/repo-hygiene` during Task 2 of the r2 execution plan.

### Group 2: Delete orphan `GuiLogSink` dead code
`git rm` two tracked files that are not compiled by `src/CMakeLists.txt` and not used by any other source file:
- `src/core/gui_log_sink.cpp`
- `include/kalahari/core/gui_log_sink.h`

See **Section: GuiLogSink Dead Code Analysis** below for evidence.

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
- No `git rm --cached` anywhere — the r1 plan was built on the incorrect audit claim that `.vs/`, `build-*/`, etc. were tracked. They are not. Everything in this spec is either `git rm` (removes tracked files + disk) or straight file edits.

### Commit Sequence

| # | Commit | Type | Build Verify |
|---|---|---|---|
| 1 | `chore(repo): remove wxFormBuilder legacy concept files` | `git rm -r` | No |
| 2 | `refactor(core): remove orphan GuiLogSink dead code` | `git rm` | **Yes (build + tests)** |
| 3 | `chore(tests): remove dead wxWidgets test files` | `git rm` + edit | **Yes (test build)** |
| 4 | `chore: finalize repo hygiene cleanup` | Edits + docs | No (clang-format validate only) |

## GuiLogSink Dead Code Analysis

### Evidence of dead-code status

1. **Not in the build**. `grep "gui_log_sink" src/CMakeLists.txt` returns empty output. `src/CMakeLists.txt:136` lists `core/log_panel_sink.cpp` (the active Qt sink) but not `core/gui_log_sink.cpp`.

2. **Not included by any source file**. `grep -rn "include.*gui_log_sink\|GuiLogSink" src/` returns only the files themselves (`src/core/gui_log_sink.cpp:4` — the self-include) and `include/kalahari/core/gui_log_sink.h` internal references. No other `.cpp` or `.h` in `src/` or `include/kalahari/` references `GuiLogSink`, `GuiLogSinkImpl`, `GuiLogSinkSt`, or `gui_log_sink.h`.

3. **Would not compile even if force-added to build**. `include/kalahari/gui/panels/log_panel.h:95` declares `void appendLog(int level, const QString& message);` — the only overload. `src/core/gui_log_sink.cpp:55,92` calls `panel->appendLog(log_message)` with one argument where `log_message` is `std::string`. Compiling this would produce "too few arguments" errors plus an `std::string → QString` implicit conversion error. Template explicit instantiation at `src/core/gui_log_sink.cpp:117-118` would force evaluation, so inclusion in the build is not possible without code changes.

4. **Qt replacement exists and is in use**. `include/kalahari/core/log_panel_sink.h` declares `class LogPanelSink : public QObject, public spdlog::sinks::base_sink<std::mutex>` with a Qt signal `void logMessage(int level, const QString& message)`. `src/gui/panels/log_panel.cpp:37,55` constructs an instance and connects its signal to `LogPanel::appendLog` via `Qt::QueuedConnection`. `src/gui/dock_coordinator.cpp:17` and `src/gui/main_window.cpp:31` include `log_panel_sink.h` and wire up the sink. This is the spdlog → GUI bridge that Kalahari actually uses; it was introduced by OpenSpec #00024 during the Qt migration.

### Conclusion

`gui_log_sink.{cpp,h}` are tracked source files that have been unused since the Qt migration. The correct action is to delete both files as dead code. The shipping goal of Sub-Project A — **no active wxWidgets API calls remain in `src/` or `include/kalahari/`** — is achieved by this deletion. Porting the code to Qt would be wasted work; even if done correctly, the result would be a second, redundant Qt sink behind the already-deployed `LogPanelSink`.

### Safety

Deleting a file that is not in `src/CMakeLists.txt` and not `#included` by any other source file cannot affect the build. The only way this could fail is if an out-of-tree artifact (for example a cached object file in `build-windows/CMakeFiles/` or a stale `compile_commands.json` entry) still references the removed file. CMake reconfigure during the next build will regenerate both, so this risk is bounded to a fresh reconfigure (i.e., deleting the relevant `build-windows/CMakeFiles/kalahari_core.dir/src/core/gui_log_sink.cpp.obj` if present, or letting Ninja notice the source is gone).

## Verification Plan

| # | Commit | Verification | Build |
|---|---|---|---|
| 1 | Remove wxFormBuilder | (a) `git ls-files concept_files/wxFormBuilder/` → empty output; (b) `git status` shows clean working tree | No |
| 2 | Delete GuiLogSink dead code | (a) `git ls-files src/core/gui_log_sink.cpp include/kalahari/core/gui_log_sink.h` → empty output; (b) `grep -rn "gui_log_sink\|GuiLogSink" src/ include/kalahari/` → empty output; (c) full build `scripts/build_windows.bat Debug` to confirm no hidden dependency; (d) `./build-windows/bin/kalahari-tests.exe` passing at baseline count | Yes |
| 3 | Remove dead tests | (a) `grep -rn "test_bwx" tests/` returns empty output (files gone, comment block gone); (b) test build rebuild via `scripts/build_windows.bat Debug` | Yes (test build) |
| 4 | Config + docs | (a) YAML parse `.clang-format` (e.g., `python -c "import yaml; yaml.safe_load(open('.clang-format'))"`); (b) clang-format dry-run on a sample file (`clang-format --dry-run src/main.cpp`); (c) visual check CHANGELOG.md rendering; (d) `ls src/README.md` → does not exist | No |

**Build time estimate**: ~10 min per full build × 2 full builds (commits 2, 3) = ~20 min build time. Test run time ~2-5 min. **No manual smoke test required in any commit** (the r2 smoke test for the `GuiLogSink` port is removed with the port itself). Total verification overhead: ~25 min on top of implementation time.

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

**Status under r3 execution**: already completed during r2 execution as `wip/framework-migration` commit `9f4ee80`, `backup/pre-cleanup-hygiene`, and `cleanup/repo-hygiene` feature branch. Commit 1 (wxFormBuilder removal) also already landed on `cleanup/repo-hygiene` as commit `419a4c3`. Execution resumes from Commit 2 under r3 semantics (delete instead of port).

### Per-commit rollback (during A)

- Static verification fails (commits 1, 4): fix inline before `git commit` — never commit broken state
- Build fail in commit 2 or 3: `git commit --amend` after fix (if no subsequent commits exist) — merges fix into the same commit
- Build OK but problem discovered later: `git revert <sha>` on feature branch — adds a reverting commit instead of rewriting history
- Want to discard commit 2 from feature branch history (before any push): `git reset --hard HEAD~1` — destructive but safe on unpushed branch

**Rule**: on feature branch before first push, `reset --hard` is allowed. After first push, only `revert`.

### Full Sub-Project A rollback

- Abort during work: `git checkout main && git branch -D cleanup/repo-hygiene`
- Restore post-merge regression: `git reset --hard backup/pre-cleanup-hygiene`

### Commit 2 failure scenarios

With the r3 semantics (delete orphan dead code, not port), the only plausible failure modes are:

1. **Build fails after deletion** — would indicate a hidden dependency on `gui_log_sink` that the grep-based analysis missed. If this happens, `git commit --amend` after restoring the files, investigate the real dependency (probably a stale `compile_commands.json` or CMake cache entry), and only then retry deletion. Not expected; the dead-code analysis above is thorough.

2. **Test count regression** — would indicate a test that was somehow linking against `gui_log_sink` (extremely unlikely since the test binary's source list does not include it). Same fix path as #1.

The r2 smoke test escalation (E1) is removed — there is no port to fail, so no smoke test exists to gate.

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

**Status under r3**: already landed as `419a4c3` during r2 execution.

### Commit 2
```
refactor(core): remove orphan GuiLogSink dead code

Delete two tracked but unused source files:
- src/core/gui_log_sink.cpp
- include/kalahari/core/gui_log_sink.h

GuiLogSink (a template class GuiLogSinkImpl<Mutex> with explicit
instantiations for std::mutex and null_mutex) has been orphan dead
code since the Qt migration:

- Not in src/CMakeLists.txt (never compiled)
- Not #included by any other source file in src/ or include/
- sink_it_() calls panel->appendLog(log_message) with one std::string
  argument, but LogPanel::appendLog has a single overload taking
  (int level, const QString& message) — the code would not even
  compile if force-added to the build

The active Qt sink that replaces this legacy class already exists:
LogPanelSink (include/kalahari/core/log_panel_sink.h, OpenSpec #00024).
It uses Q_OBJECT plus a Qt signal logMessage(int, QString) connected
via Qt::QueuedConnection, and is wired up by LogPanel, DockCoordinator,
and MainWindow.

Deletion completes the Sub-Project A goal of removing active
wxWidgets API calls from src/ and include/kalahari/, via a simpler
and more correct path than porting dead code.
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
    '^<wx/' -> '^<(Q|qt)/' in IncludeCategories
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
  - Removed orphan `GuiLogSink` template class (`src/core/gui_log_sink.cpp` and
    `include/kalahari/core/gui_log_sink.h`) — tracked but never compiled since
    the Qt migration; Qt replacement `LogPanelSink` already in use (OpenSpec #00024)
  - Removed dead `tests/gui/test_bwx_text_{document,renderer}.cpp`
    and the corresponding stale comment block in `tests/CMakeLists.txt`
  - Updated `.clang-format` include priority from wxWidgets to Qt
  - Deleted stale `src/README.md` (described pre-Qt6 MVP architecture
    that never materialized; current architecture docs live in
    `project_docs/03_architecture.md`)

### Removed
- wxWidgets legacy concept files, orphan GuiLogSink, and dead test files
  (see "Repo Hygiene" above)
- Stale `src/README.md`
```

## Success Criteria

Sub-Project A is **done** when all of the following are ✅:

1. All 4 commits land on `cleanup/repo-hygiene`
2. `git ls-files | grep -E "concept_files/wxFormBuilder|tests/gui/test_bwx_|src/README\.md|src/core/gui_log_sink\.|include/kalahari/core/gui_log_sink\."` returns empty output
3. `grep -rn "wxTheApp\|wx/app\.h\|#include <wx\|GuiLogSink" src/ include/kalahari/` returns empty — no active wxWidgets includes or API calls remain, and no `GuiLogSink` references. Occurrences of the literal string "wxWidgets" in rationale/history comments are permitted (e.g., "ported from wxWidgets in 2026-04-11")
4. `scripts/build_windows.bat Debug` passes without errors and without cleanup-related warnings
5. `./build-windows/bin/kalahari-tests.exe` — 100% pass rate (same count as pre-A baseline, not fewer)
6. `.clang-format` validates (YAML parseable, dry-run passes); no `^<wx/` regex remains
7. `CHANGELOG.md` has entry under `[Unreleased]`
8. `wip/framework-migration` branch exists with pre-work snapshot
9. `backup/pre-cleanup-hygiene` branch exists as safety net

(r3 note: the r2 list had 10 items including a manual smoke test criterion. With the port removed, smoke test is gone and the criterion count drops to 9. Success criterion numbering shifted accordingly.)

After 1-9 are green, the user is asked for explicit approval to merge the feature branch into main. **Merge is not part of Sub-Project A** — it is a separate conscious decision after review of the full diff.

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
- Porting `GuiLogSink` to Qt (r3 removed this; the file is orphan dead code, not active wxWidgets code)

## Deliverables

- This design document: `docs/superpowers/specs/2026-04-11-sub-project-a-repo-hygiene-cleanup-design.md` (r3)
- After user approval of this revised spec: a small implementation plan revision (r3) updating Task 3 from "port" to "delete", and execution resumption from Commit 2

## Next Steps After Sub-Project A

After Sub-Project A merges, start a fresh brainstorming cycle for **Sub-Project D: CI Hardening & Build**. Do not chain sub-projects within a single spec — each gets its own spec → plan → implementation cycle to preserve the safety of staged reviews and bounded scope.
