# Kalahari Development Roadmap

> **Writer's IDE** - Qt6 Architecture | Phase-based Development

**Current Phase:** Phase 0 (Qt Foundation) 🔄 IN PROGRESS
**Current Task:** Step 0.5 - Create Fresh ROADMAP.md
**Version:** 0.3.0-alpha
**Last Updated:** 2025-11-19

---

## 🔄 MIGRATION CONTEXT (2025-11-19)

**Decision:** Migrated from wxWidgets to Qt6 for long-term quality and maintainability.

**Reason:** wxWidgets limitations (manual DPI scaling, wxStaticBoxSizer bugs, complex reactive patterns) incompatible with "opus magnum" quality standards. Qt6 provides automatic DPI scaling, QApplication::setFont() global styling, QSS theming, and superior documentation.

**Strategy:** Clean Slate Approach (Option B)
- **Archived:** wxwidgets-archive branch + v0.2.0-alpha-wxwidgets tag
- **Preserved:** Core (5,966 LOC), Tests (5,912 LOC), Bindings (120 LOC), Plugin system
- **Deleted:** GUI layer (28,098 LOC), 49 task files, bwx_sdk submodule
- **Timeline:** +4 weeks for Qt Foundation (Phase 0)

**Reference:** [QT_MIGRATION_ROADMAP.md](QT_MIGRATION_ROADMAP.md) - Complete migration plan

---

## PHASE 0: Qt Foundation 🔄 IN PROGRESS

**Status:** 🔄 IN PROGRESS (Started 2025-11-19, ~85% complete)
**Duration:** 1-2 days (estimate: 4 weeks) - 12/13 tasks complete
**Target:** 0.3.0-alpha ✅ RELEASED (basic GUI) → 0.3.1-alpha (Command Registry)

### Step 0: Preparation ⚡ IN PROGRESS (Day 1, ~6 hours)

**Goal:** Archive wxWidgets, update build system, refresh documentation

- [x] **Step 0.1:** Archive Current State (30 min)
  - Created wxwidgets-archive branch
  - Created v0.2.0-alpha-wxwidgets tag
  - Deleted 3 feature branches (dpi-scaling, dpi-support-clean, theme-manager)
  - Reset main to commit e191390 (last stable)

- [x] **Step 0.2:** Clean Main Branch (60 min)
  - Deleted 28,098 LOC (103 files)
  - Removed src/gui/ and include/kalahari/gui/
  - Removed 49 wxWidgets task files
  - Removed bwx_sdk submodule
  - Removed wxWidgets skills

- [x] **Step 0.3:** Update Project Configuration (90 min)
  - vcpkg.json: wxWidgets → Qt6 6.5.0+ (qtbase, qttools)
  - CMakeLists.txt: Qt6 find_package, CMAKE_AUTOMOC/AUTORCC/AUTOUIC
  - src/CMakeLists.txt: Removed bwx_sdk, updated to Qt-agnostic core
  - src/main.cpp: Qt6 placeholder with QApplication

- [x] **Step 0.4:** Update CLAUDE.md (60 min)
  - Replaced wxWidgets patterns with Qt6 patterns
  - Updated Cardinal Rules §2 (Qt6 Layout)
  - Updated Technology Stack (Qt6 6.5.0+)
  - Updated Current Status (Qt migration progress)
  - Added v6.0 update history

- [ ] **Step 0.5:** Create Fresh ROADMAP.md (90 min) 🔄 IN PROGRESS
  - Define Phase 0-5 structure for Qt
  - Fresh task numbering (00001+)
  - Remove all wxWidgets-specific content

- [ ] **Step 0.6:** Update CHANGELOG.md (30 min)
  - Add [0.3.0-alpha] section with BREAKING CHANGE
  - Document migration decision and rationale
  - Archive note for wxWidgets version

- [ ] **Step 0.7:** Update project_docs/ (60 min)
  - 02_tech_stack.md (Qt6 6.5.0+)
  - 03_architecture.md (Qt patterns)
  - 08_gui_design.md (QMainWindow, QDockWidget)
  - 09_i18n.md (Qt i18n: tr() + .ts/.qm files)
  - Delete wxWidgets-specific sections

- [ ] **Step 0.8:** Update .claude/ Resources (30 min)
  - Update kalahari-i18n skill (Qt tr() patterns)
  - Verify kalahari-plugin-system skill (Qt-agnostic)
  - Update slash commands if needed

- [ ] **Step 0.9:** Update Serena Memories (30 min)
  - Create qt_migration_decision.md memory
  - Archive wxWidgets-specific memories
  - Update project status memory

- [ ] **Step 0.10:** Final Push & Verification (10 min)
  - Push all commits to main
  - Verify git state (clean, all changes committed)
  - Verify GitHub state (all branches/tags correct)

### Week 1: Qt Hello World (Tasks #00001-00003)

**Goal:** Basic Qt6 application with window, menu, logging

- [x] **Task #00001:** Qt6 vcpkg Installation & CMake Configuration (2-3h) ✅ **COMPLETE** (2025-11-20)
  - Qt6 6.9.1 installed via vcpkg (qtbase, qttools)
  - CMake finds Qt6::Core, Qt6::Widgets, Qt6::Gui
  - Built minimal QApplication + QMainWindow test
  - CI/CD: Workflows updated for Qt dependencies
  - Commit: `2b7d779` - Qt6 Hello World

- [x] **Task #00002:** QMainWindow Skeleton (3-4h) ✅ **COMPLETE** (2025-11-20)
  - Created MainWindow (QMainWindow subclass)
  - Menu bar: File (New/Open/Save/Exit), Edit (Undo/Redo/Cut/Copy/Paste/Settings)
  - Toolbar: File actions (New, Open, Save)
  - Status bar with "Ready" message
  - Integrated Logger and SettingsManager
  - Commit: `78ce960` - QMainWindow structure with menus/toolbars

- [x] **Task #00003:** Basic QDockWidget System (4-5h) ✅ **COMPLETE** (2025-11-20)
  - Created 5 dock panels: Navigator, Properties, Log, Search, Assistant
  - View menu with toggle actions for each panel
  - Perspective save/restore via QSettings (geometry + dock state)
  - Default layout: Navigator (left), Editor (center), Properties (right)
  - Commit: `8e0caaf` - QDockWidget system with 6 panels + View menu

### Week 2: Settings System (Tasks #00004-00006)

**Goal:** Settings dialog with Qt layouts, JSON persistence

- [x] **Task #00004:** Settings Dialog Structure (3-4h) ✅ **COMPLETE** (2025-11-20)
  - **Part A:** Migrated SettingsManager and CmdLineParser to Qt6 (QSize, QPoint)
    - Commit: `37019b1` - SettingsManager Qt6 migration
  - **Part B:** Created SettingsDialog (QDialog) with QTabWidget
    - QTabWidget for category navigation (2 tabs: Appearance, Editor)
    - Apply/OK/Cancel buttons (QDialogButtonBox)
    - Connected with SettingsManager (load/save on open/close)
    - Commit: `dc17e16` - Settings Dialog structure

- [x] **Task #00005:** Appearance Settings Panel (2-3h) ✅ COMPLETE (2025-11-20)
  - Create AppearancePanel (QWidget)
  - Font size controls (QComboBox with 6 presets)
  - Theme controls (QComboBox: light/dark)
  - Icon size controls (QSpinBox)
  - Apply settings with QApplication::setFont()

- [x] **Task #00006:** Editor Settings Panel (2-3h) ✅ **COMPLETE** (2025-11-20)
  - Created Editor tab in Settings Dialog (QWidget)
  - 5 controls: Font Family (QFontComboBox), Font Size (QSpinBox), Tab Size (QSpinBox), Line Numbers (QCheckBox), Word Wrap (QCheckBox)
  - Settings persistence via SettingsManager (generic get<T>/set<T> API)
  - All values load/save correctly to settings.json

### Week 3: Core Editor Foundation (Tasks #00007-00009)

**Goal:** Basic text editing with QPlainTextEdit

- [x] **Task #00007:** EditorWidget Basic Implementation (4-5h) ✅ **COMPLETE** (2025-11-20)
  - Implemented settings integration in EditorPanel (QPlainTextEdit)
  - Font family/size applied from editor.fontFamily, editor.fontSize
  - Tab size converted to pixels: tabSize × QFontMetrics::horizontalAdvance(' ')
  - Word wrap mode: setLineWrapMode(NoWrap/WidgetWidth)
  - Public API: setText(), getText() for Document I/O (Task #00008)
  - Syntax highlighter stub created (KalahariSyntaxHighlighter, Phase 1 implementation)
  - Actual time: ~90 minutes (estimate: 4-5h)

- [x] **Task #00008:** File Operations (3-4h) ✅ **COMPLETE** (2025-11-20)
  - File operations: New (empty document), Open (QFileDialog + DocumentArchive::load)
  - Save (to current file), Save As (QFileDialog save + DocumentArchive::save)
  - Dirty state tracking: textChanged signal → "*" in window title
  - Unsaved changes dialog: Prompts on New/Open/Close (Save/Discard/Cancel)
  - Phase 0 content storage: metadata["_phase0_content"] in first chapter (temp solution)
  - Window title format: "Kalahari - filename.klh" / "Kalahari - *filename.klh" (dirty)
  - Actual time: ~2.5 hours (estimate: 3-4h)

- [x] **Task #00009:** Edit Operations (2-3h) ✅ **COMPLETE** (2025-11-20)
  - Implemented 6 Edit operations via QPlainTextEdit delegation
  - Edit → Undo/Redo/Cut/Copy/Paste/Select All (Ctrl+Z/Y/X/C/V/A)
  - Keyboard shortcuts (QKeySequence standard bindings)
  - Menu integration (Edit menu with separators)
  - Status bar feedback ("Undo performed", "Copied to clipboard", etc.)
  - Simple delegation pattern (Phase 0 simplicity - Qt handles all logic)
  - Actual time: ~1.5 hours (estimate: 2-3h)

### Week 4: Panels & Polish (Tasks #00010-00012)

**Goal:** Navigator panel, About dialog, first release

- [x] **Task #00010:** Navigator Panel with QTreeWidget (4-5h) ✅ **COMPLETE** (2025-11-20)
  - Integrated NavigatorPanel with Document structure for book hierarchy display
  - loadDocument() method: Parses Book (frontMatter, body, backMatter) and populates QTreeWidget
  - Tree structure: Document (root) → Sections (Front Matter/Body/Back Matter) → Parts → Chapters
  - Body expanded by default, Front/Back Matter collapsed
  - Double-click stub: Shows "Phase 1 feature" message (no navigation in Phase 0)
  - MainWindow integration: Called on New/Open operations
  - Actual time: ~2 hours (estimate: 4-5h)

- [x] **Task #00011:** About Dialog & Help Menu (2h) ✅ **COMPLETE** (2025-11-20)
  - Added Help menu with 2 actions (About Kalahari, About Qt)
  - Help → About Kalahari: QMessageBox::about() with app info
  - Help → About Qt: QMessageBox::aboutQt() standard dialog
  - Displays: Version (0.3.0-alpha), License (MIT), Copyright, Qt version
  - Phase 0: Built-in Qt dialogs (no custom AboutDialog class)
  - Actual time: ~1 hour (estimate: 2h)

- [x] **Task #00012:** Qt Foundation Release (3-4h) ✅ **COMPLETE** (2025-11-20)
  - Updated version: CMakeLists.txt 0.0.1 → 0.3.0
  - Finalized CHANGELOG.md ([Unreleased] → [0.3.0-alpha])
  - Created git tag v0.3.0-alpha with release notes
  - ROADMAP.md: Phase 0 marked as COMPLETE
  - 🎉 Qt migration complete! All 12 tasks done in 1 day!

### Week 5: Command Registry Migration 🔄 IN PROGRESS (2025-11-21)

**Goal:** Migrate Command Registry system from wxWidgets to Qt6

**Context:** Command Registry was fully implemented and tested in wxWidgets (Tasks #00032-#00035, 46+ test cases). System includes CommandRegistry singleton, Command/IconSet/KeyboardShortcut structures, MenuBuilder, ToolbarBuilder. Currently MainWindow uses hardcoded QAction connections - must migrate to unified command system for plugin support and customizable UI.

**Architecture Recovered:**
- ✅ Command Registry (singleton, ~200 LOC)
- ✅ Command struct (id, label, category, icons, shortcuts, execute/isEnabled/isChecked callbacks)
- ✅ IconSet (16/24/32px pre-rendered bitmaps)
- ✅ KeyboardShortcut (key + modifiers, toString/fromString)
- ✅ ToolbarBuilder (dynamic toolbar generation from registry)
- ✅ MenuBuilder (not recovered yet, will implement fresh)

**Benefits:**
- ✅ Single source of truth for all commands
- ✅ Plugin commands integrate seamlessly (ICommandProvider)
- ✅ Customizable toolbars (user can add/remove/reorder)
- ✅ Command Palette ready (Ctrl+Shift+P, Phase 1)
- ✅ Keyboard shortcuts management
- ✅ No hardcoded QAction connections

- [ ] **Task #00047:** Command Registry Qt Migration (1-2 days) 🔄 **IN PROGRESS** (2025-11-21)
  - **Day 1 Morning:** Recover files from wxwidgets-archive
    - command.h/cpp (Command, IconSet, KeyboardShortcut structs)
    - command_registry.h/cpp (CommandRegistry singleton)
    - toolbar_builder.h/cpp (ToolbarBuilder class)
  - **Day 1 Afternoon:** Adapt to Qt6
    - IconSet: wxBitmap → QPixmap, add toQIcon()
    - KeyboardShortcut: wxKeyCode → Qt::Key, add toQKeySequence()
    - Command: Keep std::function<void()>, add toQAction() helper
    - CommandRegistry: No changes (framework-agnostic)
  - **Day 2 Morning:** Implement MenuBuilder and ToolbarBuilder for Qt
    - MenuBuilder: Build QMenuBar from CommandRegistry
    - ToolbarBuilder: Build QToolBar from CommandRegistry
    - Integration tests
  - **Day 2 Afternoon:** Refactor MainWindow
    - Replace createActions() → registerCommands()
    - Remove 15+ hardcoded connect() calls
    - Rebuild menus/toolbars via builders
    - Verify all shortcuts work
  - **Documentation:** Update project_docs/08_gui_design.md (Qt code examples)
  - **Testing:** Manual testing (menu, toolbar, shortcuts)
  - **Target:** 0.3.1-alpha release

---

## PHASE 1: Core Editor (Weeks 1-20)

**Status:** ⏳ PLANNED
**Target:** 0.4.0-alpha
**Timeline:** ~5 months

### 1.1 Rich Text Editor

- [ ] Custom QTextDocument subclass (Book-aware)
- [ ] Formatting toolbar (bold, italic, underline, alignment)
- [ ] Font selection (QFontComboBox)
- [ ] Paragraph styles (heading, body, quote)
- [ ] Character formatting (color, size, font family)

### 1.2 Document Structure

- [ ] Part management (QListWidget or QTreeView)
- [ ] Chapter CRUD operations
- [ ] Drag-and-drop reordering
- [ ] Document outline synchronization
- [ ] Word count aggregation

### 1.3 Project Management

- [ ] Recent files (QSettings + QAction list)
- [ ] File history navigation
- [ ] Project templates
- [ ] Metadata editing (title, author, language)

### 1.4 Search & Replace

- [ ] Find dialog (QDialog with QLineEdit)
- [ ] Replace dialog
- [ ] Find in selection, whole word, case-sensitive
- [ ] Regular expression support (QRegularExpression)

---

## PHASE 2: Plugin System MVP (Weeks 21-30)

**Status:** ⏳ PLANNED
**Target:** 0.5.0-alpha

### 2.1 Plugin UI Integration

- [ ] Plugin Manager dialog (QDialog)
- [ ] Plugin discovery from ~/.kalahari/plugins/
- [ ] Install/Uninstall/Enable/Disable UI
- [ ] Plugin settings panels (QWidget subclasses)

### 2.2 Four MVP Plugins

- [ ] **Meerkat (Statistics):** Word count, character count, reading time
- [ ] **Lion (Writing Goals):** Daily/weekly goals, progress tracking
- [ ] **Elephant (Notes):** Research notes, character cards
- [ ] **Cheetah (Quick Actions):** Keyboard shortcuts, snippets

### 2.3 Extension Point API

- [ ] IExporter C++ interface (PDF, DOCX, HTML)
- [ ] IPanelProvider C++ interface (dock panels)
- [ ] IAssistant C++ interface (AI assistants)
- [ ] Plugin API documentation

---

## PHASE 3: Feature Plugins (Weeks 31-44)

**Status:** ⏳ PLANNED
**Target:** 0.6.0-beta

### 3.1 Premium Plugin Development

- [ ] **AI Assistant Pro** ($19)
- [ ] **Advanced Analytics** ($14)
- [ ] **Export Suite** ($29)

### 3.2 Plugin Marketplace

- [ ] Online plugin repository
- [ ] License verification system
- [ ] Update notification system

---

## PHASE 4: Advanced Plugins (Weeks 45-56)

**Status:** ⏳ PLANNED
**Target:** 0.7.0-beta

### 4.1 Research & Collaboration

- [ ] **Research Pro** ($39)
- [ ] **Collaboration Pack** ($29)

### 4.2 Cloud Integration

- [ ] Cloud Sync Pro (SaaS $5-10/month)
- [ ] Auto-save to cloud
- [ ] Device synchronization

---

## PHASE 5: Polish & Release (Weeks 57-68)

**Status:** ⏳ PLANNED
**Target:** 1.0.0

### 5.1 Testing & QA

- [ ] Comprehensive test suite (unit + integration)
- [ ] Manual testing on all platforms
- [ ] Beta testing program (100 users)
- [ ] Bug triage and fixes

### 5.2 Documentation

- [ ] User manual (MkDocs)
- [ ] Plugin development guide
- [ ] API reference (Doxygen)
- [ ] Video tutorials

### 5.3 Packaging & Distribution

- [ ] Windows installer (NSIS or WiX)
- [ ] macOS .dmg (notarized)
- [ ] Linux .deb/.rpm packages
- [ ] Flatpak/Snap for universal Linux

### 5.4 Release 🎉

- [ ] Version 1.0.0 release
- [ ] Press kit
- [ ] Launch announcement
- [ ] Community support channels

---

## Key Milestones

- [x] **2025-10-31:** Phase 0 Foundation Complete (wxWidgets)
- [x] **2025-11-19:** Qt Migration Decision & Start
- [ ] **2025-12-17:** Phase 0 Qt Foundation Complete (4 weeks)
- [ ] **2026-05:** Phase 1 Core Editor Complete
- [ ] **2026-07:** Phase 2 Plugin System MVP Complete
- [ ] **2026-Q3:** Beta Release (0.7.0)
- [ ] **2026-Q4:** Kalahari 1.0 Release 🚀

---

## Success Criteria

### Phase 0 (Qt Foundation)
- ✅ wxWidgets archived (branch + tag)
- ✅ Qt6 build system working (all platforms)
- ✅ Documentation updated (CLAUDE.md, CHANGELOG.md, project_docs/)
- ⏳ QMainWindow with menu/toolbar/statusbar
- ⏳ Settings dialog with Qt layouts
- ⏳ Basic text editor with QPlainTextEdit
- ⏳ All 12 tasks complete (00001-00012)

### Phase 1 (Core Editor)
- Rich text editing with formatting
- Document structure (Parts, Chapters)
- Search & Replace
- Project management
- Word count & statistics

### Phases 2-5
- See individual phase sections above

---

## Technical Debt & Future Improvements

### Immediate (Phase 0-1)
- [ ] Qt Designer .ui files for dialogs (consider for Phase 1)
- [ ] QTest unit tests for GUI (supplement Catch2)
- [ ] Qt Linguist integration (.ts/.qm i18n)

### Medium-term (Phase 2-3)
- [ ] Custom QSyntaxHighlighter for book-specific formatting
- [ ] QUndoStack/QUndoCommand for undo/redo
- [ ] QDataStream for binary serialization (performance)

### Long-term (Phase 4-5)
- [ ] Qt WebEngine for HTML preview
- [ ] Qt Quick/QML for modern UI (future consideration)
- [ ] Qt Network for cloud sync
- [ ] Qt Concurrent for background tasks

---

## Notes

1. **Task Numbering:** Fresh start from #00001 (Qt era)
2. **wxWidgets Archive:** All wxWidgets code preserved in wxwidgets-archive branch
3. **Atomic Workflow:** Maintained from v5.x (30-120 min tasks)
4. **Testing:** Catch2 for core, QTest for GUI (Phase 1+)
5. **Qt LGPL:** Dynamic linking, no commercial license needed
6. **Plugin API:** Pure C++ types (no Qt types in API) → proprietary plugins OK

---

**Document Version:** 1.0 (Qt Migration)
**Last Update:** 2025-11-19
**Updated By:** Claude (Qt migration - Clean Slate approach)
