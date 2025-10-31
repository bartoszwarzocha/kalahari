# Phase 1: Core Editor - Task Breakdown

**Phase:** Phase 1 (Weeks 9-20)
**Duration:** 12 weeks (3 months)
**Target Version:** 0.2.0-alpha
**Goal:** Functional rich text editor with project management

---

## Task Overview (15 Tasks)

### Critical Path (Foundation → Core → Features)
```
Week 9-10:  Task #1 → Task #2 → Task #3 (Foundation: UI + Editor + Navigator)
Week 11-12: Task #4 → Task #5 (Core: CRUD + Persistence)
Week 13-15: Task #6 → Task #7 → Task #8 (Features: Formatting + Undo + Find)
Week 16-18: Task #9 → Task #10 (Data Safety: Auto-save + Backup)
Week 19-20: Task #11-15 (Polish: UX + Productivity)
```

---

## Task List with Dependencies

### Week 9: Foundation (UI Infrastructure)

**Task #00013: wxAUI Docking System + Panel Management**
- **Priority:** P0 (Foundation)
- **Dependencies:** None (Phase 0 complete)
- **Duration:** 1 week
- **Components:**
  - wxAUI docking manager initialization
  - 6 core panels: Navigator, Editor, Properties, Statistics, Search, Assistant
  - Panel visibility controls (View menu + toolbar buttons)
  - Default layout configuration
  - Perspective system (save/load layouts)
- **Testing:** Panel docking, undocking, resizing, layout persistence
- **Deliverable:** Multi-panel workspace with dockable panels

---

### Week 10: Core Editor

**Task #00014: wxRichTextCtrl Integration + Basic Formatting**
- **Priority:** P0 (Core)
- **Dependencies:** Task #00013 (wxAUI panels ready)
- **Duration:** 1 week
- **Components:**
  - wxRichTextCtrl integration in Editor panel
  - Basic text formatting: bold, italic, underline
  - Font selection (family, size, color)
  - Text alignment (left, center, right, justify)
  - Event handling (text changes, cursor movement)
- **Testing:** Text input, formatting application, event propagation
- **Deliverable:** Working rich text editor with basic formatting

---

### Week 11: Document Structure

**Task #00015: Project Navigator Panel + wxTreeCtrl**
- **Priority:** P0 (Core)
- **Dependencies:** Task #00013 (wxAUI panels), Document Model (Phase 0)
- **Duration:** 1 week
- **Components:**
  - wxTreeCtrl integration in Navigator panel
  - Book → Parts → Chapters hierarchy visualization
  - Tree node icons (book, part, chapter)
  - Node selection synchronization with editor
  - Context menu (right-click operations)
- **Testing:** Tree population, node selection, context menu
- **Deliverable:** Hierarchical document structure view

---

### Week 12: CRUD Operations

**Task #00016: Chapter Management CRUD Operations**
- **Priority:** P0 (Core)
- **Dependencies:** Task #00015 (Navigator panel ready)
- **Duration:** 1 week
- **Components:**
  - Add chapter (toolbar button, menu, context menu)
  - Delete chapter (with confirmation dialog)
  - Rename chapter (inline edit or dialog)
  - Move chapter (drag & drop or up/down buttons)
  - Reorder chapters within Part
  - Part management (add, delete, rename, move)
- **Testing:** All CRUD operations, data consistency, undo/redo
- **Deliverable:** Complete chapter and part management

---

### Week 13: Persistence Enhancement

**Task #00017: RTF Content Save/Load Integration**
- **Priority:** P0 (Core)
- **Dependencies:** Task #00014 (wxRichTextCtrl), DocumentArchive (Phase 0)
- **Duration:** 1 week
- **Components:**
  - RTF serialization from wxRichTextCtrl
  - RTF storage in .klh ZIP archive (chapters/*.rtf)
  - RTF loading into wxRichTextCtrl
  - DocumentArchive enhancement (Phase 0 MVP → full implementation)
  - Lazy loading strategy (load RTF on chapter open)
- **Testing:** Save/load round-trip, large documents, RTF format preservation
- **Deliverable:** Complete .klh file format with RTF content

---

### Week 14: Advanced Formatting

**Task #00018: Text Styles + Paragraph Formatting**
- **Priority:** P1 (Feature)
- **Dependencies:** Task #00014 (basic formatting working)
- **Duration:** 1 week
- **Components:**
  - Text styles: H1-H6, body text, quotes, code
  - Style selector (toolbar dropdown)
  - Paragraph spacing (before, after)
  - Line spacing (single, 1.5, double, custom)
  - Indentation (left, right, first line)
  - Bullet lists and numbered lists
- **Testing:** Style application, paragraph formatting, list behavior
- **Deliverable:** Professional text styling and formatting

---

### Week 15: Editor Reliability

**Task #00019: Undo/Redo Command Pattern**
- **Priority:** P0 (Core)
- **Dependencies:** Task #00014 (editor working), Task #00016 (CRUD operations)
- **Duration:** 1 week
- **Components:**
  - Command interface (ICommand with execute/undo)
  - TextEditCommand (insert, delete, format text)
  - StructureEditCommand (add/delete/move chapters)
  - Command history (100 commands default, configurable)
  - Undo/Redo UI (toolbar buttons, menu, Ctrl+Z/Ctrl+Y)
  - Mergeable consecutive edits (typing optimization)
- **Testing:** Undo/redo sequences, command merging, memory management
- **Deliverable:** Full undo/redo system for editor and structure

---

### Week 16: Editor Utility

**Task #00020: Find & Replace**
- **Priority:** P1 (Feature)
- **Dependencies:** Task #00014 (editor working)
- **Duration:** 1 week
- **Components:**
  - Find dialog (modal or dockable panel)
  - Search options: case-sensitive, whole word, regex
  - Find Next/Previous navigation
  - Replace single occurrence
  - Replace All with preview
  - Search scope: current chapter, all chapters, selection
- **Testing:** Search patterns, replace operations, edge cases
- **Deliverable:** Full-featured find & replace

---

### Week 17: Data Safety (Auto-Save)

**Task #00021: Auto-Save System**
- **Priority:** P0 (Core)
- **Dependencies:** Task #00017 (RTF save/load), Threading (Phase 0)
- **Duration:** 1 week
- **Components:**
  - Auto-save timer (configurable interval: 1-10 minutes)
  - Background save thread (non-blocking UI)
  - Dirty flag tracking (document modified since last save)
  - Save indicator (status bar, subtle animation)
  - Settings integration (enable/disable, interval)
  - Recovery on crash (auto-save location: ~/.kalahari/autosave/)
- **Testing:** Timer behavior, thread safety, crash recovery
- **Deliverable:** Reliable auto-save with crash recovery

---

### Week 18: Data Safety (Backup)

**Task #00022: Backup System**
- **Priority:** P1 (Feature)
- **Dependencies:** Task #00017 (save/load), Task #00021 (auto-save)
- **Duration:** 1 week
- **Components:**
  - Backup strategy: rolling snapshots (hourly, daily, weekly)
  - Backup location: ~/.kalahari/backups/<document_id>/
  - Retention policy (configurable: keep last N backups)
  - Backup restore UI (File → Restore from Backup...)
  - Timestamp-based backup naming (YYYY-MM-DD_HH-MM-SS.klh)
  - Background backup thread
- **Testing:** Backup creation, retention, restore, disk space
- **Deliverable:** Automatic backup system with restore capability

---

### Week 19: UX Polish

**Task #00023: Focus Modes + Perspectives**
- **Priority:** P2 (Polish)
- **Dependencies:** Task #00013 (wxAUI panels)
- **Duration:** 1 week
- **Components:**
  - 3 focus modes:
    - Normal: All panels visible
    - Focused: Editor + Navigator only
    - Distraction-Free: Editor only, fullscreen
  - Mode switching (View menu, keyboard shortcuts: F11, F12)
  - Perspective save/load (JSON persistence)
  - Default perspectives: "Default", "Writing", "Editing", "Research"
  - Perspective management UI (save, load, delete, rename)
- **Testing:** Mode transitions, perspective persistence, fullscreen behavior
- **Deliverable:** Customizable workspace layouts

---

### Week 19: Productivity (Keyboard Shortcuts)

**Task #00024: Keyboard Shortcuts System**
- **Priority:** P1 (Feature)
- **Dependencies:** Task #00013-#00020 (all core features ready)
- **Duration:** 0.5 weeks (parallel with Task #00023)
- **Components:**
  - Keyboard shortcut registry (global)
  - 80+ default shortcuts (Ctrl+B, Ctrl+I, Ctrl+S, etc.)
  - Shortcut customization UI (Edit → Keyboard Shortcuts...)
  - Conflict detection (warn on duplicate bindings)
  - Settings integration (shortcuts.json persistence)
  - Platform-specific defaults (Cmd on macOS, Ctrl on Windows/Linux)
- **Testing:** Shortcut execution, conflict resolution, persistence
- **Deliverable:** Comprehensive keyboard shortcuts system

---

### Week 20: Writing Feedback

**Task #00025: Word Count Live + Statistics Panel**
- **Priority:** P1 (Feature)
- **Dependencies:** Task #00014 (editor), Task #00013 (panels)
- **Duration:** 0.5 weeks
- **Components:**
  - Live word count (status bar, updates on text change)
  - Character count (with/without spaces)
  - Reading time estimation (configurable WPM: 200-300)
  - Session statistics (words written today, this week, this month)
  - Writing streak tracking (consecutive days)
  - Statistics panel (dockable): charts, progress bars
  - Daily/weekly/monthly goals (configurable)
- **Testing:** Word count accuracy, statistics calculation, performance
- **Deliverable:** Comprehensive writing statistics

---

### Week 20: UI Polish

**Task #00026: Status Bar + Info Bar**
- **Priority:** P2 (Polish)
- **Dependencies:** Task #00013 (UI foundation)
- **Duration:** 0.5 weeks
- **Components:**
  - Status bar (8 segments):
    - Word count / Character count
    - Cursor position (Line:Column)
    - Current time
    - Document status (saved/modified)
    - Language/encoding
    - Zoom level
    - Focus mode indicator
    - Writing streak
  - Info bar (5 message types):
    - Success (green): "Document saved successfully"
    - Warning (yellow): "Auto-save enabled"
    - Error (red): "Failed to save document"
    - Info (blue): "Tip: Press F11 for distraction-free mode"
    - Hint (gray): "Did you know? Ctrl+Shift+F searches all chapters"
  - Auto-hide after timeout (5 seconds, configurable)
- **Testing:** Message display, timeout behavior, multi-message queue
- **Deliverable:** Professional status and info feedback

---

### Week 20: Writing Assistance

**Task #00027: Spell Checking Integration**
- **Priority:** P2 (Polish)
- **Dependencies:** Task #00014 (editor working)
- **Duration:** 0.5 weeks
- **Components:**
  - Spell checker library integration (hunspell or wxWidgets built-in)
  - Red wavy underline for misspelled words
  - Right-click context menu: suggestions, add to dictionary
  - Language selection (Settings → Spell Check → Language)
  - Custom dictionary support (~/.kalahari/dictionaries/)
  - Enable/disable per document
- **Testing:** Spell checking accuracy, dictionary management, performance
- **Deliverable:** Basic spell checking support

---

## Summary Table

| # | Task | Priority | Duration | Week | Dependencies |
|---|------|----------|----------|------|--------------|
| 13 | wxAUI Docking System | P0 | 1 week | 9 | Phase 0 |
| 14 | wxRichTextCtrl + Formatting | P0 | 1 week | 10 | #13 |
| 15 | Project Navigator | P0 | 1 week | 11 | #13 |
| 16 | Chapter Management CRUD | P0 | 1 week | 12 | #15 |
| 17 | RTF Save/Load | P0 | 1 week | 13 | #14 |
| 18 | Text Styles + Paragraph | P1 | 1 week | 14 | #14 |
| 19 | Undo/Redo | P0 | 1 week | 15 | #14, #16 |
| 20 | Find & Replace | P1 | 1 week | 16 | #14 |
| 21 | Auto-Save System | P0 | 1 week | 17 | #17 |
| 22 | Backup System | P1 | 1 week | 18 | #17, #21 |
| 23 | Focus Modes + Perspectives | P2 | 1 week | 19 | #13 |
| 24 | Keyboard Shortcuts | P1 | 0.5 week | 19 | #13-20 |
| 25 | Word Count + Statistics | P1 | 0.5 week | 20 | #14, #13 |
| 26 | Status Bar + Info Bar | P2 | 0.5 week | 20 | #13 |
| 27 | Spell Checking | P2 | 0.5 week | 20 | #14 |

**Total:** 15 tasks, 12 weeks, P0=7, P1=5, P2=3

---

## Priority Definitions

- **P0 (Critical):** Core functionality, blocking other features
- **P1 (High):** Important features, significantly improve UX
- **P2 (Medium):** Polish features, nice-to-have

---

## Risk Assessment

### High-Risk Tasks (Technical Complexity)
1. **Task #14 (wxRichTextCtrl):** Complex API, steep learning curve
   - Mitigation: Prototype first, study wxWidgets samples
2. **Task #19 (Undo/Redo):** Command pattern complexity, memory management
   - Mitigation: Design pattern review, Phase 0 architectural decisions
3. **Task #21 (Auto-Save):** Threading, data consistency
   - Mitigation: Phase 0 threading infrastructure, mutex protection

### Medium-Risk Tasks (Integration)
1. **Task #17 (RTF Save/Load):** File format complexity
   - Mitigation: Use wxRichTextCtrl native RTF support
2. **Task #22 (Backup System):** Disk space, performance
   - Mitigation: Configurable retention, background thread

### Low-Risk Tasks (Standard Implementation)
- Tasks #13, #15, #16, #18, #20, #23-27 (well-documented wxWidgets features)

---

## Success Criteria

### Functional Requirements (Must Have)
- ✅ Users can write and format text in rich text editor
- ✅ Users can organize content in chapters and parts
- ✅ Users can save/load projects (.klh files)
- ✅ Auto-save protects work automatically
- ✅ Undo/redo works reliably

### Non-Functional Requirements
- **Performance:** Text editing feels responsive (<50ms latency)
- **Reliability:** No data loss (auto-save + backup)
- **Usability:** Intuitive UI, keyboard shortcuts work
- **Cross-Platform:** Works on Windows, macOS, Linux

### Testing Requirements
- 80+ unit tests (all P0 features)
- 20+ integration tests (save/load, undo/redo)
- Manual testing on all 3 platforms

---

**Document Version:** 1.0
**Created:** 2025-10-31
**Last Updated:** 2025-10-31
**Status:** Planning Phase
