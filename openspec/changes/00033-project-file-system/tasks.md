# Tasks for #00033: Project File System - Solution-like Architecture

## Phase A: Analysis & Design ✅
- [x] Review existing implementation (DocumentArchive, Document, Book, BookElement)
- [x] Analyze architecture options (ZIP vs folder vs hybrid)
- [x] Review project_docs specifications
- [x] **Decision: Solution-like folder structure**
- [x] Document full specification in proposal.md

## Phase B: Core Infrastructure ✅
- [x] Create `ProjectManager` class (project folder operations)
  - [x] `createProject(path, name)` - create folder structure
  - [x] `openProject(klhPath)` - load manifest, validate structure
  - [x] `closeProject()` - cleanup, save pending changes
  - [x] `saveManifest()` - write .klh file
  - [x] `getProjectPath()`, `getContentPath()`, etc.
- [x] Define `WorkMode` enum (NoDocument, ProjectMode, StandaloneMode)
- [x] Update Document/Book classes for folder-based structure
  - [x] BookElement: dirty tracking (m_isDirty) and content cache (m_content)
  - [x] ProjectManager: loadStructureFromManifest(), saveStructureToManifest()
  - [x] ProjectManager: loadChapterContent(), saveChapterContent()
  - [x] ProjectManager: findElement(), findPart(), getDirtyElements(), saveAllDirty()
- [x] Create example project folder manually for development
  - [x] examples/ExampleNovel/ with .klh manifest and RTF content files

## Phase C: Project Creation ✅
- [x] Create `TemplateRegistry` class for project/file templates
  - [x] `TemplateInfo` struct with metadata for display
  - [x] Singleton pattern (like ArtProvider)
  - [x] Plugin-friendly registration API
  - [x] 6 built-in project templates (Novel, Short Story, Non-fiction, etc.)
  - [x] 6 built-in file templates (Chapter, Mind Map, Timeline, etc.)
- [x] Create `NewItemDialog` (QDialog) - dual-purpose dialog
  - [x] Project name / file name input
  - [x] Location selector (folder picker) - project mode only
  - [x] Template selector using TemplateRegistry with icon grid
  - [x] Author/Language defaults from settings
  - [x] Left panel: description with 64x64 icon and features
  - [x] Right panel: template grid with 48x48 icons
  - [x] Theme-aware icon refresh
- [ ] Implement folder structure creation
  - [ ] Create project folder with subfolders
  - [ ] Create initial .klh manifest
  - [ ] Create first chapter (optional)
- [ ] Add "File > New > Project..." menu item
- [ ] Register `file.new.project` command

## Phase D: Project Loading - IN PROGRESS
- [ ] Implement `ProjectManager::openProject()`
  - [ ] Read and parse .klh manifest
  - [ ] Validate folder structure
  - [ ] Build Document/Book objects from manifest
  - [ ] Handle missing files gracefully
- [x] Update `NavigatorPanel` for new structure
  - [x] Changed signal: `chapterDoubleClicked` -> `elementSelected(elementId, elementTitle)`
  - [x] Store element ID and type in tree items using Qt::UserRole
  - [x] Add icons to tree items using ArtProvider (folder/chapter)
  - [x] Connect to ArtProvider::resourcesChanged() for theme refresh
  - [x] Added refreshIcons() private method
  - [x] Updated MainWindow slot to new signal signature
  - [ ] Display project tree from manifest (needs ProjectManager integration)
  - [ ] Show Front Matter / Body / Back Matter sections (needs ProjectManager integration)
  - [ ] Add Mind Maps section (future)
  - [ ] Add Timelines section (future)
  - [ ] Add "Other Files" section for standalone files (future)
- [ ] Update "File > Open > Project..." to use new loader
- [ ] Handle double-click on .klh file (command line argument)

## Phase E: Chapter Editing - IN PROGRESS

> **Architecture Decision (ADR-005):** Use QTextEdit as interim editor.
> ProjectManager handles RTF I/O, EditorPanel uses QTextEdit for display.
> This does NOT conflict with future custom editor - file format and editor
> format are separate concerns. See `project_docs/15_text_editor_architecture.md`.

- [x] Upgrade EditorPanel from QPlainTextEdit to QTextEdit
  - [x] Changed header from QPlainTextEdit to QTextEdit
  - [x] Updated implementation with QTextEdit
  - [x] Added setContent()/getContent() methods (plain text for MVP)
  - [x] Fixed word wrap mode enum (QTextEdit::WidgetWidth/NoWrap)
- [ ] Implement RTF ↔ HTML conversion in ProjectManager (future phase)
  - [ ] `loadChapterContent()` returns HTML for QTextEdit
  - [ ] `saveChapterContent()` converts HTML back to RTF
- [x] Implement chapter loading integration
  - [x] `ProjectManager::loadChapterContent()` already implemented in Phase B
  - [x] Lazy loading (load on demand) via Navigator click
  - [x] Track loaded/unloaded state in BookElement
- [x] Implement chapter saving integration
  - [x] `ProjectManager::saveChapterContent()` already implemented in Phase B
  - [x] Save only changed files (incremental) via m_dirtyChapters tracking
  - [ ] Update word count in manifest (future)
- [x] Connect Navigator double-click to chapter opening
  - [x] Save current chapter content cache before switching
  - [x] Load new chapter via ProjectManager
  - [x] Track m_currentElementId for save operations
- [x] Connect Editor changes to dirty state tracking
  - [x] Added m_dirtyChapters QMap<QString, bool> per elementId
  - [x] Connect QTextEdit::textChanged to update dirty state
  - [x] Update tab title with asterisk (*) when dirty
- [x] Implement "Save All" command
  - [x] Added onSaveAll() slot in MainWindow
  - [x] Iterates all open tabs, updates content cache
  - [x] Calls ProjectManager::saveAllDirty()
  - [x] Clears dirty flags and removes asterisks from tabs

## Phase F: Standalone Mode
- [x] Implement `MainWindow::openStandaloneFile(path)`
  - [ ] Set WorkMode::StandaloneMode (pending - currently loads file directly)
  - [x] Load file into editor
  - [x] Add to "Other Files" in Navigator
- [x] Create info bar for standalone mode (`StandaloneInfoBar` widget)
  - [x] Info icon + message label
  - [x] "Add to Project" button with signal
  - [x] Close/dismiss button with signal
  - [x] Theme-aware styling (light/dark)
  - [x] Connect to ThemeManager for dynamic theme changes
- [x] Implement "Add to Project" action
  - [x] Create `AddToProjectDialog` (QDialog) for target selection
    - [x] Target section combo (frontmatter, body, backmatter, mindmaps, timelines)
    - [x] Target part combo (enabled only for body section)
    - [x] Display title input
    - [x] Copy/Move file radio buttons
    - [x] Validates input before enabling Add button
    - [x] Uses QFormLayout for clean form structure
    - [x] Integrates with ProjectManager to get parts list
  - [ ] Copy/move file to project folder (dialog UI ready, action pending)
  - [ ] Update manifest and refresh Navigator (post-MVP)
- [x] Handle file associations (.rtf, .kmap, .ktl) - icons mapped in Navigator

## Phase G: Properties Panel ✅
- [x] Implement contextual `PropertiesPanel` (QStackedWidget)
  - [x] Three views: NoProject, Project, Chapter
  - [x] QStackedWidget with page switching
  - [x] QScrollArea for responsive content
- [x] Project Properties view
  - [x] Title (editable QLineEdit)
  - [x] Author (editable QLineEdit)
  - [x] Language (QComboBox with 10 languages)
  - [x] Genre (editable QLineEdit)
  - [x] Statistics section (QGroupBox):
    - [x] Total Chapters (read-only)
    - [x] Total Words (read-only)
    - [x] Created date (read-only)
    - [x] Modified date (read-only)
- [x] Chapter Properties view
  - [x] Title (editable QLineEdit)
  - [x] Word Count (read-only QLabel)
  - [x] Status (QComboBox: Draft, In Progress, Complete, Final)
  - [x] Notes (QTextEdit with min/max height)
- [x] No Project view
  - [x] Placeholder message "Open a project to see properties"
- [x] Connect to ProjectManager signals
  - [x] projectOpened -> showProjectProperties()
  - [x] projectClosed -> showNoProject()
- [x] Auto-save changes on edit (no Save button needed)
- [x] m_isUpdating flag to prevent recursive updates
- [x] formatDate() helper for time_point display
- [x] All UI strings use tr() for i18n

## Phase H: Export/Import Archive
- [ ] Implement `ProjectManager::exportArchive(outputPath)`
  - [ ] Create .klh.zip containing entire project
  - [ ] Exclude .kalahari/ cache folder
  - [ ] Progress dialog for large projects
- [ ] Implement `ProjectManager::importArchive(archivePath)`
  - [ ] Extract .klh.zip to selected folder
  - [ ] Open extracted project
- [ ] Add "Export > Project Archive..." menu item
- [ ] Add "Import > Project Archive..." menu item

## Phase I: Migration & Cleanup
- [ ] Create migration for old .klh ZIP format
  - [ ] Detect ZIP vs folder on open
  - [ ] Extract ZIP to folder with user confirmation
- [ ] Remove/deprecate old DocumentArchive ZIP methods
- [ ] Update file filters in open/save dialogs
- [ ] Delete old test.klh (ZIP format)

## Phase J: Polish & Testing
- [ ] Create comprehensive example project
  - [ ] 2 front matter items
  - [ ] 3 parts with 3 chapters each
  - [ ] 2 back matter items
  - [ ] Sample mind map, timeline files
- [ ] Unit tests for ProjectManager
- [ ] Integration tests for save/load cycle
- [ ] Performance test: 100 chapters < 2 seconds
- [ ] Manual testing:
  - [ ] New project creation
  - [ ] Open existing project
  - [ ] Edit and save chapter
  - [ ] Add standalone file to project
  - [ ] Project properties edit
  - [ ] Export/Import archive
  - [ ] File association (double-click .klh)

## Documentation
- [ ] Update CHANGELOG.md
- [ ] Update ROADMAP.md section 1.2
- [ ] Document .klh format in project_docs/04_data_formats.md
- [ ] User guide for project management

---

## Status Summary

**Current Phase:** G (Properties Panel) - COMPLETE
**Next Phase:** Phase H (Export/Import Archive) or integration with Navigator

**Architecture Decision:** Solution-like folder structure
- `.klh` = JSON manifest file (like .sln)
- `content/` = RTF chapter files (human-readable)
- Incremental save (only changed files)
- Standalone mode for loose files
- "Other Files" group in Navigator

**Implementation Order:**
1. Phase B - ProjectManager (foundation)
2. Phase D - Loading (test with manual project)
3. Phase E - Chapter editing
4. Phase C - Project creation
5. Phase F - Standalone mode
6. Phase G - Properties dialog
7. Phase H - Export/Import
8. Phase I - Migration
9. Phase J - Testing

**Estimated:** ~8-10 sessions total
