# OpenSpec #00045: Architecture Cleanup - Tasks

## Phase 1: Fix Immediate Crash
- [x] 1.1. Add `m_lazyLayoutManager.reset()` before `m_textBuffer` destruction in `fromKml()`
- [x] 1.2. Test Chapter 2 opens without crash
- [x] 1.3. Run full test suite (735 tests)

## Phase 2: Remove Legacy Components

### 2.1 Remove m_document (KmlDocument)
- [ ] 2.1.1. Remove `KmlDocument* m_document` member
- [ ] 2.1.2. Remove `setDocument(KmlDocument*)` method
- [ ] 2.1.3. Remove `document()` getter
- [ ] 2.1.4. Update `EditorPanel` to not use m_document

### 2.2 Remove m_layoutManager (LayoutManager)
- [ ] 2.2.1. Remove `std::unique_ptr<LayoutManager> m_layoutManager` member
- [ ] 2.2.2. Remove `layoutManager()` getters
- [ ] 2.2.3. Migrate `moveCursorUp()` to use LazyLayoutManager
- [ ] 2.2.4. Migrate `moveCursorDown()` to use LazyLayoutManager
- [ ] 2.2.5. Migrate `calculateCursorRect()` to use RenderEngine
- [ ] 2.2.6. Migrate `getMisspelledWordAt()` to use TextBuffer
- [ ] 2.2.7. Migrate `selectWordAtCursor()` to use TextBuffer
- [ ] 2.2.8. Remove `updateLayoutWidth()` (use LazyLayoutManager::setWidth)
- [ ] 2.2.9. Remove all 35 `m_layoutManager->` usages

### 2.3 Remove m_scrollManager (VirtualScrollManager)
- [ ] 2.3.1. Remove `std::unique_ptr<VirtualScrollManager> m_scrollManager` member
- [ ] 2.3.2. Remove `scrollManager()` getters
- [ ] 2.3.3. Use `ViewportManager` for all scroll operations
- [ ] 2.3.4. Remove all 23 `m_scrollManager->` usages

### 2.4 Remove m_pageLayoutManager (PageLayoutManager)
- [ ] 2.4.1. Remove `std::unique_ptr<PageLayoutManager> m_pageLayoutManager` member
- [ ] 2.4.2. Implement page mode using ViewportManager + RenderEngine
- [ ] 2.4.3. Remove all 11 `m_pageLayoutManager->` usages

### 2.5 Remove syncDocumentToNewArchitecture()
- [x] 2.5.1. Remove `syncDocumentToNewArchitecture()` method
- [x] 2.5.2. `fromKml()` is now the only way to load content (setDocument() syncs to TextBuffer for test compatibility)

## Phase 3: Cleanup Methods

### 3.1 Remove NewArch Suffix
- [x] 3.1.1. Rename `positionFromPointNewArch()` to `positionFromPoint()`
- [x] 3.1.2. Rename `paintPageModeNewArch()` to `paintPageMode()`
- [x] 3.1.3. Rename `getFocusedRangeNewArch()` to `getFocusedRange()`
- [x] 3.1.4. Rename `paintFocusOverlayNewArch()` to `paintFocusOverlay()`
- [x] 3.1.5. Rename `getWordCountNewArch()` to `getWordCount()`

### 3.2 Remove Old Method Versions
- [x] 3.2.1. Remove old `positionFromPoint()` (now uses new version)
- [x] 3.2.2. Remove old `paintPageMode()` (now uses new version)
- [x] 3.2.3. Remove old `getFocusedRange()` (now uses new version)
- [x] 3.2.4. Remove old `paintFocusOverlay()` (now uses new version)
- [x] 3.2.5. Remove old `getWordCount()` (now uses new version)

### 3.3 Header Cleanup
- [x] 3.3.1. Remove legacy includes from book_editor.h
- [x] 3.3.2. Remove legacy method declarations
- [ ] 3.3.3. Update class documentation

## Phase 4: Documentation

### 4.1 Project Documentation
- [ ] 4.1.1. Create `project_docs/20_new_editor_architecture.md`
- [ ] 4.1.2. Document TextBuffer + Fenwick Tree
- [ ] 4.1.3. Document FormatLayer + Interval Tree
- [ ] 4.1.4. Document LazyLayoutManager
- [ ] 4.1.5. Document ViewportManager
- [ ] 4.1.6. Document RenderEngine
- [ ] 4.1.7. Document component interaction diagram

### 4.2 Changelog & Roadmap
- [ ] 4.2.1. Update CHANGELOG.md
- [ ] 4.2.2. Update ROADMAP.md

## Phase 5: Testing

- [x] 5.1. Build without warnings
- [x] 5.2. All 735 unit tests pass
- [x] 5.3. Manual: Open ExampleNovel Chapter 1 - OK
- [x] 5.4. Manual: Open ExampleNovel Chapter 2 (77KB single paragraph) - OK
- [x] 5.5. Manual: Text selection - OK
- [ ] ~~5.6. Manual: Edit text~~ - **REGRESJA: kursor niewidoczny, edycja nie działa, program się zawiesza**
- [ ] ~~5.7. Manual: Cursor navigation~~ - **REGRESJA: nie działa**
- [ ] 5.8. Manual: Spell check - blocked by 5.6
- [ ] 5.9. Manual: Find/Replace - blocked by 5.6
- [ ] 5.10. Manual: Undo/Redo - blocked by 5.6

## REGRESJE DO NAPRAWY (osobny task)

1. **Kursor niewidoczny** - nie można umieścić kursora w tekście
2. **Edycja tekstu nie działa** - program się zawiesza przy próbie edycji
3. **Nawigacja kursorem nie działa** - strzałki nie działają

## Summary

| Phase | Tasks | Status |
|-------|-------|--------|
| 1. Fix Crash | 3/3 | DONE |
| 2. Remove Legacy | 2/21 | 2.5 done, 2.1-2.4 pending |
| 3. Cleanup Methods | 12/13 | 92% done |
| 4. Documentation | 0/9 | Pending |
| 5. Testing | 2/9 | Build + unit tests done |

**Progress: 19/55 tasks complete (35%)**
