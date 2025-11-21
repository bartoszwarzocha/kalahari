# Task #00015: Central Tabbed Workspace (QTabWidget)

**Status:** 🔄 IN PROGRESS
**Priority:** 🔴 HIGH (Architectural Fix)
**Estimated Time:** 60-90 minutes
**Created:** 2025-11-21
**Category:** Architecture / GUI

---

## Problem

**Current state:** Central widget is a single `EditorPanel` instance (`setCentralWidget(m_editorPanel)`).

**Issue:** This architecture doesn't allow opening multiple tabs with different content types:
- Multiple document editors (chapters, scenes)
- Mind map view
- Statistics dashboard
- Timeline/Schedule view
- Character/Location cards

**Impact:** Fundamental limitation preventing multi-document interface (MDI) and future features.

**Historical context:** This bug existed even in wxWidgets version - never properly fixed.

---

## Solution

Replace single `EditorPanel` with `QTabWidget` as central widget, allowing multiple tabs with different panel types.

### Architecture Change

**Before (current):**
```cpp
// main_window.cpp:680-681
m_editorPanel = new EditorPanel(this);
setCentralWidget(m_editorPanel);
```

**After (target):**
```cpp
// Create central tabbed workspace
m_centralTabs = new QTabWidget(this);
m_centralTabs->setTabsClosable(true);
m_centralTabs->setMovable(true);
m_centralTabs->setDocumentMode(true);  // Better look on Windows
setCentralWidget(m_centralTabs);

// Add Dashboard as first tab (default at startup, but closable)
m_dashboardPanel = new DashboardPanel(this);
int dashboardIndex = m_centralTabs->addTab(m_dashboardPanel, tr("Dashboard"));
m_centralTabs->setCurrentIndex(dashboardIndex);

// Note: EditorPanel tabs created on-demand (File > New, File > Open)
```

---

## Implementation Plan

### 1. Create DashboardPanel (placeholder for Phase 0)

**File:** `include/kalahari/gui/panels/dashboard_panel.h`
```cpp
#pragma once
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

/// @brief Dashboard panel - welcome screen and quick actions
class DashboardPanel : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPanel(QWidget* parent = nullptr);
    ~DashboardPanel() override = default;

private:
    QLabel* m_welcomeLabel;
    // Phase 1+: Add Recent Files, Quick Actions, Tips
};

} // namespace gui
} // namespace kalahari
```

**File:** `src/gui/panels/dashboard_panel.cpp`
```cpp
#include "kalahari/gui/panels/dashboard_panel.h"

namespace kalahari {
namespace gui {

DashboardPanel::DashboardPanel(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    m_welcomeLabel = new QLabel(tr("<h1>Welcome to Kalahari</h1>"
                                   "<p>Create a new document or open an existing one to get started.</p>"), this);
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    m_welcomeLabel->setWordWrap(true);

    layout->addStretch();
    layout->addWidget(m_welcomeLabel);
    layout->addStretch();

    setLayout(layout);
}

} // namespace gui
} // namespace kalahari
```

### 2. Update main_window.h

**Add members:**
```cpp
private:
    QTabWidget* m_centralTabs;     // Central tabbed workspace
    DashboardPanel* m_dashboardPanel;  // Welcome/Dashboard panel
```

**Update constructor initialization:**
```cpp
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_centralTabs(nullptr)
    , m_dashboardPanel(nullptr)
    , m_editorPanel(nullptr)  // Now created on-demand
    // ... rest
```

### 3. Update main_window.cpp

**Modify createDocks():**
- Lines 679-681: Replace direct EditorPanel with QTabWidget + DashboardPanel
- Add tab close signal connection: `connect(m_centralTabs, &QTabWidget::tabCloseRequested, ...)`
- Dashboard is closable (user can close it if they want)
- Allow closing all tabs (empty QTabWidget is valid state)

**Future-ready design:**
- Tab close should check for unsaved changes
- Support for different panel types (not just EditorPanel)
- Tab ordering (drag & drop already enabled via `setMovable(true)`)

### 4. Update File Operations

**Affected methods:**
- `onNewDocument()`: Create new EditorPanel tab (on-demand)
- `onOpenDocument()`: Create new EditorPanel tab with loaded document
- `onSaveDocument()`: Save currently active tab (if it's EditorPanel)
- `onSaveAsDocument()`: Save currently active tab (if it's EditorPanel)

**Key change:** EditorPanel created ON-DEMAND, not at startup!

**Implementation:**
```cpp
void MainWindow::onNewDocument() {
    // Create new editor panel (on-demand)
    EditorPanel* editor = new EditorPanel(this);
    int tabIndex = m_centralTabs->addTab(editor, tr("Untitled"));
    m_centralTabs->setCurrentIndex(tabIndex);

    // Initialize with empty document
    m_currentDocument = core::Document("Untitled", "User", "en");
    editor->setText("");

    // Connect signals
    connect(editor->getTextEdit(), &QPlainTextEdit::textChanged,
            this, [this]() { setDirty(true); });

    setDirty(false);
    updateWindowTitle();
}
```

**Helper method:**
```cpp
EditorPanel* MainWindow::getCurrentEditor() {
    QWidget* currentWidget = m_centralTabs->currentWidget();
    return qobject_cast<EditorPanel*>(currentWidget);  // Returns nullptr if not EditorPanel
}
```

---

## Acceptance Criteria

- [x] Task file created
- [ ] DashboardPanel created (placeholder welcome screen)
- [ ] QTabWidget added as central widget (not EditorPanel)
- [ ] First tab contains DashboardPanel (default at startup, closable)
- [ ] EditorPanel tabs created ON-DEMAND (not at startup)
- [ ] All tabs are closable (`setTabsClosable(true)`)
- [ ] All tabs are movable (`setMovable(true)`)
- [ ] Tab close signal connected (with unsaved changes check for EditorPanel)
- [ ] File > New creates new EditorPanel tab
- [ ] File > Open creates new EditorPanel tab
- [ ] File > Save saves active tab (only if EditorPanel)
- [ ] Application builds without errors
- [ ] Application launches and shows Dashboard tab
- [ ] Manual test: File > New creates editor tab, can switch between Dashboard and Editor
- [ ] Manual test: Can close Dashboard tab (leaves empty QTabWidget or only Editor tabs)
- [ ] Manual test: Can close all tabs (empty central widget is valid)

---

## Files to Modify

1. **NEW: include/kalahari/gui/panels/dashboard_panel.h**
   - Create DashboardPanel class (placeholder for Phase 0)

2. **NEW: src/gui/panels/dashboard_panel.cpp**
   - Implement DashboardPanel with welcome message

3. **include/kalahari/gui/main_window.h**
   - Add `QTabWidget* m_centralTabs;` member
   - Add `DashboardPanel* m_dashboardPanel;` member
   - Change `m_editorPanel` to be created on-demand (nullable)
   - Update constructor initialization list
   - Add helper method: `EditorPanel* getCurrentEditor();`

4. **src/gui/main_window.cpp**
   - Update `createDocks()` method (lines 679-681) - use DashboardPanel
   - Add tab close handler (check if EditorPanel, verify unsaved changes)
   - Update `onNewDocument()` to create new EditorPanel tab
   - Update `onOpenDocument()` to create new EditorPanel tab
   - Update `onSaveDocument()` to work with current EditorPanel tab
   - Update `onSaveAsDocument()` to work with current EditorPanel tab
   - Implement `getCurrentEditor()` helper

5. **src/CMakeLists.txt**
   - Add `src/gui/panels/dashboard_panel.cpp` to build

---

## Testing Strategy

### Manual Testing
1. **Launch application** - Should see Dashboard tab (closable)
2. **Close Dashboard tab** - Should close successfully, leaving empty QTabWidget
3. **File > New** - Should create "Untitled" EditorPanel tab
4. **File > New** again - Should create "Untitled 2" tab
5. **Close first Editor tab** - Should leave "Untitled 2"
6. **File > New** to create Dashboard again - Dashboard should appear as new tab
7. **Type text** in Editor tab 1, switch to tab 2, type different text
8. **Close middle tab** - Should close without crash
9. **File > Save** in Editor tab - Should save only that tab
10. **Close Editor tab with unsaved changes** - Should prompt to save
11. **Close all tabs** - Should leave empty QTabWidget (no tabs)
12. **Relaunch application** - Should show Dashboard tab again (fresh start)

### Expected Behavior
- ✅ Application starts with Dashboard tab (closable)
- ✅ Dashboard tab CAN be closed (user choice)
- ✅ EditorPanel tabs created on File > New/Open
- ✅ Multiple tabs (Dashboard + EditorPanel) can coexist
- ✅ Can switch between Dashboard and Editor tabs
- ✅ Can close any tab (including Dashboard)
- ✅ Can move tabs by dragging
- ✅ Each EditorPanel tab maintains independent content
- ✅ File operations affect only active EditorPanel tab
- ✅ Empty QTabWidget is valid state (no tabs open)

---

## Future Enhancements (Phase 1+)

### Dashboard Enhancements
- [ ] Recent files list with thumbnails
- [ ] Quick actions buttons (styled)
- [ ] Tips & Tricks carousel
- [ ] Getting Started tutorial
- [ ] Statistics overview (total words, documents, etc.)

### Tab System Enhancements
- [ ] Different tab types (not just EditorPanel):
  - Mind Map tab
  - Statistics Dashboard tab
  - Timeline/Schedule tab
  - Character Card tab
  - Location Card tab
- [ ] Tab icons (based on content type)
- [ ] Tab context menu (Close, Close Others, Close All, Close to Right)
- [ ] Split view (horizontal/vertical) for comparing chapters
- [ ] Tab groups (colored tabs by chapter/part)
- [ ] Recently closed tabs (Ctrl+Shift+T to reopen)

### Settings
- [ ] "Reopen last project on startup" checkbox (default: false)
- [ ] "Show Dashboard on startup" checkbox (default: true)

---

## Notes

- This is a **breaking change** to existing architecture
- All references to `m_editorPanel` must be updated to use `getCurrentEditor()` helper
- QTabWidget manages child widget lifetime (no manual delete needed)
- Phase 0 scope: Basic tabbed interface only
- Advanced features (different panel types, split view) are Phase 1+
- **Dashboard is closable** - gives user full control
- **Empty QTabWidget is valid** - no tabs is acceptable state
- Next session always starts with Dashboard (unless user has "reopen last tabs" setting in Phase 1+)

---

**Next Steps:**
1. Get user approval ("Approved, proceed")
2. Implement changes
3. Build & test
4. Update ROADMAP.md and CHANGELOG.md
5. Git commit with message: "fix: Replace single EditorPanel with QTabWidget for multi-document support (Task #00015)"
