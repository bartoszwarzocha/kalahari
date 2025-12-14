# Change Proposal: Toolbar Manager System

**Change ID:** `00019-toolbar-manager`
**Type:** Feature
**Status:** Proposed
**Phase:** 0 (Qt Foundation)
**Created:** 2025-11-22
**Author:** Claude Code (AI Assistant)

---

## 📋 Summary

Implement **ToolbarManager** - centralized system for managing multiple toolbars with icons, configuration, and persistence. Replaces single hardcoded toolbar with flexible multi-toolbar architecture.

**Key Features:**
- ✅ Multiple toolbars (File, Edit, Format, View, Tools)
- ✅ Icon system with placeholder icons (16/24/32px)
- ✅ Toolbar configuration (show/hide via View menu)
- ✅ State persistence (position, visibility, floating)
- ✅ Integration with Command Registry

---

## 🎯 Motivation

**Current State:**
- **ONE toolbar** only (`m_fileToolbar`)
- **8 commands** in toolbar (New, Open, Save, SaveAs, Exit, Undo, Redo, Cut, Copy, Paste)
- **No icons** (commands don't have IconSet configured)
- **No configuration** - can't hide/show toolbars
- **No persistence** - toolbar state not saved

**Problems:**
1. ❌ All toolbar actions mixed in one toolbar (File + Edit together)
2. ❌ No visual icons - text-only buttons
3. ❌ Can't customize which toolbars are visible
4. ❌ Toolbar positions/state reset on restart
5. ❌ Difficult to extend - hardcoded single toolbar

**Solution:**
Implement **ToolbarManager** to manage multiple toolbars with icons, configuration, and state persistence.

---

## 🏗️ Architecture

### 1. **ToolbarManager Class** (New)

```cpp
/// @brief Centralized toolbar management system
class ToolbarManager {
public:
    /// Toolbar descriptor
    struct ToolbarConfig {
        std::string id;              // "file", "edit", "format"
        std::string label;           // "File Toolbar"
        Qt::ToolBarArea defaultArea; // Qt::TopToolBarArea
        bool defaultVisible;         // true
        std::vector<std::string> commandIds; // Commands to show
    };

    explicit ToolbarManager(QMainWindow* mainWindow);

    /// Create all toolbars from configuration
    void createToolbars(CommandRegistry& registry);

    /// Get toolbar by ID
    QToolBar* getToolbar(const std::string& id);

    /// Show/hide toolbar
    void showToolbar(const std::string& id, bool visible);
    bool isToolbarVisible(const std::string& id);

    /// Save/restore toolbar state (position, visibility, floating)
    void saveState();
    void restoreState();

    /// Create View menu actions for toolbar toggles
    void createViewMenuActions(QMenu* viewMenu);

private:
    QMainWindow* m_mainWindow;
    std::unordered_map<std::string, QToolBar*> m_toolbars;
    std::unordered_map<std::string, ToolbarConfig> m_configs;
    std::unordered_map<std::string, QAction*> m_viewActions;
};
```

**Responsibilities:**
- Create and manage all toolbars
- Toolbar visibility configuration
- State persistence (QSettings)
- View menu integration (toggle actions)

---

### 2. **Toolbar Definitions**

**5 Toolbars** (Phase 0):
1. **File Toolbar**: New, Open, Save, SaveAs, Close
2. **Edit Toolbar**: Undo, Redo, Cut, Copy, Paste, SelectAll
3. **Format Toolbar**: Bold, Italic, Underline, AlignLeft, AlignCenter, AlignRight
4. **View Toolbar**: Navigator, Properties, Search, Assistant, Log panels
5. **Tools Toolbar**: Spellcheck, Word Count, Focus Mode

**Later (Phase 1+):**
- Insert Toolbar (Image, Table, Link, Footnote)
- Book Toolbar (New Chapter, New Character, Timeline)

---

### 3. **Icon System**

**Problem:** IconSet currently loads from file path, but we don't have icon files yet.

**Solution (Phase 0):** Use **Qt Standard Icons** + **Placeholder Icons**

```cpp
// IconSet enhancement - add factory methods
struct IconSet {
    // Existing
    explicit IconSet(const QString& path);

    // NEW: Standard Qt icons
    static IconSet fromStandardIcon(QStyle::StandardPixmap icon);

    // NEW: Placeholder icons (colored squares with letter)
    static IconSet createPlaceholder(const QString& letter, const QColor& color);
};
```

**Standard Icons** (Qt provides built-in):
- SP_FileIcon → New
- SP_DirOpenIcon → Open
- SP_DialogSaveButton → Save
- SP_DialogCancelButton → Close
- SP_ArrowBack → Undo
- SP_ArrowForward → Redo
- QStyle::SP_* for common actions

**Placeholder Icons** (for actions without standard icons):
- Generate 16/24/32px colored squares with white letter
- Example: "B" for Bold (blue), "I" for Italic (green)
- Temporary until real icon set designed

---

### 4. **Toolbar Configuration**

**View Menu** - toggle toolbar visibility:
```
VIEW
├─ Panels ▼
│  ├─ □ Navigator
│  ├─ □ Properties
│  └─ ...
├─ ───────────────
├─ Toolbars ▼
│  ├─ ☑ File Toolbar
│  ├─ ☑ Edit Toolbar
│  ├─ ☑ Format Toolbar
│  ├─ □ View Toolbar
│  └─ □ Tools Toolbar
└─ ───────────────
   └─ Reset Layout
```

**Checkable Actions:**
- Each toolbar has QAction in View > Toolbars submenu
- Action is checkable (shows ☑/□)
- Click toggles toolbar visibility
- State synced with actual toolbar visibility

---

### 5. **State Persistence**

**QSettings keys:**
```ini
[MainWindow]
geometry=...
windowState=...  # Includes toolbar positions (Qt saves automatically)

[Toolbars]
file/visible=true
edit/visible=true
format/visible=false
view/visible=false
tools/visible=false
```

**Workflow:**
1. User moves/hides/floats toolbar
2. `MainWindow::closeEvent()` calls `toolbarManager.saveState()`
3. `ToolbarManager::saveState()` saves visibility to QSettings
4. Qt automatically saves positions in `windowState`
5. `MainWindow::showEvent()` calls `toolbarManager.restoreState()`
6. Toolbars restored to last known state

---

## 🔧 Implementation Plan

### Phase 0 (Task #00019) - MVP

**Goal:** Replace single toolbar with ToolbarManager + 5 toolbars + placeholder icons

**Files to Create:**
1. `include/kalahari/gui/toolbar_manager.h` (~150 LOC)
2. `src/gui/toolbar_manager.cpp` (~300 LOC)

**Files to Modify:**
1. `include/kalahari/gui/command.h` - add IconSet factory methods
2. `src/gui/command.cpp` - implement IconSet::fromStandardIcon(), createPlaceholder()
3. `include/kalahari/gui/main_window.h` - replace m_fileToolbar with ToolbarManager
4. `src/gui/main_window.cpp` - update createToolbars(), add toolbar icons
5. `src/gui/register_commands.hpp` - add REG_CMD_TOOL_ICON macro

**Tasks:**
1. ✅ Create ToolbarManager class (150 LOC header, 300 LOC cpp)
2. ✅ Add IconSet factory methods (50 LOC)
3. ✅ Configure toolbar definitions (5 toolbars)
4. ✅ Assign icons to commands (Standard Qt + Placeholders)
5. ✅ Integrate with MainWindow (replace m_fileToolbar)
6. ✅ Add View > Toolbars submenu (toggle actions)
7. ✅ Implement state persistence (save/restore)
8. ✅ Test toolbar visibility, floating, positioning
9. ✅ Update CHANGELOG.md

---

## 📊 Detailed Design

### ToolbarManager::createToolbars() Logic

```cpp
void ToolbarManager::createToolbars(CommandRegistry& registry) {
    // Define toolbar configurations
    m_configs["file"] = {
        "file", "File Toolbar", Qt::TopToolBarArea, true,
        {"file.new", "file.open", "file.save", "file.saveAs", "file.close"}
    };

    m_configs["edit"] = {
        "edit", "Edit Toolbar", Qt::TopToolBarArea, true,
        {"edit.undo", "edit.redo", "_SEPARATOR_",
         "edit.cut", "edit.copy", "edit.paste", "edit.selectAll"}
    };

    m_configs["format"] = {
        "format", "Format Toolbar", Qt::TopToolBarArea, false,
        {"format.bold", "format.italic", "format.underline", "_SEPARATOR_",
         "format.alignLeft", "format.alignCenter", "format.alignRight"}
    };

    // ... etc

    // Create toolbars
    for (const auto& [id, config] : m_configs) {
        QToolBar* toolbar = new QToolBar(tr(config.label.c_str()), m_mainWindow);
        toolbar->setObjectName(QString::fromStdString(id)); // For QSettings

        // Add actions
        for (const std::string& cmdId : config.commandIds) {
            if (cmdId == "_SEPARATOR_") {
                toolbar->addSeparator();
            } else {
                Command* cmd = registry.getCommand(cmdId);
                if (cmd && cmd->canExecute()) {
                    QAction* action = cmd->toQAction(toolbar);
                    toolbar->addAction(action);
                    connect(action, &QAction::triggered, [cmdId, &registry]() {
                        registry.executeCommand(cmdId);
                    });
                }
            }
        }

        // Configure toolbar
        toolbar->setMovable(true);
        toolbar->setFloatable(true);
        toolbar->setIconSize(QSize(24, 24)); // Default 24x24
        toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

        m_mainWindow->addToolBar(config.defaultArea, toolbar);
        toolbar->setVisible(config.defaultVisible);

        m_toolbars[id] = toolbar;
    }
}
```

---

### IconSet Factory Methods

**1. Standard Qt Icons:**
```cpp
IconSet IconSet::fromStandardIcon(QStyle::StandardPixmap icon) {
    IconSet iconSet;
    QStyle* style = QApplication::style();
    QIcon qIcon = style->standardIcon(icon);

    iconSet.icon16 = qIcon.pixmap(16, 16);
    iconSet.icon24 = qIcon.pixmap(24, 24);
    iconSet.icon32 = qIcon.pixmap(32, 32);

    return iconSet;
}
```

**2. Placeholder Icons:**
```cpp
IconSet IconSet::createPlaceholder(const QString& letter, const QColor& color) {
    IconSet iconSet;

    auto createPixmap = [&](int size) {
        QPixmap pixmap(size, size);
        pixmap.fill(color);

        QPainter painter(&pixmap);
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPixelSize(size * 0.6);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, letter);

        return pixmap;
    };

    iconSet.icon16 = createPixmap(16);
    iconSet.icon24 = createPixmap(24);
    iconSet.icon32 = createPixmap(32);

    return iconSet;
}
```

---

### Icon Assignments (Phase 0)

**File Toolbar:**
- New → QStyle::SP_FileIcon
- Open → QStyle::SP_DirOpenIcon
- Save → QStyle::SP_DialogSaveButton
- SaveAs → Placeholder "A" (blue)
- Close → QStyle::SP_DialogCancelButton

**Edit Toolbar:**
- Undo → QStyle::SP_ArrowBack
- Redo → QStyle::SP_ArrowForward
- Cut → Placeholder "X" (red)
- Copy → Placeholder "C" (blue)
- Paste → Placeholder "V" (green)
- SelectAll → Placeholder "A" (purple)

**Format Toolbar:**
- Bold → Placeholder "B" (blue)
- Italic → Placeholder "I" (green)
- Underline → Placeholder "U" (orange)
- AlignLeft → Placeholder "L" (gray)
- AlignCenter → Placeholder "C" (gray)
- AlignRight → Placeholder "R" (gray)

**Later (Phase 1):** Replace placeholders with professional SVG icons.

---

## 📝 REG_CMD_TOOL_ICON Macro

**Problem:** REG_CMD_TOOL doesn't support icons

**Solution:** Add new macro:

```cpp
// Menu command with toolbar, shortcut, and icon
#define REG_CMD_TOOL_ICON(id_, label_tr_, path_, order_, sep_, phase_, shortcut_, icon_, callback_) \
    do { \
        Command cmd; \
        cmd.id = id_; \
        cmd.label = tr(label_tr_).toStdString(); \
        cmd.tooltip = tr(label_tr_).toStdString(); \
        cmd.category = std::string(path_).substr(0, std::string(path_).find('/')); \
        cmd.menuPath = path_; \
        cmd.menuOrder = order_; \
        cmd.addSeparatorAfter = sep_; \
        cmd.phase = phase_; \
        cmd.showInMenu = true; \
        cmd.showInToolbar = true; \
        cmd.shortcut = shortcut_; \
        cmd.icons = icon_; \
        cmd.execute = callback_; \
        registry.registerCommand(cmd); \
        count++; \
    } while(0)
```

**Usage:**
```cpp
REG_CMD_TOOL_ICON("file.new", "New Book...", "FILE/New Book...", 10, false, 0,
                  KeyboardShortcut::fromQKeySequence(QKeySequence::New),
                  IconSet::fromStandardIcon(QStyle::SP_FileIcon),
                  [this]() { onNewDocument(); });
```

---

## ✅ Acceptance Criteria

1. **Multiple Toolbars:**
   - ✅ 5 toolbars created (File, Edit, Format, View, Tools)
   - ✅ Each toolbar shows correct commands
   - ✅ Toolbars have separators where appropriate

2. **Icons:**
   - ✅ All toolbar commands have icons (Standard Qt or Placeholder)
   - ✅ Icons display correctly (24x24 default size)
   - ✅ No missing/broken icons

3. **Configuration:**
   - ✅ View > Toolbars submenu with 5 checkable actions
   - ✅ Clicking action toggles toolbar visibility
   - ✅ Checkmark synced with actual visibility

4. **Persistence:**
   - ✅ Toolbar positions saved on close
   - ✅ Toolbar visibility saved on close
   - ✅ Toolbars restored on next launch
   - ✅ Floating toolbars restored correctly

5. **Behavior:**
   - ✅ Toolbars movable (drag to reposition)
   - ✅ Toolbars floatable (double-click title)
   - ✅ Toolbar buttons execute commands correctly
   - ✅ Tooltips show on hover

6. **Code Quality:**
   - ✅ ToolbarManager class follows RAII pattern
   - ✅ No memory leaks (Qt parent-child ownership)
   - ✅ Consistent with Command Registry architecture

---

## 🚫 Out of Scope

**Phase 0 does NOT include:**
- ❌ Custom SVG icons (placeholder icons only)
- ❌ Icon theme system (Light/Dark variants)
- ❌ Toolbar customization dialog (add/remove commands)
- ❌ Toolbar lock/unlock feature
- ❌ Toolbar size options (Small/Medium/Large)
- ❌ Right-click context menu on toolbars
- ❌ Keyboard navigation for toolbar buttons

**These will come in Phase 1+.**

---

## 📚 References

- **Qt Documentation:**
  - QToolBar: https://doc.qt.io/qt-6/qtoolbar.html
  - QMainWindow Toolbar Management: https://doc.qt.io/qt-6/qmainwindow.html#toolbar-handling
  - QStyle Standard Pixmaps: https://doc.qt.io/qt-6/qstyle.html#StandardPixmap-enum

- **Related Tasks:**
  - Task #00013: Command Registry Qt Migration
  - Task #00016: Complete Menu Design

- **Design Patterns:**
  - Manager Pattern (centralized toolbar management)
  - Factory Pattern (IconSet creation)
  - Observer Pattern (View menu actions sync)

---

## 📊 Effort Estimate

**Total:** ~6-8 hours

**Breakdown:**
1. ToolbarManager class: 2h
2. IconSet factory methods: 1h
3. Icon assignments (50+ commands): 1.5h
4. View menu integration: 1h
5. State persistence: 1h
6. Testing & debugging: 1.5h
7. Documentation: 0.5h

---

**Status:** Proposed
**Next Step:** User approval → Create tasks.md and specs/

---

**Generated with Claude Code** | Task #00019 | 2025-11-22
