# Sub-Project D1: Build Quick Wins — Design Document

## Goal

Two small, safe, long-term positive CMake changes: activate the already-defined precompiled headers (PCH) function to speed up the compilation of `kalahari_core` and `kalahari` targets, and convert the SVG icon conversion step from a `configure-time` `execute_process` into a proper `add_custom_command` + `add_custom_target(ALL)` pair so the script only runs when its inputs actually change.

These are the two genuinely "easy wins" from the original Sub-Project D (CI Hardening) audit punch list. The other D items (clang-format enforcement, clang-tidy enforcement, ASAN/UBSAN CI, additional warning flags) are deferred to later phases (D2 and beyond) because they would trigger large-scale fixes in the existing codebase; D1 is deliberately scoped to zero-risk infrastructure improvements that do not touch application code.

## Revision History

- **2026-04-11 r1** — Initial spec.
- **Scope reductions during brainstorming exploration (not saved as separate revision)**:
  - Audit claim "macOS CI not triggered" → **FALSE**. `.github/workflows/ci-macos.yml` already triggers on `push: main/develop`, `pull_request: main/develop`, and `workflow_dispatch`, verified via `gh run list` showing MACOS workflow runs against recent commits. Point removed from D1 scope.
  - Audit claim "add clang-format CI job" → **NOT AN EASY WIN**. Direct verification with `clang-format --dry-run` on 10 sample source files showed an average of ~224 formatting violations per file across the codebase; extrapolated to ~50 000 violation lines across 229 tracked source/header files. Enabling CI enforcement without a prior auto-format pass would immediately block all future pushes. Deferred to its own mini-project (separate brainstorming cycle) where the strategy (full auto-format vs. incremental `clang-format-diff`) can be decided deliberately.

## Position in Overall Initiative

```
A: Repo Hygiene Cleanup         ← DONE (merged to main, pushed)
   ↓
D1: Build Quick Wins            ← THIS SPEC
   ↓
D2: CI Hardening                (clang-format enforcement, clang-tidy, ASAN/UBSAN, extra warnings)
   ↓
C: Theme System Foundation
   ↓
B: BookEditor Refactor
```

## Goals / Non-Goals

### Goals
- Activate precompiled headers for both `kalahari_core` (SHARED library) and `kalahari` (executable) targets by calling the existing `kalahari_add_pch()` function defined in `cmake/PrecompiledHeaders.cmake`
- Replace the `configure-time` `execute_process` block for SVG icon conversion (root `CMakeLists.txt:260-271`) with `add_custom_command` + `add_custom_target(ALL)` driven by `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` on the actual SVG source set plus a dependency on the Python script itself
- Verify full build (Windows Debug) and test baseline (603 test cases passing) after each commit

### Non-Goals
- **Adding Qt6 headers (`<QtCore/QtCore>`, `<QtWidgets/QtWidgets>`, `<QtGui/QtGui>`) to the PCH set** — higher potential speedup (~30-40% vs ~10-15% with stdlib+spdlog+json alone) but also higher risk (MOC macro collisions, Qt's namespace-polluting headers, potential breakage of MSVC `/W4 /WX` builds). Future tuning, not D1.
- **clang-format CI enforcement** — see Revision History above; defer to its own mini-project
- **clang-tidy CI enforcement, ASAN/UBSAN CI jobs, additional warning flags (-Wshadow etc.)** — all D2 scope; each would surface large-scale existing-code issues requiring its own design cycle
- **macOS CI trigger** — already active, nothing to do
- **Modifying `scripts/convert_all_icons.py`** — the script is idempotent (checks for `{COLOR_PRIMARY}` before rewriting), so no change is needed to make `add_custom_command` feedback-loop-free; the script stays untouched
- **Linux / macOS build verification** — primary verification is Windows (project's primary dev platform); the Linux and macOS CI runs that trigger automatically on push will provide cross-platform verification in the background

## Scope — Two Atomic Commits

### Commit 1: Activate precompiled headers on `kalahari_core` and `kalahari`

**File modified:** `src/CMakeLists.txt`

**Change:** After `add_library(kalahari_core SHARED ${KALAHARI_CORE_SOURCES})` (around line 59), add:

```cmake
# Precompiled headers — activate the function defined in cmake/PrecompiledHeaders.cmake
# (stdlib + spdlog + nlohmann_json; Qt6 headers intentionally left out of PCH scope
# in Sub-Project D1 because they carry higher risk of macro/MOC conflicts). Guarded
# with if(COMMAND ...) so the build still works if PrecompiledHeaders.cmake is ever
# removed or renamed.
if(COMMAND kalahari_add_pch)
    kalahari_add_pch(kalahari_core)
endif()
```

After `add_executable(kalahari ${KALAHARI_SOURCES})` (around line 257), add the analogous call for the executable target:

```cmake
if(COMMAND kalahari_add_pch)
    kalahari_add_pch(kalahari)
endif()
```

**Rationale:** `kalahari_add_pch` is already defined in `cmake/PrecompiledHeaders.cmake:5-37` and already included via `include(PrecompiledHeaders)` at root `CMakeLists.txt:251`, but it is never called. Wiring it up is a two-block addition.

**Expected speedup:** 10-15% reduction in clean build time (the PCH currently covers 14 C++ stdlib headers plus `spdlog/spdlog.h` and `nlohmann/json.hpp`; Qt6 headers are commented out in the PCH function body and are deliberately NOT enabled in this phase).

**Risk:** Low. `target_precompile_headers` is supported since CMake 3.16, the project requires 3.21+, and the header set has no known macro conflicts with the rest of the codebase. If PCH activation breaks the build for any reason, reverting the single commit restores the previous state.

### Commit 2: Convert SVG icon conversion from `execute_process` to `add_custom_command`

**File modified:** root `CMakeLists.txt`

**Change:** Replace lines 257-271 (`execute_process` block with its surrounding comments) with a `add_custom_command` + `add_custom_target` pair that tracks the actual SVG sources and the Python script as dependencies:

```cmake
# Convert ALL icons to use color placeholders (incremental build via custom_command).
# This ensures SVG icons have {COLOR_PRIMARY} placeholders for theming.
# The conversion is rerun only when an SVG source file or the script itself changes;
# on subsequent configures where nothing changed, CMake skips the custom_command
# because the stamp file timestamp is newer than all declared inputs.
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

**Rationale:** The current `execute_process` runs unconditionally every time CMake configures the project (which happens whenever `CMakeLists.txt` is touched, the CMake cache is cleared, or the build directory is regenerated). Converting to `add_custom_command` ties the conversion to its real inputs: the SVG source files under `resources/icons/` and the Python script itself. On incremental builds where nothing in those inputs changed, Ninja skips the command entirely.

**Feedback-loop safety:** `convert_all_icons.py` rewrites SVG files in place, but only paths that do not already contain `{COLOR_PRIMARY}` or `{COLOR_SECONDARY}` (see `scripts/convert_all_icons.py` `convert_svg_file` function, lines ~20-30). After the first run, subsequent runs of the script are no-ops at the file level. The `add_custom_command` adds a final `${CMAKE_COMMAND} -E touch ${KALAHARI_ICONS_STAMP}` step that creates the stamp file AFTER the script finishes, so the stamp is always newer than the just-(possibly-)touched SVG files. On the next build, CMake compares input timestamps to the stamp and correctly skips the command.

**CONFIGURE_DEPENDS caveat:** `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` tells CMake to re-glob on every build (not just configure), so adding or removing an SVG file triggers a reconfigure automatically. This is supported since CMake 3.12 and the project requires 3.21+. The documentation warns it has some runtime overhead on very large glob sets, but ~200-300 SVG files is well within the safe zone.

**Risk:** Low. Build graph changes only; no runtime or behavior impact.

## Verification Plan

| # | Commit | Verification | Build |
|---|---|---|---|
| 1 | PCH activation | (a) `scripts/build_windows.bat Debug` — clean rebuild succeeds without errors or new warnings; (b) `./build-windows/bin/kalahari-tests.exe` — exit code 0, test count matches the 603 baseline from Sub-Project A Task 1; (c) build log shows the "Precompiled headers enabled for kalahari_core" and "Precompiled headers enabled for kalahari" STATUS messages from `PrecompiledHeaders.cmake:33` | Yes (clean rebuild) |
| 2 | Icon custom_command | (a) `scripts/build_windows.bat Debug` — build succeeds; (b) running the build a **second time** immediately afterwards must complete with Ninja skipping the `convert_svg_icons` step (no re-run of the Python script); (c) touching any file under `resources/icons/` and rebuilding must trigger exactly one re-run of the conversion step; (d) `./build-windows/bin/kalahari-tests.exe` — exit code 0 | Yes |

The first commit requires a clean rebuild (`rm -rf build-windows/CMakeFiles/kalahari*` or equivalent) to actually see the PCH activation effect on build time; an incremental build on top of an already-compiled tree will not recompile anything.

**Baseline reminder**: 603 test cases passing (from Sub-Project A Task 1 Step 1.6 measurement). Both commits must preserve this.

## Rollback

Both commits are isolated to CMake files and touch no application code, no headers, no tests. Rollback for either commit is a single `git revert <sha>` or, if still on an unpushed feature branch, `git reset --hard HEAD~1`.

If the PCH commit causes a subtle Windows-only compilation error that is only caught during post-merge CI runs, revert on main (`git revert <sha>`, push) — the other D1 commit remains independently valid and does not need to be rolled back with it. The two commits are intentionally independent.

If the icon custom_command commit produces stale icons (the script does not run when it should), the quickest workaround is to delete the stamp file: `rm build-windows/.icons_converted.stamp` and rebuild. If the custom_command produces excessive re-runs (the script runs on every build instead of only when SVGs change), revert and investigate separately.

## Branch Strategy

Feature branch `cleanup/d1-build-wins` from current `main` (`c4c28ec` or newer). Two atomic commits on the feature branch, then merge `--no-ff` to main with a summary merge commit, then push. No pre-work safety branch needed (the working tree on `main` is currently clean; there is no framework-migration WIP to isolate this time).

## Commit Message Templates

### Commit 1
```
build(cmake): activate precompiled headers for kalahari_core and kalahari

The kalahari_add_pch() function defined in cmake/PrecompiledHeaders.cmake
has been included via `include(PrecompiledHeaders)` at root CMakeLists.txt
since the Qt6 migration, but never called on any target.

Add two calls: one for the kalahari_core SHARED library, one for the
kalahari executable. Both calls are guarded with if(COMMAND kalahari_add_pch)
so the build still works if the PrecompiledHeaders module is ever removed.

The PCH currently pre-compiles 14 C++ standard library headers plus
spdlog/spdlog.h and nlohmann/json.hpp. Qt6 headers (QtCore/QtCore, etc.)
are intentionally left out of the PCH set for now — they are commented out
in PrecompiledHeaders.cmake because they carry higher risk of MOC macro
collisions and are better handled in a later tuning pass.

Expected speedup: ~10-15% clean build time reduction. Verified on
Windows Debug; Linux and macOS CI will confirm cross-platform compatibility
automatically on push.
```

### Commit 2
```
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
```

### Merge commit
```
Merge Sub-Project D1: build quick wins (2 commits)

Activates precompiled headers on kalahari_core and kalahari, and
converts the SVG icon conversion from a configure-time execute_process
to an incremental add_custom_command.

Out of original D1 audit scope: macOS CI trigger (already active,
confirmed via gh run list) and clang-format CI enforcement (deferred
— existing codebase has ~50000 formatting violations across 229 files,
needs its own strategy decision in a separate brainstorm).
```

## Success Criteria

Sub-Project D1 is done when all of the following are ✅:

1. Two commits on `cleanup/d1-build-wins` branch, each with the message above
2. Clean rebuild of `scripts/build_windows.bat Debug` succeeds after each commit
3. `./build-windows/bin/kalahari-tests.exe` exits 0 with 603 test cases after each commit (baseline preserved)
4. Build log (on commit 1) shows `Precompiled headers enabled for kalahari_core` and `Precompiled headers enabled for kalahari`
5. Consecutive build on commit 2 (no source changes) shows Ninja skipping the `kalahari_convert_svg_icons` target (i.e., script does not re-run on an unchanged tree)
6. Merge to main completes without conflict (branch is built off current main, touches only two CMake files)
7. `git push origin main` succeeds
8. CHANGELOG.md has an `[Unreleased]` entry summarizing the D1 changes

## Out of Scope (reminder)

- Qt6 headers in PCH — deferred
- clang-format enforcement — deferred to its own mini-project
- clang-tidy enforcement — D2 scope
- ASAN/UBSAN CI jobs — D2 scope
- Additional warning flags (`-Wshadow` etc.) — D2 scope
- macOS CI trigger — already active
- Modifying the Python icon conversion script — not needed

## Deliverables

- This design document: `docs/superpowers/specs/2026-04-11-sub-project-d1-build-wins-design.md`
- After user implicit approval (user said "robimy zgodnie z twoją rekomendacją — ufam ci"): invoke `superpowers:writing-plans` to produce a short implementation plan, then execute inline.
