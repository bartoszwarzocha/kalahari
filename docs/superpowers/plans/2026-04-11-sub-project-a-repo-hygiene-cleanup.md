# Sub-Project A: Repo Hygiene Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate the last tracked wxWidgets-era artifacts and port `GuiLogSink` from `wxTheApp->CallAfter()` to `QMetaObject::invokeMethod(panel, ..., Qt::QueuedConnection)`, completing the Qt6 migration in all production code.

**Architecture:** Four atomic commits on a feature branch `cleanup/repo-hygiene` (from a clean `main` isolated via `wip/framework-migration` snapshot branch). Each commit is independently verifiable; commit 2 (the `GuiLogSink` port) is the only real code change and is gated by a manual smoke test verifying that log messages still reach `LogPanel` after the port.

**Tech Stack:** Git (feature branch + safety nets, `git rm`, no history rewrite), C++20 with Qt 6.5+ (`QMetaObject::invokeMethod`, `Qt::QueuedConnection`), spdlog template sink (unchanged structurally), CMake + Ninja build (`scripts/build_windows.bat Debug`), Catch2 3 test runner.

**Spec:** `docs/superpowers/specs/2026-04-11-sub-project-a-repo-hygiene-cleanup-design.md` (r2, commit `85bdf8b`).

---

## File Structure

### Files created by this plan

None. This plan only deletes and modifies.

### Files deleted

- `concept_files/wxFormBuilder/Kalahari.fbp` (tracked, Task 2)
- `concept_files/wxFormBuilder/GUI koncepcja 1.jpg` (tracked, Task 2)
- `concept_files/wxFormBuilder/GUI koncepcja 2.jpg` (tracked, Task 2)
- `concept_files/wxFormBuilder/GUI koncepcja.txt` (tracked, Task 2)
- `src/core/gui_log_sink.cpp` (tracked, Task 3 — r3 revision: orphan dead code, not a port)
- `include/kalahari/core/gui_log_sink.h` (tracked, Task 3 — r3 revision: orphan dead code, not a port)
- `tests/gui/test_bwx_text_document.cpp` (tracked, Task 4)
- `tests/gui/test_bwx_text_renderer.cpp` (tracked, Task 4)
- `src/README.md` (tracked, Task 5)

### Files modified

- `tests/CMakeLists.txt` — remove stale comment block at lines 58-62 (Task 4)
- `.clang-format` — replace wxWidgets include priority block (lines 98-100) with Qt (Task 5)
- `CHANGELOG.md` — add entry under `## [Unreleased]` (Task 5)

### Files not touched (out of scope per spec)

- Anything under `build-windows/`, `build-vs2026/`, `build-linux-vmware/`, `.vs/`, `compile_commands.json`, `lib/x64-*/`, `src/presenters/`, `src/services/`, `include/kalahari/presenters/`, `include/kalahari/services/`, `external/`, `bin/x64-*/`, `kalahari.log`, `nul`, `temp_build.bat`, `ninja_output.txt`, `.vscode-ai-assistant/` — these are **not tracked** by git. Their physical deletion, if desired, is an optional user-run step documented in the spec under "Post-Merge Local Disk Cleanup" and is explicitly **not** part of any commit in this plan.

---

## Task 1: Pre-work — Safety net branches and feature branch setup

**Purpose:** Isolate the uncommitted framework-migration WIP currently in the working directory onto its own branch so that Sub-Project A commits start from a clean `main`. Create a backup branch as a safety net, then switch to the feature branch.

**Files:** No files modified in this task. Only git branch operations.

- [ ] **Step 1.1: Verify starting state and save a baseline report**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git status --short | head -40
git branch --show-current
```

Expected output: the current branch is `main`. Many `M` and `D` lines (the OpenSpec → Superpowers framework migration WIP). No `??` (untracked) lines expected from the Sub-Project A scope (the spec confirms the cleanup targets are already in `.gitignore`).

- [ ] **Step 1.2: Create the WIP snapshot branch and commit the uncommitted framework migration state**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git checkout -b wip/framework-migration
git add -A
git commit -m "WIP: snapshot of framework migration pre-cleanup state

Snapshot of uncommitted OpenSpec -> Superpowers framework migration
work present in working directory at start of Sub-Project A.
Out of scope for Sub-Project A; to be revisited after A merges
(rebase onto updated main, cherry-pick selectively, or reset)."
```

Expected: commit succeeds. The working directory becomes clean.

- [ ] **Step 1.3: Return to main and verify it is clean**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git checkout main
git status
```

Expected: `On branch main` and `nothing to commit, working tree clean`. If anything remains uncommitted, STOP and report — `main` must be pristine before creating the feature branch.

- [ ] **Step 1.4: Create the backup branch pointing at current main HEAD**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git branch backup/pre-cleanup-hygiene
git branch --list 'backup/*' 'wip/*'
```

Expected output: both `backup/pre-cleanup-hygiene` and `wip/framework-migration` listed.

- [ ] **Step 1.5: Create and check out the feature branch**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git checkout -b cleanup/repo-hygiene
git branch --show-current
```

Expected output: `cleanup/repo-hygiene`.

- [ ] **Step 1.6: Record baseline test count for later comparison (commit 2 verification)**

Build must already be present from pre-A work. If it is not, run `scripts/build_windows.bat Debug` first (may take ~10 min). Then:
```bash
cd E:/Python/Projekty/Kalahari
./build-windows/bin/kalahari-tests.exe --list-tests | wc -l
```

Record the baseline number (example: `664`). Save it mentally or in a scratch note — Task 3 Step 3.10 will compare against it, and Task 6 Step 6.5 will re-verify after all commits.

If the build binary does not yet exist (`kalahari-tests.exe` missing), run `scripts/build_windows.bat Debug` to produce it before proceeding. This step is a prerequisite baseline, not a deliverable of Task 1.

---

## Task 2: Commit 1 — Remove wxFormBuilder legacy concept files

**Purpose:** Remove the 4 tracked files under `concept_files/wxFormBuilder/` that describe pre-Qt6 UI mockups.

**Files:**
- Delete (git): `concept_files/wxFormBuilder/Kalahari.fbp`
- Delete (git): `concept_files/wxFormBuilder/GUI koncepcja 1.jpg`
- Delete (git): `concept_files/wxFormBuilder/GUI koncepcja 2.jpg`
- Delete (git): `concept_files/wxFormBuilder/GUI koncepcja.txt`

- [ ] **Step 2.1: Verify current tracked state of `concept_files/wxFormBuilder/`**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git ls-files concept_files/wxFormBuilder/
```

Expected output (exactly 4 lines):
```
concept_files/wxFormBuilder/GUI koncepcja 1.jpg
concept_files/wxFormBuilder/GUI koncepcja 2.jpg
concept_files/wxFormBuilder/GUI koncepcja.txt
concept_files/wxFormBuilder/Kalahari.fbp
```

If the count differs, STOP and report — the file set must match the plan exactly before deletion.

- [ ] **Step 2.2: Remove the files from git index and disk**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git rm -r concept_files/wxFormBuilder/
```

Expected output: 4 `rm 'concept_files/wxFormBuilder/...'` lines. Git stages the deletions.

- [ ] **Step 2.3: Verify staging**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git status --short concept_files/
```

Expected output: 4 `D` (deleted) lines for the 4 files. No other entries.

- [ ] **Step 2.4: Commit**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git commit -m "$(cat <<'EOF'
chore(repo): remove wxFormBuilder legacy concept files

Delete concept_files/wxFormBuilder/ (392 KB, 4 tracked files):
- Kalahari.fbp (wxFormBuilder project file)
- GUI koncepcja 1.jpg, GUI koncepcja 2.jpg (mockup screenshots)
- GUI koncepcja.txt (design notes)

These files describe wxWidgets-era UI layouts that were superseded
by Qt6 QDockWidget-based architecture during the Nov 2025 migration.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected output: commit succeeds, pre-commit hook "Kalahari C++ Pattern Validator" runs and passes (no C++ files staged). `[cleanup/repo-hygiene <sha>]` line followed by `4 files changed, 0 insertions(+), <N> deletions(-)`.

- [ ] **Step 2.5: Verification — no more tracked wxFormBuilder files**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git ls-files concept_files/wxFormBuilder/
git ls-files | grep -i "wxFormBuilder\|Kalahari\.fbp"
```

Expected output: both commands produce empty output. No more tracked files under `concept_files/wxFormBuilder/`, no occurrences anywhere in the tracked set.

---

## Task 3 (r3): Commit 2 — Delete orphan GuiLogSink dead code

**Purpose (r3 revision):** Delete `src/core/gui_log_sink.cpp` and `include/kalahari/core/gui_log_sink.h` as orphan dead code. During r2 plan execution, direct verification revealed that `gui_log_sink` is not in `src/CMakeLists.txt` (never compiled), is not `#included` by any other source file in `src/` or `include/`, and its `sink_it_()` / `setPanel()` methods call `panel->appendLog(log_message)` with a single `std::string` argument against a `LogPanel::appendLog(int level, const QString& message)` signature that would not compile if the file were added to the build. The proper Qt replacement `LogPanelSink` already exists at `include/kalahari/core/log_panel_sink.h` (OpenSpec #00024) and is the actual sink used by `LogPanel`, `DockCoordinator`, and `MainWindow`. This task deletes both tracked dead files rather than porting them. No smoke test is required — deleting files not in the build cannot break the build.

**Files:**
- Delete (git): `src/core/gui_log_sink.cpp`
- Delete (git): `include/kalahari/core/gui_log_sink.h`

- [ ] **Step 3.1: Verify both files are tracked and confirm the dead-code analysis is still accurate**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git ls-files src/core/gui_log_sink.cpp include/kalahari/core/gui_log_sink.h
```

Expected output (exactly 2 lines):
```
include/kalahari/core/gui_log_sink.h
src/core/gui_log_sink.cpp
```

Then verify the dead-code status is still accurate (run three quick greps):
```bash
cd E:/Python/Projekty/Kalahari
grep -n "gui_log_sink" src/CMakeLists.txt
grep -rn "include.*gui_log_sink\|GuiLogSink" src/ include/kalahari/ --include="*.cpp" --include="*.h" | grep -v -E "(src/core/gui_log_sink\.cpp|include/kalahari/core/gui_log_sink\.h)"
grep -n "LogPanelSink" src/CMakeLists.txt
```

Expected:
- Grep 1 (`gui_log_sink` in `src/CMakeLists.txt`): empty output — confirms the file is not compiled
- Grep 2 (`#include` or `GuiLogSink` references outside the two files themselves): empty output — confirms no source file uses the dead code
- Grep 3 (`LogPanelSink` in `src/CMakeLists.txt`): a match around line 136 — confirms the Qt replacement is in the build

If any grep returns unexpected results (especially grep 1 or 2), STOP and re-run the dead-code analysis before deleting. The spec r3 analysis was thorough but repo state may have changed.

- [ ] **Step 3.2: Delete both files from git and disk**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git rm src/core/gui_log_sink.cpp include/kalahari/core/gui_log_sink.h
```

Expected output: 2 `rm '...'` lines. Both files are removed from the working tree and staged for commit.

- [ ] **Step 3.3: Verify staging**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git status --short src/core/gui_log_sink.cpp include/kalahari/core/gui_log_sink.h
```

Expected output: 2 `D ` lines (first column `D`, second column space — staged deletions):
```
D  include/kalahari/core/gui_log_sink.h
D  src/core/gui_log_sink.cpp
```

- [ ] **Step 3.4: Full Debug build to confirm no hidden dependency**

Run:
```bash
cd E:/Python/Projekty/Kalahari
scripts/build_windows.bat Debug
```

Expected: build succeeds. Since the deleted files are not in `src/CMakeLists.txt`, the only way the build could break is if a stale entry in `build-windows/CMakeFiles/` or `compile_commands.json` referenced the removed files. CMake reconfigure during this build regenerates both, so any stale reference is dropped automatically. Build takes ~10 min.

If the build fails:
- Read the error output carefully
- If the error names `gui_log_sink.cpp` or `gui_log_sink.h`, the dead-code analysis in spec r3 was incomplete — STOP, restore via `git restore --staged --worktree src/core/gui_log_sink.cpp include/kalahari/core/gui_log_sink.h`, and re-investigate
- If the error is unrelated, the build was already broken before this task — that is a separate concern and should be reported

- [ ] **Step 3.5: Run the test binary and compare count to baseline**

Run:
```bash
cd E:/Python/Projekty/Kalahari
./build-windows/bin/kalahari-tests.exe --list-tests | wc -l
./build-windows/bin/kalahari-tests.exe > /tmp/r3_test_stdout.txt 2>&1
echo "EXIT CODE: $?"
```

Expected:
- Test count matches the baseline from Task 1 Step 1.6 (e.g., 1209 lines from `--list-tests` output; 603 test cases from the summary)
- Exit code `0` (all tests pass)

If the count is lower or exit code is non-zero, STOP and investigate. The deleted files are not in the test binary's source list either, so a test regression here would be unexpected and worth understanding.

- [ ] **Step 3.6: Stage and commit**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git status --short src/core/gui_log_sink.cpp include/kalahari/core/gui_log_sink.h
```

Expected: same 2 `D ` lines from Step 3.3 (deletions still staged; the build did not affect the staging).

Then:
```bash
cd E:/Python/Projekty/Kalahari
git commit -m "$(cat <<'EOF'
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
  (int level, const QString& message) -- the code would not even
  compile if force-added to the build

The active Qt sink that replaces this legacy class already exists:
LogPanelSink (include/kalahari/core/log_panel_sink.h, OpenSpec #00024).
It uses Q_OBJECT plus a Qt signal logMessage(int, QString) connected
via Qt::QueuedConnection, and is wired up by LogPanel, DockCoordinator,
and MainWindow.

Deletion completes the Sub-Project A goal of removing active
wxWidgets API calls from src/ and include/kalahari/, via a simpler
and more correct path than porting dead code.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: commit succeeds, pre-commit hook runs and passes. `2 files changed, <N> deletions(-)`.

- [ ] **Step 3.7: Post-commit verification**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git ls-files src/core/gui_log_sink.cpp include/kalahari/core/gui_log_sink.h
grep -rn "GuiLogSink\|gui_log_sink" src/ include/kalahari/
```

Expected:
- First command: empty output (files no longer tracked)
- Second command: empty output (no references anywhere in source or headers)

---

## Task 4: Commit 3 — Remove dead wxWidgets test files and clean `tests/CMakeLists.txt`

**Purpose:** Delete the two disabled `test_bwx_*.cpp` files and remove the stale comment block in `tests/CMakeLists.txt` (lines 58-62) that references those files plus a third file (`test_bwx_text_editor.cpp`) that no longer exists on disk.

**Files:**
- Delete (git): `tests/gui/test_bwx_text_document.cpp`
- Delete (git): `tests/gui/test_bwx_text_renderer.cpp`
- Modify: `tests/CMakeLists.txt` — remove comment block at lines 58-62

- [ ] **Step 4.1: Verify current tracked state of the test files**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git ls-files tests/gui/test_bwx_*.cpp
```

Expected output (exactly 2 lines):
```
tests/gui/test_bwx_text_document.cpp
tests/gui/test_bwx_text_renderer.cpp
```

- [ ] **Step 4.2: Remove the two test files**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git rm tests/gui/test_bwx_text_document.cpp tests/gui/test_bwx_text_renderer.cpp
```

Expected output: 2 `rm 'tests/gui/test_bwx_...'` lines.

- [ ] **Step 4.3: Remove the stale comment block in `tests/CMakeLists.txt`**

File: `tests/CMakeLists.txt`

Find:
```
set(TEST_SOURCES ${TEST_SOURCES}
    # GUI tests REMOVED - all require GTK/X11 display connection (incompatible with console-only CI/CD)
    # - test_bwx_text_editor.cpp: All 9 test cases created wxFrame windows
    # - test_bwx_text_document.cpp: Uses wxMemoryDC (requires display on Linux/macOS)
    # - test_bwx_text_renderer.cpp: Uses wxMemoryDC (requires display on Linux/macOS)
    # These tests will be moved to manual/integration test suite in Phase 1
    # Source files under test (needed for linking)
```

Replace with:
```
set(TEST_SOURCES ${TEST_SOURCES}
    # Source files under test (needed for linking)
```

This removes the 5-line comment block (lines 58-62 in the original file) that referenced the now-deleted test files and the already-missing `test_bwx_text_editor.cpp`.

- [ ] **Step 4.4: Verify no `test_bwx` references remain anywhere in `tests/`**

Run:
```bash
cd E:/Python/Projekty/Kalahari
grep -rn "test_bwx" tests/
```

Expected output: empty. If any match remains, remove it before committing.

- [ ] **Step 4.5: Test build rebuild to ensure CMake reconfigures cleanly**

Run:
```bash
cd E:/Python/Projekty/Kalahari
scripts/build_windows.bat Debug
```

Expected: build succeeds. CMake reconfigure picks up the removed test files without error. `kalahari-tests.exe` still builds and contains the same number of tests as the baseline (the removed files were already excluded from `TEST_SOURCES`, so test count is unaffected).

- [ ] **Step 4.6: Run the test binary — sanity check**

Run:
```bash
cd E:/Python/Projekty/Kalahari
./build-windows/bin/kalahari-tests.exe --list-tests | wc -l
```

Expected: same count as baseline from Task 1 Step 1.6.

- [ ] **Step 4.7: Stage and commit**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git add tests/CMakeLists.txt
git status --short tests/
```

Expected: 2 `D` lines for the deleted test files + 1 `M` line for `tests/CMakeLists.txt`.

Then:
```bash
cd E:/Python/Projekty/Kalahari
git commit -m "$(cat <<'EOF'
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

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: commit succeeds, pre-commit hook runs and passes. `3 files changed, 1 insertion(+), <N> deletions(-)`.

---

## Task 5: Commit 4 — Finalize repo hygiene cleanup (config, README, CHANGELOG)

**Purpose:** Update `.clang-format` include priority block to reference Qt instead of wxWidgets, delete the stale `src/README.md`, and add the Sub-Project A entry to `CHANGELOG.md` under `## [Unreleased]`.

**Files:**
- Modify: `.clang-format` — replace wxWidgets priority block at lines 98-100
- Delete (git): `src/README.md`
- Modify: `CHANGELOG.md` — add entry under `## [Unreleased]`

- [ ] **Step 5.1: Update `.clang-format` include priority**

File: `.clang-format`

Find:
```
  # wxWidgets headers
  - Regex: '^<wx/'
    Priority: 2
```

Replace with:
```
  # Qt headers
  - Regex: '^<(Q|qt)/'
    Priority: 2
```

- [ ] **Step 5.2: Validate `.clang-format` is still valid YAML**

Run:
```bash
cd E:/Python/Projekty/Kalahari
python -c "import yaml; yaml.safe_load(open('.clang-format'))" && echo "YAML OK"
```

Expected output: `YAML OK` with no Python traceback. If Python is not available, use:
```bash
cd E:/Python/Projekty/Kalahari
clang-format --dry-run src/main.cpp > /dev/null && echo "clang-format OK"
```

Expected: `clang-format OK` with no errors.

- [ ] **Step 5.3: Delete the stale `src/README.md`**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git rm src/README.md
```

Expected: `rm 'src/README.md'`.

- [ ] **Step 5.4: Add the `## [Unreleased]` entry to `CHANGELOG.md`**

File: `CHANGELOG.md`

Open the file and locate the `## [Unreleased]` section near the top. Under that section, add a new `### Changed` subsection (or append to the existing one if it already exists). The exact content to add is:

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

**Integration note for the executor:** If `## [Unreleased]` already has a `### Changed` or `### Removed` subsection, merge the new bullets under the existing subsection headings rather than creating duplicate subsection headings. Do not add the new entries above any existing `### Changed` content — the standard convention is that the most recent change goes at the top of the subsection.

- [ ] **Step 5.5: Verify the final set of changes is clean**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git status --short
git diff --stat
```

Expected output: `M .clang-format`, `D src/README.md`, `M CHANGELOG.md` — exactly three entries, no unrelated noise.

- [ ] **Step 5.6: Stage and commit**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git add .clang-format CHANGELOG.md
git status --short
```

Expected: `M .clang-format`, `M CHANGELOG.md`, `D src/README.md` (the last one was already staged by `git rm`).

Then:
```bash
cd E:/Python/Projekty/Kalahari
git commit -m "$(cat <<'EOF'
chore: finalize repo hygiene cleanup

- .clang-format: replace wxWidgets include priority with Qt:
    '^<wx/' -> '^<(Q|qt)/' in IncludeCategories
- src/README.md: delete (extensively stale; described pre-Qt6
  MVP architecture that never materialized; real architecture
  docs live in project_docs/03_architecture.md)
- CHANGELOG.md: add entry under [Unreleased] documenting
  the cleanup sub-project

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: commit succeeds, pre-commit hook runs and passes.

---

## Task 6: Final verification and merge preparation

**Purpose:** Walk through the Success Criteria checklist from the spec, verify all 10 items are green, and present the branch state to the user for merge approval.

**Files:** No files modified in this task. Only verification commands.

- [ ] **Step 6.1: Verify commit count on the feature branch**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git log --oneline main..cleanup/repo-hygiene
```

Expected: 4 commits listed (or 3 if E1 escalation was triggered in Task 3). Commit messages match the plan's subject lines.

- [ ] **Step 6.2: Success Criterion 2 — no leftover tracked cleanup targets**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git ls-files | grep -E "concept_files/wxFormBuilder|tests/gui/test_bwx_|src/README\.md"
```

Expected: empty output.

- [ ] **Step 6.3: Success Criterion 3 — no active wxWidgets code in src/ or include/kalahari/**

Run:
```bash
cd E:/Python/Projekty/Kalahari
grep -rn "wxTheApp\|wx/app\.h\|#include <wx" src/ include/kalahari/
```

Expected: empty output. Occurrences of the literal string "wxWidgets" in rationale/history comments are permitted per the spec.

- [ ] **Step 6.4: Success Criterion 4 — full Debug build passes**

If a build has been run since the last commit in Task 5, skip this step. Otherwise:
```bash
cd E:/Python/Projekty/Kalahari
scripts/build_windows.bat Debug
```

Expected: build succeeds, no errors.

- [ ] **Step 6.5: Success Criterion 5 — tests pass at baseline count**

Run:
```bash
cd E:/Python/Projekty/Kalahari
./build-windows/bin/kalahari-tests.exe --list-tests | wc -l
./build-windows/bin/kalahari-tests.exe
```

Expected: test count matches Task 1 Step 1.6 baseline. All tests pass.

- [ ] **Step 6.6: Success Criterion 7 — `.clang-format` validates**

Run:
```bash
cd E:/Python/Projekty/Kalahari
python -c "import yaml; yaml.safe_load(open('.clang-format'))" && echo "YAML OK"
grep -n "^<wx/" .clang-format
```

Expected: `YAML OK` output plus empty `grep` (no more wxWidgets regex).

- [ ] **Step 6.7: Success Criterion 7 — verify CHANGELOG entry exists under `[Unreleased]`**

_(Note: r3 spec revision removed the manual-smoke-test success criterion entirely since Task 3 no longer ports GuiLogSink — it deletes it. Success criterion numbering shifted: r2 had 10 criteria including smoke test; r3 has 9 criteria. See spec r3 Success Criteria section for the canonical list.)_

Run:
```bash
cd E:/Python/Projekty/Kalahari
grep -A 20 "^## \[Unreleased\]" CHANGELOG.md | grep -E "Repo Hygiene|Sub-Project A|wxWidgets Finalization"
```

Expected output: at least one line containing "Repo Hygiene", "Sub-Project A", or "wxWidgets Finalization" from the entry added in Task 5 Step 5.4.

- [ ] **Step 6.8: Success Criteria 8 and 9 — safety net branches exist**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git branch --list 'backup/*' 'wip/*'
```

Expected output:
```
  backup/pre-cleanup-hygiene
  wip/framework-migration
```

- [ ] **Step 6.9: Produce a summary diff for user review**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git log --oneline main..cleanup/repo-hygiene
git diff --stat main..cleanup/repo-hygiene
```

Expected output: 4 commits, the diff stat showing the modified/deleted files grouped by commit. This is the report to show the user when requesting merge approval.

- [ ] **Step 6.10: Present results and request merge approval**

Display the results of Steps 6.1–6.9 to the user. Explicitly state which of the 10 Success Criteria (listed in the spec) are ✅ and which (if any) are not. Ask the user for explicit approval to merge `cleanup/repo-hygiene` into `main`.

**Do NOT merge automatically.** Merge is not part of Sub-Project A per the spec — it requires the user's explicit "yes" after they review the diff.

If the user approves merge, the procedure is:
```bash
cd E:/Python/Projekty/Kalahari
git checkout main
git merge --no-ff cleanup/repo-hygiene -m "Merge Sub-Project A: repo hygiene cleanup (4 commits)"
git log --oneline -5
```

After merge, do not delete `backup/pre-cleanup-hygiene` or `wip/framework-migration` without asking. The user decides when to remove the safety nets and what to do with the WIP branch.

---

## Notes for the Executor

- **r3 note**: Task 3 no longer ports `GuiLogSink` to Qt — it deletes the orphan dead-code files. There is no smoke test in r3. If the build in Task 3 Step 3.4 fails with an error naming `gui_log_sink`, the dead-code analysis was incomplete and deletion must be reverted (`git restore --staged --worktree <files>`); investigate the real dependency before retrying.
- **Do not amend commits once they are followed by another commit on the feature branch.** `git commit --amend` is only safe for the most recent commit. Use `git revert` instead of rewriting history for anything already followed by another commit.
- **Do not use `git add -A`** in any task. All `git add` / `git rm` calls in this plan specify explicit file paths. Broad staging risks accidentally including the framework-migration WIP if it somehow leaks back into the working directory.
- **Language policy:** all new code comments, commit messages, file contents, and doc comments in this plan are in English per the project's naming rules (`.claude/rules/naming.md`). User conversation is in Polish.
