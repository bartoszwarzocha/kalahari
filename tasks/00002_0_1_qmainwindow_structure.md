# Task #00002: QMainWindow Structure with Menus and Toolbars

**Phase:** 0 (Qt Foundation)
**Week:** 1 (Hello World & Structure)
**Zagadnienie:** 0.1 (Qt6 Integration)
**Estimated Time:** 16 hours (Days 2-3 of Week 1)
**Priority:** HIGH
**Dependencies:** Task #00001 (Qt6 Hello World)
**Status:** 🚀 IN PROGRESS (PLAN phase)

---

## 📋 Objective

Create a proper QMainWindow subclass (`MainWindow`) with:
- File and Edit menus (functional actions)
- File toolbar with icons
- Status bar
- Signal/slot connections (actions log to console)
- Proper Qt patterns (Q_OBJECT macro, createXxx() methods)

**Success Criteria:** Application shows structured window with working menus/toolbar/statusbar, all actions connected and logging to console.

---

## 🔍 Context

### Current State (After Task #00001)
- ✅ Qt6 configured (Core, Widgets, Gui components)
- ✅ Empty QMainWindow in `src/main.cpp` (1280x720 px)
- ✅ LGPL v3 compliance verified (dynamic linking)
- ✅ Event loop functional (`app.exec()`)

### Required Changes
- Create `MainWindow` class (replaces inline QMainWindow in main.cpp)
- Implement menu system (File, Edit menus)
- Add toolbar with icons
- Add status bar with ready message
- Connect actions to slots (log on click)

### Files to Create/Modify
1. ✅ **Create:** `include/kalahari/gui/main_window.h` (MainWindow class declaration)
2. ✅ **Create:** `src/gui/main_window.cpp` (MainWindow implementation)
3. ✅ **Modify:** `src/main.cpp` (use MainWindow instead of QMainWindow)
4. ✅ **Modify:** `src/CMakeLists.txt` (add gui/ sources to KALAHARI_SOURCES)

---

## 🏗️ Technical Analysis

### Directory Structure Changes

**Before:**
```
include/kalahari/
├── core/
├── presenters/
├── resources/
└── services/

src/
├── bindings/
├── core/
├── main.cpp
├── presenters/
└── services/
```

**After:**
```
include/kalahari/
├── core/
├── gui/           ← NEW
│   └── main_window.h
├── presenters/
├── resources/
└── services/

src/
├── bindings/
├── core/
├── gui/           ← NEW
│   └── main_window.cpp
├── main.cpp
├── presenters/
└── services/
```

### MainWindow Class Design

**Key Qt Patterns:**
- Inherit from `QMainWindow`
- Use `Q_OBJECT` macro (required for signals/slots!)
- Private helper methods: `createActions()`, `createMenus()`, `createToolbars()`, `createStatusBar()`
- Store actions/menus as member variables (`m_newAction`, `m_fileMenu`, etc.)

**Actions Needed:**
- **File Menu:** New, Open, Save, Save As, Exit
- **Edit Menu:** Undo, Redo, Cut, Copy, Paste
- **Toolbar:** New, Open, Save (with icons)

**Slots:**
- Each action connects to a slot (e.g., `onNewDocument()`)
- Slots log action to console: `logger.info("Action triggered: {}", actionName)`

### CMake Integration

**Current KALAHARI_SOURCES (lines 84-86):**
```cmake
set(KALAHARI_SOURCES
    main.cpp
)
```

**After Task #00002:**
```cmake
set(KALAHARI_SOURCES
    main.cpp
    gui/main_window.cpp
)
```

**Note:** Headers automatically found via `target_include_directories(kalahari PRIVATE ${CMAKE_SOURCE_DIR}/include)`

---

## 📝 Implementation Plan

### Step 1: Create Header File (include/kalahari/gui/main_window.h)

```cpp
/// @file main_window.h
/// @brief Main application window (Qt6 QMainWindow subclass)
///
/// This file defines the MainWindow class, which is the primary GUI window
/// for Kalahari Writer's IDE. It manages menus, toolbars, status bar, and
/// dockable panels.

#pragma once

#include <QMainWindow>
#include <QAction>
#include <QMenu>
#include <QToolBar>

namespace kalahari {
namespace gui {

/// @brief Main application window
///
/// MainWindow is the top-level QMainWindow subclass for Kalahari.
/// It provides:
/// - File and Edit menus
/// - Toolbar with common actions
/// - Status bar
/// - Signal/slot connections for actions
///
/// Example usage:
/// @code
/// QApplication app(argc, argv);
/// MainWindow window;
/// window.show();
/// return app.exec();
/// @endcode
class MainWindow : public QMainWindow {
    Q_OBJECT  // Required for signals/slots!

public:
    /// @brief Constructor
    /// @param parent Parent widget (nullptr for top-level window)
    explicit MainWindow(QWidget* parent = nullptr);

    /// @brief Destructor
    ~MainWindow() override = default;

private:
    /// @brief Create all QAction objects
    ///
    /// Initializes actions for File and Edit menus with:
    /// - Display names
    /// - Keyboard shortcuts
    /// - Status tip text
    /// - Icons (if available)
    void createActions();

    /// @brief Create menu bar with File and Edit menus
    ///
    /// Adds actions to menus and sets up menu structure.
    void createMenus();

    /// @brief Create main toolbar
    ///
    /// Adds File actions (New, Open, Save) to toolbar with icons.
    void createToolbars();

    /// @brief Create status bar
    ///
    /// Shows "Ready" message on application start.
    void createStatusBar();

private slots:
    /// @brief Slot for File > New action
    void onNewDocument();

    /// @brief Slot for File > Open action
    void onOpenDocument();

    /// @brief Slot for File > Save action
    void onSaveDocument();

    /// @brief Slot for File > Save As action
    void onSaveAsDocument();

    /// @brief Slot for File > Exit action
    void onExit();

    /// @brief Slot for Edit > Undo action
    void onUndo();

    /// @brief Slot for Edit > Redo action
    void onRedo();

    /// @brief Slot for Edit > Cut action
    void onCut();

    /// @brief Slot for Edit > Copy action
    void onCopy();

    /// @brief Slot for Edit > Paste action
    void onPaste();

private:
    // Actions
    QAction* m_newAction;
    QAction* m_openAction;
    QAction* m_saveAction;
    QAction* m_saveAsAction;
    QAction* m_exitAction;

    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_cutAction;
    QAction* m_copyAction;
    QAction* m_pasteAction;

    // Menus
    QMenu* m_fileMenu;
    QMenu* m_editMenu;

    // Toolbars
    QToolBar* m_fileToolbar;
};

} // namespace gui
} // namespace kalahari
```

### Step 2: Create Implementation File (src/gui/main_window.cpp)

```cpp
/// @file main_window.cpp
/// @brief Main application window implementation

#include "kalahari/gui/main_window.h"
#include "kalahari/core/logger.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QApplication>

namespace kalahari {
namespace gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_newAction(nullptr)
    , m_openAction(nullptr)
    , m_saveAction(nullptr)
    , m_saveAsAction(nullptr)
    , m_exitAction(nullptr)
    , m_undoAction(nullptr)
    , m_redoAction(nullptr)
    , m_cutAction(nullptr)
    , m_copyAction(nullptr)
    , m_pasteAction(nullptr)
    , m_fileMenu(nullptr)
    , m_editMenu(nullptr)
    , m_fileToolbar(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.info("MainWindow constructor called");

    // Set window properties
    setWindowTitle("Kalahari Writer's IDE");
    resize(1280, 720);

    // Create UI components
    createActions();
    createMenus();
    createToolbars();
    createStatusBar();

    logger.info("MainWindow initialized successfully");
}

void MainWindow::createActions() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating actions");

    // File actions
    m_newAction = new QAction(tr("&New"), this);
    m_newAction->setShortcut(QKeySequence::New);
    m_newAction->setStatusTip(tr("Create a new document"));
    connect(m_newAction, &QAction::triggered, this, &MainWindow::onNewDocument);

    m_openAction = new QAction(tr("&Open..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setStatusTip(tr("Open an existing document"));
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpenDocument);

    m_saveAction = new QAction(tr("&Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setStatusTip(tr("Save the current document"));
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveDocument);

    m_saveAsAction = new QAction(tr("Save &As..."), this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setStatusTip(tr("Save the current document with a new name"));
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAsDocument);

    m_exitAction = new QAction(tr("E&xit"), this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setStatusTip(tr("Exit the application"));
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::onExit);

    // Edit actions
    m_undoAction = new QAction(tr("&Undo"), this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setStatusTip(tr("Undo the last operation"));
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndo);

    m_redoAction = new QAction(tr("&Redo"), this);
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setStatusTip(tr("Redo the last undone operation"));
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    m_cutAction = new QAction(tr("Cu&t"), this);
    m_cutAction->setShortcut(QKeySequence::Cut);
    m_cutAction->setStatusTip(tr("Cut the selection to clipboard"));
    connect(m_cutAction, &QAction::triggered, this, &MainWindow::onCut);

    m_copyAction = new QAction(tr("&Copy"), this);
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_copyAction->setStatusTip(tr("Copy the selection to clipboard"));
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::onCopy);

    m_pasteAction = new QAction(tr("&Paste"), this);
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_pasteAction->setStatusTip(tr("Paste from clipboard"));
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::onPaste);

    logger.debug("Actions created successfully");
}

void MainWindow::createMenus() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating menus");

    // File menu
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileMenu->addAction(m_newAction);
    m_fileMenu->addAction(m_openAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_saveAction);
    m_fileMenu->addAction(m_saveAsAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAction);

    // Edit menu
    m_editMenu = menuBar()->addMenu(tr("&Edit"));
    m_editMenu->addAction(m_undoAction);
    m_editMenu->addAction(m_redoAction);
    m_editMenu->addSeparator();
    m_editMenu->addAction(m_cutAction);
    m_editMenu->addAction(m_copyAction);
    m_editMenu->addAction(m_pasteAction);

    logger.debug("Menus created successfully");
}

void MainWindow::createToolbars() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating toolbars");

    // File toolbar
    m_fileToolbar = addToolBar(tr("File"));
    m_fileToolbar->addAction(m_newAction);
    m_fileToolbar->addAction(m_openAction);
    m_fileToolbar->addAction(m_saveAction);

    logger.debug("Toolbars created successfully");
}

void MainWindow::createStatusBar() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating status bar");

    statusBar()->showMessage(tr("Ready"), 3000);  // Show for 3 seconds

    logger.debug("Status bar created successfully");
}

// Slots implementation
void MainWindow::onNewDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: New Document");
    statusBar()->showMessage(tr("New document created"), 2000);
}

void MainWindow::onOpenDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Open Document");
    statusBar()->showMessage(tr("Open document dialog (not implemented)"), 2000);
}

void MainWindow::onSaveDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save Document");
    statusBar()->showMessage(tr("Document saved (not implemented)"), 2000);
}

void MainWindow::onSaveAsDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save As Document");
    statusBar()->showMessage(tr("Save As dialog (not implemented)"), 2000);
}

void MainWindow::onExit() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Exit");
    QApplication::quit();
}

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

} // namespace gui
} // namespace kalahari
```

### Step 3: Update src/main.cpp

**Current (lines 1-35):**
```cpp
/// @file main.cpp
/// @brief Kalahari Writer's IDE - Main entry point (Qt6)
///
/// Task #00001: Qt6 Hello World - Minimal QMainWindow

#include <QApplication>
#include <QMainWindow>
#include "core/logger.h"
#include "core/settings_manager.h"

int main(int argc, char *argv[]) {
    // Initialize Qt application
    QApplication app(argc, argv);
    app.setApplicationName("Kalahari");
    app.setOrganizationName("Bartosz W. Warzocha & Kalahari Team");
    app.setApplicationVersion("0.3.0-alpha");

    // Initialize core systems
    kalahari::core::Logger::initialize("kalahari.log");
    kalahari::core::SettingsManager::initialize("settings.json");

    auto& logger = kalahari::core::Logger::getInstance();
    logger.info("Kalahari {} starting (Qt6 Hello World)", app.applicationVersion().toStdString());

    // Create minimal main window
    QMainWindow window;
    window.setWindowTitle("Kalahari Writer's IDE");
    window.resize(1280, 720);
    window.show();

    logger.info("Main window shown - entering event loop");

    // Enter Qt event loop
    int result = app.exec();

    logger.info("Application exited with code: {}", result);
    return result;
}
```

**After Task #00002:**
```cpp
/// @file main.cpp
/// @brief Kalahari Writer's IDE - Main entry point (Qt6)
///
/// Task #00002: QMainWindow Structure with Menus and Toolbars

#include <QApplication>
#include "kalahari/gui/main_window.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"

int main(int argc, char *argv[]) {
    // Initialize Qt application
    QApplication app(argc, argv);
    app.setApplicationName("Kalahari");
    app.setOrganizationName("Bartosz W. Warzocha & Kalahari Team");
    app.setApplicationVersion("0.3.0-alpha");

    // Initialize core systems
    kalahari::core::Logger::initialize("kalahari.log");
    kalahari::core::SettingsManager::initialize("settings.json");

    auto& logger = kalahari::core::Logger::getInstance();
    logger.info("Kalahari {} starting", app.applicationVersion().toStdString());

    // Create main window with menus/toolbars
    kalahari::gui::MainWindow window;
    window.show();

    logger.info("Main window shown - entering event loop");

    // Enter Qt event loop
    int result = app.exec();

    logger.info("Application exited with code: {}", result);
    return result;
}
```

**Changes:**
- Line 4: Updated comment to Task #00002
- Line 7: `#include "kalahari/gui/main_window.h"` (new header)
- Line 7: Removed `#include <QMainWindow>` (no longer needed directly)
- Line 23: Removed "(Qt6 Hello World)" from log message
- Lines 26-29: `QMainWindow window;` → `kalahari::gui::MainWindow window;`
- Lines 26-29: Removed window properties (now in MainWindow constructor)

### Step 4: Update src/CMakeLists.txt

**Current (lines 84-86):**
```cmake
set(KALAHARI_SOURCES
    main.cpp
)
```

**After Task #00002:**
```cmake
set(KALAHARI_SOURCES
    main.cpp
    gui/main_window.cpp
)
```

**Change:** Add `gui/main_window.cpp` to sources list.

---

## ✅ Acceptance Criteria

### Functional Requirements
- [ ] **AC-001:** Application launches with MainWindow (1280x720 px)
- [ ] **AC-002:** File menu visible with 5 actions (New, Open, Save, Save As, Exit)
- [ ] **AC-003:** Edit menu visible with 5 actions (Undo, Redo, Cut, Copy, Paste)
- [ ] **AC-004:** File toolbar visible with 3 buttons (New, Open, Save)
- [ ] **AC-005:** Status bar visible with "Ready" message (3 seconds)
- [ ] **AC-006:** All menu actions trigger (verified in console log)
- [ ] **AC-007:** Status bar updates on action click (shows action name)
- [ ] **AC-008:** Exit action closes application cleanly
- [ ] **AC-009:** Keyboard shortcuts work (Ctrl+N, Ctrl+O, Ctrl+S, etc.)

### Technical Requirements
- [ ] **AC-010:** `include/kalahari/gui/main_window.h` created (Q_OBJECT macro present)
- [ ] **AC-011:** `src/gui/main_window.cpp` created (all slots implemented)
- [ ] **AC-012:** `src/main.cpp` updated (uses MainWindow class)
- [ ] **AC-013:** `src/CMakeLists.txt` updated (gui/main_window.cpp added)
- [ ] **AC-014:** Logger outputs action triggers (visible in console)
- [ ] **AC-015:** No compiler warnings
- [ ] **AC-016:** No linker errors

### Verification Requirements
- [ ] **AC-017:** CI/CD passes (Linux, macOS, Windows)
- [ ] **AC-018:** Executable runs without crashes
- [ ] **AC-019:** No memory leaks (Qt handles QAction/QMenu cleanup automatically)

---

## 🧪 Test Cases

### TC-001: Window Appearance
**Precondition:** Application launched
**Steps:**
1. Run `./build-windows/bin/kalahari.exe` (or Linux/macOS equivalent)
2. Observe window appearance

**Expected:**
- Window title: "Kalahari Writer's IDE"
- Window size: 1280x720 px
- Menu bar visible with "File" and "Edit" menus
- Toolbar visible with 3 buttons
- Status bar visible with "Ready" message

**Pass Criteria:** All elements visible and correctly positioned

---

### TC-002: File Menu Actions
**Precondition:** Application running
**Steps:**
1. Click "File" menu
2. Click "New"
3. Observe console log
4. Repeat for "Open", "Save", "Save As"

**Expected:**
- Console log shows: `[info] Action triggered: New Document`
- Status bar shows: "New document created" (2 seconds)
- Same for other actions (appropriate messages)

**Pass Criteria:** All actions log correctly and status bar updates

---

### TC-003: Edit Menu Actions
**Precondition:** Application running
**Steps:**
1. Click "Edit" menu
2. Click "Undo"
3. Observe console log
4. Repeat for "Redo", "Cut", "Copy", "Paste"

**Expected:**
- Console log shows: `[info] Action triggered: Undo`
- Status bar shows: "Undo (not implemented)" (2 seconds)
- Same for other actions

**Pass Criteria:** All actions log correctly and status bar updates

---

### TC-004: Keyboard Shortcuts
**Precondition:** Application running, focus on main window
**Steps:**
1. Press Ctrl+N
2. Observe console log and status bar
3. Repeat for Ctrl+O, Ctrl+S, Ctrl+Shift+S, Ctrl+Q

**Expected:**
- Ctrl+N triggers "New Document"
- Ctrl+O triggers "Open Document"
- Ctrl+S triggers "Save Document"
- Ctrl+Shift+S triggers "Save As Document"
- Ctrl+Q triggers "Exit" (application closes)

**Pass Criteria:** All shortcuts work correctly

---

### TC-005: Exit Action
**Precondition:** Application running
**Steps:**
1. Click "File" > "Exit"
2. Observe application behavior

**Expected:**
- Console log shows: `[info] Action triggered: Exit`
- Console log shows: `[info] Application exited with code: 0`
- Application closes cleanly

**Pass Criteria:** Application exits without crashes or errors

---

## 🚀 Execution Steps

1. **Create directories:**
   ```bash
   mkdir -p include/kalahari/gui
   mkdir -p src/gui
   ```

2. **Create files:**
   - Write `include/kalahari/gui/main_window.h` (Step 1 content)
   - Write `src/gui/main_window.cpp` (Step 2 content)

3. **Update existing files:**
   - Modify `src/main.cpp` (Step 3 changes)
   - Modify `src/CMakeLists.txt` (Step 4 changes)

4. **Build and test:**
   ```bash
   cmake --build build-windows --config Debug
   ./build-windows/bin/kalahari.exe
   ```

5. **Verify all acceptance criteria**

6. **Update documentation:**
   - Mark task as DONE in this file
   - Update CHANGELOG.md ([Unreleased] section)
   - Git commit with message: `feat: Task #00002 - QMainWindow structure with menus/toolbars`

---

## 📊 Implementation Summary

**Status:** ✅ DONE (Implementation complete, awaiting CI/CD verification)
**Files Created:** 2/2
  - ✅ include/kalahari/gui/main_window.h (127 lines)
  - ✅ src/gui/main_window.cpp (227 lines)
**Files Modified:** 2/2
  - ✅ src/main.cpp (updated to use MainWindow class)
  - ✅ src/CMakeLists.txt (added gui/main_window.cpp)
**Acceptance Criteria:** Awaiting CI/CD verification (19 criteria)
**Test Cases:** Awaiting CI/CD build (5 test cases)

**Implementation Date:** 2025-11-20
**Verification:** CI/CD build required (Linux, macOS, Windows)

---

## 📝 Notes

### Qt6 Patterns Used
- **Q_OBJECT macro:** Required for signals/slots (moc must process this file)
- **tr() function:** Enables internationalization (Qt Linguist support)
- **QKeySequence:** Standard shortcuts (Ctrl+N, Ctrl+O, etc.)
- **connect():** Modern syntax with lambdas or member function pointers
- **QAction ownership:** Qt parent/child system handles cleanup automatically

### Future Enhancements (Not in This Task)
- Icons for actions (requires resource system - Task #00004)
- Recent Files menu (Phase 1 Week 2)
- Dock widgets for panels (Task #00003)
- Settings dialog (Phase 1 Week 2)

### References
- Qt6 Documentation: https://doc.qt.io/qt-6/qmainwindow.html
- Qt6 Actions and Menus: https://doc.qt.io/qt-6/qtwidgets-mainwindows-menus-example.html
- Kalahari QT_MIGRATION_ROADMAP.md: Lines 758-857

---

**Created:** 2025-11-20
**Last Updated:** 2025-11-20
**Author:** Claude (AI Assistant)
**Approved By:** [Awaiting user approval]
