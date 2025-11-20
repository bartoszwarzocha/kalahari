# Task #00008: File Operations (New/Open/Save/Save As)

**Status:** ✅ COMPLETE
**Phase:** Phase 0 (Qt Foundation)
**Zagadnienie:** 0.1 (Qt Migration)
**Estimated Time:** 3-4h
**Started:** 2025-11-20
**Completed:** 2025-11-20
**Actual Time:** ~2.5 hours

---

## 🎯 Goal

Implement complete File menu operations with Document integration:
- File → New (create empty document)
- File → Open (load .klh file)
- File → Save (save to current file)
- File → Save As (save to new file)
- Dirty state tracking (unsaved changes detection)
- Unsaved changes dialog on close

**ATOMIC SCOPE:** File I/O operations ONLY. No Document model refactoring.

---

## ✅ Acceptance Criteria

1. ✅ File → New creates empty document:
   - Clears editor (setText(""))
   - Creates new Document with default metadata
   - Resets dirty state (m_isDirty = false)
   - Clears current file path (m_currentFilePath = "")
   - Updates window title: "Kalahari - Untitled"

2. ✅ File → Open loads .klh file:
   - Shows QFileDialog (filter: "Kalahari Files (*.klh)")
   - Calls DocumentArchive::load(path)
   - Extracts text from first chapter metadata["_phase0_content"]
   - Loads text into editor (setText())
   - Resets dirty state
   - Updates window title: "Kalahari - [filename].klh"

3. ✅ File → Save works correctly:
   - If no current file → calls Save As
   - Gets text from editor (getText())
   - Updates chapter metadata["_phase0_content"] = text
   - Calls DocumentArchive::save(m_currentDocument, m_currentFilePath)
   - Resets dirty state
   - Shows status message: "Document saved"

4. ✅ File → Save As prompts for location:
   - Shows QFileDialog save dialog
   - Gets text from editor
   - Creates/updates Document
   - Calls DocumentArchive::save(doc, new_path)
   - Updates m_currentFilePath
   - Resets dirty state
   - Updates window title

5. ✅ Dirty state tracking works:
   - Connect to m_editorPanel's textChanged() signal
   - Set m_isDirty = true when text changes
   - Show "*" in window title when dirty: "Kalahari - *filename.klh"
   - Don't mark dirty on programmatic setText() calls

6. ✅ Unsaved changes dialog on close:
   - Override closeEvent()
   - If m_isDirty == true, show QMessageBox::question():
     - "Do you want to save changes to [filename]?"
     - Buttons: Save, Don't Save, Cancel
   - Save → call onSaveDocument(), then close
   - Don't Save → close without saving
   - Cancel → abort close (event->ignore())

7. ✅ Build succeeds (scripts\build_windows.bat)
8. ✅ Manual testing passes (all file operations work)

---

## 📝 Implementation Plan

### Phase 0 Content Storage Strategy

**Problem:** Document/Book model uses RTF files, but Phase 0 has simple QPlainTextEdit.

**Solution:** Temporary metadata storage (will be refactored in Phase 1)
- Store editor text in chapter metadata: `metadata["_phase0_content"] = editorText`
- Create ONE chapter in Book.body for Phase 0
- "_phase0_" prefix marks it as temporary (Phase 1 will replace with RTF files)

**Why this works:**
- ✅ Tests .klh format (DocumentArchive::save/load)
- ✅ Uses existing Document/Book model (no new classes)
- ✅ Clean migration path (Phase 1 removes metadata, uses RTF files)
- ✅ Atomic scope (no Document model changes)

### Step 1: Add Document Members to MainWindow

**File:** `include/kalahari/gui/main_window.h`

Add private members:

```cpp
private:
    // ... existing members ...

    // Document management (Phase 0)
    std::optional<core::Document> m_currentDocument;  ///< Current loaded document
    std::filesystem::path m_currentFilePath;          ///< Current .klh file path
    bool m_isDirty;                                   ///< Unsaved changes flag

    // Helper methods
    /// @brief Mark document as modified (add "*" to title)
    void setDirty(bool dirty);

    /// @brief Update window title with filename and dirty state
    void updateWindowTitle();

    /// @brief Get text from first chapter metadata (Phase 0 hack)
    /// @param doc Document to extract text from
    /// @return Editor text content, or empty string if no content
    QString getPhase0Content(const core::Document& doc) const;

    /// @brief Set text in first chapter metadata (Phase 0 hack)
    /// @param doc Document to update
    /// @param text Editor text content
    void setPhase0Content(core::Document& doc, const QString& text);
```

### Step 2: Initialize Members in Constructor

**File:** `src/gui/main_window.cpp`

In MainWindow::MainWindow():

```cpp
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_firstShow(true)
    , m_currentDocument(std::nullopt)  // NEW
    , m_currentFilePath("")            // NEW
    , m_isDirty(false)                 // NEW
{
    // ... existing code ...

    // Connect editor textChanged signal to dirty state tracking
    connect(m_editorPanel->getTextEdit(), &QPlainTextEdit::textChanged,
            this, [this]() {
                if (!m_currentDocument.has_value()) return;
                setDirty(true);
            });

    logger.debug("MainWindow initialized");
}
```

**Note:** Need to add `getTextEdit()` accessor to EditorPanel for signal connection.

### Step 3: Implement Helper Methods

**File:** `src/gui/main_window.cpp`

```cpp
void MainWindow::setDirty(bool dirty) {
    m_isDirty = dirty;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    QString title = "Kalahari - ";

    if (!m_currentFilePath.empty()) {
        QString filename = QString::fromStdString(m_currentFilePath.filename().string());
        title += filename;
    } else {
        title += "Untitled";
    }

    if (m_isDirty) {
        title = "Kalahari - *" + title.mid(11);  // Insert "*" after "Kalahari - "
    }

    setWindowTitle(title);
}

QString MainWindow::getPhase0Content(const core::Document& doc) const {
    const auto& book = doc.getBook();
    const auto& body = book.getBody();

    if (body.empty()) return "";

    const auto& firstPart = body[0];
    const auto& chapters = firstPart->getChapters();

    if (chapters.empty()) return "";

    const auto& firstChapter = chapters[0];
    auto content = firstChapter->getMetadata("_phase0_content");

    return content.has_value() ? QString::fromStdString(content.value()) : "";
}

void MainWindow::setPhase0Content(core::Document& doc, const QString& text) {
    auto& book = doc.getBook();
    auto& body = book.getBody();

    // Create Part if doesn't exist
    if (body.empty()) {
        auto part = std::make_shared<core::Part>("part-001", "Content");
        body.push_back(part);
    }

    auto& firstPart = body[0];
    auto& chapters = firstPart->getChapters();

    // Create Chapter if doesn't exist
    if (chapters.empty()) {
        auto chapter = std::make_shared<core::BookElement>(
            "chapter", "ch-001", "Chapter 1", ""
        );
        chapters.push_back(chapter);
    }

    // Store text in metadata
    chapters[0]->setMetadata("_phase0_content", text.toStdString());
    chapters[0]->touch();  // Update modified timestamp
    doc.touch();
}
```

### Step 4: Implement File → New

**File:** `src/gui/main_window.cpp`

Replace stub with implementation:

```cpp
void MainWindow::onNewDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: New Document");

    // Check for unsaved changes
    if (m_isDirty) {
        auto reply = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to the current document?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            onSaveDocument();
            if (m_isDirty) return;  // Save was cancelled or failed
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
        // Discard → continue
    }

    // Create new document
    m_currentDocument = core::Document("Untitled", "User", "en");
    m_currentFilePath = "";
    m_editorPanel->setText("");
    setDirty(false);

    logger.info("New document created");
    statusBar()->showMessage(tr("New document created"), 2000);
}
```

### Step 5: Implement File → Open

**File:** `src/gui/main_window.cpp`

```cpp
void MainWindow::onOpenDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Open Document");

    // Check for unsaved changes
    if (m_isDirty) {
        auto reply = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to the current document?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            onSaveDocument();
            if (m_isDirty) return;
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    // Show file dialog
    QString filename = QFileDialog::getOpenFileName(
        this,
        tr("Open Document"),
        QString(),
        tr("Kalahari Files (*.klh)")
    );

    if (filename.isEmpty()) {
        logger.info("Open cancelled by user");
        return;
    }

    // Load document
    std::filesystem::path filepath = filename.toStdString();
    auto loaded = core::DocumentArchive::load(filepath);

    if (!loaded.has_value()) {
        QMessageBox::critical(
            this,
            tr("Open Error"),
            tr("Failed to open document: %1").arg(filename)
        );
        logger.error("Failed to load document: {}", filepath.string());
        return;
    }

    // Success - update state
    m_currentDocument = std::move(loaded.value());
    m_currentFilePath = filepath;

    // Extract text and load into editor
    QString content = getPhase0Content(m_currentDocument.value());
    m_editorPanel->setText(content);

    setDirty(false);
    logger.info("Document loaded: {}", filepath.string());
    statusBar()->showMessage(tr("Document opened: %1").arg(filename), 2000);
}
```

### Step 6: Implement File → Save

**File:** `src/gui/main_window.cpp`

```cpp
void MainWindow::onSaveDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save Document");

    // If no current file, delegate to Save As
    if (m_currentFilePath.empty()) {
        onSaveAsDocument();
        return;
    }

    // Ensure we have a document
    if (!m_currentDocument.has_value()) {
        m_currentDocument = core::Document("Untitled", "User", "en");
    }

    // Get text from editor and update document
    QString text = m_editorPanel->getText();
    setPhase0Content(m_currentDocument.value(), text);

    // Save to file
    bool saved = core::DocumentArchive::save(m_currentDocument.value(), m_currentFilePath);

    if (!saved) {
        QMessageBox::critical(
            this,
            tr("Save Error"),
            tr("Failed to save document: %1").arg(QString::fromStdString(m_currentFilePath.string()))
        );
        logger.error("Failed to save document: {}", m_currentFilePath.string());
        return;
    }

    setDirty(false);
    logger.info("Document saved: {}", m_currentFilePath.string());
    statusBar()->showMessage(tr("Document saved"), 2000);
}
```

### Step 7: Implement File → Save As

**File:** `src/gui/main_window.cpp`

```cpp
void MainWindow::onSaveAsDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save As Document");

    // Show save file dialog
    QString filename = QFileDialog::getSaveFileName(
        this,
        tr("Save Document As"),
        QString(),
        tr("Kalahari Files (*.klh)")
    );

    if (filename.isEmpty()) {
        logger.info("Save As cancelled by user");
        return;
    }

    // Ensure .klh extension
    if (!filename.endsWith(".klh", Qt::CaseInsensitive)) {
        filename += ".klh";
    }

    // Ensure we have a document
    if (!m_currentDocument.has_value()) {
        m_currentDocument = core::Document("Untitled", "User", "en");
    }

    // Get text from editor and update document
    QString text = m_editorPanel->getText();
    setPhase0Content(m_currentDocument.value(), text);

    // Update document title from filename
    std::filesystem::path filepath = filename.toStdString();
    std::string title = filepath.stem().string();
    m_currentDocument->setTitle(title);

    // Save to file
    bool saved = core::DocumentArchive::save(m_currentDocument.value(), filepath);

    if (!saved) {
        QMessageBox::critical(
            this,
            tr("Save Error"),
            tr("Failed to save document: %1").arg(filename)
        );
        logger.error("Failed to save document: {}", filepath.string());
        return;
    }

    // Success - update state
    m_currentFilePath = filepath;
    setDirty(false);
    logger.info("Document saved as: {}", filepath.string());
    statusBar()->showMessage(tr("Document saved as: %1").arg(filename), 2000);
}
```

### Step 8: Update closeEvent() for Unsaved Changes

**File:** `src/gui/main_window.cpp`

Update existing closeEvent() implementation:

```cpp
void MainWindow::closeEvent(QCloseEvent* event) {
    auto& logger = core::Logger::getInstance();
    logger.debug("MainWindow::closeEvent triggered");

    // Check for unsaved changes
    if (m_isDirty) {
        QString filename = m_currentFilePath.empty()
            ? "Untitled"
            : QString::fromStdString(m_currentFilePath.filename().string());

        auto reply = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to %1?").arg(filename),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            onSaveDocument();
            if (m_isDirty) {
                // Save was cancelled or failed
                event->ignore();
                return;
            }
        } else if (reply == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        // Discard → continue with close
    }

    // Save perspective (existing code)
    auto& settings = core::SettingsManager::getInstance();
    settings.set("window.geometry", saveGeometry());
    settings.set("window.state", saveState());
    logger.debug("Window geometry and state saved");

    event->accept();
}
```

### Step 9: Add getTextEdit() Accessor to EditorPanel

**File:** `include/kalahari/gui/panels/editor_panel.h`

Add public method:

```cpp
public:
    // ... existing methods ...

    /// @brief Get underlying QPlainTextEdit widget (for signal connections)
    /// @return Pointer to text edit widget
    ///
    /// WARNING: Use only for signal connections, not for direct manipulation.
    /// Use setText()/getText() for content access.
    QPlainTextEdit* getTextEdit() { return m_textEdit; }
```

### Step 10: Add #include Directives

**File:** `src/gui/main_window.cpp`

Add at top:

```cpp
#include <QFileDialog>
#include <QMessageBox>
#include <filesystem>
#include "kalahari/core/document.h"
#include "kalahari/core/document_archive.h"
#include "kalahari/core/book_element.h"
#include "kalahari/core/part.h"
```

---

## 📂 Files to Modify

1. **include/kalahari/gui/main_window.h**
   - Add: m_currentDocument, m_currentFilePath, m_isDirty
   - Add: setDirty(), updateWindowTitle(), getPhase0Content(), setPhase0Content()

2. **src/gui/main_window.cpp**
   - Initialize new members in constructor
   - Connect textChanged() signal
   - Implement: onNewDocument(), onOpenDocument(), onSaveDocument(), onSaveAsDocument()
   - Update: closeEvent() for unsaved changes dialog
   - Implement: setDirty(), updateWindowTitle(), getPhase0Content(), setPhase0Content()
   - Add includes: QFileDialog, QMessageBox, Document, DocumentArchive

3. **include/kalahari/gui/panels/editor_panel.h**
   - Add: getTextEdit() accessor method

---

## 🔨 Build & Test Plan

### Build:
```cmd
scripts\build_windows.bat Debug
```

### Manual Test Cases:

**Test 1: New Document**
1. Run kalahari.exe
2. File → New
3. Verify: Editor is empty
4. Verify: Title shows "Kalahari - Untitled"
5. Type some text
6. Verify: Title shows "Kalahari - *Untitled"

**Test 2: Save As**
1. Type text in editor
2. File → Save As
3. Save as "test_save.klh"
4. Verify: File created
5. Verify: Title shows "Kalahari - test_save.klh" (no *)
6. Type more text
7. Verify: Title shows "Kalahari - *test_save.klh"

**Test 3: Save**
1. With dirty document, File → Save
2. Verify: "*" disappears from title
3. Close and reopen app
4. Verify: No unsaved changes prompt

**Test 4: Open**
1. File → Open
2. Select test_save.klh
3. Verify: Content loaded correctly
4. Verify: Title shows "Kalahari - test_save.klh"

**Test 5: Unsaved Changes on New**
1. Open document, type text (dirty)
2. File → New
3. Verify: "Unsaved changes?" dialog appears
4. Click Cancel → verify document still loaded
5. File → New again, click Discard → verify new document
6. Type text, File → New, click Save → verify saves then creates new

**Test 6: Unsaved Changes on Open**
1. New document, type text (dirty)
2. File → Open
3. Verify: "Unsaved changes?" dialog appears
4. Test all 3 options (Save, Discard, Cancel)

**Test 7: Unsaved Changes on Close**
1. New document, type text (dirty)
2. Close window (X button)
3. Verify: "Unsaved changes?" dialog appears
4. Click Cancel → verify window doesn't close
5. Close again, click Discard → verify closes without saving
6. Reopen, type text, close, click Save → verify saves then closes

**Test 8: .klh Format Verification**
1. Save document with text content
2. Use unzip -l test.klh → verify manifest.json exists
3. Use unzip -p test.klh manifest.json → verify metadata["_phase0_content"] contains text

---

## 📊 Estimated Timeline

- Step 1 (Header update): 10 minutes
- Step 2 (Constructor): 5 minutes
- Step 3 (Helper methods): 20 minutes
- Step 4 (File → New): 15 minutes
- Step 5 (File → Open): 20 minutes
- Step 6 (File → Save): 15 minutes
- Step 7 (File → Save As): 20 minutes
- Step 8 (closeEvent update): 15 minutes
- Step 9 (EditorPanel accessor): 5 minutes
- Step 10 (Includes): 5 minutes
- Build & test: 45 minutes
- CHANGELOG & ROADMAP update: 10 minutes

**Total:** ~3 hours (within 3-4h estimate)

---

## 🚀 Next Task

After Task #00008 completion:
- **Task #00009:** Edit Operations (Undo/Redo/Cut/Copy/Paste/Select All) - 2-3h
  - QPlainTextEdit has built-in undo/redo
  - Just need to connect menu actions

---

## 📝 Notes

### ATOMIC SCOPE - What's INCLUDED:
- ✅ File → New/Open/Save/Save As implementations
- ✅ .klh format integration (DocumentArchive)
- ✅ Dirty state tracking (textChanged signal)
- ✅ Unsaved changes dialog (closeEvent + menu prompts)

### ATOMIC SCOPE - What's EXCLUDED (other tasks):
- ❌ Edit menu operations (Task #00009)
- ❌ Navigator Panel / Book structure UI (Task #00010)
- ❌ RTF file handling (Phase 1)
- ❌ Multi-chapter support (Phase 1)

### Design Decisions:

1. **Phase 0 Content Storage:**
   - Use metadata["_phase0_content"] to store editor text
   - Create single chapter in Book.body
   - "_phase0_" prefix marks temporary solution
   - Phase 1 will replace with RTF files

2. **Dirty State Tracking:**
   - Connect to QPlainTextEdit::textChanged() signal
   - Don't mark dirty on programmatic setText()
   - Show "*" in window title when dirty

3. **Unsaved Changes Dialog:**
   - Prompt in 3 scenarios: New, Open, Close
   - QMessageBox::question with Save/Discard/Cancel
   - Save → call onSaveDocument(), check if dirty cleared

4. **Window Title Format:**
   - No file: "Kalahari - Untitled"
   - Clean file: "Kalahari - filename.klh"
   - Dirty file: "Kalahari - *filename.klh"

5. **Save vs Save As:**
   - Save with no file path → delegate to Save As
   - Save As updates m_currentFilePath
   - Save As sets document title from filename

### Qt6 APIs Used:
- `QFileDialog::getOpenFileName()` - Open file dialog
- `QFileDialog::getSaveFileName()` - Save file dialog
- `QMessageBox::question()` - Unsaved changes prompts
- `QPlainTextEdit::textChanged()` - Dirty state detection
- `QMainWindow::closeEvent()` - Handle window close
- `std::filesystem::path` - Cross-platform file paths
