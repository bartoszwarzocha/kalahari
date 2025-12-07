# Kalahari Development Roadmap

> **Writer's IDE** - Qt6 Architecture | Phase-based Development

**Current Phase:** Phase 1 (Core Editor) IN PROGRESS
**Version:** 0.3.2-alpha
**Last Updated:** 2025-12-07

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

### 1.0 Complete Menu System

**Philosophy:** 9 menus (FILE, EDIT, BOOK, INSERT, FORMAT, TOOLS, ASSISTANT, VIEW, HELP)

- [ ] Menu BOOK (book structure operations)
- [ ] Menu INSERT (text content elements)
- [ ] Menu FORMAT (rich text formatting)
- [ ] Menu TOOLS (utilities & plugins)
- [ ] Menu ASSISTANT (AI assistants)
- [ ] All commands registered in CommandRegistry
- [ ] All menu items show QMessageBox with "Coming in Phase X"
- [ ] Toolbar customization

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

### 1.5 Statistics Architecture (3-Tier System)

**Philosophy:** Live monitoring (Bar) + Weekly analysis (Panel) + Deep dive (Central Window)

- [ ] **Statistics Bar** (top of central window, always visible)
- [ ] **Weekly Statistics Panel** (dockable, toggleable via VIEW menu)
- [ ] **Advanced Analytics** (central window tab, Premium plugin $14)

### 1.6 Mind Maps & Timelines (Library Architecture)

**Philosophy:** Multiple maps/timelines per project, edited in central window

- [ ] **Mind Maps Library** (MindMaps/*.kmap files)
- [ ] **Timelines Library** (Timelines/*.ktl files)

### 1.7 Theme & Icon System COMPLETE

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
- [ ] Centralized icon refresh (IconRegistry::iconsInvalidated signal)

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

**Document Version:** 2.2
**Last Update:** 2025-12-07
**Updated By:** Claude (OpenSpec #00027 Theme Color Configuration Complete)
