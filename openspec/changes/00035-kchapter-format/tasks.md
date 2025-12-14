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
- [ ] Add `getChapterDocument()` method (deferred - not needed for MVP)
- [x] Modify `loadChapterContent()` to use .kchapter
- [x] Modify `saveChapterContent()` to use .kchapter
- [x] Update file path generation (.kchapter extension)

### Task 1.3: Migration Logic
- [x] Implement RTF→KChapter migration (inline in loadChapterContent)
- [x] Add automatic migration in `loadChapterContent()`
- [x] Keep .rtf.bak backup
- [x] Log migration status

### Task 1.4: EditorPanel Integration
- [x] EditorPanel works without changes (uses HTML strings via ProjectManager)
- [ ] Pass statistics to PropertiesPanel (Phase 2)

### Task 1.5: Testing
- [x] Build passes
- [ ] Unit tests for ChapterDocument (Phase 2)
- [ ] Manual test: create new chapter → .kchapter created
- [ ] Manual test: open RTF project → migrated to .kchapter
- [ ] Manual test: edit, save, reopen → content preserved

## Progress Log

### 2025-12-14
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

## Next Steps (Phase 2)

- [ ] Add unit tests for ChapterDocument
- [ ] Pass word count from ChapterDocument to PropertiesPanel
- [ ] Create new chapters directly in .kchapter format
- [ ] Update example project to use .kchapter
