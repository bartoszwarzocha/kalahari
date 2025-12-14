# OpenSpec #00035: KChapter Format - Tasks

## Phase 1: Core Implementation

### Task 1.1: ChapterDocument Class
- [x] Create `include/kalahari/core/chapter_document.h`
- [x] Create `src/core/chapter_document.cpp`
- [x] Implement JSON serialization (toJson/fromJson)
- [x] Implement file I/O (load/save)
- [x] Implement statistics calculation (word count, etc.)
- [x] Add to CMakeLists.txt

### Task 1.2: ProjectManager Integration
- [x] Modify `loadChapterContent()` to use .kchapter
- [x] Modify `saveChapterContent()` to use .kchapter
- [x] Update file path generation (.kchapter extension)
- [ ] Add `getChapterDocument()` method (deferred - not needed for MVP)

### Task 1.3: Migration Logic
- [x] Implement RTF->KChapter migration (inline in loadChapterContent)
- [x] Add automatic migration in `loadChapterContent()`
- [x] Keep .rtf.bak backup
- [x] Log migration status

### Task 1.4: EditorPanel Integration
- [x] EditorPanel works without changes (uses HTML strings via ProjectManager)
- [x] Pass statistics to PropertiesPanel

### Task 1.5: Testing
- [x] Build passes
- [x] Manual test: create new chapter -> .kchapter created
- [x] Manual test: open RTF project -> migrated to .kchapter
- [x] Manual test: edit, save, reopen -> content preserved
- [ ] Unit tests for ChapterDocument (deferred to Phase 2)

---

## Status Feature (Extended Scope)

### Navigator Status Display
- [x] Show chapter status in Navigator: `Chapter One [Draft]`
- [x] Final status = no suffix (clean display)
- [x] Helper function `getDisplayTitle()` in navigator_panel.cpp

### Statistics by Status
- [x] Project statistics panel: count files by status
- [x] PropertiesPanel: Draft: X, Revision: Y, Final: Z
- [x] `ProjectManager::getStatusStatistics()` method

### Export Warnings
- [x] Warn on export if project contains Draft/Revision files
- [x] Groups files by status in warning dialog
- [x] Option to export anyway or cancel
- [x] `ProjectManager::getIncompleteElements()` method

### Status Persistence
- [x] Status saved in manifest.json (`loadStructureFromManifest`)
- [x] Status loaded from manifest on project open
- [x] Status changes remembered after restart

### Navigator UX
- [x] Single-click -> shows Properties panel
- [x] Root/Section click -> shows Section/Project properties (not collapse)
- [x] New signals: `requestSectionProperties`, `requestPartProperties`

### Hierarchical Statistics
- [x] PropertiesPanel: Section page with aggregate stats
- [x] PropertiesPanel: Part page with aggregate stats
- [x] Shows chapter count, word count, status breakdown per container

---

## Bug Fixes (Session 2025-12-14)

### Status & Notes Persistence
- [x] Status persistence - now saved to .kchapter metadata
- [x] Notes persistence - now saved to .kchapter metadata
- [x] Clean data architecture (manifest stores structure, .kchapter stores chapter data)

### Navigator Improvements
- [x] Keyboard navigation (arrow keys to navigate, Enter to open)
- [x] Single-click shows Properties panel (not double-click)
- [x] Navigator refresh when status changes in Properties panel

### PropertiesPanel Fixes
- [x] Status ComboBox options fixed (Draft/Revision/Final - proper display)
- [x] Section/Part hierarchical statistics display
- [x] Notes save on focus lost (not every keystroke - prevents lag)

---

## Progress Log

### 2025-12-14 Session 1 (Core Implementation)
- Created OpenSpec #00035 proposal
- Created feature branch: `feature/kchapter-format`
- Task 1.1: ChapterDocument class - COMPLETE
  - Full JSON serialization with kalahari header, content, statistics, metadata, annotations
  - File I/O with UTF-8 encoding
  - Statistics: wordCount, characterCount, paragraphCount (auto-calculated)
  - HTML to plainText conversion using QTextDocument
- Task 1.2: ProjectManager Integration - COMPLETE
  - loadChapterContent() now checks for .kchapter first, falls back to RTF
  - saveChapterContent() always saves to .kchapter format
  - Automatic extension update in BookElement
- Task 1.3: Migration Logic - COMPLETE
  - RTF files auto-migrated to .kchapter on first load
  - Original RTF backed up as .rtf.bak
  - Element file path updated in memory
- Build: PASS | Tests: 73/73 PASS (571 assertions)

### 2025-12-14 Session 2 (Status Feature + Bug Fixes)
- Extended scope: Status feature implementation
- Navigator: Status suffix display [Draft]/[Revision], Final = clean
- Export: Warning dialog for non-final files
- PropertiesPanel: Status statistics per section/part/project
- Bug fixes:
  - Status now persisted to .kchapter (was lost on restart)
  - Notes now persisted to .kchapter (was lost on restart)
  - Navigator keyboard navigation (arrow keys + Enter)
  - Navigator single-click shows Properties (was double-click)
  - Section/Part hierarchical statistics in Properties
  - Status ComboBox options fixed (proper display text)
  - Navigator refresh when status changes in Properties
  - Notes save on focus lost (prevents lag from keystroke saves)
- Verified clean architecture: manifest.json = structure, .kchapter = chapter data

---

## Summary

**Status: DEPLOYED**

All acceptance criteria met:
- [x] New chapters created as .kchapter files
- [x] Existing RTF chapters migrated automatically on open
- [x] Content preserved during migration (HTML roundtrip)
- [x] Statistics (word count, etc.) calculated and stored
- [x] Metadata (status, notes) saved per-chapter
- [x] Plaintext backup included in .kchapter
- [x] Build passes, tests pass
- [x] Manual test: edit chapter, save, reopen - content preserved

Extended scope delivered:
- [x] Status feature (Navigator display, export warnings, statistics)
- [x] Hierarchical statistics (Section/Part aggregation)
- [x] Navigator UX improvements (keyboard nav, single-click)
- [x] Bug fixes (persistence, refresh, ComboBox)

Deferred to Phase 2:
- [ ] Unit tests for ChapterDocument class
