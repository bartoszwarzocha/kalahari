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
- `tests/gui/test_bwx_text_document.cpp` (tracked, Task 4)
- `tests/gui/test_bwx_text_renderer.cpp` (tracked, Task 4)
- `src/README.md` (tracked, Task 5)

### Files modified

- `src/core/gui_log_sink.cpp` — replace `wxTheApp->CallAfter()` at two call sites with `QMetaObject::invokeMethod(panel, ..., Qt::QueuedConnection)` (Task 3)
- `include/kalahari/core/gui_log_sink.h` — update doc comments (wxWidgets → Qt terminology) (Task 3)
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

## Task 3: Commit 2 — Port GuiLogSink from wxWidgets to Qt

**Purpose:** Replace `wxTheApp->CallAfter()` with `QMetaObject::invokeMethod(panel, ..., Qt::QueuedConnection)` at both call sites in `src/core/gui_log_sink.cpp`, remove `#include <wx/app.h>`, add `#include <QMetaObject>` and `#include <QCoreApplication>`, and update stale wxWidgets terminology in `include/kalahari/core/gui_log_sink.h` doc comments. This is the last active wxWidgets code in `src/` — after this commit, the Qt6 migration is complete in all production code.

**Files:**
- Modify: `src/core/gui_log_sink.cpp` (2 call sites + includes)
- Modify: `include/kalahari/core/gui_log_sink.h` (doc comments only — no include changes)

- [ ] **Step 3.1: Read the current state of both files to confirm line numbers have not drifted**

Run:
```bash
cd E:/Python/Projekty/Kalahari
grep -n "wx" src/core/gui_log_sink.cpp include/kalahari/core/gui_log_sink.h
```

Expected: matches at `src/core/gui_log_sink.cpp:6` (the `#include <wx/app.h>`), `:46`, `:50` (`wxTheApp->CallAfter`), `:53` (`IsBeingDeleted`), `:75`, `:86` (second call site), plus doc-comment matches in the header. If line numbers differ significantly from these (±5 lines), re-read the files before applying edits below.

- [ ] **Step 3.2: Update includes in `src/core/gui_log_sink.cpp`**

Use the Edit tool to perform the following replacement:

File: `src/core/gui_log_sink.cpp`

Find:
```
#include <kalahari/core/gui_log_sink.h>
#include <kalahari/gui/panels/log_panel.h>
#include <wx/app.h>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/pattern_formatter.h>
```

Replace with:
```
#include <kalahari/core/gui_log_sink.h>
#include <kalahari/gui/panels/log_panel.h>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/pattern_formatter.h>
#include <QMetaObject>
#include <QCoreApplication>
```

- [ ] **Step 3.3: Replace the `sink_it_()` marshalling block**

File: `src/core/gui_log_sink.cpp`

Find:
```
    // Active mode: Forward to LogPanel immediately
    // Marshal to main GUI thread using wxTheApp->CallAfter()
    // This is thread-safe - wxWidgets queues the lambda for execution on main thread
    if (wxTheApp) {
        // Capture raw pointer (safe: CallAfter won't execute if app is shutting down)
        gui::LogPanel* panel = m_panel;

        wxTheApp->CallAfter([panel, log_message]() {
            // On main thread: check if panel is still valid
            // wxWidgets handles deleted windows gracefully
            if (panel && !panel->IsBeingDeleted()) {
                panel->appendLog(log_message);
            }
        });
    }
```

Replace with:
```
    // Active mode: Forward to LogPanel immediately.
    // Marshal to main GUI thread via Qt event loop.
    // QMetaObject::invokeMethod with panel as target + Qt::QueuedConnection
    // posts the lambda to the panel's event loop (main thread). If the panel
    // is destroyed before the event is processed, Qt drops the invocation
    // silently (no IsBeingDeleted equivalent needed).
    if (qApp) {
        gui::LogPanel* panel = m_panel;
        QMetaObject::invokeMethod(
            panel,
            [panel, log_message]() {
                panel->appendLog(log_message);
            },
            Qt::QueuedConnection);
    }
```

- [ ] **Step 3.4: Replace the `setPanel()` backfill block**

File: `src/core/gui_log_sink.cpp`

Find:
```
    // Backfill panel with limited history (respect panel's ring buffer size)
    if (!m_messageHistory.empty() && wxTheApp) {
        // Get panel's max buffer size to avoid overwhelming it
        size_t panelMaxSize = panel->getMaxBufferSize();
        size_t backfillCount = std::min(m_messageHistory.size(), panelMaxSize);

        // Copy last N messages to temporary buffer
        std::deque<std::string> history;
        auto start_it = m_messageHistory.end() - static_cast<std::deque<std::string>::difference_type>(backfillCount);
        history.assign(start_it, m_messageHistory.end());

        // Backfill on main thread
        wxTheApp->CallAfter([panel, history]() {
            if (panel && !panel->IsBeingDeleted()) {
                for (const auto& msg : history) {
                    panel->appendLog(msg);
                }
            }
        });

        // Clear history buffer after backfill
        m_messageHistory.clear();
    }
```

Replace with:
```
    // Backfill panel with limited history (respect panel's ring buffer size)
    if (!m_messageHistory.empty() && qApp) {
        // Get panel's max buffer size to avoid overwhelming it
        size_t panelMaxSize = panel->getMaxBufferSize();
        size_t backfillCount = std::min(m_messageHistory.size(), panelMaxSize);

        // Copy last N messages to temporary buffer
        std::deque<std::string> history;
        auto start_it = m_messageHistory.end() - static_cast<std::deque<std::string>::difference_type>(backfillCount);
        history.assign(start_it, m_messageHistory.end());

        // Backfill on main thread via Qt event loop.
        // invokeMethod with panel as target silently drops if panel destroyed.
        QMetaObject::invokeMethod(
            panel,
            [panel, history]() {
                for (const auto& msg : history) {
                    panel->appendLog(msg);
                }
            },
            Qt::QueuedConnection);

        // Clear history buffer after backfill
        m_messageHistory.clear();
    }
```

- [ ] **Step 3.5: Update `include/kalahari/core/gui_log_sink.h` doc comment in the file-level block**

File: `include/kalahari/core/gui_log_sink.h`

Find:
```
/// @file gui_log_sink.h
/// @brief Custom spdlog sink for GUI log panel
///
/// Thread-safe spdlog sink that forwards log messages to LogPanel.
/// Uses wxTheApp->CallAfter() to marshal GUI calls to main thread.
```

Replace with:
```
/// @file gui_log_sink.h
/// @brief Custom spdlog sink for GUI log panel
///
/// Thread-safe spdlog sink that forwards log messages to LogPanel.
/// Uses QMetaObject::invokeMethod with Qt::QueuedConnection to marshal
/// GUI calls to main thread.
```

- [ ] **Step 3.6: Update the Thread Safety / Lifetime Safety block in the class doc comment**

File: `include/kalahari/core/gui_log_sink.h`

Find:
```
/// Thread Safety:
/// - sink_it_() can be called from any thread (protected by base_sink mutex)
/// - GUI operations marshalled to main thread via wxTheApp->CallAfter()
/// - Raw pointer is safe: wxTheApp->CallAfter() won't execute if app is shutting down
///
/// Lifetime Safety:
/// - LogPanel is owned by wxAUI/MainWindow
/// - Sink is destroyed when logger is destroyed (app shutdown)
/// - If LogPanel is destroyed first, CallAfter() lambda will safely do nothing
```

Replace with:
```
/// Thread Safety:
/// - sink_it_() can be called from any thread (protected by base_sink mutex)
/// - GUI operations marshalled to main thread via QMetaObject::invokeMethod
/// - Panel target: if panel is destroyed before event delivery, Qt silently
///   drops the invocation (no manual validity check needed)
///
/// Lifetime Safety:
/// - LogPanel is owned by QMainWindow / QDockWidget parent
/// - Sink is destroyed when logger is destroyed (app shutdown)
/// - If LogPanel is destroyed first, queued invokeMethod is dropped silently
```

- [ ] **Step 3.7: Update the `m_panel` member doc comment**

File: `include/kalahari/core/gui_log_sink.h`

Find:
```
    /// @brief Raw pointer to LogPanel (owned by wxWidgets, not ref-counted)
    gui::LogPanel* m_panel;
```

Replace with:
```
    /// @brief Raw pointer to LogPanel (owned by Qt parent, not ref-counted)
    gui::LogPanel* m_panel;
```

- [ ] **Step 3.8: Verify no wxWidgets references remain in the two files**

Run:
```bash
cd E:/Python/Projekty/Kalahari
grep -n "wx\|WX\|CallAfter\|IsBeingDeleted" src/core/gui_log_sink.cpp include/kalahari/core/gui_log_sink.h
```

Expected output: empty. If any match remains, STOP and fix before proceeding to build.

- [ ] **Step 3.9: Full Debug build**

Run:
```bash
cd E:/Python/Projekty/Kalahari
scripts/build_windows.bat Debug
```

Expected: build succeeds, no errors, no new warnings related to `gui_log_sink`. Build takes ~10 min. If build fails with errors in `gui_log_sink.cpp` or `gui_log_sink.h`, do NOT commit — go back to the failing step and fix.

- [ ] **Step 3.10: Run the test binary and compare count to baseline**

Run:
```bash
cd E:/Python/Projekty/Kalahari
./build-windows/bin/kalahari-tests.exe --list-tests | wc -l
./build-windows/bin/kalahari-tests.exe
```

Expected: test count matches the baseline recorded in Step 1.6. All tests pass (same count as baseline). If the count is lower or any tests fail, STOP and investigate before committing.

- [ ] **Step 3.11: Manual smoke test — log messages reach LogPanel**

Run:
```bash
cd E:/Python/Projekty/Kalahari
./build-windows/bin/kalahari.exe
```

In the running app:
1. Ensure the LogPanel dock is visible. If hidden, enable it via the View menu (look for "Log" or "Log Panel" item).
2. Open an example project from the `examples/` directory (e.g., `examples/Test001_ShortStory/`) — this should generate project-load log messages.
3. Open the Settings dialog, switch the theme (Light ↔ Dark, or another available theme), and apply. This should generate theme-change log messages.
4. Close the current document, then reopen it. This should generate document-close and document-open log messages.
5. Visually confirm that after each action, **new log lines appeared in LogPanel** with timestamps matching the actions just performed.

If log messages do not appear in LogPanel, DO NOT COMMIT. Execute the E1 escalation per the spec: attempt up to 3 fixes; if all fail, `git reset --hard HEAD~1` to drop the port and continue Sub-Project A without it as a 3-commit version. The rest of Sub-Project A has independent value.

If log messages appear correctly, close `kalahari.exe` and proceed.

- [ ] **Step 3.12: Stage and commit**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git add src/core/gui_log_sink.cpp include/kalahari/core/gui_log_sink.h
git status --short src/core/ include/kalahari/core/
```

Expected: 2 `M` lines for the modified files.

Then:
```bash
cd E:/Python/Projekty/Kalahari
git commit -m "$(cat <<'EOF'
refactor(core): port GuiLogSink from wxWidgets to Qt

Replace wxTheApp->CallAfter() with QMetaObject::invokeMethod and
Qt::QueuedConnection for marshalling log messages to the main GUI
thread.

- Remove #include <wx/app.h>
- Add #include <QMetaObject>, #include <QCoreApplication>
- sink_it_(): post lambda to LogPanel event loop
- setPanel(): same change for backfill path
- Drop panel->IsBeingDeleted() check (Qt handles deleted targets)
- Update header doc comments (wxTheApp -> QMetaObject::invokeMethod,
  wxAUI -> QMainWindow/QDockWidget)

This is the last active wxWidgets code in src/. After this commit,
the Qt6 migration is complete in all production code.

Verified via manual smoke test: log messages appear in LogPanel
after the port.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

Expected: commit succeeds, pre-commit hook runs and passes. `2 files changed, <N> insertions(+), <M> deletions(-)`.

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

- [ ] **Step 6.7: Success Criterion 6 — confirm smoke test result is on record**

The manual smoke test was performed in Task 3 Step 3.11 (or skipped if E1 escalation was triggered). Confirm out loud which outcome applies:
- ✅ Smoke test passed — log messages appeared in LogPanel during Task 3 Step 3.11
- ⚠️ E1 escalation — commit 2 was dropped, `GuiLogSink` port deferred to a follow-up sub-project

No re-run needed here; this is a documentation-of-outcome step.

- [ ] **Step 6.8: Success Criterion 8 — verify CHANGELOG entry exists under `[Unreleased]`**

Run:
```bash
cd E:/Python/Projekty/Kalahari
grep -A 20 "^## \[Unreleased\]" CHANGELOG.md | grep -E "Repo Hygiene|Sub-Project A|wxWidgets Finalization"
```

Expected output: at least one line containing "Repo Hygiene", "Sub-Project A", or "wxWidgets Finalization" from the entry added in Task 5 Step 5.4.

- [ ] **Step 6.9: Success Criteria 9 and 10 — safety net branches exist**

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

- [ ] **Step 6.10: Produce a summary diff for user review**

Run:
```bash
cd E:/Python/Projekty/Kalahari
git log --oneline main..cleanup/repo-hygiene
git diff --stat main..cleanup/repo-hygiene
```

Expected output: 4 commits, the diff stat showing the modified/deleted files grouped by commit. This is the report to show the user when requesting merge approval.

- [ ] **Step 6.11: Present results and request merge approval**

Display the results of Steps 6.1–6.10 to the user. Explicitly state which of the 10 Success Criteria (listed in the spec) are ✅ and which (if any) are not. Ask the user for explicit approval to merge `cleanup/repo-hygiene` into `main`.

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

- **Do not skip manual smoke test in Task 3 Step 3.11.** It is the only verification gate for the `GuiLogSink` port. If the executor runs as an automated agent that cannot interactively run `kalahari.exe`, pause and ask the user to run the smoke test manually, then report the result back.
- **Do not amend commits once they are followed by another commit on the feature branch.** `git commit --amend` is only safe for the most recent commit. If Task 4 Step 4.7 commits and then Task 3 needs a fix, use `git revert` instead of rewriting history.
- **Do not use `git add -A`** in any task. All `git add` calls in this plan specify explicit file paths. Broad staging risks accidentally including the framework-migration WIP if it somehow leaks back into the working directory.
- **If Task 3 Step 3.11 smoke test fails and E1 escalation is triggered**, adjust Task 6 Step 6.1 expectation from 4 commits to 3. The plan is designed so that Tasks 2, 4, and 5 retain independent value without Task 3.
- **Language policy:** all new code comments, commit messages, file contents, and doc comments in this plan are in English per the project's naming rules (`.claude/rules/naming.md`). User conversation is in Polish.
