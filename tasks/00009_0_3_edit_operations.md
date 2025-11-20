# Task #00009: Edit Operations (Undo/Redo/Cut/Copy/Paste/Select All)

**Phase:** 0 (Qt Foundation)
**Zagadnienie:** 0.3 (Core Editor Foundation - Week 3)
**Estimated Time:** 2-3 hours
**Actual Time:** ~1.5 hours
**Status:** ✅ COMPLETE
**Started:** 2025-11-20
**Completed:** 2025-11-20

---

## 📋 Task Overview

**Goal:** Implement Edit menu operations by delegating to QPlainTextEdit's built-in methods.

**Scope (ATOMIC):**
- ONE functionality: Edit operations (Undo/Redo/Cut/Copy/Paste/Select All)
- Simple delegation to QPlainTextEdit API
- NO complex logic, NO undo/redo stack implementation (Qt handles it)
- Add status bar feedback for user actions

**Why Atomic:**
- Single responsibility: Enable standard text editing operations
- All operations delegate to same widget (m_editorPanel->getTextEdit())
- No external dependencies beyond QPlainTextEdit
- Changes confined to main_window.h/.cpp (2 files)

---

## 🎯 Requirements (from ROADMAP)

From ROADMAP.md Task #00009:
- Edit → Undo/Redo/Cut/Copy/Paste/Select All
- Keyboard shortcuts (QKeySequence) ✅ **ALREADY EXISTS** for 5/6 operations
- Menu/toolbar integration ✅ **ALREADY EXISTS** (Edit menu)
- Status bar updates ❌ **TO IMPLEMENT**

**What's Already Done:**
1. ✅ Actions created: m_undoAction, m_redoAction, m_cutAction, m_copyAction, m_pasteAction
2. ✅ Keyboard shortcuts: Ctrl+Z, Ctrl+Y, Ctrl+X, Ctrl+C, Ctrl+V (QKeySequence standard)
3. ✅ Edit menu: All 5 actions added with separators
4. ✅ Signal connections: All connected to slot methods
5. ✅ Placeholder implementations: Show "not implemented" messages

**What Needs to Be Done:**
1. ❌ Add m_selectAllAction (Ctrl+A)
2. ❌ Add onSelectAll() slot
3. ❌ Implement all 6 operations (delegate to QPlainTextEdit)
4. ❌ Add status bar messages

---

## 🧠 ULTRATHINK Analysis

### Current State Assessment

**File:** `src/gui/main_window.cpp` (lines 407-435)

```cpp
void MainWindow::onUndo() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Undo");
    statusBar()->showMessage(tr("Undo (not implemented)"), 2000);
}

void MainWindow::onRedo() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Redo");
    statusBar()->showMessage(tr("Redo (not implemented)"), 2000);
}

void MainWindow::onCut() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Cut");
    statusBar()->showMessage(tr("Cut (not implemented)"), 2000);
}

void MainWindow::onCopy() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Copy");
    statusBar()->showMessage(tr("Copy (not implemented)"), 2000);
}

void MainWindow::onPaste() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Paste");
    statusBar()->showMessage(tr("Paste (not implemented)"), 2000);
}
```

**All operations:**
- ✅ Logging present
- ✅ Status bar message (but says "not implemented")
- ❌ No actual QPlainTextEdit delegation

### QPlainTextEdit API (from Context7 Qt6 docs)

**Undo/Redo:**
```cpp
editor->undo();   // Slot: Undo last operation
editor->redo();   // Slot: Redo last undone operation
```

**Clipboard Operations:**
```cpp
editor->copy();   // Slot: Copy selected text to clipboard
editor->cut();    // Slot: Cut selected text to clipboard
editor->paste();  // Slot: Paste from clipboard at cursor
```

**Selection:**
```cpp
editor->selectAll();  // Slot: Select all text in document
```

**All methods are PUBLIC SLOTS** - can be called directly from our slots.

### Design Decision: Direct Delegation

**Pattern:**
```cpp
void MainWindow::onUndo() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Undo");

    m_editorPanel->getTextEdit()->undo();

    statusBar()->showMessage(tr("Undo performed"), 2000);
}
```

**Why This Works:**
1. QPlainTextEdit has built-in undo/redo stack (enabled by default)
2. Qt automatically tracks text changes and builds undo/redo history
3. No need to implement custom undo/redo logic (Phase 0 simplicity)
4. Cut/Copy/Paste integrate with system clipboard automatically
5. Status bar provides user feedback

**Phase 0 Simplicity:**
- No custom undo stack needed (Phase 1 may add QUndoStack for Document-level operations)
- No clipboard format customization (Phase 1 may add rich text support)
- No selection validation (QPlainTextEdit handles empty selection gracefully)

### Select All - New Action Needed

**Missing from current implementation:**
- No m_selectAllAction member variable
- No onSelectAll() slot
- Not in Edit menu

**Standard Pattern (following existing actions):**

**In createActions():**
```cpp
m_selectAllAction = new QAction(tr("Select &All"), this);
m_selectAllAction->setShortcut(QKeySequence::SelectAll);  // Ctrl+A
m_selectAllAction->setStatusTip(tr("Select all text"));
connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::onSelectAll);
```

**In createMenus():**
```cpp
m_editMenu->addAction(m_pasteAction);
m_editMenu->addSeparator();
m_editMenu->addAction(m_selectAllAction);  // NEW: After Paste
m_editMenu->addSeparator();
m_editMenu->addAction(m_settingsAction);
```

**In main_window.h:**
```cpp
private slots:
    // ... existing slots ...
    void onSelectAll();  // NEW

private:
    // ... existing actions ...
    QAction* m_selectAllAction;  // NEW
```

---

## 📐 Implementation Plan

### Step 1: Add Select All Action (main_window.h)

**Location:** After `m_pasteAction` declaration (line ~150)

```cpp
QAction* m_pasteAction;
QAction* m_selectAllAction;  // NEW
QAction* m_settingsAction;
```

**Location:** After `onPaste()` slot declaration (line ~132)

```cpp
void onPaste();
void onSelectAll();  // NEW
void onSettings();
```

### Step 2: Create Select All Action (main_window.cpp)

**Location:** In `createActions()`, after `m_pasteAction` creation (line ~150)

```cpp
m_pasteAction = new QAction(tr("&Paste"), this);
m_pasteAction->setShortcut(QKeySequence::Paste);
m_pasteAction->setStatusTip(tr("Paste from clipboard"));
connect(m_pasteAction, &QAction::triggered, this, &MainWindow::onPaste);

// NEW: Select All action
m_selectAllAction = new QAction(tr("Select &All"), this);
m_selectAllAction->setShortcut(QKeySequence::SelectAll);
m_selectAllAction->setStatusTip(tr("Select all text"));
connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::onSelectAll);

m_settingsAction = new QAction(tr("&Settings..."), this);
```

### Step 3: Add Select All to Edit Menu (main_window.cpp)

**Location:** In `createMenus()`, after `m_pasteAction` (line ~181)

```cpp
m_editMenu->addAction(m_pasteAction);
m_editMenu->addSeparator();
m_editMenu->addAction(m_selectAllAction);  // NEW
m_editMenu->addSeparator();
m_editMenu->addAction(m_settingsAction);
```

### Step 4: Implement Undo Operation (main_window.cpp)

**Location:** Replace existing `onUndo()` implementation (line 407)

```cpp
void MainWindow::onUndo() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Undo");

    m_editorPanel->getTextEdit()->undo();

    statusBar()->showMessage(tr("Undo performed"), 2000);
}
```

### Step 5: Implement Redo Operation (main_window.cpp)

**Location:** Replace existing `onRedo()` implementation (line 413)

```cpp
void MainWindow::onRedo() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Redo");

    m_editorPanel->getTextEdit()->redo();

    statusBar()->showMessage(tr("Redo performed"), 2000);
}
```

### Step 6: Implement Cut Operation (main_window.cpp)

**Location:** Replace existing `onCut()` implementation (line 419)

```cpp
void MainWindow::onCut() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Cut");

    m_editorPanel->getTextEdit()->cut();

    statusBar()->showMessage(tr("Cut to clipboard"), 2000);
}
```

### Step 7: Implement Copy Operation (main_window.cpp)

**Location:** Replace existing `onCopy()` implementation (line 425)

```cpp
void MainWindow::onCopy() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Copy");

    m_editorPanel->getTextEdit()->copy();

    statusBar()->showMessage(tr("Copied to clipboard"), 2000);
}
```

### Step 8: Implement Paste Operation (main_window.cpp)

**Location:** Replace existing `onPaste()` implementation (line 431)

```cpp
void MainWindow::onPaste() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Paste");

    m_editorPanel->getTextEdit()->paste();

    statusBar()->showMessage(tr("Pasted from clipboard"), 2000);
}
```

### Step 9: Implement Select All Operation (main_window.cpp)

**Location:** After `onPaste()` implementation (new method)

```cpp
void MainWindow::onSelectAll() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Select All");

    m_editorPanel->getTextEdit()->selectAll();

    statusBar()->showMessage(tr("All text selected"), 2000);
}
```

---

## ✅ Acceptance Criteria

**Functional Requirements:**
1. [ ] **Undo (Ctrl+Z)** - Reverts last text change
2. [ ] **Redo (Ctrl+Y)** - Re-applies last undone change
3. [ ] **Cut (Ctrl+X)** - Removes selected text and copies to clipboard
4. [ ] **Copy (Ctrl+C)** - Copies selected text to clipboard
5. [ ] **Paste (Ctrl+V)** - Inserts clipboard text at cursor
6. [ ] **Select All (Ctrl+A)** - Selects all text in editor

**UI Requirements:**
7. [ ] All 6 operations appear in Edit menu
8. [ ] Keyboard shortcuts work correctly
9. [ ] Status bar shows feedback for each operation
10. [ ] Select All action has separator before it (visual grouping)

**Technical Requirements:**
11. [ ] Build succeeds on Windows (MSVC)
12. [ ] No compilation warnings
13. [ ] All existing tests pass (68 tests, 532 assertions)
14. [ ] No regression in existing functionality

**Edge Cases:**
15. [ ] Undo/Redo work correctly with no history (graceful no-op)
16. [ ] Cut/Copy work with no selection (graceful no-op)
17. [ ] Paste works with empty clipboard (graceful no-op)
18. [ ] Select All works on empty document (selects nothing)

**Documentation:**
19. [ ] Task file updated to COMPLETE status
20. [ ] CHANGELOG.md updated with Task #00009 entry
21. [ ] ROADMAP.md updated (Task #00009 checkbox marked)

---

## 🧪 Testing Strategy

### Manual Testing Checklist

**1. Undo/Redo (Text Editing):**
- [ ] Open Kalahari
- [ ] Type "Hello World" in editor
- [ ] Press Ctrl+Z → Text should disappear
- [ ] Press Ctrl+Y → Text should reappear
- [ ] Verify status bar shows "Undo performed" / "Redo performed"

**2. Cut/Copy/Paste (Clipboard):**
- [ ] Type "Test Text" in editor
- [ ] Select "Test" with mouse
- [ ] Press Ctrl+C → Status bar: "Copied to clipboard"
- [ ] Move cursor to end of document
- [ ] Press Ctrl+V → "Test" should appear again
- [ ] Select "Text"
- [ ] Press Ctrl+X → "Text" should disappear, status bar: "Cut to clipboard"
- [ ] Press Ctrl+V → "Text" should reappear

**3. Select All:**
- [ ] Type multi-line text in editor
- [ ] Press Ctrl+A → All text should be highlighted
- [ ] Status bar should show "All text selected"

**4. Menu Integration:**
- [ ] Open Edit menu
- [ ] Verify all 6 actions present: Undo, Redo, Cut, Copy, Paste, Select All
- [ ] Verify keyboard shortcuts shown in menu (Ctrl+Z, etc.)
- [ ] Click each menu item → Operation should work

**5. Edge Cases:**
- [ ] Press Ctrl+Z on empty editor → No crash, status bar shows message
- [ ] Press Ctrl+C with no selection → No crash, clipboard unchanged
- [ ] Press Ctrl+V with empty clipboard → No crash, no text inserted
- [ ] Press Ctrl+A on empty editor → No crash, nothing selected

---

## 📦 Files Modified

### 1. `include/kalahari/gui/main_window.h`
**Changes:**
- Add `m_selectAllAction` member variable (after `m_pasteAction`)
- Add `onSelectAll()` slot declaration (after `onPaste()`)

**Lines affected:** ~2 additions

### 2. `src/gui/main_window.cpp`
**Changes:**
- Create `m_selectAllAction` in `createActions()` (after `m_pasteAction` creation)
- Add `m_selectAllAction` to Edit menu in `createMenus()` (after `m_pasteAction`)
- Replace 5 placeholder implementations (onUndo/onRedo/onCut/onCopy/onPaste)
- Add new `onSelectAll()` implementation

**Lines affected:** ~40 changes (5 method replacements + 1 new method + action/menu setup)

---

## 📊 Metrics

**Estimate vs Actual:**
- Estimated: 2-3 hours
- Actual: TBD

**Code Changes:**
- Files modified: 2 (main_window.h, main_window.cpp)
- Lines added: ~45
- Lines removed: ~5 (placeholder messages)
- Net change: ~40 lines

**Complexity:**
- Cyclomatic complexity: Low (simple delegation)
- Risk level: Low (using Qt built-in methods)
- Testing effort: Medium (manual testing required)

---

## 🔗 Related Tasks

**Previous:**
- Task #00007: EditorWidget Basic Implementation (provided getTextEdit() accessor)
- Task #00008: File Operations (established MainWindow ↔ EditorPanel pattern)

**Next:**
- Task #00010: Navigator Panel with QTreeWidget (4-5h)

**Future Enhancements (Phase 1+):**
- QUndoStack for Document-level undo/redo (beyond text editing)
- Rich text clipboard formats (HTML, RTF)
- Context menu with Edit operations (right-click in editor)
- Action enable/disable based on editor state (e.g., disable Paste if clipboard empty)

---

## 📝 Notes

### Phase 0 Simplicity Principle

This task follows Phase 0's "get it working" philosophy:
- ✅ Use Qt's built-in undo/redo stack (no custom implementation)
- ✅ System clipboard integration (no custom formats)
- ✅ Direct delegation (no abstraction layers)

Phase 1 may add:
- Custom QUndoCommand for Document changes (Part/Chapter CRUD)
- Rich text clipboard support (HTML, RTF)
- Undo/redo for non-text operations (e.g., chapter reordering)

### Qt Undo/Redo Architecture

**QPlainTextEdit has built-in undo/redo:**
- Enabled by default (`setUndoRedoEnabled(true)`)
- Automatically tracks text insertions/deletions
- No manual command creation needed
- Undo stack managed internally by Qt

**User can call:**
- `undo()` - Undoes last operation
- `redo()` - Redoes last undone operation
- `isUndoAvailable()` - Check if undo possible
- `isRedoAvailable()` - Check if redo possible

### Status Bar Feedback Philosophy

**Design pattern:**
- Show brief message (2 seconds) for user feedback
- Don't show error messages for graceful no-ops (Qt handles it)
- Consistent message format: "[Operation] performed/done/completed"

**Examples:**
- "Undo performed"
- "Copied to clipboard"
- "All text selected"

---

## 🚀 Deployment Notes

**No deployment impact:**
- Internal GUI functionality only
- No API changes
- No file format changes
- No settings changes

**Build verification:**
- Windows (MSVC 2022): Required
- Linux (GCC via WSL): Optional (CI/CD will verify)
- macOS (Clang): Optional (CI/CD will verify)

---

## ✅ Task Completion Checklist

**Before Starting:**
- [x] Read ROADMAP.md Task #00009 requirements
- [x] ULTRATHINK analysis complete
- [x] Task plan document created
- [ ] User approval received ("Approved, proceed")

**During Implementation:**
- [ ] Step 1-3: Add Select All action (main_window.h + createActions + createMenus)
- [ ] Step 4-9: Implement all 6 operations
- [ ] Build verification (Windows MSVC)
- [ ] Manual testing (all acceptance criteria)

**After Implementation:**
- [ ] Update this task file (mark COMPLETE, record actual time)
- [ ] Update CHANGELOG.md ([Unreleased] section)
- [ ] Update ROADMAP.md (mark Task #00009 checkbox)
- [ ] Git commit (single atomic commit)

---

**Task File Version:** 1.0
**Created:** 2025-11-20
**Last Updated:** 2025-11-20
**Author:** Claude (AI Assistant)
**Approved By:** [PENDING USER APPROVAL]
