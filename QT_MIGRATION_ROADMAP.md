# Qt Migration Roadmap - Strategic Plan

**Decision Date:** 2025-11-19
**Current State:** wxWidgets 3.3.0 + bwx_sdk (Phase 1, Week 13)
**Target State:** Qt 6.x (LGPL) + Clean Architecture
**Decision:** APPROVED - Migrate to Qt for long-term quality ("opus magnum")

---

## EXECUTIVE SUMMARY

**Why Qt?**
- ✅ Automatic DPI scaling (no 400 LOC custom reactive system!)
- ✅ Global font scaling: `QApplication::setFont()` (1 line vs bwx_sdk broadcast!)
- ✅ QSS styling (CSS-like, professional themes)
- ✅ Better documentation, larger community
- ✅ LGPL licensing works with paid Python plugins (proprietary OK!)
- ✅ No wxStaticBoxSizer label bugs (doesn't exist in Qt!)

**Migration Effort:** ~4 weeks (NOT 2-3 months - clean architecture!)

**What Stays (48% of codebase):**
- ✅ Core business logic (5,966 LOC) - ZERO wx dependencies!
- ✅ Tests (5,912 LOC) - ZERO wx dependencies!
- ✅ Python bindings (120 LOC) - Pure pybind11!
- ✅ Plugin system architecture
- ✅ Document model (Book, Part, BookElement)
- ✅ Settings, Logging, Serialization

**What Rewrites (34% of codebase):**
- ⚠️ GUI layer (8,500 LOC) → Qt equivalents (~6,000 LOC, simpler!)

**What Deletes (18%):**
- ❌ bwx_sdk integration (obsolete for Qt)
- ❌ 46 wxWidgets task files
- ❌ Feature branches (3 obsolete)

---

## STRATEGIC OPTIONS

### Option A: Incremental Migration (Git Branch Strategy)

**Concept:** Keep wxWidgets as legacy branch, migrate main to Qt incrementally.

**Steps:**
1. Branch `main` @ e191390 → `wxwidgets-legacy` (archive)
2. Reset `main` to clean state (keep core/tests/bindings)
3. Create `qt-migration` branch for development
4. Migrate GUI layer piece-by-piece
5. Merge `qt-migration` → `main` when complete

**Pros:**
- ✅ Git history preserved
- ✅ Can reference wxWidgets implementation
- ✅ Gradual transition (less risky)

**Cons:**
- ❌ Task files stay (confusing - they're wxWidgets-specific)
- ❌ ROADMAP phases 0-1 obsolete (wxWidgets architecture)
- ❌ Messy git history (feature branches, abandoned tasks)
- ❌ bwx_sdk submodule stays (dead weight)

**Timeline:** ~5 weeks (overhead from legacy baggage)

---

### Option B: Clean Slate Reset (RECOMMENDED!)

**Concept:** Archive wxWidgets project, fresh start for Qt with only portable code.

**Steps:**
1. Branch `main` @ e191390 → `wxwidgets-archive` (frozen, read-only)
2. **DELETE from main:**
   - All GUI code (`src/gui/*`, `include/kalahari/gui/*`)
   - All task files (`tasks/*.md`)
   - bwx_sdk submodule (`external/bwx_sdk/`)
   - Feature branches (3 branches deleted)
   - wxWidgets skill (`.claude/skills/kalahari-wxwidgets/`)
3. **KEEP in main:**
   - Core (`src/core/*`, `include/kalahari/core/*`) - 5,966 LOC
   - Tests (`tests/*`) - 5,912 LOC
   - Bindings (`src/bindings/*`) - 120 LOC
   - Project docs (`project_docs/*`) - UPDATE for Qt
   - Skills: plugin-system, i18n (portable)
   - Commands: all (UPDATE for Qt workflow)
4. **REWRITE from scratch:**
   - ROADMAP.md (Qt-based phases, fresh task numbering)
   - CLAUDE.md (Qt patterns, no wxWidgets references)
   - GUI layer (Qt 6.x from clean slate)

**Pros:**
- ✅ **Clean history** - no wxWidgets baggage
- ✅ **Fresh start** - Qt best practices from day 1
- ✅ **Clear separation** - wxwidgets-archive frozen, main = Qt
- ✅ **Faster** - no incremental merge conflicts
- ✅ **New task system** - 00001, 00002, etc. for Qt work

**Cons:**
- ⚠️ Cannot easily reference old code (but: wxwidgets-archive still accessible!)
- ⚠️ "Feels" like restarting project (but: core/tests/plugins preserved!)

**Timeline:** ~4 weeks (cleanest path)

---

## RECOMMENDATION: Option B (Clean Slate)

**Why?**

1. **Psychological Fresh Start**
   - "Opus magnum" deserves clean foundation
   - No confusion (old tasks, old branches)
   - Qt patterns from day 1

2. **Technical Clarity**
   - ROADMAP reflects Qt architecture (not wxWidgets hacks)
   - Task numbers restart (00001 = Qt MainWindow, not wx confusion)
   - No bwx_sdk submodule (Qt doesn't need it!)

3. **Faster Execution**
   - No incremental merge overhead
   - No "which branch am I on?" confusion
   - Straight line: Delete GUI → Rebuild in Qt

4. **Historical Preservation**
   - wxwidgets-archive branch stays forever
   - Can reference anytime
   - Shows design evolution (good for case studies!)

**User Quote:** "ROZWAŻ reset" - Option B aligns with this intuition!

---

## DETAILED EXECUTION PLAN (Option B)

### PHASE 0: Preparation (Day 1 - CRITICAL!)

**Duration:** 4 hours

#### Step 0.1: Archive Current State (30 min)

```bash
# 1. Checkout main at stable commit
git checkout main
git reset --hard e191390  # Last stable (BWX Reactive Controls)

# 2. Create archive branch
git branch wxwidgets-archive
git push origin wxwidgets-archive

# 3. Tag for easy reference
git tag -a v0.2.0-alpha-wxwidgets -m "Kalahari with wxWidgets + bwx_sdk (archived)"
git push origin v0.2.0-alpha-wxwidgets

# 4. Delete feature branches (local + remote)
git branch -D feature/dpi-scaling
git branch -D feature/dpi-support-clean
git branch -D feature/theme-manager
git push origin --delete feature/dpi-scaling
git push origin --delete feature/dpi-support-clean
git push origin --delete feature/theme-manager
```

**Result:** wxWidgets project frozen at `wxwidgets-archive` branch + tag.

---

#### Step 0.2: Clean Main Branch (60 min)

```bash
# Checkout fresh main
git checkout main

# Delete GUI layer
git rm -rf src/gui/
git rm -rf include/kalahari/gui/

# Delete task files
git rm -rf tasks/

# Delete bwx_sdk submodule
git rm -rf external/bwx_sdk/
git config --file .gitmodules --remove-section submodule.external/bwx_sdk
rm .gitmodules  # If no other submodules

# Delete wxWidgets skill
git rm -rf .claude/skills/kalahari-wxwidgets/

# Commit cleanup
git commit -m "chore: Remove wxWidgets GUI layer for Qt migration

BREAKING CHANGE: All GUI code removed. Core, tests, and bindings preserved.

Migration strategy: Clean slate approach
- Archived wxWidgets version: wxwidgets-archive branch
- Next: Rebuild GUI with Qt 6.x
- See: QT_MIGRATION_ROADMAP.md for details"
```

**Result:** Main branch = core + tests + bindings ONLY.

---

#### Step 0.3: Update Project Configuration (90 min)

**Update vcpkg.json:**

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "kalahari",
  "version": "0.3.0-alpha",
  "description": "Writer's IDE - Advanced writing environment for book authors",
  "homepage": "https://github.com/kalahari-project/kalahari",
  "license": "MIT",
  "builtin-baseline": "271a5b8850aa50f9a40269cbf3cf414b36e333d6",
  "dependencies": [
    {
      "name": "qt",
      "version>=": "6.5.0",
      "features": [
        "widgets",
        "gui",
        "core"
      ]
    },
    {
      "name": "curl",
      "default-features": false
    },
    "spdlog",
    "nlohmann-json",
    {
      "name": "libzip",
      "default-features": false,
      "features": [
        "bzip2"
      ]
    },
    "catch2",
    "python3",
    "pybind11"
  ]
}
```

**Update CMakeLists.txt:**

```cmake
# Find Qt6 (replaces wxWidgets)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# Remove wxWidgets references
# find_package(wxWidgets CONFIG REQUIRED)  # DELETE

# Update kalahari_gui target (future)
# target_link_libraries(kalahari_gui PRIVATE Qt6::Widgets Qt6::Gui)
```

**Commit:**

```bash
git add vcpkg.json CMakeLists.txt
git commit -m "build: Replace wxWidgets with Qt6 in dependencies

- vcpkg.json: qt 6.5+ (widgets, gui, core)
- CMakeLists.txt: Qt6 find_package (wxWidgets removed)
- Version bump: 0.2.0-alpha → 0.3.0-alpha (Qt pivot)

Dependencies preserved: spdlog, nlohmann-json, libzip, catch2, python3, pybind11"
```

---

#### Step 0.4: Update CLAUDE.md (60 min)

**Major changes:**
1. Remove all wxWidgets patterns (wxBoxSizer, wxStaticBoxSizer, etc.)
2. Add Qt patterns (QVBoxLayout, QGroupBox, QApplication::setFont)
3. Update "What is DECIDED" (Qt instead of wxWidgets)
4. Remove bwx_sdk references
5. Update CARDINAL RULES (no more "wxStaticBoxSizer MANDATORY")

**Key sections to rewrite:**
- Technology Stack: Qt 6.x
- GUI Patterns: Qt layouts, QSS styling
- Font Scaling: `QApplication::setFont()` (1 line!)
- DPI Handling: Automatic (Qt built-in)
- Remove: All bwx_sdk, wxAUI, wxWidgets mentions

**Commit:**

```bash
git add CLAUDE.md
git commit -m "docs: Update CLAUDE.md for Qt migration

Major changes:
- Technology: wxWidgets → Qt 6.x (LGPL)
- Patterns: wxBoxSizer → QVBoxLayout, wxAUI → QDockWidget
- Font scaling: QApplication::setFont() (automatic!)
- DPI: Qt native support (no custom code!)
- Removed: bwx_sdk, wxWidgets patterns

Version: 6.0 (Qt Migration)
Last Update: 2025-11-19"
```

---

#### Step 0.5: Create Fresh ROADMAP.md (90 min)

**Structure:**

```markdown
# Kalahari Development Roadmap - Qt Edition

**Technology Stack:** C++20 + Qt 6.x (LGPL)
**Current Phase:** Phase 0 (Qt Foundation) 🚀 IN PROGRESS
**Version:** 0.3.0-alpha
**Last Updated:** 2025-11-19

---

## PHASE 0: Qt Foundation (Week 1)

**Status:** 🚀 IN PROGRESS
**Duration:** 1 week
**Goal:** Replace wxWidgets GUI layer with Qt equivalents

### 0.1 Qt Setup & Main Window

- [ ] Task #00001: Qt CMake configuration (1 day)
  - vcpkg Qt6 installation
  - CMakeLists.txt Qt integration
  - Verify build on all platforms

- [ ] Task #00002: QMainWindow skeleton (2 days)
  - QMainWindow subclass
  - QMenuBar creation
  - QToolBar creation
  - QStatusBar creation
  - Verify basic window appears

- [ ] Task #00003: QDockWidget system (2 days)
  - Replace wxAuiManager with QDockWidget
  - 6 dockable panels (placeholders)
  - Perspective save/restore (QSettings)

### 0.2 Qt Dialogs & Settings

- [ ] Task #00004: Settings Dialog (Qt) (2 days)
  - QTreeWidget navigation (left pane)
  - QStackedWidget content (right pane)
  - 3 settings panels (Appearance, Editor, Log)
  - Apply/OK/Cancel buttons

- [ ] Task #00005: About Dialog (Qt) (0.5 days)
  - QDialog with project info
  - Logo display (QLabel with QPixmap)

### 0.3 Testing & Polish

- [ ] Task #00006: Manual Testing Session (1 day)
  - Verify all Qt features work
  - Cross-platform testing (Windows, macOS, Linux)
  - Fix regressions

---

## PHASE 1: Core Editor (Weeks 2-5)

### 1.1 Custom Text Editor (Qt)

- [ ] Task #00007: QPlainTextEdit subclass (3 days)
  - Port bwxTextDocument to Qt-compatible
  - Gap buffer storage preserved
  - Undo/redo integration with QTextDocument

- [ ] Task #00008: Syntax Highlighting (2 days)
  - QSyntaxHighlighter subclass
  - Markdown formatting rules

### 1.2 Outline Panel (Qt)

- [ ] Task #00009: QTreeWidget outline (2 days)
  - Chapter/scene hierarchy
  - Drag & drop reordering (Qt native!)
  - Context menu (QMenu)

...
```

**Commit:**

```bash
git add ROADMAP.md
git commit -m "docs: Create fresh ROADMAP.md for Qt migration

Clean slate approach:
- Task numbering restarts (00001, 00002, ...)
- Qt-specific phases (no wxWidgets references)
- Realistic timeline (4 weeks for Phase 0-1)
- Atomic task structure preserved

Previous ROADMAP: See wxwidgets-archive branch"
```

---

#### Step 0.6: Update CHANGELOG.md (30 min)

Add major entry:

```markdown
## [0.3.0-alpha] - 2025-11-19

### BREAKING CHANGE: Technology Stack Pivot (wxWidgets → Qt 6.x)

**Decision:** Migrate from wxWidgets to Qt for long-term quality.

**Rationale:**
- Qt has automatic DPI scaling (vs 400 LOC custom bwx_sdk reactive system)
- Qt has `QApplication::setFont()` for global font scaling (1 line!)
- Qt has QSS styling (CSS-like themes)
- Qt has better documentation, larger community
- Qt LGPL license works with paid Python plugins (proprietary OK!)
- wxWidgets limitations (wxStaticBoxSizer labels, manual DPI, etc.) are blockers for "opus magnum"

**Migration Strategy:**
- **Clean Slate Approach:** Archive wxWidgets (wxwidgets-archive branch), rebuild GUI in Qt
- **Preserved:** Core (5,966 LOC), Tests (5,912 LOC), Bindings (120 LOC) - ZERO changes!
- **Rewritten:** GUI layer (~8,500 LOC wxWidgets → ~6,000 LOC Qt, simpler!)
- **Deleted:** bwx_sdk submodule, 46 wxWidgets task files, feature branches

**Timeline:**
- Phase 0 (Qt Foundation): 1 week
- Phase 1 (Core Editor Qt): 3 weeks
- Total: ~4 weeks to feature parity

**References:**
- Archived wxWidgets: `wxwidgets-archive` branch + `v0.2.0-alpha-wxwidgets` tag
- Migration plan: QT_MIGRATION_ROADMAP.md

### Changed
- Build system: vcpkg.json now requires Qt6 (widgets, gui, core) instead of wxWidgets
- CMakeLists.txt: Qt6 find_package replaces wxWidgets
- Version: 0.2.0-alpha → 0.3.0-alpha (major architecture change)

### Removed
- All GUI code (src/gui/, include/kalahari/gui/) - will be rewritten in Qt
- bwx_sdk submodule (external/bwx_sdk/) - obsolete for Qt
- 46 task files (tasks/*.md) - wxWidgets-specific architecture
- 3 feature branches (dpi-scaling, dpi-support-clean, theme-manager) - obsolete
- .claude/skills/kalahari-wxwidgets/ - replaced with Qt patterns

### Preserved (ZERO changes)
- Core business logic (src/core/, include/kalahari/core/) - 5,966 LOC
- Tests (tests/*.cpp) - 5,912 LOC - ZERO wx dependencies!
- Python bindings (src/bindings/) - 120 LOC - Pure pybind11!
- Plugin system architecture
- Document model (Book, Part, BookElement)
- Settings, Logging, Serialization (JSON, libzip, spdlog)

---

## [0.2.0-alpha] - 2025-11-15 (wxWidgets - ARCHIVED)

**Note:** This is the last wxWidgets-based version. Archived at `wxwidgets-archive` branch.

### Added (wxWidgets)
- BWX SDK reactive system (bwxReactive, bwxManaged<T>)
- Font scaling infrastructure
- Theme management system

See wxwidgets-archive branch for full history.
```

**Commit:**

```bash
git add CHANGELOG.md
git commit -m "docs: Document Qt migration in CHANGELOG

Added [0.3.0-alpha] entry with BREAKING CHANGE notice
- Rationale for Qt migration (DPI, fonts, QSS, license)
- Clean slate strategy (archive + rebuild)
- Preserved code (core, tests, bindings - 48% unchanged!)
- Timeline (4 weeks to feature parity)"
```

---

#### Step 0.7: Update project_docs/ (60 min)

**Files to update:**

1. **02_tech_stack.md** - Replace wxWidgets → Qt 6.x
2. **03_architecture.md** - Update MVP pattern for Qt (QObject signals/slots!)
3. **08_gui_design.md** - Qt command system (QAction!)
4. **14_bwx_sdk_patterns.md** - DELETE (obsolete!)

**New document:**

5. **19_qt_patterns.md** - Qt best practices (QSS, signals/slots, layouts)

**Commit:**

```bash
git add project_docs/
git rm project_docs/14_bwx_sdk_patterns.md
git commit -m "docs: Update project_docs for Qt migration

Updated:
- 02_tech_stack.md: Qt 6.x (LGPL) replaces wxWidgets
- 03_architecture.md: MVP with QObject signals/slots
- 08_gui_design.md: QAction command system

Added:
- 19_qt_patterns.md: Qt best practices (QSS, layouts, signals)

Removed:
- 14_bwx_sdk_patterns.md: Obsolete (wxWidgets-specific)"
```

---

#### Step 0.8: Update .claude/ Resources (30 min)

**Delete:**
- `.claude/skills/kalahari-wxwidgets/` (already done in Step 0.2)

**Update:**
- `.claude/skills/kalahari-i18n/` - Qt uses `tr()` instead of `_()`!

**Create:**
- `.claude/skills/kalahari-qt-patterns/` - Qt widget creation, QSS styling

**Commands (no changes needed!):**
- save-session, load-session, next-task, push, cleanup-serena - all portable!

**Commit:**

```bash
git add .claude/
git commit -m "chore: Update Claude Code resources for Qt

Updated:
- kalahari-i18n: Qt tr() macro (replaces wxWidgets _())

Added:
- kalahari-qt-patterns: Qt best practices skill

Preserved:
- All slash commands (save-session, load-session, etc.)
- kalahari-plugin-system skill (portable!)"
```

---

#### Step 0.9: Update Serena Memories (30 min)

**Create new memory:**

```bash
# Via Serena MCP
mcp__serena__write_memory(
  "qt_migration_decision_2025-11-19.md",
  content="""
# Qt Migration Decision - 2025-11-19

## Decision
APPROVED: Migrate from wxWidgets to Qt 6.x (LGPL)

## Rationale
- Automatic DPI scaling (Qt built-in vs 400 LOC bwx_sdk)
- Global font scaling: QApplication::setFont() (1 line!)
- QSS styling (CSS-like themes)
- Better docs, larger community
- LGPL works with paid Python plugins
- wxWidgets limitations are blockers for "opus magnum"

## Strategy
Clean Slate Approach:
- Archive wxWidgets: wxwidgets-archive branch
- Delete GUI layer: src/gui/, tasks/*.md, bwx_sdk
- Preserve core: 5,966 LOC (ZERO wx dependencies!)
- Rebuild GUI: Qt 6.x from scratch

## Timeline
- Week 1: Qt Foundation (MainWindow, docks, settings)
- Weeks 2-4: Core Editor (text, outline, panels)
- Total: ~4 weeks to feature parity

## References
- Archived: wxwidgets-archive branch + v0.2.0-alpha-wxwidgets tag
- Plan: QT_MIGRATION_ROADMAP.md
"""
)
```

**Archive old memories:**

Move wxWidgets-specific memories to `.serena/memories/archive/`:
- `bwx_sdk_*` memories (3 files)
- `session_2025-11-15_bwx-sdk-architecture-decision.md`
- `session_2025-11-16_*` (font scaling sessions)

**Commit:**

```bash
git add .serena/
git commit -m "chore: Archive wxWidgets memories, add Qt decision

Added:
- qt_migration_decision_2025-11-19.md (rationale, strategy)

Archived (moved to memories/archive/):
- bwx_sdk_* memories (3 files)
- wxWidgets font scaling sessions (2 files)

Preserved:
- Phase 0 completion memory
- Plugin system memories (portable!)"
```

---

#### Step 0.10: Final Commit & Push (10 min)

```bash
# Create checkpoint
git add QT_MIGRATION_ROADMAP.md
git commit -m "docs: Add Qt migration roadmap (Option B - Clean Slate)

Comprehensive migration plan:
- Phase 0: Preparation (Day 1 - archive, cleanup, docs)
- Phase 1-3: Qt GUI rebuild (Weeks 1-4)
- Detailed task breakdown
- Preservation strategy (core, tests, bindings)
- Risk analysis

Decision: Clean slate approach (recommended)
Timeline: 4 weeks to feature parity"

# Push to GitHub
git push origin main --force-with-lease  # Force because of reset!

# Push archive branch
git push origin wxwidgets-archive

# Push tag
git push origin v0.2.0-alpha-wxwidgets

# Verify
git log --oneline -5
git branch -a
```

**Result:** Phase 0 complete! Main branch clean, ready for Qt.

---

### PHASE 1: Qt Foundation (Week 1)

**Duration:** 5 days (full-time development)

#### Week 1, Day 1: Task #00001 - Qt CMake Setup (8 hours)

**Goal:** Configure CMake for Qt6, verify build on all platforms.

**Steps:**

1. **Install Qt6 via vcpkg** (already done in vcpkg.json):
   ```bash
   cd build-windows
   cmake ..  # vcpkg auto-installs Qt6
   ```

2. **Update src/CMakeLists.txt:**
   ```cmake
   # Qt6 integration
   find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
   set(CMAKE_AUTOMOC ON)  # Qt meta-object compiler
   set(CMAKE_AUTORCC ON)  # Qt resource compiler
   set(CMAKE_AUTOUIC ON)  # Qt UI compiler

   # GUI library (future)
   add_library(kalahari_gui STATIC
       # Empty for now - will add Qt files
   )

   target_link_libraries(kalahari_gui PUBLIC
       Qt6::Core
       Qt6::Widgets
       Qt6::Gui
       kalahari_core  # Core is portable!
   )

   # Executable
   add_executable(kalahari
       main.cpp
   )

   target_link_libraries(kalahari PRIVATE
       kalahari_gui
       kalahari_core
   )
   ```

3. **Create minimal main.cpp (Qt):**
   ```cpp
   #include <QApplication>
   #include <QMainWindow>

   int main(int argc, char* argv[]) {
       QApplication app(argc, argv);
       QMainWindow window;
       window.setWindowTitle("Kalahari Writer's IDE");
       window.resize(1280, 720);
       window.show();
       return app.exec();
   }
   ```

4. **Build & test:**
   ```bash
   cmake --build build-windows --config Debug
   ./build-windows/bin/kalahari.exe  # Should show empty window!
   ```

5. **Verify CI/CD** (GitHub Actions):
   - Linux: Ubuntu + Qt6
   - macOS: Qt6 via homebrew
   - Windows: vcpkg Qt6

**Acceptance Criteria:**
- [x] Qt6 installed via vcpkg
- [x] CMake configuration successful
- [x] Empty QMainWindow appears
- [x] CI/CD passing (all 3 platforms)

**Commit:**
```bash
git add CMakeLists.txt src/CMakeLists.txt src/main.cpp
git commit -m "feat: Task #00001 - Qt6 CMake setup + minimal window

- Qt6 find_package (Core, Widgets, Gui)
- CMAKE_AUTOMOC/AUTORCC/AUTOUIC enabled
- Minimal main.cpp with QApplication + QMainWindow
- Build verified on all platforms

Acceptance: Empty Qt window appears, CI/CD passing"
```

---

#### Week 1, Days 2-3: Task #00002 - QMainWindow Structure (16 hours)

**Goal:** Create QMainWindow subclass with menu/toolbar/statusbar.

**Implementation:**

1. **Create src/gui/main_window.h:**
   ```cpp
   #pragma once
   #include <QMainWindow>

   class QAction;
   class QMenu;
   class QToolBar;

   namespace kalahari {
   namespace gui {

   class MainWindow : public QMainWindow {
       Q_OBJECT  // Required for signals/slots!

   public:
       MainWindow(QWidget* parent = nullptr);
       ~MainWindow() override = default;

   private:
       void createActions();
       void createMenus();
       void createToolbars();
       void createStatusBar();

       // Actions
       QAction* m_newAction;
       QAction* m_openAction;
       QAction* m_saveAction;
       QAction* m_exitAction;

       // Menus
       QMenu* m_fileMenu;
       QMenu* m_editMenu;

       // Toolbars
       QToolBar* m_fileToolbar;
   };

   } // namespace gui
   } // namespace kalahari
   ```

2. **Implement menu/toolbar creation** (Qt pattern).

3. **Test manually** - verify menus appear.

**Acceptance Criteria:**
- [x] MainWindow class created (QObject, signals/slots)
- [x] File menu (New, Open, Save, Exit)
- [x] Edit menu (Undo, Redo, Cut, Copy, Paste)
- [x] File toolbar (icons)
- [x] Status bar (ready message)
- [x] Actions connected to slots (log on click)

---

#### Week 1, Days 4-5: Task #00003 - QDockWidget System (16 hours)

**Goal:** Replace wxAuiManager with Qt dock system.

**Implementation:**

1. **Create 6 panel placeholders:**
   - EditorPanel (QPlainTextEdit placeholder)
   - NavigatorPanel (QTreeWidget placeholder)
   - PropertiesPanel (QLabel "Properties")
   - SearchPanel (QLineEdit placeholder)
   - AssistantPanel (QLabel "Assistant")
   - LogPanel (QPlainTextEdit placeholder)

2. **Add to MainWindow:**
   ```cpp
   void MainWindow::createDocks() {
       // Navigator (left)
       QDockWidget* navDock = new QDockWidget("Navigator", this);
       navDock->setWidget(new NavigatorPanel());
       addDockWidget(Qt::LeftDockWidgetArea, navDock);

       // Properties (right)
       QDockWidget* propDock = new QDockWidget("Properties", this);
       propDock->setWidget(new PropertiesPanel());
       addDockWidget(Qt::RightDockWidgetArea, propDock);

       // Log (bottom)
       QDockWidget* logDock = new QDockWidget("Log", this);
       logDock->setWidget(new LogPanel());
       addDockWidget(Qt::BottomDockWidgetArea, logDock);

       // Editor (central)
       setCentralWidget(new EditorPanel());
   }
   ```

3. **Save/restore perspective:**
   ```cpp
   void MainWindow::saveGeometry() {
       QSettings settings("Kalahari", "WriterIDE");
       settings.setValue("geometry", saveGeometry());
       settings.setValue("windowState", saveState());
   }

   void MainWindow::loadGeometry() {
       QSettings settings("Kalahari", "WriterIDE");
       restoreGeometry(settings.value("geometry").toByteArray());
       restoreState(settings.value("windowState").toByteArray());
   }
   ```

**Acceptance Criteria:**
- [x] 6 dockable panels (placeholders)
- [x] Drag & drop docking works (Qt native!)
- [x] Close/reopen panels via View menu
- [x] Perspective save/restore (QSettings)
- [x] Default layout sensible

---

### PHASE 2: Settings & Dialogs (Week 2)

#### Week 2, Days 1-2: Task #00004 - Settings Dialog (Qt)

**Goal:** QTreeWidget navigation + 3 settings panels.

**Implementation:**

1. **SettingsDialog structure** (QDialog + QTreeWidget + QStackedWidget)
2. **3 panels:**
   - AppearanceSettingsPanel (theme, font size - QGroupBox)
   - EditorSettingsPanel (word wrap, syntax highlighting)
   - LogSettingsPanel (log level, max lines)

3. **Apply/OK/Cancel** buttons (QDialogButtonBox)

**Acceptance Criteria:**
- [x] Tree navigation (Appearance, Editor, Log)
- [x] Panels switch correctly (QStackedWidget)
- [x] Apply button works (SettingsManager)
- [x] OK saves and closes
- [x] Cancel discards changes

---

### PHASE 3: Core Editor (Weeks 3-4)

#### Tasks #00007-00009: Text Editor, Outline, Panels

**Goal:** Port core functionality to Qt.

**Tasks:**
- #00007: QPlainTextEdit custom editor (gap buffer integration)
- #00008: Syntax highlighting (QSyntaxHighlighter)
- #00009: Outline panel (QTreeWidget with chapters/scenes)
- #00010: Log panel (colored output)
- #00011: Perspective manager (QSettings)

**Acceptance Criteria:**
- [x] Text editing works (type, select, copy/paste)
- [x] Undo/redo works (Qt native!)
- [x] Outline tree populates (drag & drop!)
- [x] Log shows colored messages
- [x] Perspectives save correctly

---

### PHASE 4: Testing & Polish (Week 4, final days)

#### Task #00012: Manual Testing Session

**Goal:** Verify feature parity with wxWidgets version.

**Test cases:**
1. Menu commands work
2. Toolbar buttons work
3. Dock panels drag correctly
4. Settings persist
5. Text editor functional
6. Cross-platform verification (Windows, macOS, Linux)

**Acceptance Criteria:**
- [x] All tests pass
- [x] No regressions
- [x] CI/CD green
- [x] Ready for Phase 1 development

---

## RISK MITIGATION

### Risk 1: Qt6 vcpkg Installation Fails

**Likelihood:** Low (vcpkg has Qt6)
**Impact:** High (blocks development)

**Mitigation:**
- **Plan B:** Use system Qt6 (apt, homebrew, official installer)
- **Test early:** Verify Qt6 install in Day 1

### Risk 2: QDockWidget Perspective Restore Buggy

**Likelihood:** Medium (Qt dock state is complex)
**Impact:** Medium (frustrating UX)

**Mitigation:**
- Use QSettings properly (QByteArray for geometry/state)
- Test perspective save/restore early (Task #00003)
- Fallback: Default layout always available

### Risk 3: Custom Text Editor Integration Hard

**Likelihood:** Medium (gap buffer + Qt integration)
**Impact:** High (core feature!)

**Mitigation:**
- **Option A:** Port bwxTextDocument to QTextDocument (recommended)
- **Option B:** Use QPlainTextEdit + external gap buffer (harder)
- Start early (Week 3, Task #00007)

### Risk 4: CI/CD Breaks on macOS/Linux

**Likelihood:** Low (Qt is cross-platform)
**Impact:** High (blocks merge)

**Mitigation:**
- Test locally on all platforms first
- Use GitHub Actions Qt setup actions
- Ubuntu: `sudo apt install qt6-base-dev`
- macOS: `brew install qt6`

---

## SUCCESS CRITERIA

**Phase 0 Complete (Day 1):**
- [x] wxWidgets archived (wxwidgets-archive branch)
- [x] Main branch clean (GUI deleted, core preserved)
- [x] Docs updated (CLAUDE.md, ROADMAP.md, CHANGELOG.md)
- [x] Git clean, pushed to GitHub

**Week 1 Complete (Qt Foundation):**
- [x] Qt6 builds on all platforms
- [x] Empty QMainWindow appears
- [x] Menu/toolbar functional
- [x] 6 dock panels (placeholders)

**Week 2 Complete (Settings & Dialogs):**
- [x] Settings dialog works (3 panels)
- [x] Apply/OK/Cancel functional
- [x] QSettings persistence

**Weeks 3-4 Complete (Core Editor):**
- [x] Text editing works
- [x] Outline panel functional
- [x] Log panel shows messages
- [x] Perspectives save/restore

**Migration Complete (4 weeks):**
- [x] Feature parity with wxWidgets v0.2.0-alpha
- [x] All tests passing (core tests rerun!)
- [x] CI/CD green (all platforms)
- [x] Ready to continue Phase 1 development

---

## TIMELINE SUMMARY

| Phase | Duration | Tasks | Status |
|-------|----------|-------|--------|
| **Phase 0: Preparation** | Day 1 (4h) | Archive, cleanup, docs | ⏳ READY |
| **Week 1: Qt Foundation** | 5 days | #00001-00003 (CMake, MainWindow, Docks) | ⏳ PENDING |
| **Week 2: Settings** | 5 days | #00004-00006 (Dialogs, panels) | ⏳ PENDING |
| **Weeks 3-4: Editor** | 10 days | #00007-00011 (Text, outline, panels) | ⏳ PENDING |
| **Week 4: Testing** | 2 days | #00012 (Manual testing, polish) | ⏳ PENDING |

**Total:** ~4 weeks (20 working days)

---

## POST-MIGRATION

**After Week 4 (Qt feature parity achieved):**

1. **Resume Phase 1 Development** (original plan):
   - Find & Replace
   - Auto-Save & Backup
   - UX Polish (focus modes, stats)

2. **Continue Phase 2+ (Plugin System MVP):**
   - Core + tests + bindings already portable!
   - Plugin API works unchanged!
   - Python paid plugins strategy works (LGPL OK!)

3. **Leverage Qt Advantages:**
   - QSS styling (professional themes!)
   - Automatic DPI scaling (users will love this!)
   - Better community support (Qt forums, Stack Overflow)

---

## FINAL NOTES

**Why Clean Slate (Option B) Wins:**

1. **Clarity:** No confusion (old tasks, old branches, old patterns)
2. **Speed:** Straight line (delete → rebuild), no merge overhead
3. **Quality:** Qt best practices from day 1, no wxWidgets baggage
4. **Psychology:** Fresh start for "opus magnum"

**What We're NOT Losing:**

- ✅ 48% of codebase preserved (core, tests, bindings)
- ✅ Architecture decisions preserved (plugin system, document model)
- ✅ Business model unchanged (Open Core + paid plugins)
- ✅ Git history accessible (wxwidgets-archive forever)

**What We're GAINING:**

- ✅ Modern GUI framework (Qt 6.x)
- ✅ Automatic DPI/font scaling (zero custom code!)
- ✅ Professional styling (QSS)
- ✅ Better developer experience (Qt docs, community)
- ✅ Long-term sustainability (Qt is industry standard)

---

**DECISION:** Execute Option B (Clean Slate) - APPROVED

**Next Step:** Execute Phase 0 (Day 1, 4 hours)

---

**Document Version:** 1.0
**Created:** 2025-11-19
**Author:** Claude Code (with user approval)
**Status:** READY FOR EXECUTION
