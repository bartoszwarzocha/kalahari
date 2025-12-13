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
- [ ] Implement `MainWindow::openStandaloneFile(path)`
  - [ ] Set WorkMode::StandaloneMode
  - [ ] Load file into editor
  - [ ] Add to "Other Files" in Navigator
- [ ] Create info bar for standalone mode
  - [ ] "Add to project for full features" message
  - [ ] "Add to Project" button
- [ ] Implement "Add to Project" action
  - [ ] Dialog to select target location
  - [ ] Copy/move file to project folder
  - [ ] Update manifest and refresh Navigator
- [ ] Handle file associations (.rtf, .kmap, .ktl)

## Phase G: Project Properties
- [ ] Create `ProjectPropertiesDialog` (QDialog)
  - [ ] Title, Author, Language, Genre inputs
  - [ ] Statistics display (word count, chapters)
  - [ ] Created/Modified dates (read-only)
- [ ] Connect to "File > Project Properties..." menu
- [ ] Save changes to manifest on OK

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

**Current Phase:** E (Chapter Editing) - Core implementation DONE
**Next Phase:** RTF conversion (future) or Phase F (Standalone Mode)

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
