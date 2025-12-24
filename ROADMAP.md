# Kalahari Development Roadmap

> **Writer's IDE** - Qt6 Architecture | Phase-based Development

**Current Phase:** Phase 1 (Core Editor) IN PROGRESS
**Version:** 0.3.2-alpha
**Last Updated:** 2025-12-11

---

## MIGRATION CONTEXT (2025-11-19)

**Decision:** Migrated from wxWidgets to Qt6 for long-term quality and maintainability.

**Reason:** wxWidgets limitations (manual DPI scaling, wxStaticBoxSizer bugs, complex reactive patterns) incompatible with "opus magnum" quality standards. Qt6 provides automatic DPI scaling, QApplication::setFont() global styling, QSS theming, and superior documentation.

**Strategy:** Clean Slate Approach (Option B)
- **Archived:** wxwidgets-archive branch + v0.2.0-alpha-wxwidgets tag
- **Preserved:** Core (5,966 LOC), Tests (5,912 LOC), Bindings (120 LOC), Plugin system
- **Deleted:** GUI layer (28,098 LOC), 49 task files, bwx_sdk submodule
- **Timeline:** +4 weeks for Qt Foundation (Phase 0)

**Reference:** [QT_MIGRATION_ROADMAP.md](QT_MIGRATION_ROADMAP.md) - Complete migration plan

---

## PHASE 0: Qt Foundation COMPLETE

**Status:** COMPLETE (Started 2025-11-19, Finished 2025-11-21)
**Duration:** 2 days (estimate: 4 weeks)
**Target:** 0.3.0-alpha RELEASED (basic GUI) -> 0.3.1-alpha (Command Registry) COMPLETE

### Step 0: Preparation COMPLETE (Day 1, ~6 hours)

**Goal:** Archive wxWidgets, update build system, refresh documentation

- [x] Archive Current State - wxwidgets-archive branch, v0.2.0-alpha-wxwidgets tag
- [x] Clean Main Branch - Deleted 28,098 LOC (103 files)
- [x] Update Project Configuration - vcpkg.json Qt6, CMakeLists.txt Qt6 integration
- [x] Update CLAUDE.md - Qt6 patterns, Cardinal Rules
- [x] Create Fresh ROADMAP.md - Phase 0-5 structure
- [x] Update CHANGELOG.md - Added [0.3.0-alpha], [0.3.1-alpha] sections
- [x] Update project_docs/ - 02_tech_stack.md, 08_gui_design.md, 01_overview.md
- [x] Update .claude/ Resources - Skills verified Qt-compatible
- [x] Update Serena Memories - Migration decision documented
- [x] Final Push & Verification - All changes committed

### Week 1: Qt Hello World COMPLETE

**Goal:** Basic Qt6 application with window, menu, logging

- [x] Qt6 vcpkg Installation & CMake Configuration - Qt6 6.9.1, CI/CD updated
- [x] QMainWindow Skeleton - Menu bar, toolbar, status bar
- [x] Basic QDockWidget System - 5 dock panels, perspective save/restore

### Week 2: Settings System COMPLETE

**Goal:** Settings dialog with Qt layouts, JSON persistence

- [x] Settings Dialog Structure - QTabWidget, Apply/OK/Cancel, SettingsManager integration
- [x] Appearance Settings Panel - Font size, theme, icon size
- [x] Editor Settings Panel - Font family/size, tab size, line numbers, word wrap

### Week 3: Core Editor Foundation COMPLETE

**Goal:** Basic text editing with QPlainTextEdit

- [x] EditorWidget Basic Implementation - Settings integration, syntax highlighter stub
- [x] File Operations - New/Open/Save/SaveAs, dirty state, .klh files
- [x] Edit Operations - Undo/Redo/Cut/Copy/Paste/SelectAll

### Week 4: Panels & Polish COMPLETE

**Goal:** Navigator panel, About dialog, first release

- [x] Navigator Panel with QTreeWidget - Book hierarchy display, document integration
- [x] About Dialog & Help Menu - QMessageBox dialogs
- [x] Qt Foundation Release - v0.3.0-alpha tag

### Week 5: Command Registry Migration COMPLETE (2025-11-21)

**Goal:** Migrate Command Registry system from wxWidgets to Qt6

**Architecture Migrated:**
- Command Registry (singleton, ~200 LOC, framework-agnostic)
- Command struct (id, label, category, icons, shortcuts, execute/isEnabled/isChecked callbacks)
- IconSet (16/24/32px QPixmap, toQIcon() helper)
- KeyboardShortcut (Qt::Key + Qt::KeyboardModifiers, toQKeySequence())
- ToolbarBuilder (dynamic QToolBar generation from registry)
- MenuBuilder (hierarchical menu support, 150 LOC)

**Benefits Delivered:**
- Single source of truth for all commands (15 registered: File, Edit, Help)
- Plugin commands integrate seamlessly (ICommandProvider interface ready)
- Customizable toolbars (user can add/remove/reorder in Phase 1)
- Command Palette ready (Ctrl+Shift+P foundation, Phase 1 implementation)
- No hardcoded QAction connections (MainWindow uses builders)

- [x] Command Registry Qt Migration - Recovered from wxwidgets-archive, adapted to Qt6
- [x] Plugin Integration Foundation - ICommandProvider, EventBus Qt6, QWidget* panels

---

## PHASE 1: Core Editor (Weeks 1-20) IN PROGRESS

**Status:** IN PROGRESS (Started 2025-11-21)
**Target:** 0.4.0-alpha
**Timeline:** ~5 months

**Development Order:** Menu → Toolbars → Project File → Perspectives → Navigator → Editor

### 1.0 Menu System Review & Cleanup COMPLETE

**Philosophy:** Clean, coherent menu structure without duplicates. Every menu item serves a purpose.

**Resolved Issues (OpenSpec #00030):**
- ~~Duplicate toolbar entries in VIEW menu~~ → Dynamic Toolbars submenu via ToolbarManager
- ~~Inconsistent naming~~ → Toolbars now use consistent names
- Added standard keyboard shortcuts (F1-F6, F11, Ctrl+B/I/U/F/H/W)
- Created RecentBooksManager for FILE/Recent Books submenu

**Tasks:**
- [x] Full menu audit - document all 9 menus and their items
- [x] Remove VIEW/Toolbars duplicates (dynamic submenu)
- [x] Verify all CommandRegistry commands have correct phase markers
- [x] Ensure menu structure matches application architecture
- [x] Review keyboard shortcuts for conflicts
- [x] Document final menu specification (in OpenSpec #00030)

### 1.1 Toolbar System COMPLETE (OpenSpec #00031)

**Philosophy:** Toolbars mirror menu structure. Each toolbar groups related actions.

**Tasks:**
- [x] Synchronize toolbar names with menu structure
- [x] Create Format Toolbar (font dropdown, size spinner, formatting commands)
- [x] Create Insert Toolbar, Styles Toolbar (optional, hidden by default)
- [x] Toolbar customization UI ("Customize Toolbars..." dialog)
- [x] Drag & drop toolbar button reordering (via dialog Move Up/Down)
- [x] Save/restore toolbar configuration per user
- [x] Context menu on toolbars (visibility toggles, lock positions)
- [x] User-defined toolbar creation/deletion/rename

**Known Limitations:**
- Toolbar drag behavior shrinks adjacent toolbar instead of swapping (Qt default)
- Overflow menu (chevron) deferred - Qt6 lacks built-in support

### 1.2 Project File System (OpenSpec #00033)

**Philosophy:** Solution-like folder structure with .klh JSON manifest.

**Current State:** COMPLETE - All phases implemented

**Architecture Decision (ADR-005):** Interim RTF editing uses QTextEdit.
- ProjectManager handles RTF file I/O (separation of concerns)
- EditorPanel uses QTextEdit for display (can swap to custom editor later)
- See `project_docs/15_text_editor_architecture.md` for migration path

**Completed:**
- [x] Phase A: Analysis & Design
- [x] Phase B: Core Infrastructure (ProjectManager, BookElement)
- [x] Phase C: NewItemDialog
- [x] Phase D: Project Loading
- [x] Phase E: Chapter Editing (QTextEdit, dirty tracking, Save All)
- [x] Phase F: Standalone Mode (StandaloneInfoBar, AddToProject, Other Files)
- [x] Phase G: PropertiesPanel (project/chapter properties, contextual views)

**Completed:**
- [x] Phase H: Export/Import Archive (.klh.zip)
- [x] Phase I: SKIPPED (no legacy users)
- [x] Phase J: Manual testing done

**Status: COMPLETE (100%)**

**Dependencies for full functionality:**
- Word count (chapters): Requires Custom Text Editor (1.5)
- Statistics aggregation: Requires Statistics Module (1.7)
- Chapter status field: Add to BookElement class

### 1.3 Perspective System

**Philosophy:** Named window layouts for different workflows (Writer, Editor, Researcher).

**Current State:** Basic saveState/restoreState works, but no named perspectives

**Tasks:**
- [ ] Define default perspectives (Writer, Editor, Researcher, Planner)
- [ ] Perspective save/load mechanism
- [ ] "Save Current Perspective..." dialog
- [ ] "Manage Perspectives..." dialog
- [ ] Default perspective on first run
- [ ] Perspective switching via VIEW menu and keyboard shortcuts

### 1.4 Navigator Panel COMPLETE

**Philosophy:** Tree view of project structure with full editing capabilities.

**Current State:** ~~Basic QTreeWidget showing book structure~~ Full-featured navigator (OpenSpec #00034, #00036)

**Tasks:**
- [x] Context menu (right-click) for all operations (OpenSpec #00034)
- [x] Drag & drop reordering of chapters/parts (OpenSpec #00034)
- [x] Icons for element types (Part, Chapter, Scene, Note) (OpenSpec #00034)
- [x] Double-click opens chapter in editor (OpenSpec #00034)
- [x] Synchronization with editor (highlight current chapter) (OpenSpec #00034)
- [x] Search/filter within navigator (OpenSpec #00034)
- [x] Status submenu for chapters (OpenSpec #00036)

### 1.5 Custom Text Editor (OpenSpec #00042) IN PROGRESS

**Philosophy:** Rich text editor optimized for long-form writing (novels, books).

**Current State:** Phases 1-7 largely complete, testing in progress

**Phase 1: KML Model Layer** COMPLETE
- [x] KML Element base class with ElementType enum
- [x] KML Text Run with styling support
- [x] KML Inline Elements (Bold, Italic, Underline, Strike)
- [x] KML Paragraph with text manipulation
- [x] KML Document with observer pattern
- [x] KML Parser (read/write)
- [x] KML Table support (`<table>`, `<tr>`, `<td>`, `<th>`)

**Phase 2: Layout Engine** COMPLETE
- [x] Paragraph layout with text measurement
- [x] Table layout with cell sizing
- [x] Virtual scroll manager for performance
- [x] Layout manager integration

**Phase 3: BookEditor Widget** COMPLETE
- [x] Custom QWidget-based editor
- [x] Rendering pipeline (QPainter)
- [x] Cursor and selection handling
- [x] Keyboard navigation
- [x] Mouse interaction
- [x] Accessibility support

**Phase 4: Text Input & Editing** COMPLETE
- [x] Unicode input with IME support
- [x] Rich text formatting commands
- [x] Undo/Redo with QUndoStack
- [x] Clipboard (KML, HTML, plain text)

**Phase 5: View Modes** COMPLETE
- [x] Continuous Mode (default)
- [x] Page Mode with A4/Letter sizes
- [x] Typewriter Mode with scroll animation
- [x] Focus Mode (paragraph/sentence/line dimming)
- [x] Distraction-Free Mode with overlays
- [x] Split View (horizontal/vertical)

**Phase 6: Analytics & Language Services** COMPLETE
- [x] StatisticsCollector (word count, reading time)
- [x] WordFrequencyAnalyzer (overused words)
- [x] SpellCheckService (Hunspell integration)
- [x] GrammarCheckService (LanguageTool API)

**Phase 7: Testing & Documentation** IN PROGRESS
- [x] 7.17 Unit Tests (SpellCheck, Grammar)
- [x] 7.18 Integration Tests (full workflow)
- [ ] 7.19 Manual Testing
- [ ] 7.20 Documentation
- [ ] 7.21 Final Review

**Test Status:** 623 test cases, 4216 assertions passing

### 1.6 Search & Replace

**Philosophy:** Fast, powerful search within current document and entire project.

**Tasks:**
- [ ] Find dialog (Ctrl+F)
- [ ] Replace dialog (Ctrl+H)
- [ ] Options: case-sensitive, whole word, regex
- [ ] Find in selection
- [ ] Find in entire project (all chapters)
- [ ] Search results panel
- [ ] Highlight all matches

### 1.7 Statistics Architecture (3-Tier System)

**Philosophy:** Live monitoring (Bar) + Weekly analysis (Panel) + Deep dive (Central Window)

- [ ] **Statistics Bar** (top of central window, always visible)
- [ ] **Weekly Statistics Panel** (dockable, toggleable via VIEW menu)
- [ ] **Advanced Analytics** (central window tab, Premium plugin $14)

### 1.8 Mind Maps & Timelines (Library Architecture)

**Philosophy:** Multiple maps/timelines per project, edited in central window

- [ ] **Mind Maps Library** (MindMaps/*.kmap files)
- [ ] **Timelines Library** (Timelines/*.ktl files)

### 1.9 Theme & Icon System COMPLETE

**Philosophy:** User-configurable themes with per-theme color customization and centralized icon management.

**Architecture (Implemented):**

1. **ThemeManager (QObject singleton)**
   - Loads theme JSON files (Light.json, Dark.json)
   - Emits `themeChanged(const Theme&)` signal on theme switch
   - Stores per-theme user color overrides in SettingsManager
   - Applies QPalette to QApplication for native widget styling

2. **IconRegistry (Runtime SVG rendering)**
   - SVG templates with `{COLOR_PRIMARY}` / `{COLOR_SECONDARY}` placeholders
   - Renders icons on-demand with current theme colors
   - Connected to ThemeManager::themeChanged for color updates
   - Caches rendered QPixmaps for performance

3. **Per-Theme Icon Colors (SettingsManager)**
   - Each theme can have custom primary/secondary icon colors
   - Stored as: `iconColors.Light.primary`, `iconColors.Dark.primary`, etc.
   - User overrides persist across sessions
   - "Restore Defaults" resets to theme file values

4. **ArtProvider (Central Visual Resource Manager)**
   - Singleton facade for all icon/image requests
   - 9 icon contexts (toolbar, menu, treeView, tabBar, statusBar, button, panel, dialog, comboBox)
   - Self-updating QActions via `createAction()` method
   - `getPreviewPixmap()` for HiDPI icon previews
   - Automatic icon refresh on theme/color changes

5. **KalahariStyle (QProxyStyle)**
   - Reads icon sizes from ArtProvider for each context
   - Applied globally in main.cpp
   - Provides consistent icon sizing across all Qt widgets

6. **BusyIndicator (Reusable Spinner Widget)**
   - Modal overlay with animated 3 pulsating dots
   - Theme-aware primary color
   - Static `tick()` method for animation during blocking operations
   - `BusyIndicator::run()` helper for simple usage

**Settings Dialog Structure:**

- **Appearance/General:** Font size, language selection
- **Appearance/Theme:** Theme selector, color overrides, "Restore Defaults"
- **Appearance/Icons:** Icon theme selector (twotone/filled/outlined/rounded), sizes for all 9 contexts, primary/secondary color buttons with preview
- **Editor:** Font, line spacing, spell check (placeholder)
- **Advanced:** Diagnostic mode, log configuration

**Implementation Status:**
- [x] Theme JSON schema with full color palette
- [x] ThemeManager class with QPalette integration
- [x] Default themes: Light.json + Dark.json
- [x] IconRegistry SVG color replacement
- [x] Settings Dialog with QTreeWidget + QStackedWidget (14 pages)
- [x] Per-theme icon color storage and persistence
- [x] GUI icon refresh on theme/color change (toolbar + menu)
- [x] ArtProvider central visual resource manager
- [x] KalahariStyle QProxyStyle integration
- [x] Extended icon size configuration (9 contexts)
- [x] Icon theme selector UI with preview
- [x] BusyIndicator reusable spinner widget
- [x] Log Panel enhanced (real-time logs, colored output, batched updates) - OpenSpec #00027
- [x] Centralized icon color management (ArtProvider single source of truth) - OpenSpec #00032

---

## PHASE 2: Plugin System MVP (Weeks 21-30)

**Status:** PLANNED
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

**Status:** PLANNED
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

**Status:** PLANNED
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

**Status:** PLANNED
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

### 5.4 Release

- [ ] Version 1.0.0 release
- [ ] Press kit
- [ ] Launch announcement
- [ ] Community support channels

---

## Key Milestones

- [x] **2025-10-31:** Phase 0 Foundation Complete (wxWidgets)
- [x] **2025-11-19:** Qt Migration Decision & Start
- [x] **2025-11-21:** Phase 0 Qt Foundation Complete (2 days!)
- [x] **2025-11-27:** Theme & Icon System Complete (ArtProvider, BusyIndicator)
- [x] **2025-11-27:** Enhanced Log Panel Complete (real-time spdlog, mode visibility)
- [x] **2025-12-07:** Theme Color Configuration Complete (OpenSpec #00027)
- [x] **2025-12-10:** Menu System Review & Cleanup Complete (OpenSpec #00030)
- [x] **2025-12-11:** Theme & Icons Optimization Complete (OpenSpec #00032)
- [x] **2025-12-15:** Navigator Panel Enhancements Complete (OpenSpec #00034)
- [x] **2025-12-16:** Dashboard & Navigator Enhancements Complete (OpenSpec #00036)
- [x] **2025-12-16:** Quick Actions & Help Toolbar Complete (OpenSpec #00037)
- [x] **2025-12-23:** Custom Text Editor Phase 6 Analytics Complete (OpenSpec #00042)
- [ ] **2025-12:** Custom Text Editor Phase 7 Testing Complete (OpenSpec #00042)
- [ ] **2026-05:** Phase 1 Core Editor Complete
- [ ] **2026-07:** Phase 2 Plugin System MVP Complete
- [ ] **2026-Q3:** Beta Release (0.7.0)
- [ ] **2026-Q4:** Kalahari 1.0 Release

---

## Success Criteria

### Phase 0 (Qt Foundation) COMPLETE
- wxWidgets archived (branch + tag)
- Qt6 build system working (all platforms)
- Documentation updated (CLAUDE.md, CHANGELOG.md, project_docs/)
- QMainWindow with menu/toolbar/statusbar
- Settings dialog with Qt layouts
- Basic text editor with QPlainTextEdit
- Command Registry migrated

### Phase 1 (Core Editor) IN PROGRESS
- Rich text editing with formatting
- Document structure (Parts, Chapters)
- Search & Replace
- Project management
- Word count & statistics
- Theme & Icon system foundation

### Phases 2-5
- See individual phase sections above

---

## Technical Debt & Future Improvements

### Immediate (Phase 0-1)
- [ ] Qt Designer .ui files for dialogs (consider for Phase 1)
- [ ] QTest unit tests for GUI (supplement Catch2)
- [ ] Qt Linguist integration (.ts/.qm i18n)
- [x] Centralized icon refresh (ArtProvider::resourcesChanged signal) - OpenSpec #00032

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

1. **OpenSpec Tasks:** All implementation tasks tracked in `openspec/changes/` directory
2. **wxWidgets Archive:** All wxWidgets code preserved in wxwidgets-archive branch
3. **Atomic Workflow:** Maintained from v5.x (30-120 min tasks)
4. **Testing:** Catch2 for core, QTest for GUI (Phase 1+)
5. **Qt LGPL:** Dynamic linking, no commercial license needed
6. **Plugin API:** Pure C++ types (no Qt types in API) -> proprietary plugins OK

---

**Document Version:** 2.6
**Last Update:** 2025-12-23
**Updated By:** Claude (Custom Text Editor 1.5 Phase 6 COMPLETE, Phase 7 testing in progress)
