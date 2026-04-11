# Sub-Project D1: Build Quick Wins Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Activate the already-defined `kalahari_add_pch()` function on the two CMake targets and convert the SVG icon conversion from a configure-time `execute_process` to an incremental `add_custom_command`, then document both changes in CHANGELOG.

**Architecture:** Two atomic commits on a feature branch `cleanup/d1-build-wins`. Commit 1 touches only `src/CMakeLists.txt` (add two `kalahari_add_pch()` calls). Commit 2 touches only `CMakeLists.txt` at the root (replace the `execute_process` block with `add_custom_command` + `add_custom_target(ALL)` driven by a `CONFIGURE_DEPENDS` SVG glob) and appends a `[Unreleased]` entry to `CHANGELOG.md`. Both commits are verified by a clean Windows Debug rebuild and a full test suite run at the 603 test case baseline.

**Tech Stack:** CMake 3.21+, `target_precompile_headers` (CMake 3.16+), `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` (CMake 3.12+), `add_custom_command` / `add_custom_target`, Ninja generator, MSVC 19.x, `scripts/build_windows.bat Debug`, Catch2 3 test runner.

**Spec:** `docs/superpowers/specs/2026-04-11-sub-project-d1-build-wins-design.md` (commit `26a45f3`).

---

## File Structure

### Files modified

- `src/CMakeLists.txt` — add two `kalahari_add_pch(...)` calls guarded with `if(COMMAND kalahari_add_pch)` (Task 2)
- `CMakeLists.txt` (root) — replace lines 257-271 (`execute_process` block + surrounding comments) with an `add_custom_command` + `add_custom_target(ALL)` pair driven by a `CONFIGURE_DEPENDS` glob of `resources/icons/**/*.svg` (Task 3)
- `CHANGELOG.md` — append `[Unreleased]` bullet describing both D1 changes (Task 3, same commit as the icon work, per spec "two atomic commits" constraint)

### Files created

None.

### Files deleted

None.

### Files NOT touched (out of scope)

- `cmake/PrecompiledHeaders.cmake` — function definition is already correct; only the caller side changes. Do not touch the header list; Qt6 headers stay commented out per spec non-goal.
- `scripts/convert_all_icons.py` — the script is idempotent (checks `{COLOR_PRIMARY}` before rewriting), so `add_custom_command` works correctly without any Python changes. Do not modify the script.
- `.github/workflows/*` — all three workflows (`ci-linux.yml`, `ci-windows.yml`, `ci-macos.yml`) are already correctly triggered. No changes needed.
- Anything under `src/`, `include/`, `tests/` — this is a build-system-only change. Application code is not touched.

---

## Task 1: Pre-work — Safety check and feature branch setup

**Purpose:** Confirm the working tree is clean, confirm we are starting from the current tip of `main`, and create the feature branch `cleanup/d1-build-wins`. D1 does not need a separate "backup" branch because the feature branch itself is recoverable via `git reset --hard main` at any point and the two commits are independently revertable.

**Files:** No files modified. Only git branch operations.

- [ ] **Step 1.1: Verify clean working tree on main**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git branch --show-current
git status --short
```

Expected output:
- First line: `main`
- Second command: empty, OR at most a single ` M .claude/settings.local.json` line (benign IDE session state that has been drifting throughout the session)

If any OTHER file shows as modified, staged, or untracked, STOP and report before creating the feature branch — unexpected WIP must not be mixed into D1.

- [ ] **Step 1.2: Confirm we are at the expected main HEAD**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git log --oneline -5
```

Expected top entry: `26a45f3 docs(superpowers): add Sub-Project D1 build quick wins design` (or a later commit that does not touch CMake).

- [ ] **Step 1.3: Create and check out the feature branch**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git checkout -b cleanup/d1-build-wins
git branch --show-current
```

Expected output: `cleanup/d1-build-wins`.

- [ ] **Step 1.4: Record the test baseline for post-commit verification**

Run:
```bash
cd E:/Python/Projekty/Kalahari
./build-windows/bin/kalahari-tests.exe --list-tests 2>&1 | tail -2
```

Expected output: a line reading `603 test cases`.

If the binary does not yet exist or the count differs, run a full build first (`scripts/build_windows.bat Debug`) and re-check. The number `603` is the baseline from Sub-Project A Task 1 Step 1.6 and must be preserved across both D1 commits.

---

## Task 2: Commit 1 — Activate precompiled headers

**Purpose:** Call the `kalahari_add_pch()` function on both CMake targets (`kalahari_core` and `kalahari`). The function is already defined in `cmake/PrecompiledHeaders.cmake:5-37` and already `include()`d at root `CMakeLists.txt:251`, but never called.

**Files:**
- Modify: `src/CMakeLists.txt` — two additions

- [ ] **Step 2.1: Read the current `add_library` and `add_executable` lines to confirm line numbers**

Run:
```bash
cd E:/Python/Projekty/Kalahari
grep -n "add_library(kalahari_core\|add_executable(kalahari " src/CMakeLists.txt
```

Expected output:
```
59:add_library(kalahari_core SHARED ${KALAHARI_CORE_SOURCES})
257:add_executable(kalahari ${KALAHARI_SOURCES})
```

Line numbers may drift by ±5 lines across future CMake edits; confirm the `add_library(kalahari_core ...)` and `add_executable(kalahari ...)` calls exist before editing. If the line numbers drifted significantly, adjust the `old_string` in Steps 2.2 and 2.3 by re-reading the surrounding context first.

- [ ] **Step 2.2: Add `kalahari_add_pch(kalahari_core)` after the library definition**

Use the Edit tool on `src/CMakeLists.txt`. Find (the library definition plus the next non-empty line — the exact 3-line window around line 59):

```
add_library(kalahari_core SHARED ${KALAHARI_CORE_SOURCES})
```

Replace with:

```
add_library(kalahari_core SHARED ${KALAHARI_CORE_SOURCES})

# Precompiled headers — activate the function defined in cmake/PrecompiledHeaders.cmake.
# Covers stdlib + spdlog + nlohmann_json. Qt6 headers are intentionally left out of the
# PCH set in Sub-Project D1 (they carry higher risk of MOC macro collisions and will be
# tuned in a later phase). Guarded with if(COMMAND ...) so the build still works if
# PrecompiledHeaders.cmake is ever removed or renamed.
if(COMMAND kalahari_add_pch)
    kalahari_add_pch(kalahari_core)
endif()
```

If the Edit tool reports that `old_string` is not unique or not found, re-read `src/CMakeLists.txt` around line 59 and include one or two lines of surrounding context in both `old_string` and `new_string` until the edit succeeds without ambiguity. Do NOT hand-write positional edits — only use the Edit tool against a read file.

- [ ] **Step 2.3: Add `kalahari_add_pch(kalahari)` after the executable definition**

Use the Edit tool on `src/CMakeLists.txt`. Find:

```
add_executable(kalahari ${KALAHARI_SOURCES})
```

Replace with:

```
add_executable(kalahari ${KALAHARI_SOURCES})

# Precompiled headers for the executable target (same rationale as kalahari_core above).
if(COMMAND kalahari_add_pch)
    kalahari_add_pch(kalahari)
endif()
```

Same fallback rule as Step 2.2: if the edit is ambiguous or fails, re-read the file and include more surrounding context.

- [ ] **Step 2.4: Verify both edits applied**

Run:
```bash
cd E:/Python/Projekty/Kalahari
grep -n "kalahari_add_pch" src/CMakeLists.txt
```

Expected: exactly 2 lines, one near line 59 (inside the `if(COMMAND ...)` guard for `kalahari_core`) and one near line 257+ (for the `kalahari` executable).

- [ ] **Step 2.5: Clean rebuild to trigger PCH generation**

Incremental builds on an already-compiled tree will not show the PCH activation effect. Delete the previous build artifacts for the two targets and rebuild:

```bash
cd E:/Python/Projekty/Kalahari
rm -rf build-windows/CMakeFiles/kalahari_core.dir build-windows/CMakeFiles/kalahari.dir
scripts/build_windows.bat Debug
```

Expected: build succeeds without errors or new warnings. The build log must contain exactly two "Precompiled headers enabled for ..." STATUS messages:

```
-- Precompiled headers enabled for kalahari_core
-- Precompiled headers enabled for kalahari
```

These messages come from `cmake/PrecompiledHeaders.cmake:33` inside the `kalahari_add_pch` function — if they are missing, the edits are not being picked up by CMake; re-verify Steps 2.2 and 2.3.

If the build FAILS with a compilation error that mentions a PCH header (`<string>`, `<vector>`, etc.) in a source file, the PCH header set is conflicting with something in the target's sources. Revert the edits (`git checkout -- src/CMakeLists.txt`) and report the error — do not commit broken state.

- [ ] **Step 2.6: Run tests to confirm the baseline is preserved**

Run:
```bash
cd E:/Python/Projekty/Kalahari
./build-windows/bin/kalahari-tests.exe > /tmp/d1_commit1_tests.txt 2>&1
echo "EXIT: $?"
./build-windows/bin/kalahari-tests.exe --list-tests 2>&1 | tail -2
```

Expected:
- `EXIT: 0`
- Final line: `603 test cases`

If exit is non-zero or the count dropped below 603, STOP and investigate before committing.

- [ ] **Step 2.7: Stage and commit**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git add src/CMakeLists.txt
git status --short
```

Expected output: exactly one line, `M  src/CMakeLists.txt`. If other files appear, verify they are either `.claude/settings.local.json` (harmless background IDE state, do NOT stage) or explicitly expected — otherwise STOP.

Then commit:

```bash
cd E:/Python/Projekty/Kalahari
git commit -m "$(cat <<'EOF'
build(cmake): activate precompiled headers for kalahari_core and kalahari

The kalahari_add_pch() function defined in cmake/PrecompiledHeaders.cmake
has been included via `include(PrecompiledHeaders)` at root CMakeLists.txt
since the Qt6 migration, but never called on any target.

Add two calls: one for the kalahari_core SHARED library, one for the
kalahari executable. Both calls are guarded with if(COMMAND kalahari_add_pch)
so the build still works if the PrecompiledHeaders module is ever removed.

The PCH currently pre-compiles 14 C++ standard library headers plus
spdlog/spdlog.h and nlohmann/json.hpp. Qt6 headers (QtCore/QtCore, etc.)
are intentionally left out of the PCH set for now -- they are commented out
in PrecompiledHeaders.cmake because they carry higher risk of MOC macro
collisions and are better handled in a later tuning pass.

Expected speedup: ~10-15% clean build time reduction. Verified on
Windows Debug; Linux and macOS CI will confirm cross-platform compatibility
automatically on push.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: commit succeeds; pre-commit hook runs and reports `[OK] No C++ files staged for commit.` (the hook only validates C++ files, not CMake). `[cleanup/d1-build-wins <sha>]` appears followed by `1 file changed, <N> insertions(+)`.

---

## Task 3: Commit 2 — Convert `execute_process` to `add_custom_command` + CHANGELOG entry

**Purpose:** Replace the `configure-time` `execute_process` block that unconditionally runs `scripts/convert_all_icons.py` with an `add_custom_command` + `add_custom_target(ALL)` pair that only runs when the SVG sources or the script itself have changed. Also append the D1 `[Unreleased]` entry to `CHANGELOG.md` in the same commit (per spec's "two atomic commits" constraint).

**Files:**
- Modify: `CMakeLists.txt` (root) — replace lines 257-271 (the `execute_process` block plus its two leading comment lines plus the `find_package(Python3 ...)` line and the trailing `if(CONVERT_RESULT ...)` message block)
- Modify: `CHANGELOG.md` — append bullet under `## [Unreleased]` → `### Changed`

- [ ] **Step 3.1: Read the current `execute_process` block to confirm exact content**

Run:
```bash
cd E:/Python/Projekty/Kalahari
sed -n '256,272p' CMakeLists.txt
```

Expected content (approximate, line numbers may drift by ±2):

```
# Convert ALL icons to use color placeholders (run before copying)
# This ensures SVG icons have {COLOR_PRIMARY} placeholders for theming
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/convert_all_icons.py"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE CONVERT_RESULT
    OUTPUT_VARIABLE CONVERT_OUTPUT
    ERROR_VARIABLE CONVERT_ERROR
)
if(CONVERT_RESULT EQUAL 0)
    message(STATUS "SVG icon conversion completed")
else()
    message(WARNING "SVG icon conversion failed: ${CONVERT_ERROR}")
endif()
```

If the block is not found or looks different, STOP, re-read the file, and update the `old_string` in Step 3.2 accordingly before proceeding.

- [ ] **Step 3.2: Replace the `execute_process` block**

Use the Edit tool on `CMakeLists.txt` (root). Find (exact content from Step 3.1):

```
# Convert ALL icons to use color placeholders (run before copying)
# This ensures SVG icons have {COLOR_PRIMARY} placeholders for theming
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/convert_all_icons.py"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE CONVERT_RESULT
    OUTPUT_VARIABLE CONVERT_OUTPUT
    ERROR_VARIABLE CONVERT_ERROR
)
if(CONVERT_RESULT EQUAL 0)
    message(STATUS "SVG icon conversion completed")
else()
    message(WARNING "SVG icon conversion failed: ${CONVERT_ERROR}")
endif()
```

Replace with:

```
# Convert SVG icons to use color placeholders (incremental build via custom_command).
# The conversion only runs when an SVG source file under resources/icons/ or the
# Python script itself changes. On unchanged builds, Ninja skips the step entirely
# because the stamp file is newer than all declared DEPENDS inputs.
#
# Feedback-loop safety: convert_all_icons.py is idempotent (it checks for an existing
# {COLOR_PRIMARY} placeholder before rewriting a path element), and the final
# `${CMAKE_COMMAND} -E touch ${KALAHARI_ICONS_STAMP}` step ensures the stamp is
# always newer than the just-(possibly-)modified SVGs, so the next build correctly
# decides to skip the command.
find_package(Python3 COMPONENTS Interpreter REQUIRED)

file(GLOB_RECURSE KALAHARI_ICON_SVG_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/resources/icons/*.svg"
)

set(KALAHARI_ICONS_STAMP "${CMAKE_BINARY_DIR}/.icons_converted.stamp")

add_custom_command(
    OUTPUT ${KALAHARI_ICONS_STAMP}
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/convert_all_icons.py"
    COMMAND ${CMAKE_COMMAND} -E touch ${KALAHARI_ICONS_STAMP}
    DEPENDS
        ${KALAHARI_ICON_SVG_SOURCES}
        ${CMAKE_SOURCE_DIR}/scripts/convert_all_icons.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Converting SVG icons to color-placeholder format"
    VERBATIM
)

add_custom_target(kalahari_convert_svg_icons ALL
    DEPENDS ${KALAHARI_ICONS_STAMP}
)
```

- [ ] **Step 3.3: Verify the edit applied cleanly**

Run:
```bash
cd E:/Python/Projekty/Kalahari
grep -n "execute_process\|add_custom_command\|kalahari_convert_svg_icons\|KALAHARI_ICONS_STAMP" CMakeLists.txt
```

Expected: no `execute_process` line (the old block is gone), one `add_custom_command` line, one `add_custom_target(kalahari_convert_svg_icons ALL` line, and two references to `KALAHARI_ICONS_STAMP` (the `set()` call and inside the command).

If any `execute_process` match still shows up in the root `CMakeLists.txt`, the edit did not apply; re-read the file and redo Step 3.2.

- [ ] **Step 3.4: First rebuild — expected to execute the conversion**

Run:
```bash
cd E:/Python/Projekty/Kalahari
scripts/build_windows.bat Debug 2>&1 | tail -15
```

Expected: build succeeds; during the build, Ninja runs the icon conversion step exactly once (look for a line matching `Converting SVG icons to color-placeholder format` in the output, or for the Python script's stdout if it prints anything). The build log should NOT contain the old `-- SVG icon conversion completed` STATUS message from the removed `execute_process` — if it does, the edit was incomplete and the old block is still present somewhere.

- [ ] **Step 3.5: Second rebuild — expected to skip the conversion**

Immediately run the build again without changing any files:

```bash
cd E:/Python/Projekty/Kalahari
scripts/build_windows.bat Debug 2>&1 | tail -15
```

Expected: build finishes quickly, Ninja reports `no work to do` or a small number of incremental actions, and the `Converting SVG icons to color-placeholder format` line does NOT appear in the output. If the script runs again on an unchanged tree, the `DEPENDS` list or the stamp file logic is broken — revert and investigate.

- [ ] **Step 3.6: Run the test suite**

Run:
```bash
cd E:/Python/Projekty/Kalahari
./build-windows/bin/kalahari-tests.exe > /tmp/d1_commit2_tests.txt 2>&1
echo "EXIT: $?"
./build-windows/bin/kalahari-tests.exe --list-tests 2>&1 | tail -2
```

Expected:
- `EXIT: 0`
- Final line: `603 test cases`

- [ ] **Step 3.7: Append the D1 entry to `CHANGELOG.md`**

Read the current state of the `## [Unreleased]` section:

```bash
cd E:/Python/Projekty/Kalahari
sed -n '10,30p' CHANGELOG.md
```

The top of `### Changed` should currently contain the "Repo Hygiene" entry from Sub-Project A. Use the Edit tool to insert the new D1 entry immediately ABOVE that entry (most-recent-first order).

Find (exact content):

```
## [Unreleased]

### Changed

- **Repo Hygiene:** wxWidgets Finalization (Sub-Project A + follow-up) - 2026-04-11
```

Replace with:

```
## [Unreleased]

### Changed

- **Build Quick Wins:** Sub-Project D1 - 2026-04-11
  - Activated `kalahari_add_pch()` on both `kalahari_core` (SHARED library) and
    `kalahari` (executable) CMake targets. The function was defined in
    `cmake/PrecompiledHeaders.cmake` since the Qt6 migration but never called.
    Precompiled headers now cover the C++ standard library, spdlog, and
    nlohmann_json, giving a ~10-15% clean-build speedup. Qt6 headers are
    intentionally still excluded from the PCH set (higher risk of MOC macro
    collisions; reserved for a later tuning pass).
  - Replaced the configure-time `execute_process` that ran `convert_all_icons.py`
    on every CMake configure with an `add_custom_command` + `add_custom_target(ALL)`
    pair driven by a `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` of the SVG sources
    under `resources/icons/` and a dependency on the Python script itself. The
    conversion now only runs when an input actually changes; unchanged builds skip
    the step entirely.
  - Out of original Sub-Project D scope: macOS CI trigger (already active,
    confirmed via `gh run list` against recent commits) and clang-format CI
    enforcement (deferred to its own mini-project — existing codebase has roughly
    50 000 formatting violations across 229 tracked source/header files, so
    enforcement requires a dedicated strategy decision before it can be safely
    turned on).

- **Repo Hygiene:** wxWidgets Finalization (Sub-Project A + follow-up) - 2026-04-11
```

- [ ] **Step 3.8: Verify the CHANGELOG edit**

Run:
```bash
cd E:/Python/Projekty/Kalahari
grep -A 2 "Build Quick Wins" CHANGELOG.md | head -5
```

Expected: the bullet appears under `### Changed` in `## [Unreleased]`.

- [ ] **Step 3.9: Stage and commit**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git add CMakeLists.txt CHANGELOG.md
git status --short
```

Expected: exactly two lines:
```
M  CHANGELOG.md
M  CMakeLists.txt
```

Plus the benign ` M .claude/settings.local.json` (unstaged, ignore).

Then commit:

```bash
cd E:/Python/Projekty/Kalahari
git commit -m "$(cat <<'EOF'
build(cmake): convert SVG icon conversion to add_custom_command

Replace the configure-time execute_process block at root CMakeLists.txt
with add_custom_command + add_custom_target(ALL) driven by a CONFIGURE_DEPENDS
glob of resources/icons/*.svg and a dependency on the Python script itself.

Before: The script ran on every `cmake` configure, regardless of whether
any SVG input or the script itself had actually changed.

After: The script runs only when at least one input is newer than the
stamp file. On subsequent builds where nothing changed, Ninja skips the
step entirely.

Feedback-loop safety: convert_all_icons.py is idempotent (it checks for
an existing {COLOR_PRIMARY} placeholder in each path element before
rewriting) and the custom_command ends with `touch` on the stamp file,
so the stamp is always newer than the (possibly-modified) SVGs. Subsequent
builds compare timestamps and correctly decide to skip.

Also appends a [Unreleased] entry to CHANGELOG.md summarizing both
Sub-Project D1 changes (PCH activation and this icon conversion refactor).

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: commit succeeds; pre-commit hook reports `[OK] No C++ files staged for commit.`; `[cleanup/d1-build-wins <sha>]` line with `2 files changed, <N> insertions(+), <M> deletions(-)`.

---

## Task 4: Merge to main and push to GitHub

**Purpose:** Integrate the two-commit feature branch into `main` with a `--no-ff` merge commit, run a final verification on the merged result, then push to `origin/main`.

**Files:** No files modified. Git operations only.

- [ ] **Step 4.1: Verify the feature branch state before merge**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git log --oneline main..cleanup/d1-build-wins
```

Expected output: exactly 2 lines — one `build(cmake): convert SVG icon conversion to add_custom_command` on top of `build(cmake): activate precompiled headers for kalahari_core and kalahari`.

- [ ] **Step 4.2: Switch to main and confirm it is clean**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git checkout main
git status
```

Expected: `On branch main`, `working tree clean` (or only the benign `.claude/settings.local.json` showing as unstaged — if so, `git restore .claude/settings.local.json` first to avoid confusing the merge).

- [ ] **Step 4.3: Fetch and confirm `origin/main` has not moved**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git fetch origin main
git rev-list --count main..origin/main
```

Expected output: `0` (origin/main has not advanced past local main since the last push in Sub-Project A wrap-up).

If the count is non-zero, someone pushed to the remote between Sub-Project A's push and now — STOP and report; that pre-existing scenario is out of scope for D1 and needs separate handling (rebase local work).

- [ ] **Step 4.4: Merge with `--no-ff`**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git merge --no-ff cleanup/d1-build-wins -m "Merge Sub-Project D1: build quick wins (2 commits)

Activates precompiled headers on kalahari_core and kalahari, and
converts the SVG icon conversion from a configure-time execute_process
to an incremental add_custom_command.

Out of original D1 audit scope: macOS CI trigger (already active,
confirmed via gh run list) and clang-format CI enforcement (deferred
-- existing codebase has ~50000 formatting violations across 229 files,
needs its own strategy decision in a separate brainstorm)."
```

Expected: merge succeeds without conflicts (both commits touch files unrelated to anything else currently tracked). `Merge made by the 'ort' strategy.` or similar, followed by a summary of changed files (should be `src/CMakeLists.txt`, `CMakeLists.txt`, and `CHANGELOG.md`).

- [ ] **Step 4.5: Verify the merge commit landed**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git log --oneline -5
```

Expected top entry: `<sha> Merge Sub-Project D1: build quick wins (2 commits)`.

- [ ] **Step 4.6: Final post-merge build + test**

Run:
```bash
cd E:/Python/Projekty/Kalahari
scripts/build_windows.bat Debug 2>&1 | tail -8
./build-windows/bin/kalahari-tests.exe > /tmp/d1_post_merge_tests.txt 2>&1
echo "EXIT: $?"
./build-windows/bin/kalahari-tests.exe --list-tests 2>&1 | tail -2
```

Expected: build succeeds (Ninja likely reports `no work to do` because both commit 2's rebuild and the merge don't introduce new work); `EXIT: 0`; final line `603 test cases`.

- [ ] **Step 4.7: Push main to origin**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git push origin main 2>&1 | tail -5
```

Expected output: the push succeeds with a line like `<old-sha>..<new-sha> main -> main`. GitHub will automatically trigger the three CI workflows (Linux, Windows, macOS) on the pushed commit; those are not part of D1's success criteria but their cross-platform verification confirms the PCH and custom_command changes work beyond local Windows.

- [ ] **Step 4.8: Delete the merged feature branch**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git branch -d cleanup/d1-build-wins
git branch --list 'cleanup/*' 2>&1 || echo "(no cleanup branches)"
```

Expected: `Deleted branch cleanup/d1-build-wins (was <sha>).`

- [ ] **Step 4.9: Final success criteria walkthrough**

Walk through the 8 success criteria from the spec and confirm each:

1. Two commits on `cleanup/d1-build-wins` — verified in Step 4.1
2. Clean rebuild after each commit — verified in Steps 2.5 and 3.4
3. `kalahari-tests.exe` exits 0 with 603 test cases after each commit — verified in Steps 2.6, 3.6, and 4.6
4. Build log shows `Precompiled headers enabled for kalahari_core` and `Precompiled headers enabled for kalahari` — verified in Step 2.5
5. Consecutive build on commit 2 shows Ninja skipping the `kalahari_convert_svg_icons` target — verified in Step 3.5
6. Merge to main without conflict — verified in Step 4.4
7. `git push origin main` succeeds — verified in Step 4.7
8. `CHANGELOG.md` has an `[Unreleased]` entry — verified in Step 3.8

Report the outcome to the user. If any success criterion is ❌, explain what failed and what is needed to recover.

---

## Notes for the Executor

- **Do not touch `.claude/settings.local.json`** — it drifts as the session runs (background Claude Code permission state). It is tracked but intentionally ignored by every commit in this plan. If it appears as staged at any point (`M  .claude/settings.local.json`, first-column M), unstage it with `git restore --staged .claude/settings.local.json` before committing.
- **Do not use `git add -A` or `git add .`** anywhere in this plan. Every `git add` call specifies explicit file paths to avoid accidentally including the `.claude/settings.local.json` drift or any other unrelated working-tree change.
- **If PCH activation (Task 2 Step 2.5) breaks the build with a header-related compilation error**, revert the commit (`git checkout -- src/CMakeLists.txt`), report the specific header and source file involved, and STOP. Do not try to fix the issue inline — that would expand D1 scope into "PCH compatibility triage" which is D2 territory.
- **If the icon `add_custom_command` (Task 3 Step 3.5) re-runs on an unchanged tree**, it means the stamp file logic or the `DEPENDS` list is incorrect. Revert Task 3 (`git reset --hard HEAD~1` BEFORE committing Task 3, OR `git revert <sha>` if already committed) and report. Do not chase the issue inside D1.
- **If the user is present, pause briefly between Task 2 commit and Task 3 start** to let them confirm commit 1 looks right. This is `inline execution with checkpoints` per executing-plans skill; checkpoints are after each commit.
- **Language policy:** all new code comments, commit messages, file contents, and doc comments in this plan are in English per the project's naming rules (`.claude/rules/naming.md`). User conversation continues in Polish.
