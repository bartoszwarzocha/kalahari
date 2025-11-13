# Kalahari Development Roadmap

> **Writer's IDE** - Atomic Task Structure | Phase-based Development

**Current Phase:** Phase 1 (Core Editor) 🚀 IN PROGRESS
**Current Task:** Zagadnienie 1.2 - Command Registry Architecture (Tasks #00031-00032 ✅ COMPLETE)
**Version:** 0.2.0-alpha
**Last Updated:** 2025-11-13

---

## PHASE 0: Foundation ✅ COMPLETE

**Status:** ✅ 100% COMPLETE (2025-10-26 to 2025-11-03)
**Duration:** 8 days

### 0.1 Core Infrastructure ✅ COMPLETE

- [x] CMake build system (all platforms: Windows, macOS, Linux)
- [x] vcpkg integration (manifest mode)
- [x] wxWidgets 3.3.0+ basic application window
- [x] Main window with menu bar, toolbar, status bar
- [x] Settings system (JSON persistence with nlohmann_json)
- [x] Logging system (spdlog - structured, multi-level)
- [x] Build automation scripts (cross-platform)
- [x] CI/CD pipelines (GitHub Actions: Linux, macOS, Windows)

### 0.2 Plugin Architecture ✅ COMPLETE

- [x] Python 3.11 embedding (bundled with application)
- [x] pybind11 integration (C++/Python interop)
- [x] kalahari_api module with Logger bindings
- [x] Plugin Manager singleton (discovery, loading, lifecycle)
- [x] Extension Points system (IExporter, IPanelProvider, IAssistant, IPlugin)
- [x] ExtensionPointRegistry singleton (type-safe retrieval)
- [x] Event Bus (async, thread-safe, GUI-aware marshalling)
- [x] Event subscriber pattern with unsubscribe support
- [x] 8 standard event types (document, editor, plugin, goal events)
- [x] .kplugin format handler (ZIP reading/writing with libzip)
- [x] Plugin manifest structure (manifest.json + plugin.py + assets/)
- [x] Plugin extraction and validation (PluginArchive RAII wrapper)
- [x] Plugin API versioning (semantic versioning checks)

### 0.3 Document Model ✅ COMPLETE

- [x] BookElement base class (flexible string-based type system)
- [x] Part class (chapter container with aggregation)
- [x] Book class (frontMatter, body Parts, backMatter structure)
- [x] Document wrapper class (project metadata: title, author, language, UUID)
- [x] JSON serialization (toJson/fromJson pattern)
- [x] ISO 8601 timestamps (created, modified)
- [x] Metadata map for extensibility (plugin custom fields)
- [x] .klh file format (ZIP container with JSON metadata)
- [x] DocumentArchive implementation (libzip integration)
- [x] Basic CRUD operations (Create, Read, Update, Delete)
- [x] Document::load() from .klh archive
- [x] Smart pointers (std::shared_ptr) for RAII memory management
- [x] Word count aggregation (Part → Book → Document)

---

## PHASE 1: Core Editor 🚀 IN PROGRESS

**Status:** 🚀 IN PROGRESS (Started 2025-11-04)
**Target:** 0.2.0-alpha
**Timeline:** Weeks 9-20 (12 weeks)

### 1.1 Custom Text Editor Control ✅ COMPLETE

**Status:** ✅ COMPLETE (2025-11-04 to 2025-11-06, 3 days)

- [x] bwxTextDocument class (Gap Buffer storage, undo/redo, formatting) - 1,450 LOC
- [x] FullViewRenderer class (layout, hit testing, viewport culling) - 850 LOC
- [x] bwxTextEditor main control (~1,000 LOC)
- [x] Two-phase construction (default + full constructor)
- [x] Keyboard event handling (OnChar, OnKeyDown)
- [x] Mouse event handling (OnLeftDown, OnMotion, OnMouseWheel)
- [x] Focus management (OnSetFocus, OnKillFocus)
- [x] Editing operations (Copy/Cut/Paste/SelectAll/Undo/Redo)
- [x] Keyboard shortcuts (Ctrl+C/X/V/A/Z/Y)
- [x] Formatting shortcuts (Ctrl+B/I/U for Bold/Italic/Underline)
- [x] Navigation (Arrow keys, Home/End with Shift for selection)
- [x] Caret management (blinking animation 500ms, scroll-to-cursor)
- [x] Mouse wheel scrolling, auto-scroll to keep cursor visible
- [x] View mode architecture (VIEW_FULL MVP, VIEW_PAGE/TYPEWRITER/PUBLISHER future)
- [x] MVC architecture (Model: bwxTextDocument, View: ITextRenderer, Controller: bwxTextEditor)
- [x] Observer Pattern (IDocumentObserver for document notifications)
- [x] EditorPanel implements IDocumentObserver (4 callbacks)
- [x] Observer lifecycle management (AddObserver/RemoveObserver)
- [x] TRUE DEBOUNCING (wxTIMER_ONE_SHOT restarts on change, fires 500ms after typing stops)
- [x] UpdateWordCount() called only in debounced timer (O(n) only after idle)
- [x] Bug fixes (m_isModified tracking, hasUnsavedChanges() works)
- [x] MainWindow integration (Format menu, Edit menu, View Mode menu)
- [x] Buffered painting (wxBufferedDC for flicker-free rendering)
- [x] EditorSettingsPanel (14 configurable parameters)
- [x] Settings persistence (JSON storage in ~/.kalahari/settings.json)
- [x] Live settings updates (apply without restart)
- [x] Test suite (75+ test cases, 2,239 assertions)
- [x] Build status: ✅ All platforms (Linux, macOS, Windows) CI/CD passing

### 1.2 Command Registry Architecture ✅ COMPLETE

**Status:** ✅ COMPLETE (Tasks #00031-#00034, Full documentation, 2025-11-13)

#### Core Structures ✅ COMPLETE

- [x] IconSet struct (16/24/32px bitmap storage)
- [x] KeyboardShortcut struct (toString/fromString parsing, operators)
- [x] Command struct (complete descriptor with callbacks)
- [x] command.h (177 LOC), command.cpp (203 LOC)

#### CommandRegistry ✅ COMPLETE

- [x] CommandRegistry singleton (Meyers pattern, thread-safe C++11+)
- [x] Registration API (registerCommand, unregisterCommand, isCommandRegistered)
- [x] Query API (getCommand, getCommandsByCategory, getAllCommands, getCategories)
- [x] Storage: std::unordered_map for O(1) lookup
- [x] executeCommand() with full error handling
- [x] canExecute() and isChecked() precondition checking
- [x] CommandExecutionResult enum (5 states)
- [x] CommandErrorHandler custom callback support
- [x] command_registry.h (195 LOC), command_registry.cpp (174 LOC)
- [x] Tests: 16 test cases, 81 assertions - 100% pass rate

#### ShortcutManager ✅ COMPLETE

- [x] ShortcutManager singleton (Meyers pattern, thread-safe)
- [x] Binding API (bindShortcut, unbindShortcut, isShortcutBound)
- [x] Query API (getCommandForShortcut → std::optional, getAllBindings)
- [x] Execution API (executeShortcut → delegates to CommandRegistry)
- [x] JSON persistence (saveToFile, loadFromFile)
- [x] operator< for KeyboardShortcut (std::map compatibility)
- [x] shortcut_manager.h (145 LOC), shortcut_manager.cpp (168 LOC)
- [x] Tests: 8 test cases, 42 assertions - 100% pass rate

#### Command Registration ✅ COMPLETE

- [x] File Menu commands (6 total: new, open, save, save_as, settings, exit)
- [x] Edit Menu commands (7 total: undo, redo, cut, copy, paste, select_all)
- [x] Format Menu commands (5 total: bold, italic, underline, font, clear_formatting)
- [x] Keyboard shortcuts bound (Ctrl+N/O/S/Z/X/C/V/A/B/I/U)
- [x] Event handlers refactored to use CommandRegistry::executeCommand()
- [x] Total: 18 commands registered in CommandRegistry
- [x] Tests: Manual verification - all commands work via menu and shortcuts
- [x] Full test suite: 655 assertions, 91 test cases - 100% pass rate

#### Dynamic UI Generation ✅ COMPLETE (Tasks #00031-00032)

- [x] **Task #00031:** Create MenuBuilder class (buildFromRegistry, addSeparator, addSubmenu) ✅ 2025-11-13
- [x] Replace hardcoded createMenuBar() with MenuBuilder (createMenuBarDynamic)
- [x] Event handlers automatically bound to CommandRegistry::executeCommand()
- [x] Verify all menus build correctly from CommandRegistry (build success)
- [x] **Task #00032:** Create ToolbarBuilder class (buildFromCommands, addSeparator) ✅ 2025-11-13
- [x] Replace hardcoded createToolBar() with ToolbarBuilder (createToolBarDynamic)
- [x] Event handlers automatically bound to executeCommand()
- [x] Verify toolbar builds correctly, icons load from CommandRegistry
- [x] **Task #00033:** Settings command integration with CommandRegistry ✅ 2025-11-13
- [x] Move file.settings execute() from stub to real implementation
- [x] Remove old event table binding (EVT_MENU wxID_PREFERENCES)
- [x] Settings dialog opens via CommandRegistry::executeCommand("file.settings")
- [x] **Task #00034:** Command Registry Architecture Documentation ✅ 2025-11-13
- [x] Write Command Registry architecture document (project_docs/18, ~15k words)
- [x] Update ARCHITECTURE.md with Command Registry section
- [x] Add plugin integration guide (how plugins register commands)
- [ ] **Task #00035:** Manual Testing Session - Command Registry System 📋 Planned
- [ ] Test all menu items (File, Edit, Format) - 15+ tests
- [ ] Test all toolbar buttons - 8 tests
- [ ] Test all keyboard shortcuts - 9 tests
- [ ] Test enabled/disabled states (no document → Cut/Paste disabled)
- [ ] Test checked/unchecked states (Format menu toggles)
- [ ] Test dynamic updates (selection changes → format menu states)
- [ ] Verify state propagation (menu + toolbar sync)
- [ ] Verify Settings integration (menu/toolbar/keyboard)
- [ ] Cross-UI consistency (menu/toolbar/keyboard same behavior)
- [ ] Document test results and any issues found

### 1.3 Settings System Enhancement 📋 PLANNED

**Status:** 📋 PLANNED (7 tasks)

#### Settings Verification & Polish

- [ ] Icon size persistence verification (verify save/load from settings.json)
- [ ] After restart, icon size loaded correctly
- [ ] UI displays correct size on startup
- [ ] Font scaling live preview (text scaling spinner 0.8x-2.0x)
- [ ] Real-time font size update in example text
- [ ] EVT_SPINCTRLDOUBLE event binding
- [ ] Font scaling apply implementation (apply font scale to all UI)
- [ ] Update all wxStaticText controls with scaled font
- [ ] Rebuild UI with new font sizes
- [ ] Font scaling persistence verification (verify save/load)
- [ ] After restart, font scaling loaded correctly
- [ ] Dynamic text wrapping verification (verify word wrap setting)
- [ ] Theme restart dialog verification (confirm dialog on theme change)
- [ ] Navigator panel cleanup (remove debug code, verify docking)

### 1.4 Chapter Management 📋 PLANNED

- [ ] Add new chapter to Part (UI + model)
- [ ] Delete chapter from Part (confirmation dialog)
- [ ] Rename chapter (inline edit or dialog)
- [ ] Move chapter within Part (drag & drop or move up/down buttons)
- [ ] Move chapter between Parts (drag & drop)
- [ ] Add new Part to Book (UI + model)
- [ ] Delete Part from Book (confirmation dialog)
- [ ] Rename Part (inline edit or dialog)
- [ ] Move Part in Book structure (reorder)
- [ ] wxTreeCtrl integration (Navigator panel)
- [ ] Tree node icons for Parts and Chapters
- [ ] Right-click context menu (Add, Delete, Rename, Move)
- [ ] Keyboard shortcuts for tree operations
- [ ] Undo support for structure changes

### 1.5 Content Persistence 📋 PLANNED

- [ ] Serialize Chapter content to .klh ZIP
- [ ] Store each Chapter as separate file in ZIP (chapters/001.json, chapters/002.json)
- [ ] Lazy loading architecture (load Chapter content on-demand)
- [ ] Unload Chapter content when not in use (memory optimization)
- [ ] Save current Chapter on switch (auto-save to .klh)
- [ ] Document::save() full implementation
- [ ] Document::saveAs() full implementation
- [ ] Document::load() full implementation (with Chapter content)
- [ ] Dirty flag tracking (unsaved changes indicator)
- [ ] Modified indicator in window title
- [ ] Confirm on close with unsaved changes

### 1.6 Text Formatting 📋 PLANNED

- [ ] Heading styles (H1-H6) with keyboard shortcuts
- [ ] Bold/Italic/Underline (already in 1.1, verify)
- [ ] Font family selection (dialog)
- [ ] Font size selection (dropdown)
- [ ] Text color (color picker)
- [ ] Background color (highlight)
- [ ] Clear formatting command
- [ ] Paragraph alignment (left, center, right, justify)
- [ ] Line spacing (single, 1.5, double, custom)
- [ ] First-line indentation
- [ ] Paragraph spacing (before/after)
- [ ] Lists (bulleted, numbered)
- [ ] Nested lists support
- [ ] Formatting toolbar (quick access buttons)

### 1.7 Undo/Redo System 📋 PLANNED

- [ ] ICommand interface (Execute, Undo, Redo)
- [ ] CommandHistory class (push, undo, redo, clear)
- [ ] Command stack size limit (default 100, configurable)
- [ ] Merge consecutive typing commands (performance optimization)
- [ ] Undo/Redo menu items (already bound in 1.2, implement logic)
- [ ] Keyboard shortcuts (Ctrl+Z, Ctrl+Y - already bound)
- [ ] Document modification tracking (integrate with dirty flag)
- [ ] Undo/Redo for text edits
- [ ] Undo/Redo for formatting changes
- [ ] Undo/Redo for structure changes (Chapter add/delete/move)

### 1.8 Find & Replace 📋 PLANNED

- [ ] Find dialog (Ctrl+F)
- [ ] Find Next (F3)
- [ ] Find Previous (Shift+F3)
- [ ] Replace dialog (Ctrl+H)
- [ ] Replace current match
- [ ] Replace All (with confirmation)
- [ ] Case sensitive option
- [ ] Whole word only option
- [ ] Regular expression support
- [ ] Search scope (current Chapter, current Part, whole Book)
- [ ] Search results highlighting
- [ ] Find in files (search across all Chapters)

### 1.9 Auto-Save & Backup 📋 PLANNED

- [ ] Auto-save timer (configurable interval: 1-10 minutes)
- [ ] Auto-save to .klh file (background thread)
- [ ] Auto-save indicator (status bar)
- [ ] Disable auto-save option (in Settings)
- [ ] Crash recovery system (detect unclean shutdown)
- [ ] Recover unsaved changes dialog (on startup after crash)
- [ ] Backup system (rolling snapshots)
- [ ] Backup retention policy (keep last N backups, configurable)
- [ ] Backup location (default ~/.kalahari/backups/, configurable)
- [ ] Restore from backup UI (list backups, preview, restore)
- [ ] Manual backup command (File → Create Backup)

### 1.10 UX Polish 📋 PLANNED

- [ ] Focus modes (Normal, Focused, Distraction-Free)
- [ ] Focused mode (hide panels, show only Editor)
- [ ] Distraction-Free mode (fullscreen, hide all UI)
- [ ] Keyboard shortcuts for focus mode switching (F11, Shift+F11)
- [ ] Word count live updates (status bar)
- [ ] Session statistics (words written today, session time)
- [ ] Writing streak tracking (days in a row)
- [ ] Goal setting UI (daily word count goal)
- [ ] Goal progress indicator (status bar)
- [ ] Spell checking integration (red underline, suggestions)
- [ ] Custom dictionary (add words)
- [ ] Statistics Panel (charts, graphs)
- [ ] Reading time estimation (based on word count, configurable WPM)

---

## PHASE 2: Plugin System MVP 📋 PLANNED

**Status:** ⏳ Pending
**Target:** 0.3.0-beta
**Timeline:** Weeks 21-30 (2-3 months)

### 2.1 Plugin Management UI 📋 PLANNED

- [ ] Plugin list panel (installed plugins, status: enabled/disabled)
- [ ] Plugin details view (name, version, author, description)
- [ ] Enable/disable plugin toggle
- [ ] Plugin configuration button (opens plugin-specific settings dialog)
- [ ] .kplugin installation (drag & drop support)
- [ ] .kplugin installation (file picker dialog)
- [ ] Plugin uninstallation (with confirmation)
- [ ] Plugin update checking (version comparison)
- [ ] Update available indicator (badge or icon)
- [ ] Download and install update (from URL or file)

### 2.2 Plugin API Core 📋 PLANNED

- [ ] Command registration API (plugins add menu items, toolbar buttons)
- [ ] Panel registration API (plugins add dockable panels)
- [ ] Event subscription API (plugins listen to document events)
- [ ] Settings API (plugins save/load preferences to ~/.kalahari/plugins/<name>/)
- [ ] Resource access API (plugins access icons, translations)
- [ ] Dialog API (plugins create modal/modeless dialogs)
- [ ] Logger API extension (plugin-specific log prefix)

### 2.3 Plugin 1: DOCX Exporter (Free) 📋 PLANNED

- [ ] python-docx integration (dependency management)
- [ ] Export Document → .docx command (File → Export → DOCX)
- [ ] Formatting preservation (bold, italic, underline)
- [ ] Basic styles mapping (H1-H6 → Word Heading styles)
- [ ] Paragraph formatting export (alignment, spacing)
- [ ] Configuration UI (page size, margins, font)
- [ ] Progress indicator (for large documents)
- [ ] Error handling (write failures, disk full)

### 2.4 Plugin 2: Markdown Tools (Free) 📋 PLANNED

- [ ] Markdown parsing library integration (python-markdown or similar)
- [ ] Import .md → Document command (File → Import → Markdown)
- [ ] Export Document → .md command (File → Export → Markdown)
- [ ] Formatting conversion (bold/italic/headings → Markdown syntax)
- [ ] Markdown preview panel (real-time rendering)
- [ ] Syntax highlighting for Markdown mode (optional editor mode)
- [ ] Configuration UI (Markdown dialect, extensions)

### 2.5 Plugin 3: Basic Statistics (Free) 📋 PLANNED

- [ ] Word count calculation (with/without spaces)
- [ ] Character count calculation
- [ ] Reading time estimation (configurable WPM: 200-300)
- [ ] Session statistics (words written today, this week, this month)
- [ ] Writing streak tracking (consecutive days with writing)
- [ ] Statistics panel (dockable, with charts)
- [ ] matplotlib integration (bar charts, line graphs)
- [ ] Export statistics to CSV (for external analysis)

### 2.6 Plugin 4: Assistant Lion (Free) 📋 PLANNED

- [ ] Graphical assistant panel (dockable, bottom-right default)
- [ ] Lion animal graphics (6 moods: happy, thinking, praising, warning, sleeping, excited)
- [ ] Speech bubble UI (text messages from Lion)
- [ ] Basic triggers (break reminder after N minutes, goal reached, session milestone)
- [ ] Personality system (Lion voice/tone: encouraging, friendly)
- [ ] Configuration UI (enable/disable, reminder frequency, personality intensity)
- [ ] Click interaction (cycle moods, dismiss message)

---

## PHASE 3: Feature Plugins 📋 PLANNED

**Status:** ⏳ Pending
**Target:** 0.4.0-beta
**Timeline:** Weeks 31-44 (3-4 months)

### 3.1 Free Plugins Ecosystem 📋 PLANNED

- [ ] PDF Exporter (reportlab - export to PDF with basic formatting)
- [ ] TXT Export (plain text export, configurable line endings)
- [ ] RTF Import/Export (rich text format support)
- [ ] Spell Checker (hunspell integration, multi-language)
- [ ] Themes (Dark, Savanna, Midnight - UI color schemes)
- [ ] Keyboard Shortcuts Editor (custom shortcut configuration UI)

### 3.2 Premium Plugin: AI Assistant Pro ($19-29) 📋 PLANNED

- [ ] 4 animals (Lion, Meerkat, Elephant, Cheetah) with unique personalities
- [ ] Advanced personality system (mood detection, context awareness)
- [ ] AI-powered suggestions (OpenAI/Claude API integration)
- [ ] Context-aware prompts (character development, plot suggestions, dialogue ideas)
- [ ] Flow state detection (don't interrupt when typing rapidly)
- [ ] Custom personality creation (user-defined assistant)
- [ ] License verification system (online activation, grace period)
- [ ] Trial mode (14 days, limited functionality)

### 3.3 Premium Plugin: Advanced Analytics ($14-19) 📋 PLANNED

- [ ] Timeline visualization (plot events on interactive timeline)
- [ ] Character mention tracking (heatmap, character presence per chapter)
- [ ] Pacing analysis (action/dialogue/description ratios)
- [ ] Reading level analysis (Flesch-Kincaid, Gunning Fog index)
- [ ] Sentiment analysis (emotional tone per chapter)
- [ ] Productivity trends (daily/weekly/monthly word counts, charts)
- [ ] License verification system (online activation)

### 3.4 Core Features 📋 PLANNED

- [ ] Character Bank (character cards with photos, traits, relationships)
- [ ] Character card UI (name, photo, age, traits, notes)
- [ ] Character relationships (graph view, connections)
- [ ] Location Bank (location cards with maps, descriptions, photos)
- [ ] Location card UI (name, map, description, photos)
- [ ] Notes System (yellow sticky notes attachable to chapters)
- [ ] Note UI (title, content, color, attachment)
- [ ] Writer's Calendar (goals, deadlines, writing schedule)
- [ ] Calendar UI (month view, day view, event creation)

---

## PHASE 4: Advanced Plugins 📋 PLANNED

**Status:** ⏳ Pending
**Target:** 0.5.0-rc
**Timeline:** Weeks 45-56 (2-3 months)

### 4.1 Developer Tools 📋 PLANNED

- [ ] Plugin Development Guide (comprehensive documentation)
- [ ] Step-by-step tutorial with working examples
- [ ] Plugin manifest reference (all fields explained)
- [ ] Lifecycle hooks documentation (on_init, on_activate, on_deactivate)
- [ ] Extension Points API reference
- [ ] Event Bus usage patterns
- [ ] Developer Mode in Kalahari (optional, hidden by default)
- [ ] Menu → Tools → Developer Tools (enable in Settings → Advanced)
- [ ] Plugin Creator Wizard (step-by-step GUI)
- [ ] Plugin Validator (manifest + structure checks)
- [ ] Plugin Packager (.kplugin ZIP creator)
- [ ] Live plugin reload (for development, auto-reload on file change)
- [ ] CLI Tools (optional, for automation)
- [ ] tools/create_plugin.py (template generator)
- [ ] tools/validate_plugin.py (validation script)
- [ ] tools/package_plugin.py (ZIP packager)
- [ ] Plugin Template Repository
- [ ] examples/hello_plugin/ (working minimal example)
- [ ] examples/advanced_plugin/ (all features demonstrated)
- [ ] plugin_manifest_schema.json (VSCode autocomplete)

### 4.2 Premium Plugin: Professional Export Suite ($24-34) 📋 PLANNED

- [ ] EPUB export (ebooklib - e-book publishing ready)
- [ ] Advanced PDF (custom formatting, TOC, index, headers/footers)
- [ ] Advanced DOCX (publisher-ready templates, styles)
- [ ] HTML export (website-ready, responsive)
- [ ] LaTeX export (academic writing, thesis templates)
- [ ] Export templates (Kindle Direct Publishing, IngramSpark)
- [ ] Batch export (multiple formats at once)
- [ ] License verification system

### 4.3 Premium Plugin: Research & Sources Pro ($19-24) 📋 PLANNED

- [ ] OCR for scanned documents (pytesseract/Tesseract integration)
- [ ] Web scraping assistant (article extraction)
- [ ] Citation management (Zotero integration)
- [ ] Advanced source organization (tagging, categorization)
- [ ] Automatic fact-checking hints (highlight potential errors)
- [ ] Bibliography generation (APA, MLA, Chicago formats)
- [ ] Research timeline (when facts were verified)
- [ ] License verification system

### 4.4 Premium Plugin: Collaboration Pack ($29-39) 📋 PLANNED

- [ ] Beta-reader mode (comments, suggestions, annotations)
- [ ] Editor mode (track changes, accept/reject)
- [ ] Version comparison (git-like diffs, side-by-side)
- [ ] Shared notes & annotations (multi-user)
- [ ] Real-time writing sprints (online sessions with friends)
- [ ] Export with comments (PDF with annotations)
- [ ] License verification system

---

## PHASE 5: Polish & Release 📋 PLANNED

**Status:** ⏳ Pending
**Target:** 1.0.0
**Timeline:** Weeks 57-68 (2-3 months)

### 5.1 Testing & Quality 📋 PLANNED

- [ ] Unit test coverage 70%+ (core modules)
- [ ] Unit test coverage for all plugins
- [ ] Integration tests (critical workflows: create document, edit, save, export)
- [ ] Beta testing program (recruit 20-30 real writers)
- [ ] Beta testing feedback collection (forms, interviews)
- [ ] Bug fixing marathon (prioritized backlog, 2-week sprint)
- [ ] Performance optimization (load time < 3s, memory usage < 200MB idle)
- [ ] Profiling (identify bottlenecks, optimize hot paths)
- [ ] Accessibility review (screen readers, keyboard navigation)
- [ ] Security audit (plugin sandboxing, file handling, input validation)

### 5.2 Documentation 📋 PLANNED

- [ ] User Manual (English) - Complete guide for writers
- [ ] User Manual (Polish) - Pełny przewodnik dla pisarzy
- [ ] Plugin API Documentation - For plugin developers
- [ ] Getting Started Guide - Quick start tutorials
- [ ] Video Tutorials - Screencasts for key features (YouTube)
- [ ] FAQ - Common questions and troubleshooting
- [ ] Release Notes - Feature list and known issues

### 5.3 Packaging & Distribution 📋 PLANNED

- [ ] Windows Installer (NSIS - silent install, file associations .klh)
- [ ] macOS Installer (DMG - drag-to-Applications, code signing)
- [ ] Linux Packages (AppImage universal)
- [ ] Linux Packages (DEB for Ubuntu/Debian - optional)
- [ ] Linux Packages (RPM for Fedora/RHEL - optional)
- [ ] Embedded Python bundling (all platforms, no external dependency)
- [ ] Code signing (Windows Authenticode certificate)
- [ ] Code signing (macOS Developer ID certificate)
- [ ] Notarization (macOS - required for Gatekeeper)
- [ ] Auto-update system (check for new versions on startup, optional)
- [ ] GitHub Release automation (CI/CD release workflow)

### 5.4 Launch 📋 PLANNED

- [ ] Website (kalahari.app) - Project homepage, download links
- [ ] GitHub Public Release - MIT License, source code
- [ ] Social Media announcements (Twitter, Reddit r/writing, WritingForums)
- [ ] Blog Posts - Launch announcement, feature highlights
- [ ] Community Forum - Support and discussion platform (Discourse or similar)
- [ ] Press Kit - Screenshots, logo SVG, description, contact info

---

## POST-1.0: Future Expansion 📋 PLANNED

### Phase 6: Cloud Sync (3-6 months post-1.0) 📋 PLANNED

- [ ] Cloud Sync Pro subscription system ($5-10/month)
- [ ] Dropbox integration (OAuth, file sync)
- [ ] Google Drive integration (OAuth, file sync)
- [ ] Own backend infrastructure (AWS/Azure/GCP - Phase 2)
- [ ] End-to-end encryption (AES-256, client-side encryption)
- [ ] Conflict resolution UI (smart merging, manual review)
- [ ] Mobile companion app (iOS - read-only, view documents)
- [ ] Mobile companion app (Android - read-only, view documents)
- [ ] Web access (basic editor in browser, read/write)
- [ ] Automatic cloud backups (unlimited storage for subscribers)
- [ ] Cross-device sessions (pick up where you left off)
- [ ] Premium support (email response within 24h)

### Phase 7: Collaboration (6-12 months post-1.0) 📋 PLANNED

- [ ] Serengeti - Collaborative writing tool (separate app)
- [ ] Real-time co-writing (Google Docs style, operational transforms)
- [ ] Role-based permissions (author, editor, beta-reader)
- [ ] Comment threads (discussions on specific passages)
- [ ] Task assignments (editorial workflow, assign chapters to editors)
- [ ] Version history (time machine for documents, git-like)
- [ ] Integration with Kalahari (seamless switching, import/export)

### Phase 8: Ecosystem (12-18 months post-1.0) 📋 PLANNED

- [ ] Plugin Marketplace - Own platform for plugin distribution
- [ ] Template Marketplace - Pre-made project templates (genres, formats)
- [ ] Okavango - Research & knowledge management (separate app)
- [ ] Kilimanjaro - Project management for writers (separate app)
- [ ] Victoria - Advanced cloud sync (own backend, web app)
- [ ] Zambezi - Publishing toolkit (formatting, distribution)
- [ ] Community content (user-contributed themes, templates)
- [ ] Publishing partnerships (IngramSpark, Kindle Direct, etc.)

---

## Task File Naming Convention

**For tasks from ROADMAP:**
```
NNNNN_P_Z_description.md

NNNNN = task number (00001-99999, sequential)
P = phase number (0-5)
Z = zagadnienie (main topic) number in phase (1-9)
description = short task description (snake_case)

Example:
00034_1_2_dynamic_menu_builder.md
  ↑     ↑ ↑  ↑
  task  │ │  description
        │ zagadnienie 1.2 (Command Registry)
        phase 1 (Core Editor)
```

**For custom tasks (fixes, tests, refactors not in ROADMAP):**
```
NNNNN_description.md

NNNNN = task number (sequential)
description = short task description (snake_case)

Example:
00043_fix_windows_crash.md
00044_refactor_settings_dialog.md
00045_add_integration_tests.md
```

---

**Last Updated:** 2025-11-13
**Next Review:** After completing Zagadnienie 1.2 (Command Registry Architecture)
