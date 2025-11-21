# Task #00047 - Command Registry Qt Migration

**Phase:** 0 (Qt Foundation)
**Priority:** HIGH (Architecture)
**Estimated Time:** 1-2 days (12-16 hours)
**Started:** 2025-11-21
**Status:** 🔄 IN PROGRESS

---

## Context

**Problem:** MainWindow currently uses hardcoded QAction connections (15+ `connect()` calls). This approach:
- ❌ Doesn't scale (no plugin commands support)
- ❌ No customizable toolbars/menus
- ❌ Duplicate code (menu + toolbar + shortcuts)
- ❌ Violates Single Responsibility (actions scattered across createActions/createMenuBar/createToolBar)

**Solution:** Migrate Command Registry system from wxWidgets implementation (Tasks #00032-#00035).

**Architecture (from wxwidgets-archive):**
- ✅ **CommandRegistry** singleton (200 LOC)
- ✅ **Command** struct (id, label, category, icons, shortcuts, callbacks)
- ✅ **IconSet** (16/24/32px pre-rendered bitmaps)
- ✅ **KeyboardShortcut** (key + modifiers, toString/fromString)
- ✅ **ToolbarBuilder** (dynamic toolbar from registry)
- ✅ **MenuBuilder** (will implement fresh for Qt)

**Testing:** Original system had 46+ test cases (Tasks #00035), all PASSED.

---

## Files to Create/Modify

**New Files (6):**
```
include/kalahari/gui/command.h           # Command, IconSet, KeyboardShortcut structs
src/gui/command.cpp                      # Implementation
include/kalahari/gui/command_registry.h  # CommandRegistry singleton
src/gui/command_registry.cpp             # Implementation
include/kalahari/gui/toolbar_builder.h   # ToolbarBuilder class
src/gui/toolbar_builder.cpp              # Implementation
```

**Modified Files (2):**
```
include/kalahari/gui/main_window.h       # Add registerCommands() method
src/gui/main_window.cpp                  # Refactor to use CommandRegistry
```

---

## Implementation Plan

### Day 1 Morning: Recover Files (3-4 hours)

**Step 1: Extract from Archive**

```bash
# Command struct definition
git show wxwidgets-archive:include/kalahari/gui/command.h > /tmp/command.h
git show wxwidgets-archive:src/gui/command.cpp > /tmp/command.cpp

# CommandRegistry singleton
git show wxwidgets-archive:include/kalahari/gui/command_registry.h > /tmp/command_registry.h
git show wxwidgets-archive:src/gui/command_registry.cpp > /tmp/command_registry.cpp

# ToolbarBuilder
git show wxwidgets-archive:include/kalahari/gui/toolbar_builder.h > /tmp/toolbar_builder.h
git show wxwidgets-archive:src/gui/toolbar_builder.cpp > /tmp/toolbar_builder.cpp
```

**Step 2: Copy to Project**

Copy files to correct locations (include/kalahari/gui/, src/gui/).

**Step 3: Add to CMake**

Update `src/CMakeLists.txt`:
```cmake
# GUI sources
target_sources(kalahari_gui PRIVATE
    # ... existing files
    command.cpp
    command_registry.cpp
    toolbar_builder.cpp
)
```

---

### Day 1 Afternoon: Adapt to Qt6 (4-5 hours)

**Adaptation 1: IconSet (wx → Qt)**

```cpp
// BEFORE (wxWidgets):
struct IconSet {
    wxBitmap icon16, icon24, icon32;
    explicit IconSet(const wxString& path);
    bool isEmpty() const;
};

// AFTER (Qt6):
#include <QPixmap>
#include <QIcon>
#include <QString>

struct IconSet {
    QPixmap icon16, icon24, icon32;

    IconSet() = default;
    explicit IconSet(const QString& path);

    bool isEmpty() const {
        return icon16.isNull() && icon24.isNull() && icon32.isNull();
    }

    QIcon toQIcon() const;  // NEW: Convert to QIcon for Qt integration
};
```

**Implementation notes:**
- `wxBitmap::IsOk()` → `QPixmap::isNull()` (inverted logic!)
- `wxString` → `QString`
- Add `toQIcon()` helper to create QIcon with multiple sizes

**Adaptation 2: KeyboardShortcut (wx → Qt)**

```cpp
// BEFORE (wxWidgets):
struct KeyboardShortcut {
    int keyCode;  // wxKeyCode
    bool ctrl, alt, shift;
    wxString toString() const;
    static KeyboardShortcut fromString(const wxString& str);
};

// AFTER (Qt6):
#include <QKeySequence>
#include <QString>

struct KeyboardShortcut {
    int keyCode;  // Qt::Key
    Qt::KeyboardModifiers modifiers;

    KeyboardShortcut() : keyCode(0), modifiers(Qt::NoModifier) {}
    KeyboardShortcut(Qt::Key key, Qt::KeyboardModifiers mods = Qt::NoModifier)
        : keyCode(key), modifiers(mods) {}

    QString toString() const;
    QKeySequence toQKeySequence() const;  // NEW: Convert to QKeySequence
    static KeyboardShortcut fromQKeySequence(const QKeySequence& seq);  // NEW
    static KeyboardShortcut fromString(const QString& str);

    bool isEmpty() const { return keyCode == 0; }
    bool operator==(const KeyboardShortcut& other) const;
    bool operator<(const KeyboardShortcut& other) const;  // For std::map
};
```

**Implementation notes:**
- Replace `ctrl/alt/shift` bools with `Qt::KeyboardModifiers` flags
- Add `toQKeySequence()` for Qt integration
- wxKeyCode constants → Qt::Key enum
- Mapping: WXK_CONTROL_A → Qt::Key_A + Qt::ControlModifier

**Adaptation 3: Command struct**

```cpp
// BEFORE (wxWidgets) - minimal changes needed!
struct Command {
    std::string id;
    std::string label;
    std::string tooltip;
    std::string category;
    IconSet icons;
    KeyboardShortcut shortcut;
    std::function<void()> execute;
    std::function<bool()> isEnabled;
    std::function<bool()> isChecked;
    bool showInMenu = true;
    bool showInToolbar = false;
    bool isShortcutCustomizable = true;
};

// AFTER (Qt6) - add ONE helper method:
struct Command {
    // ... same as above (no changes to data members)

    QAction* toQAction(QObject* parent) const;  // NEW: Convert to QAction
};
```

**Implementation of `toQAction()`:**
```cpp
QAction* Command::toQAction(QObject* parent) const {
    QAction* action = new QAction(QString::fromStdString(label), parent);
    action->setToolTip(QString::fromStdString(tooltip));

    if (!icons.isEmpty()) {
        action->setIcon(icons.toQIcon());
    }

    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut.toQKeySequence());
    }

    if (isEnabled) {
        action->setEnabled(isEnabled());
    }

    if (isChecked) {
        action->setCheckable(true);
        action->setChecked(isChecked());
    }

    return action;
}
```

**Adaptation 4: CommandRegistry**

```cpp
// NO CHANGES NEEDED! Framework-agnostic design.
// Only include paths change: wx/wx.h → Qt headers removed
```

**Testing:** Build project after each adaptation, fix compilation errors.

---

### Day 2 Morning: Implement Builders (4-5 hours)

**MenuBuilder (new for Qt)**

```cpp
// include/kalahari/gui/menu_builder.h
#pragma once

#include "command_registry.h"
#include <QMenuBar>
#include <QMenu>
#include <QMainWindow>

namespace kalahari {
namespace gui {

class MenuBuilder {
public:
    MenuBuilder() = default;

    /// Build complete menu bar from CommandRegistry
    /// Creates: File, Edit, View, Help menus
    void buildMenuBar(CommandRegistry& registry, QMainWindow* window);

    /// Build single menu from category
    QMenu* buildMenu(const QString& title,
                     const std::string& category,
                     CommandRegistry& registry,
                     QObject* parent);

private:
    void addCommandsToMenu(QMenu* menu,
                          const std::vector<Command*>& commands,
                          QObject* parent);
};

} // namespace gui
} // namespace kalahari
```

**Implementation:**
- Query `CommandRegistry::getCommandsByCategory("File")`
- Create QAction via `Command::toQAction()`
- Connect QAction::triggered → `CommandRegistry::executeCommand(id)`
- Add separators between logical groups

**ToolbarBuilder (adapted from wxWidgets)**

```cpp
// include/kalahari/gui/toolbar_builder.h
#pragma once

#include "command_registry.h"
#include <QToolBar>
#include <QMainWindow>

namespace kalahari {
namespace gui {

class ToolbarBuilder {
public:
    ToolbarBuilder() = default;

    /// Build toolbar from CommandRegistry
    /// Only includes commands where showInToolbar=true
    QToolBar* buildToolBar(CommandRegistry& registry, QMainWindow* parent);

    /// Add tools from specific category
    void addToolsFromCategory(QToolBar* toolbar,
                              CommandRegistry& registry,
                              const std::string& category);

    /// Add separator
    void addSeparator(QToolBar* toolbar);
};

} // namespace gui
} // namespace kalahari
```

**Implementation:**
- Similar to MenuBuilder, but filter by `showInToolbar=true`
- Use command order: File → Edit → Format
- Add separators between categories

---

### Day 2 Afternoon: Refactor MainWindow (3-4 hours)

**Step 1: Add registerCommands() Method**

```cpp
// main_window.h
private:
    void registerCommands();  // NEW: Register all core commands
```

**Step 2: Implement registerCommands()**

```cpp
// main_window.cpp
void MainWindow::registerCommands() {
    CommandRegistry& registry = CommandRegistry::getInstance();

    // File commands
    {
        Command cmd;
        cmd.id = "file.new";
        cmd.label = "New";
        cmd.tooltip = "Create new document";
        cmd.category = "File";
        cmd.icons = IconSet(":/icons/document-new.svg");  // TODO: add icon
        cmd.shortcut = KeyboardShortcut(Qt::Key_N, Qt::ControlModifier);
        cmd.execute = [this]() { onNewDocument(); };
        cmd.showInMenu = true;
        cmd.showInToolbar = true;
        registry.registerCommand(cmd);
    }

    // ... register all 15+ commands (new, open, save, undo, redo, cut, copy, paste, etc.)
}
```

**Step 3: Replace createMenuBar()**

```cpp
// BEFORE:
void MainWindow::createMenuBar() {
    QMenuBar* menuBar = new QMenuBar(this);

    QMenu* fileMenu = new QMenu(tr("&File"), this);
    fileMenu->addAction(m_newAction);
    // ... 20+ lines of manual menu construction
}

// AFTER:
void MainWindow::createMenuBar() {
    MenuBuilder builder;
    builder.buildMenuBar(CommandRegistry::getInstance(), this);
}
```

**Step 4: Replace createToolBar()**

```cpp
// BEFORE:
void MainWindow::createToolBar() {
    QToolBar* toolbar = new QToolBar(tr("Standard"), this);
    toolbar->addAction(m_newAction);
    // ... manual toolbar construction
}

// AFTER:
void MainWindow::createToolBar() {
    ToolbarBuilder builder;
    QToolBar* toolbar = builder.buildToolBar(CommandRegistry::getInstance(), this);
    addToolBar(toolbar);
}
```

**Step 5: Remove Old Code**

Delete:
- `createActions()` method
- All `m_*Action` member variables
- All `connect()` calls

**Step 6: Update Constructor**

```cpp
// main_window.cpp constructor
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    registerCommands();  // NEW: must be BEFORE createMenuBar/createToolBar
    createMenuBar();
    createToolBar();
    // ... rest of initialization
}
```

---

## Testing Plan

**Manual Testing (30 minutes):**

1. **Menu Testing:**
   - [ ] File menu displays all items
   - [ ] Edit menu displays all items
   - [ ] Help menu displays all items
   - [ ] Menu items trigger correct actions

2. **Toolbar Testing:**
   - [ ] Toolbar displays correct icons
   - [ ] Toolbar buttons trigger actions
   - [ ] Tooltips show on hover

3. **Keyboard Shortcuts:**
   - [ ] Ctrl+N = New
   - [ ] Ctrl+O = Open
   - [ ] Ctrl+S = Save
   - [ ] Ctrl+Z = Undo
   - [ ] Ctrl+Y = Redo
   - [ ] Ctrl+X/C/V = Cut/Copy/Paste

4. **State Management:**
   - [ ] Undo/Redo disabled when no text
   - [ ] Cut/Copy disabled when no selection
   - [ ] Paste enabled when clipboard has text

5. **Build Verification:**
   - [ ] CMake configures successfully
   - [ ] Project compiles without errors/warnings
   - [ ] Application launches

---

## Acceptance Criteria

- ✅ All 6 new files created and compiling
- ✅ CommandRegistry singleton working
- ✅ MenuBuilder creates complete menu bar
- ✅ ToolbarBuilder creates functional toolbar
- ✅ MainWindow uses Command Registry (no hardcoded QActions)
- ✅ All keyboard shortcuts work
- ✅ All menu items trigger correct actions
- ✅ All toolbar buttons work
- ✅ Enable/disable state management works
- ✅ No compilation errors/warnings
- ✅ Application launches successfully

---

## Documentation Updates

**After implementation:**
1. Update `project_docs/08_gui_design.md` (Section 5: Command Registry System)
   - Replace wxWidgets code examples with Qt6 equivalents
2. Update `CHANGELOG.md` (add [0.3.1-alpha] entry)
3. Consider adding architecture diagram (optional)

---

## Risks & Mitigations

**Risk 1:** Icon paths incorrect (icons not found)
**Mitigation:** Start with `QIcon()` (no icon), add icons in Phase 1

**Risk 2:** KeyboardShortcut conversion errors
**Mitigation:** Test each shortcut manually, fallback to Qt standard shortcuts

**Risk 3:** Command execution errors (crashes)
**Mitigation:** Wrap execute() in try-catch, log errors

**Risk 4:** Takes longer than 2 days
**Mitigation:** Prioritize core functionality (File/Edit), defer Format commands to Phase 1

---

## Success Metrics

**Quantitative:**
- 0 hardcoded QAction connections remaining
- 15+ commands registered in CommandRegistry
- 3 menus built dynamically (File, Edit, Help)
- 1 toolbar built dynamically
- 100% keyboard shortcuts working

**Qualitative:**
- Code is more maintainable (single source of truth)
- Plugins can add commands easily (ICommandProvider ready)
- Customizable toolbars possible (Phase 1)

---

## Notes

- Original implementation: Tasks #00032-#00035 (Nov 13-14, 2025)
- Original testing: 46+ test cases, 4h session, all PASSED
- This is a **migration**, not new development - architecture is proven
- Focus on correct Qt adaptation, not redesign

---

**Task Created:** 2025-11-21
**Estimated Completion:** 2025-11-22 (2 days)
**Next Task:** TBD (Phase 1 planning)
