# Task #00003: Basic QDockWidget System

**Phase:** 0 (Qt Foundation)
**Week:** 1 (Hello World & Structure)
**Zagadnienie:** 0.1 (Qt6 Integration)
**Estimated Time:** 16 hours (Days 4-5 of Week 1)
**Priority:** HIGH
**Dependencies:** Task #00002 (QMainWindow Structure)
**Status:** 🚀 IN PROGRESS (PLAN phase)

---

## 📋 Objective

Replace wxAuiManager with Qt's native QDockWidget system:
- 6 dockable panel placeholders (Navigator, Properties, Log, Search, Assistant, Editor)
- Drag & drop docking (Qt native functionality)
- View menu for toggling panels
- Perspective save/restore (QSettings)
- Sensible default layout

**Success Criteria:** Application shows multi-panel layout with draggable/resizable docks, panels can be closed/reopened via View menu, perspective persists across sessions.

---

## 🔍 Context

### Current State (After Task #00002)
- ✅ MainWindow with menus/toolbar/statusbar
- ✅ File and Edit menus functional
- ✅ Empty central area (no panels yet)

### Required Changes
- Create 6 panel placeholder classes (QWidget subclasses)
- Add QDockWidget instances to MainWindow
- Create View menu for panel visibility toggles
- Implement perspective save/restore (QSettings)
- Set up default layout (Navigator left, Properties right, Log bottom, Editor center)

### Files to Create/Modify
1. ✅ **Create:** `include/kalahari/gui/panels/editor_panel.h` (EditorPanel class)
2. ✅ **Create:** `src/gui/panels/editor_panel.cpp` (placeholder implementation)
3. ✅ **Create:** `include/kalahari/gui/panels/navigator_panel.h` (NavigatorPanel class)
4. ✅ **Create:** `src/gui/panels/navigator_panel.cpp` (placeholder implementation)
5. ✅ **Create:** `include/kalahari/gui/panels/properties_panel.h` (PropertiesPanel class)
6. ✅ **Create:** `src/gui/panels/properties_panel.cpp` (placeholder implementation)
7. ✅ **Create:** `include/kalahari/gui/panels/search_panel.h` (SearchPanel class)
8. ✅ **Create:** `src/gui/panels/search_panel.cpp` (placeholder implementation)
9. ✅ **Create:** `include/kalahari/gui/panels/assistant_panel.h` (AssistantPanel class)
10. ✅ **Create:** `src/gui/panels/assistant_panel.cpp` (placeholder implementation)
11. ✅ **Create:** `include/kalahari/gui/panels/log_panel.h` (LogPanel class)
12. ✅ **Create:** `src/gui/panels/log_panel.cpp` (placeholder implementation)
13. ✅ **Modify:** `include/kalahari/gui/main_window.h` (add dock management)
14. ✅ **Modify:** `src/gui/main_window.cpp` (implement docks + View menu)
15. ✅ **Modify:** `src/CMakeLists.txt` (add panel sources)

---

## 🏗️ Technical Analysis

### Directory Structure Changes

**Before:**
```
include/kalahari/gui/
└── main_window.h

src/gui/
└── main_window.cpp
```

**After:**
```
include/kalahari/gui/
├── main_window.h
└── panels/           ← NEW
    ├── editor_panel.h
    ├── navigator_panel.h
    ├── properties_panel.h
    ├── search_panel.h
    ├── assistant_panel.h
    └── log_panel.h

src/gui/
├── main_window.cpp
└── panels/           ← NEW
    ├── editor_panel.cpp
    ├── navigator_panel.cpp
    ├── properties_panel.cpp
    ├── search_panel.cpp
    ├── assistant_panel.cpp
    └── log_panel.cpp
```

### Panel Placeholder Design

**All panels inherit from QWidget** (not QDockWidget - that's the container!)

**Simple placeholder pattern:**
```cpp
class EditorPanel : public QWidget {
    Q_OBJECT
public:
    explicit EditorPanel(QWidget* parent = nullptr);
private:
    QPlainTextEdit* m_textEdit;  // Placeholder
};
```

**Each panel:**
- Simple UI (label or basic widget)
- No complex functionality yet (Phase 1+ will add real features)
- Properly parented to QDockWidget

### QDockWidget Layout

**Default layout:**
```
┌─────────────────────────────────────────────────────┐
│ Kalahari Writer's IDE                     [File][Edit][View]
├───────────┬─────────────────────────┬───────────────┤
│           │                         │               │
│ Navigator │      Editor Panel       │  Properties   │
│ (QTree)   │    (QPlainTextEdit)     │   (QLabel)    │
│           │                         │               │
│           │                         │               │
│           ├─────────────────────────┤               │
│           │  Search Panel (QEdit)   │               │
│           ├─────────────────────────┤               │
│           │ Assistant (QLabel)      │               │
├───────────┴─────────────────────────┴───────────────┤
│ Log Panel (QPlainTextEdit)                          │
├─────────────────────────────────────────────────────┤
│ Status Bar                                          │
└─────────────────────────────────────────────────────┘
```

**Dock Areas:**
- Navigator: Qt::LeftDockWidgetArea
- Properties: Qt::RightDockWidgetArea
- Log: Qt::BottomDockWidgetArea
- Search: Qt::RightDockWidgetArea (tabbed with Properties)
- Assistant: Qt::RightDockWidgetArea (tabbed with Properties/Search)
- Editor: Central widget (setCentralWidget)

### QSettings Perspective Management

**Key methods:**
- `QMainWindow::saveGeometry()` - returns QByteArray with window geometry
- `QMainWindow::saveState()` - returns QByteArray with dock layout
- `QMainWindow::restoreGeometry(QByteArray)` - restores window geometry
- `QMainWindow::restoreState(QByteArray)` - restores dock layout

**Implementation:**
```cpp
// Save on close
void MainWindow::closeEvent(QCloseEvent* event) {
    QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    QMainWindow::closeEvent(event);
}

// Restore on show
void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}
```

### View Menu Design

**View menu actions:**
- ☑ Navigator (Ctrl+1)
- ☑ Properties (Ctrl+2)
- ☑ Log (Ctrl+3)
- ☑ Search (Ctrl+4)
- ☑ Assistant (Ctrl+5)
- [Separator]
- Reset Layout (Ctrl+0)

**Toggle actions:**
- Checkable actions (QAction::setCheckable(true))
- Connected to QDockWidget::setVisible()
- Bidirectional sync: dock close → uncheck action

---

## 📝 Implementation Plan

### Step 1: Create Panel Header Files (6 files)

All panels follow the same pattern. Example for EditorPanel:

**include/kalahari/gui/panels/editor_panel.h:**
```cpp
/// @file editor_panel.h
/// @brief Editor panel placeholder (Qt6)
///
/// This file defines the EditorPanel class, a placeholder for the
/// text editor widget. Will be enhanced in Phase 1 with real editing.

#pragma once

#include <QWidget>

class QPlainTextEdit;

namespace kalahari {
namespace gui {

/// @brief Editor panel (placeholder)
///
/// Displays a simple QPlainTextEdit for text editing.
/// This is a placeholder - full editor implementation comes in Phase 1.
class EditorPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit EditorPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~EditorPanel() override = default;

private:
    QPlainTextEdit* m_textEdit;
};

} // namespace gui
} // namespace kalahari
```

**Other panels (same pattern, different widgets):**
- NavigatorPanel: QTreeWidget (chapter/scene tree)
- PropertiesPanel: QLabel "Properties Panel (placeholder)"
- SearchPanel: QLineEdit + QListWidget
- AssistantPanel: QLabel "Assistant Panel (placeholder)"
- LogPanel: QPlainTextEdit (colored log output)

### Step 2: Create Panel Implementation Files (6 files)

**src/gui/panels/editor_panel.cpp:**
```cpp
/// @file editor_panel.cpp
/// @brief Editor panel implementation

#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/core/logger.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

EditorPanel::EditorPanel(QWidget* parent)
    : QWidget(parent)
    , m_textEdit(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("EditorPanel constructor called");

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create text edit widget
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setPlaceholderText("Editor Panel (placeholder - full implementation in Phase 1)");
    layout->addWidget(m_textEdit);

    setLayout(layout);

    logger.debug("EditorPanel initialized");
}

} // namespace gui
} // namespace kalahari
```

**Other panels (similar structure, different widgets)**

### Step 3: Modify MainWindow Header

**include/kalahari/gui/main_window.h additions:**

```cpp
// Add to includes:
#include <QDockWidget>
#include <QCloseEvent>
#include <QShowEvent>

// Add forward declarations:
class EditorPanel;
class NavigatorPanel;
class PropertiesPanel;
class SearchPanel;
class AssistantPanel;
class LogPanel;

// Add to private section:
private:
    /// @brief Create dockable panels
    ///
    /// Creates 6 dock widgets and sets up default layout.
    void createDocks();

    /// @brief Reset dock layout to default
    void resetLayout();

protected:
    /// @brief Save perspective on close
    /// @param event Close event
    void closeEvent(QCloseEvent* event) override;

    /// @brief Restore perspective on show
    /// @param event Show event
    void showEvent(QShowEvent* event) override;

private:
    // View menu
    QMenu* m_viewMenu;

    // View actions (panel toggles)
    QAction* m_viewNavigatorAction;
    QAction* m_viewPropertiesAction;
    QAction* m_viewLogAction;
    QAction* m_viewSearchAction;
    QAction* m_viewAssistantAction;
    QAction* m_resetLayoutAction;

    // Dock widgets
    QDockWidget* m_navigatorDock;
    QDockWidget* m_propertiesDock;
    QDockWidget* m_logDock;
    QDockWidget* m_searchDock;
    QDockWidget* m_assistantDock;

    // Panels (widgets inside docks)
    EditorPanel* m_editorPanel;
    NavigatorPanel* m_navigatorPanel;
    PropertiesPanel* m_propertiesPanel;
    SearchPanel* m_searchPanel;
    AssistantPanel* m_assistantPanel;
    LogPanel* m_logPanel;

    // First show flag (for geometry restore)
    bool m_firstShow;
```

### Step 4: Modify MainWindow Implementation

**src/gui/main_window.cpp changes:**

1. **Include panel headers:**
```cpp
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/search_panel.h"
#include "kalahari/gui/panels/assistant_panel.h"
#include "kalahari/gui/panels/log_panel.h"
#include <QSettings>
#include <QDockWidget>
```

2. **Add to constructor:**
```cpp
// After createStatusBar():
createDocks();
```

3. **Initialize dock pointers in constructor initializer list:**
```cpp
, m_viewMenu(nullptr)
, m_viewNavigatorAction(nullptr)
, m_viewPropertiesAction(nullptr)
, m_viewLogAction(nullptr)
, m_viewSearchAction(nullptr)
, m_viewAssistantAction(nullptr)
, m_resetLayoutAction(nullptr)
, m_navigatorDock(nullptr)
, m_propertiesDock(nullptr)
, m_logDock(nullptr)
, m_searchDock(nullptr)
, m_assistantDock(nullptr)
, m_editorPanel(nullptr)
, m_navigatorPanel(nullptr)
, m_propertiesPanel(nullptr)
, m_searchPanel(nullptr)
, m_assistantPanel(nullptr)
, m_logPanel(nullptr)
, m_firstShow(true)
```

4. **Implement createDocks():**
```cpp
void MainWindow::createDocks() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating dock widgets");

    // Create central editor panel
    m_editorPanel = new EditorPanel(this);
    setCentralWidget(m_editorPanel);

    // Navigator dock (left)
    m_navigatorPanel = new NavigatorPanel(this);
    m_navigatorDock = new QDockWidget(tr("Navigator"), this);
    m_navigatorDock->setWidget(m_navigatorPanel);
    m_navigatorDock->setObjectName("NavigatorDock");  // Required for saveState!
    addDockWidget(Qt::LeftDockWidgetArea, m_navigatorDock);

    // Properties dock (right)
    m_propertiesPanel = new PropertiesPanel(this);
    m_propertiesDock = new QDockWidget(tr("Properties"), this);
    m_propertiesDock->setWidget(m_propertiesPanel);
    m_propertiesDock->setObjectName("PropertiesDock");
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // Log dock (bottom)
    m_logPanel = new LogPanel(this);
    m_logDock = new QDockWidget(tr("Log"), this);
    m_logDock->setWidget(m_logPanel);
    m_logDock->setObjectName("LogDock");
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    // Search dock (right, tabbed with Properties)
    m_searchPanel = new SearchPanel(this);
    m_searchDock = new QDockWidget(tr("Search"), this);
    m_searchDock->setWidget(m_searchPanel);
    m_searchDock->setObjectName("SearchDock");
    addDockWidget(Qt::RightDockWidgetArea, m_searchDock);
    tabifyDockWidget(m_propertiesDock, m_searchDock);

    // Assistant dock (right, tabbed with Properties/Search)
    m_assistantPanel = new AssistantPanel(this);
    m_assistantDock = new QDockWidget(tr("Assistant"), this);
    m_assistantDock->setWidget(m_assistantPanel);
    m_assistantDock->setObjectName("AssistantDock");
    addDockWidget(Qt::RightDockWidgetArea, m_assistantDock);
    tabifyDockWidget(m_searchDock, m_assistantDock);

    // Raise Properties tab (default visible)
    m_propertiesDock->raise();

    // Create View menu
    m_viewMenu = menuBar()->addMenu(tr("&View"));

    // Create toggle actions
    m_viewNavigatorAction = m_navigatorDock->toggleViewAction();
    m_viewNavigatorAction->setShortcut(QKeySequence(tr("Ctrl+1")));
    m_viewMenu->addAction(m_viewNavigatorAction);

    m_viewPropertiesAction = m_propertiesDock->toggleViewAction();
    m_viewPropertiesAction->setShortcut(QKeySequence(tr("Ctrl+2")));
    m_viewMenu->addAction(m_viewPropertiesAction);

    m_viewLogAction = m_logDock->toggleViewAction();
    m_viewLogAction->setShortcut(QKeySequence(tr("Ctrl+3")));
    m_viewMenu->addAction(m_viewLogAction);

    m_viewSearchAction = m_searchDock->toggleViewAction();
    m_viewSearchAction->setShortcut(QKeySequence(tr("Ctrl+4")));
    m_viewMenu->addAction(m_viewSearchAction);

    m_viewAssistantAction = m_assistantDock->toggleViewAction();
    m_viewAssistantAction->setShortcut(QKeySequence(tr("Ctrl+5")));
    m_viewMenu->addAction(m_viewAssistantAction);

    m_viewMenu->addSeparator();

    // Reset layout action
    m_resetLayoutAction = new QAction(tr("Reset Layout"), this);
    m_resetLayoutAction->setShortcut(QKeySequence(tr("Ctrl+0")));
    connect(m_resetLayoutAction, &QAction::triggered, this, &MainWindow::resetLayout);
    m_viewMenu->addAction(m_resetLayoutAction);

    logger.debug("Dock widgets created successfully");
}
```

5. **Implement resetLayout():**
```cpp
void MainWindow::resetLayout() {
    auto& logger = core::Logger::getInstance();
    logger.info("Resetting dock layout to default");

    // Remove all docks
    removeDockWidget(m_navigatorDock);
    removeDockWidget(m_propertiesDock);
    removeDockWidget(m_logDock);
    removeDockWidget(m_searchDock);
    removeDockWidget(m_assistantDock);

    // Re-add in default layout
    addDockWidget(Qt::LeftDockWidgetArea, m_navigatorDock);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    addDockWidget(Qt::RightDockWidgetArea, m_searchDock);
    addDockWidget(Qt::RightDockWidgetArea, m_assistantDock);

    // Tab right-side docks
    tabifyDockWidget(m_propertiesDock, m_searchDock);
    tabifyDockWidget(m_searchDock, m_assistantDock);

    // Raise Properties tab
    m_propertiesDock->raise();

    // Show all docks
    m_navigatorDock->show();
    m_propertiesDock->show();
    m_logDock->show();
    m_searchDock->show();
    m_assistantDock->show();

    statusBar()->showMessage(tr("Layout reset to default"), 2000);
}
```

6. **Implement perspective save/restore:**
```cpp
void MainWindow::closeEvent(QCloseEvent* event) {
    auto& logger = core::Logger::getInstance();
    logger.debug("Saving window perspective");

    QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    logger.debug("Window perspective saved");

    QMainWindow::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);

    if (m_firstShow) {
        auto& logger = core::Logger::getInstance();
        logger.debug("Restoring window perspective");

        QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
        restoreGeometry(settings.value("geometry").toByteArray());
        restoreState(settings.value("windowState").toByteArray());

        logger.debug("Window perspective restored");

        m_firstShow = false;
    }
}
```

### Step 5: Update src/CMakeLists.txt

**Add panel sources to KALAHARI_SOURCES:**
```cmake
set(KALAHARI_SOURCES
    main.cpp
    gui/main_window.cpp
    gui/panels/editor_panel.cpp
    gui/panels/navigator_panel.cpp
    gui/panels/properties_panel.cpp
    gui/panels/search_panel.cpp
    gui/panels/assistant_panel.cpp
    gui/panels/log_panel.cpp
)
```

---

## ✅ Acceptance Criteria

### Functional Requirements
- [ ] **AC-001:** Application launches with 6 panels visible
- [ ] **AC-002:** Navigator panel visible on left side (QTreeWidget)
- [ ] **AC-003:** Properties panel visible on right side (QLabel placeholder)
- [ ] **AC-004:** Log panel visible at bottom (QPlainTextEdit)
- [ ] **AC-005:** Search panel tabbed with Properties (QLineEdit)
- [ ] **AC-006:** Assistant panel tabbed with Properties/Search (QLabel placeholder)
- [ ] **AC-007:** Editor panel in central area (QPlainTextEdit)
- [ ] **AC-008:** Panels can be dragged to different positions (Qt native drag & drop)
- [ ] **AC-009:** Panels can be resized by dragging splitters
- [ ] **AC-010:** Panels can be closed via X button
- [ ] **AC-011:** View menu shows 5 panel toggles + Reset Layout
- [ ] **AC-012:** View menu toggles work (Ctrl+1, Ctrl+2, Ctrl+3, Ctrl+4, Ctrl+5)
- [ ] **AC-013:** Reset Layout restores default layout (Ctrl+0)
- [ ] **AC-014:** Perspective persists across application restarts

### Technical Requirements
- [ ] **AC-015:** 6 panel header files created (include/kalahari/gui/panels/*.h)
- [ ] **AC-016:** 6 panel implementation files created (src/gui/panels/*.cpp)
- [ ] **AC-017:** MainWindow header updated (dock management methods)
- [ ] **AC-018:** MainWindow implementation updated (createDocks, perspective save/restore)
- [ ] **AC-019:** CMakeLists.txt updated (panel sources added)
- [ ] **AC-020:** No compiler warnings
- [ ] **AC-021:** No linker errors
- [ ] **AC-022:** QDockWidget objectName set (required for saveState!)

### Verification Requirements
- [ ] **AC-023:** CI/CD passes (Linux, macOS, Windows)
- [ ] **AC-024:** Application runs without crashes
- [ ] **AC-025:** Perspective saves correctly (QSettings file created)
- [ ] **AC-026:** Perspective restores correctly (layout matches saved state)

---

## 🧪 Test Cases

### TC-001: Default Layout Appearance
**Precondition:** Fresh application start (no saved perspective)
**Steps:**
1. Run application
2. Observe panel layout

**Expected:**
- Navigator on left (QTreeWidget visible)
- Properties/Search/Assistant tabs on right (Properties active)
- Log at bottom (QPlainTextEdit visible)
- Editor in center (QPlainTextEdit with placeholder text)

**Pass Criteria:** All 6 panels visible in correct positions

---

### TC-002: Panel Drag & Drop
**Precondition:** Application running with default layout
**Steps:**
1. Drag Navigator panel title bar
2. Drop on right side of window
3. Observe layout change

**Expected:**
- Navigator moves to right side
- Layout updates immediately
- No crashes or errors

**Pass Criteria:** Drag & drop works smoothly

---

### TC-003: Panel Close/Reopen
**Precondition:** Application running
**Steps:**
1. Click X button on Navigator panel
2. Observe Navigator disappears
3. Press Ctrl+1 (or View > Navigator)
4. Observe Navigator reappears

**Expected:**
- Navigator closes when X clicked
- Navigator reopens when Ctrl+1 pressed
- View menu checkmark syncs correctly

**Pass Criteria:** Panel toggle works correctly

---

### TC-004: Reset Layout
**Precondition:** Application running with modified layout
**Steps:**
1. Drag panels to random positions
2. Close some panels
3. Press Ctrl+0 (or View > Reset Layout)
4. Observe layout resets

**Expected:**
- All panels visible again
- Default positions restored
- Status bar shows "Layout reset to default"

**Pass Criteria:** Layout resets to default

---

### TC-005: Perspective Persistence
**Precondition:** Application running
**Steps:**
1. Arrange panels in custom layout
2. Resize window
3. Close application
4. Reopen application
5. Observe layout

**Expected:**
- Panel positions match previous session
- Window size matches previous session
- QSettings file created (Linux: ~/.config/Bartosz W. Warzocha & Kalahari Team/Kalahari.conf)

**Pass Criteria:** Perspective persists across sessions

---

## 🚀 Execution Steps

1. **Create directories:**
   ```bash
   mkdir -p include/kalahari/gui/panels
   mkdir -p src/gui/panels
   ```

2. **Create panel files** (12 files total)
   - Write 6 header files (Step 1)
   - Write 6 implementation files (Step 2)

3. **Update MainWindow** (2 files)
   - Modify main_window.h (Step 3)
   - Modify main_window.cpp (Step 4)

4. **Update CMakeLists.txt** (Step 5)

5. **Build and test:**
   ```bash
   cmake --build build-windows --config Debug
   ./build-windows/bin/kalahari.exe
   ```

6. **Verify all acceptance criteria**

7. **Update documentation:**
   - Mark task as DONE in this file
   - Update CHANGELOG.md
   - Git commit

---

## 📊 Implementation Summary

**Status:** ✅ DONE (Implementation complete, awaiting CI/CD verification)
**Files Created:** 12/12
  - ✅ include/kalahari/gui/panels/editor_panel.h
  - ✅ include/kalahari/gui/panels/navigator_panel.h
  - ✅ include/kalahari/gui/panels/properties_panel.h
  - ✅ include/kalahari/gui/panels/search_panel.h
  - ✅ include/kalahari/gui/panels/assistant_panel.h
  - ✅ include/kalahari/gui/panels/log_panel.h
  - ✅ src/gui/panels/editor_panel.cpp
  - ✅ src/gui/panels/navigator_panel.cpp
  - ✅ src/gui/panels/properties_panel.cpp
  - ✅ src/gui/panels/search_panel.cpp
  - ✅ src/gui/panels/assistant_panel.cpp
  - ✅ src/gui/panels/log_panel.cpp
**Files Modified:** 3/3
  - ✅ include/kalahari/gui/main_window.h (dock management added)
  - ✅ src/gui/main_window.cpp (full dock implementation)
  - ✅ src/CMakeLists.txt (6 panel sources added)
**Acceptance Criteria:** Awaiting CI/CD verification (26 criteria)
**Test Cases:** Awaiting CI/CD build (5 test cases)

**Implementation Date:** 2025-11-20
**Verification:** CI/CD build required (Linux, macOS, Windows)

**BONUS FEATURE ADDED:** Toolbars are now floating/movable (user requirement)
  - QToolBar::setMovable(true) - Can be moved between dock areas
  - QToolBar::setFloatable(true) - Can be detached as floating window

---

## 📝 Notes

### Qt6 QDockWidget Key Features
- **Native drag & drop:** Qt handles all docking logic automatically
- **toggleViewAction():** Each QDockWidget provides built-in toggle action (no manual sync needed!)
- **tabifyDockWidget():** Creates tabbed interface (like Properties/Search/Assistant)
- **setObjectName():** REQUIRED for saveState() to work correctly
- **saveState()/restoreState():** Saves/restores entire dock layout as QByteArray

### QSettings Location
- **Linux:** `~/.config/Bartosz W. Warzocha & Kalahari Team/Kalahari.conf`
- **macOS:** `~/Library/Preferences/com.Bartosz W. Warzocha & Kalahari Team.Kalahari.plist`
- **Windows:** `HKEY_CURRENT_USER\Software\Bartosz W. Warzocha & Kalahari Team\Kalahari`

### Future Enhancements (Not in This Task)
- Real Navigator implementation (Chapter/Scene tree) - Phase 1 Week 3
- Real Editor implementation (Custom text control) - Phase 1 Week 3
- Real Log panel (Colored output, filtering) - Phase 1 Week 4
- Search panel functionality - Phase 1 Week 4
- Assistant panel (AI integration) - Phase 2+

### Architecture Pattern
- **Separation of Concerns:** Panels are separate classes (easy to enhance later)
- **Qt Parent/Child:** All widgets properly parented (automatic memory management)
- **Signal/Slot:** Toggle actions automatically sync with dock visibility (Qt magic!)

---

**Created:** 2025-11-20
**Last Updated:** 2025-11-20
**Author:** Claude (AI Assistant)
**Approved By:** [Awaiting user approval]
