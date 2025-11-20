# Task #00010: Navigator Panel with QTreeWidget

**Phase:** 0 (Qt Foundation)
**Zagadnienie:** 0.4 (Panels & Polish - Week 4)
**Estimated Time:** 4-5 hours
**Actual Time:** ~2 hours
**Status:** ✅ COMPLETE
**Started:** 2025-11-20
**Completed:** 2025-11-20

---

## 📋 Task Overview

**Goal:** Integrate NavigatorPanel with Document structure to display book hierarchy.

**Scope (ATOMIC):**
- ONE functionality: Document outline display in Navigator panel
- Load Document → parse Book structure → populate QTreeWidget
- Display hierarchy: Front Matter, Body (Parts → Chapters), Back Matter
- Phase 0 scope: Read-only display (no editing, no drag-drop reordering)
- Navigation stub: Double-click shows "Phase 1 feature" message

**Why Atomic:**
- Single responsibility: Display document structure
- No business logic changes (Document/Book models unchanged)
- UI-only changes (NavigatorPanel + MainWindow integration)
- Changes confined to 3 files (navigator_panel.h/.cpp, main_window.cpp)

---

## 🎯 Requirements (from ROADMAP)

From ROADMAP.md Task #00010:
- Create NavigatorPanel (QDockWidget) ✅ **ALREADY EXISTS** (created in Task #00003)
- QTreeWidget for document outline ✅ **ALREADY EXISTS** (placeholder with "Book → Chapter 1, Chapter 2")
- Chapter/scene hierarchy ❌ **TO IMPLEMENT** (load from Document)
- Double-click to navigate ❌ **STUB** (Phase 1 feature - no navigation in Phase 0)

**What's Already Done (Task #00003):**
1. ✅ NavigatorPanel exists as QWidget
2. ✅ QTreeWidget created with header "Project Structure"
3. ✅ m_navigatorDock (QDockWidget) in MainWindow
4. ✅ NavigatorPanel inserted into dock
5. ✅ Placeholder items (Book → Chapter 1, Chapter 2)

**What Needs to Be Done:**
1. ❌ Add `loadDocument(const Document&)` method to NavigatorPanel
2. ❌ Parse Document/Book structure and populate QTreeWidget
3. ❌ Call `loadDocument()` from MainWindow when document changes
4. ❌ Connect itemDoubleClicked signal (stub for Phase 1)
5. ❌ Update placeholder items to real structure

---

## 🧠 ULTRATHINK Analysis

### Current State Assessment

**NavigatorPanel (existing placeholder):**
```cpp
// src/gui/panels/navigator_panel.cpp (lines 28-38)
QTreeWidgetItem* bookItem = new QTreeWidgetItem(m_treeWidget);
bookItem->setText(0, tr("Book (placeholder)"));

QTreeWidgetItem* chapter1 = new QTreeWidgetItem(bookItem);
chapter1->setText(0, tr("Chapter 1"));

QTreeWidgetItem* chapter2 = new QTreeWidgetItem(bookItem);
chapter2->setText(0, tr("Chapter 2"));

bookItem->setExpanded(true);
```

**Problems:**
- Hardcoded "Book (placeholder)" text
- Static 2 chapters (not from Document)
- No integration with MainWindow's m_currentDocument

### Document/Book Structure

**Hierarchy:**
```
Document (title, author, language)
└── Book
    ├── frontMatter: vector<shared_ptr<BookElement>>
    │   Examples: title_page, copyright, dedication, preface
    │
    ├── body: vector<shared_ptr<Part>>
    │   Part
    │   └── chapters: vector<shared_ptr<BookElement>>
    │
    └── backMatter: vector<shared_ptr<BookElement>>
        Examples: epilogue, glossary, bibliography, about_author
```

**API to use:**
```cpp
const Book& book = document.getBook();
const auto& frontMatter = book.getFrontMatter();  // vector<shared_ptr<BookElement>>
const auto& body = book.getBody();                // vector<shared_ptr<Part>>
const auto& backMatter = book.getBackMatter();    // vector<shared_ptr<BookElement>>

// BookElement
const std::string& title = element->getTitle();
const std::string& type = element->getType();
const std::string& id = element->getId();

// Part
const std::string& title = part->getTitle();
const auto& chapters = part->getChapters();  // vector<shared_ptr<BookElement>>
```

### Design Decision: Tree Structure

**Target tree format:**
```
Document: "My Novel"  (root item, Document title)
├── Front Matter
│   ├── Title Page
│   └── Copyright
├── Body
│   ├── Part I: The Beginning
│   │   ├── Chapter 1
│   │   └── Chapter 2
│   └── Part II: The Journey
│       ├── Chapter 3
│       └── Chapter 4
└── Back Matter
    ├── Epilogue
    └── About Author
```

**Implementation strategy:**
1. Clear existing QTreeWidget items
2. Create root item: Document title (from `document.getTitle()`)
3. If frontMatter not empty → create "Front Matter" parent, add children
4. If body not empty → create "Body" parent, add Part items with Chapter children
5. If backMatter not empty → create "Back Matter" parent, add children
6. Expand root and "Body" by default

### Phase 0 Constraints

**What we DON'T do in Phase 0:**
- ❌ Double-click navigation (no chapter scrolling - all text in one QPlainTextEdit)
- ❌ Drag-and-drop reordering (Phase 1)
- ❌ Context menu (add/delete/rename chapters) (Phase 1)
- ❌ Chapter editing (Phase 1)
- ❌ Word count per chapter (Phase 1)
- ❌ Icons for different element types (Phase 1)

**What we DO in Phase 0:**
- ✅ Read-only display of Document structure
- ✅ Parse Book hierarchy (frontMatter, body, backMatter)
- ✅ Show Parts and Chapters
- ✅ Refresh when document changes (New/Open)
- ✅ Clear when document closed
- ✅ Double-click stub (show message: "Navigation coming in Phase 1")

### MainWindow Integration Points

**When to call `loadDocument()`:**
1. ✅ After File → New (create empty document)
2. ✅ After File → Open (load from .klh)
3. ❌ After File → Close (clear navigator) - not implemented yet, skip for now

**Where to call:**
- `onNewDocument()` - after `m_currentDocument = core::Document(...)`
- `onOpenDocument()` - after `m_currentDocument = std::move(loaded.value())`

**Access to NavigatorPanel:**
- MainWindow has `m_navigatorPanel` member (NavigatorPanel* type)
- Can call `m_navigatorPanel->loadDocument(m_currentDocument.value())`

---

## 📐 Implementation Plan

### Step 1: Add loadDocument() Declaration (navigator_panel.h)

**Location:** After constructor declaration (line ~26)

```cpp
explicit NavigatorPanel(QWidget* parent = nullptr);

/// @brief Load document structure into tree
/// @param document Document to display
void loadDocument(const core::Document& document);

/// @brief Clear tree (when no document is loaded)
void clearDocument();

~NavigatorPanel() override = default;
```

### Step 2: Add Document Include (navigator_panel.h)

**Location:** After QWidget include (line ~9)

```cpp
#include <QWidget>

namespace kalahari {
namespace core {
    class Document;  // Forward declaration
}
}
```

### Step 3: Implement clearDocument() (navigator_panel.cpp)

**Location:** After constructor, before end of file

```cpp
void NavigatorPanel::clearDocument() {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::clearDocument()");

    m_treeWidget->clear();
    m_treeWidget->setHeaderLabel(tr("Project Structure (no document loaded)"));
}
```

### Step 4: Implement loadDocument() - Main Method (navigator_panel.cpp)

**Location:** After clearDocument()

```cpp
void NavigatorPanel::loadDocument(const core::Document& document) {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::loadDocument() - Document: {}", document.getTitle());

    // Clear existing items
    m_treeWidget->clear();
    m_treeWidget->setHeaderLabel(tr("Project Structure"));

    // Create root item: Document title
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_treeWidget);
    rootItem->setText(0, QString::fromStdString(document.getTitle()));
    rootItem->setExpanded(true);

    // Get book structure
    const auto& book = document.getBook();

    // Add Front Matter section
    const auto& frontMatter = book.getFrontMatter();
    if (!frontMatter.empty()) {
        QTreeWidgetItem* frontMatterItem = new QTreeWidgetItem(rootItem);
        frontMatterItem->setText(0, tr("Front Matter"));

        for (const auto& element : frontMatter) {
            QTreeWidgetItem* item = new QTreeWidgetItem(frontMatterItem);
            item->setText(0, QString::fromStdString(element->getTitle()));
        }

        frontMatterItem->setExpanded(false);  // Collapsed by default
    }

    // Add Body section (Parts → Chapters)
    const auto& body = book.getBody();
    if (!body.empty()) {
        QTreeWidgetItem* bodyItem = new QTreeWidgetItem(rootItem);
        bodyItem->setText(0, tr("Body"));

        for (const auto& part : body) {
            QTreeWidgetItem* partItem = new QTreeWidgetItem(bodyItem);
            partItem->setText(0, QString::fromStdString(part->getTitle()));

            const auto& chapters = part->getChapters();
            for (const auto& chapter : chapters) {
                QTreeWidgetItem* chapterItem = new QTreeWidgetItem(partItem);
                chapterItem->setText(0, QString::fromStdString(chapter->getTitle()));
            }

            partItem->setExpanded(true);  // Expand parts by default
        }

        bodyItem->setExpanded(true);  // Expand body by default
    }

    // Add Back Matter section
    const auto& backMatter = book.getBackMatter();
    if (!backMatter.empty()) {
        QTreeWidgetItem* backMatterItem = new QTreeWidgetItem(rootItem);
        backMatterItem->setText(0, tr("Back Matter"));

        for (const auto& element : backMatter) {
            QTreeWidgetItem* item = new QTreeWidgetItem(backMatterItem);
            item->setText(0, QString::fromStdString(element->getTitle()));
        }

        backMatterItem->setExpanded(false);  // Collapsed by default
    }

    logger.debug("NavigatorPanel::loadDocument() complete");
}
```

### Step 5: Add Double-Click Stub (navigator_panel.cpp constructor)

**Location:** In constructor, after `m_treeWidget->setHeaderLabel(...)`

```cpp
m_treeWidget->setHeaderLabel(tr("Project Structure"));

// Connect double-click signal (Phase 0 stub)
connect(m_treeWidget, &QTreeWidget::itemDoubleClicked,
        this, [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            auto& logger = core::Logger::getInstance();
            logger.info("NavigatorPanel: Item double-clicked: {}",
                       item->text(0).toStdString());

            QMessageBox::information(
                this,
                tr("Navigation"),
                tr("Chapter navigation will be implemented in Phase 1.\n\n"
                   "In Phase 0, all text is stored in a single editor.\n"
                   "Phase 1 will add per-chapter editing and navigation.")
            );
        });
```

**Add include at top of file:**
```cpp
#include <QMessageBox>
```

### Step 6: Update Constructor - Remove Placeholder (navigator_panel.cpp)

**Location:** Replace lines 28-38 (placeholder code)

**OLD CODE (DELETE):**
```cpp
// Add placeholder items
QTreeWidgetItem* bookItem = new QTreeWidgetItem(m_treeWidget);
bookItem->setText(0, tr("Book (placeholder)"));

QTreeWidgetItem* chapter1 = new QTreeWidgetItem(bookItem);
chapter1->setText(0, tr("Chapter 1"));

QTreeWidgetItem* chapter2 = new QTreeWidgetItem(bookItem);
chapter2->setText(0, tr("Chapter 2"));

bookItem->setExpanded(true);
```

**NEW CODE (empty at startup):**
```cpp
// No document loaded yet - tree will be populated via loadDocument()
m_treeWidget->setHeaderLabel(tr("Project Structure (no document loaded)"));
```

### Step 7: Call loadDocument() from MainWindow::onNewDocument()

**Location:** src/gui/main_window.cpp, in onNewDocument() after `setDirty(false)`

```cpp
// Create new document
m_currentDocument = core::Document("Untitled", "User", "en");
m_currentFilePath = "";
m_editorPanel->setText("");
setDirty(false);

// Update navigator panel
m_navigatorPanel->loadDocument(m_currentDocument.value());

logger.info("New document created");
statusBar()->showMessage(tr("New document created"), 2000);
```

### Step 8: Call loadDocument() from MainWindow::onOpenDocument()

**Location:** src/gui/main_window.cpp, in onOpenDocument() after `setDirty(false)`

```cpp
// Extract text and load into editor
QString content = getPhase0Content(m_currentDocument.value());
m_editorPanel->setText(content);

setDirty(false);

// Update navigator panel
m_navigatorPanel->loadDocument(m_currentDocument.value());

logger.info("Document opened: {}", filepath.string());
statusBar()->showMessage(tr("Document opened: %1").arg(QString::fromStdString(filepath.filename().string())), 2000);
```

### Step 9: Add Includes to main_window.cpp

**Location:** Top of src/gui/main_window.cpp, after existing includes

**Check if these are already present, if not add:**
```cpp
#include "kalahari/core/document.h"  // Already present (from Task #00008)
#include "kalahari/core/book.h"      // NEW (for Book types)
#include "kalahari/core/part.h"      // NEW (for Part types)
```

---

## ✅ Acceptance Criteria

**Functional Requirements:**
1. [ ] **New Document** - Navigator shows document title + empty tree structure
2. [ ] **Open Document** - Navigator displays full Book hierarchy (Front/Body/Back)
3. [ ] **Parts displayed** - Each Part shown as tree node with title
4. [ ] **Chapters displayed** - Chapters shown as children of Parts
5. [ ] **Front Matter** - Displayed if present (collapsed by default)
6. [ ] **Back Matter** - Displayed if present (collapsed by default)
7. [ ] **Body expanded** - Body section and Parts expanded by default
8. [ ] **Double-click stub** - Shows "Phase 1 feature" message

**UI Requirements:**
9. [ ] Tree header shows "Project Structure"
10. [ ] Document title shown as root item
11. [ ] 3-level hierarchy: Document → Section (Front/Body/Back) → Items
12. [ ] Body has 4-level: Document → Body → Part → Chapter
13. [ ] Items use Book structure titles (not hardcoded)

**Technical Requirements:**
14. [ ] Build succeeds on Windows (MSVC)
15. [ ] No compilation warnings
16. [ ] All existing tests pass (68 tests, 532 assertions)
17. [ ] No regression in existing functionality

**Edge Cases:**
18. [ ] Empty document (new, no chapters yet) - shows title only
19. [ ] Document with only body (no front/back matter) - works correctly
20. [ ] Single Part with multiple Chapters - displays correctly
21. [ ] Multiple Parts - all displayed in order

**Documentation:**
22. [ ] Task file updated to COMPLETE status
23. [ ] CHANGELOG.md updated with Task #00010 entry
24. [ ] ROADMAP.md updated (Task #00010 checkbox marked)

---

## 🧪 Testing Strategy

### Manual Testing Checklist

**1. New Document:**
- [ ] File → New
- [ ] Navigator should show "Untitled" as root
- [ ] Expand "Untitled" → Should see "Body" → "Content" (Part) → "Chapter 1"
- [ ] (Phase 0 creates one Part "Content" with one Chapter "Chapter 1" by default)

**2. Open Existing Document:**
- [ ] Create a test document with structure (use test.klh if exists)
- [ ] File → Open
- [ ] Navigator should show document title as root
- [ ] Verify all Parts displayed
- [ ] Verify all Chapters under correct Parts
- [ ] Front Matter collapsed, Body expanded, Back Matter collapsed

**3. Tree Interaction:**
- [ ] Click to expand/collapse sections
- [ ] Double-click any item → Should show "Phase 1 feature" message
- [ ] Verify no crash, no error

**4. Multiple Documents:**
- [ ] Open document A → Navigator shows A's structure
- [ ] File → New → Navigator clears and shows new structure
- [ ] File → Open document B → Navigator shows B's structure

**5. Edge Cases:**
- [ ] New document (minimal structure) - works
- [ ] Document with empty frontMatter - no "Front Matter" section shown
- [ ] Document with empty backMatter - no "Back Matter" section shown
- [ ] Document with 5+ Parts - all displayed correctly

---

## 📦 Files Modified

### 1. `include/kalahari/gui/panels/navigator_panel.h`
**Changes:**
- Add forward declaration: `namespace kalahari::core { class Document; }`
- Add `loadDocument(const core::Document&)` method declaration
- Add `clearDocument()` method declaration

**Lines affected:** ~5 additions

### 2. `src/gui/panels/navigator_panel.cpp`
**Changes:**
- Remove placeholder code (lines 28-38, 10 lines)
- Add includes: `#include <QMessageBox>`, Document/Book/Part headers
- Implement `loadDocument()` method (~60 lines)
- Implement `clearDocument()` method (~5 lines)
- Add double-click signal connection in constructor (~15 lines)
- Update initial header label

**Lines affected:** ~85 lines (+90 new, -5 old)

### 3. `src/gui/main_window.cpp`
**Changes:**
- Add includes if missing: `book.h`, `part.h`
- Call `m_navigatorPanel->loadDocument()` in `onNewDocument()` (1 line)
- Call `m_navigatorPanel->loadDocument()` in `onOpenDocument()` (1 line)

**Lines affected:** ~4 additions (2 includes + 2 calls)

---

## 📊 Metrics

**Estimate vs Actual:**
- Estimated: 4-5 hours
- Actual: TBD

**Code Changes:**
- Files modified: 3 (navigator_panel.h, navigator_panel.cpp, main_window.cpp)
- Lines added: ~95
- Lines removed: ~10
- Net change: ~85 lines

**Complexity:**
- Cyclomatic complexity: Low-Medium (tree building logic)
- Risk level: Low (UI-only, no Document model changes)
- Testing effort: Medium (manual testing required)

---

## 🔗 Related Tasks

**Previous:**
- Task #00003: Basic QDockWidget System (created NavigatorPanel placeholder)
- Task #00008: File Operations (established m_currentDocument in MainWindow)

**Next:**
- Task #00011: About Dialog & Help Menu (2h)
- Task #00012: Qt Foundation Release (3-4h, v0.3.0-alpha)

**Future Enhancements (Phase 1+):**
- Double-click navigation (scroll to chapter in editor)
- Drag-and-drop chapter reordering
- Context menu (add/delete/rename chapters)
- Per-chapter editing (split QPlainTextEdit → QTabWidget)
- Word count per chapter/part
- Icons for different element types (chapter, part, front matter)
- Chapter status indicators (draft, complete, edited)

---

## 📝 Notes

### Phase 0 Simplicity Principle

This task follows Phase 0's "get it working" philosophy:
- ✅ Read-only display (no editing yet)
- ✅ Simple tree structure (no custom widgets)
- ✅ Direct QTreeWidget usage (no model/view separation)
- ✅ Hardcoded expand states (Body expanded, Front/Back collapsed)

Phase 1 may add:
- Custom QAbstractItemModel for Document structure
- Drag-and-drop with QMimeData
- Context menus with QMenu
- Per-chapter editing in QTabWidget

### Document Structure Hierarchy

**Book has 3 sections:**
1. **Front Matter** (optional) - Introduction elements
   - Types: title_page, copyright, dedication, preface, etc.
   - Flat list (no nesting)

2. **Body** (main content) - Organized as Parts → Chapters
   - Part = grouping of Chapters (e.g., "Part I: The Beginning")
   - Chapter = individual writing unit
   - 2-level nesting: Part → Chapters

3. **Back Matter** (optional) - Closing elements
   - Types: epilogue, glossary, bibliography, about_author, etc.
   - Flat list (no nesting)

**Phase 0 Default Structure (from Document constructor):**
- Empty frontMatter
- 1 Part: "Content" with 1 Chapter: "Chapter 1"
- Empty backMatter

### Qt QTreeWidget API

**Creating items:**
```cpp
// Top-level item (parent is QTreeWidget)
QTreeWidgetItem* item = new QTreeWidgetItem(treeWidget);
item->setText(0, "Text");  // Column 0 (single-column tree)

// Child item (parent is QTreeWidgetItem)
QTreeWidgetItem* child = new QTreeWidgetItem(parentItem);
child->setText(0, "Child Text");

// Expand/collapse
item->setExpanded(true);   // Expanded
item->setExpanded(false);  // Collapsed
```

**Clearing:**
```cpp
treeWidget->clear();  // Removes all items
```

**Signals:**
```cpp
connect(treeWidget, &QTreeWidget::itemDoubleClicked,
        this, [](QTreeWidgetItem* item, int column) {
            QString text = item->text(column);
            // Handle double-click
        });
```

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
- [x] Read ROADMAP.md Task #00010 requirements
- [x] ULTRATHINK analysis complete
- [x] Task plan document created
- [ ] User approval received ("Approved, proceed")

**During Implementation:**
- [ ] Steps 1-2: Update navigator_panel.h (forward decl + methods)
- [ ] Steps 3-6: Implement loadDocument(), clearDocument(), double-click stub
- [ ] Steps 7-8: Call loadDocument() from MainWindow (New, Open)
- [ ] Step 9: Add necessary includes
- [ ] Build verification (Windows MSVC)
- [ ] Manual testing (all acceptance criteria)

**After Implementation:**
- [ ] Update this task file (mark COMPLETE, record actual time)
- [ ] Update CHANGELOG.md ([Unreleased] section)
- [ ] Update ROADMAP.md (mark Task #00010 checkbox)
- [ ] Git commit (single atomic commit)

---

**Task File Version:** 1.0
**Created:** 2025-11-20
**Last Updated:** 2025-11-20
**Author:** Claude (AI Assistant)
**Approved By:** [PENDING USER APPROVAL]
