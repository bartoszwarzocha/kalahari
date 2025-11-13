# Command Registry Architecture

**Status:** ✅ Complete (Phase 1, Zagadnienie 1.2)
**Created:** 2025-11-13
**Version:** 1.0

---

## Overview

The **Command Registry** is the heart of Kalahari's unified command execution system. It provides a single source of truth for all executable actions (menu items, toolbar buttons, keyboard shortcuts) in both core application and plugins.

**Key benefits:**
- ✅ Single source of truth for commands
- ✅ Automatic UI generation (menus, toolbars)
- ✅ Unified execution path (menu/toolbar/keyboard all route through registry)
- ✅ Plugin-friendly (plugins register commands like core does)
- ✅ Dynamic state management (enable/disable, check/uncheck)
- ✅ Error handling with custom error handlers
- ✅ Thread-safe singleton pattern

---

## Core Components

### 1. Command Structure

```cpp
struct Command {
    // Identification
    std::string id;              // Unique ID ("file.save", "edit.undo")
    std::string label;           // Display label ("Save", "Undo")
    std::string tooltip;         // Tooltip text
    std::string category;        // Category ("File", "Edit", "Format")

    // Visual representation
    IconSet icons;               // Icon set (16/24/32px)
    bool showInMenu = true;      // Show in menu bar
    bool showInToolbar = false;  // Show in toolbar

    // Keyboard binding
    KeyboardShortcut shortcut;   // Keyboard shortcut (Ctrl+S, etc.)
    bool isShortcutCustomizable = true;

    // Execution logic
    std::function<void()> execute;      // Command execution callback
    std::function<bool()> isEnabled;    // Enable/disable state callback
    std::function<bool()> isChecked;    // Check state for toggle commands

    // Plugin integration
    bool isPluginCommand = false;       // True if registered by plugin
    std::string pluginId;               // Plugin ID
    int apiVersion = 1;                 // Command API version

    // Helpers
    bool canExecute() const;            // Check if execute callback exists
    bool checkEnabled() const;          // Call isEnabled() or return true
    bool checkChecked() const;          // Call isChecked() or return false
};
```

**Key points:**
- `id` must be unique across core + plugins
- `execute` callback is mandatory (checked by `canExecute()`)
- `isEnabled` and `isChecked` are optional (default: enabled, unchecked)
- `category` determines menu grouping (case-insensitive)
- `showInMenu`/`showInToolbar` control UI visibility

### 2. CommandRegistry (Singleton)

```cpp
class CommandRegistry {
public:
    static CommandRegistry& getInstance();  // Thread-safe Meyers singleton

    // Registration
    void registerCommand(const Command& command);
    void unregisterCommand(const std::string& commandId);
    bool isCommandRegistered(const std::string& commandId) const;

    // Query
    const Command* getCommand(const std::string& commandId) const;
    Command* getCommand(const std::string& commandId);
    std::vector<Command> getCommandsByCategory(const std::string& category) const;
    std::vector<Command> getAllCommands() const;
    std::vector<std::string> getCategories() const;

    // Execution
    CommandExecutionResult executeCommand(const std::string& commandId);
    bool canExecute(const std::string& commandId) const;
    bool isChecked(const std::string& commandId) const;

    // Error handling
    void setErrorHandler(CommandErrorHandler handler);
    CommandErrorHandler getErrorHandler() const;

    // Utility
    size_t getCommandCount() const;
    void clear();  // For testing only
};
```

**Storage:**
- `std::unordered_map<std::string, Command> m_commands`
- Key = command ID, Value = Command struct

**Thread-safety:**
- Singleton initialization: thread-safe (C++11 guarantee)
- Registration: main thread only (at startup/plugin load)
- Execution: any thread (callbacks handle threading)

### 3. KeyboardShortcut

```cpp
struct KeyboardShortcut {
    int keyCode = 0;           // wxKeyCode ('S', WXK_F1, WXK_RETURN)
    bool ctrl = false;         // Ctrl/Cmd modifier
    bool alt = false;          // Alt modifier
    bool shift = false;        // Shift modifier

    wxString toString() const;                          // "Ctrl+S", "F1"
    static KeyboardShortcut fromString(const wxString& str);
    bool isEmpty() const;
    bool operator==(const KeyboardShortcut& other) const;
    bool operator<(const KeyboardShortcut& other) const;  // For std::map
};
```

**Examples:**
- `KeyboardShortcut('S', true)` → Ctrl+S
- `KeyboardShortcut('N', true, false, true)` → Ctrl+Shift+N
- `KeyboardShortcut(WXK_F1)` → F1

### 4. IconSet

```cpp
struct IconSet {
    wxBitmap icon16;  // 16x16 for menus
    wxBitmap icon24;  // 24x24 for standard toolbar
    wxBitmap icon32;  // 32x32 for large toolbar

    explicit IconSet(const wxString& path);  // Load and scale
    bool isEmpty() const;
};
```

**Usage:**
- Load from file: `IconSet("icons/save.png")`
- Automatically scales to 3 sizes
- Future: SVG support (Phase 2+)

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                          USER ACTIONS                               │
│  (Click menu item, Click toolbar button, Press keyboard shortcut)  │
└───────────────────────────┬─────────────────────────────────────────┘
                            │
                            ▼
┌───────────────────────────────────────────────────────────────────┐
│                      EVENT HANDLERS                               │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────────┐   │
│  │ wxEVT_MENU  │  │ wxEVT_TOOL   │  │ wxEVT_KEY_DOWN (Accel) │   │
│  └──────┬──────┘  └──────┬───────┘  └──────────┬─────────────┘   │
│         │                │                      │                 │
│         └────────────────┼──────────────────────┘                 │
│                          │                                        │
│         Lambda captures commandId (e.g., "file.save")            │
│                          │                                        │
│                          ▼                                        │
│         CommandRegistry::getInstance().executeCommand(commandId)  │
└───────────────────────────┬───────────────────────────────────────┘
                            │
                            ▼
┌───────────────────────────────────────────────────────────────────┐
│               COMMANDREGISTRY (Singleton)                         │
│                                                                   │
│   Storage: std::unordered_map<std::string, Command>              │
│                                                                   │
│   executeCommand(id):                                             │
│   1. Find command by ID (getCommand)                              │
│   2. Check command exists                                         │
│   3. Check command.canExecute() (has execute callback)            │
│   4. Check command.checkEnabled() (isEnabled returns true)        │
│   5. Call command.execute()                                       │
│   6. Catch exceptions → call error handler                        │
│   7. Return CommandExecutionResult                                │
│                                                                   │
└───────────────────────────┬───────────────────────────────────────┘
                            │
                            ▼
┌───────────────────────────────────────────────────────────────────┐
│                  COMMAND EXECUTION                                │
│                  command.execute()                                │
│                  (Lambda with captured context)                   │
│                                                                   │
│  Example: [this]() {                                              │
│      Document* doc = getActiveDocument();                         │
│      doc->save();                                                 │
│      m_statusBar->SetStatusText("Saved");                         │
│  }                                                                │
└───────────────────────────────────────────────────────────────────┘
```

---

## UI Builders Integration

### MenuBuilder

**Flow:**

```
MainWindow::createMenuBarDynamic()
    │
    ├─ MenuBuilder builder;
    ├─ CommandRegistry& registry = CommandRegistry::getInstance();
    └─ wxMenuBar* menuBar = builder.buildMenuBar(registry, this);

builder.buildMenuBar(registry, window):
    │
    ├─ For each category ("file", "edit", "format", "view", "help"):
    │   │
    │   └─ wxMenu* menu = buildMenu(registry, category, label, window);
    │       │
    │       ├─ std::vector<Command> commands = registry.getCommandsByCategory(category);
    │       │
    │       └─ For each command in commands:
    │           │
    │           ├─ Skip if command.showInMenu == false
    │           │
    │           └─ wxMenuItem* item = createMenuItem(command, menu, window);
    │               │
    │               ├─ int itemId = wxWindow::NewControlId();
    │               ├─ wxMenuItem* item = new wxMenuItem(menu, itemId, label, tooltip);
    │               ├─ item->SetBitmap(command.icons.icon16);
    │               │
    │               └─ window->Bind(wxEVT_MENU, [commandId](wxCommandEvent&) {
    │                       CommandRegistry::getInstance().executeCommand(commandId);
    │                   }, itemId);
    │
    └─ Return wxMenuBar
```

**Key points:**
- Queries `getCommandsByCategory()` for each menu
- Filters commands with `showInMenu == true`
- Generates unique `wxID` for each menu item (wxWindow::NewControlId())
- Binds `wxEVT_MENU` to lambda that captures `commandId`
- Lambda calls `CommandRegistry::executeCommand(commandId)`

**Example registration:**

```cpp
void MainWindow::registerFileCommands() {
    CommandRegistry& registry = CommandRegistry::getInstance();

    // file.save command
    Command cmd;
    cmd.id = "file.save";
    cmd.label = _("Save").ToStdString();
    cmd.tooltip = _("Save current document").ToStdString();
    cmd.category = "File";
    cmd.icons = IconSet("icons/save.png");
    cmd.shortcut = KeyboardShortcut('S', true);  // Ctrl+S
    cmd.showInMenu = true;
    cmd.showInToolbar = true;
    cmd.execute = [this]() {
        Document* doc = getActiveDocument();
        if (doc) {
            doc->save();
            m_statusBar->SetStatusText(_("Document saved"), 0);
        }
    };
    cmd.isEnabled = [this]() {
        Document* doc = getActiveDocument();
        return doc && doc->isModified();
    };

    registry.registerCommand(cmd);
}
```

### ToolbarBuilder

**Flow:**

```
MainWindow::createToolBarDynamic()
    │
    ├─ ToolbarBuilder builder;
    ├─ CommandRegistry& registry = CommandRegistry::getInstance();
    └─ wxToolBar* toolbar = builder.buildToolBar(registry, this, this);

builder.buildToolBar(registry, window, parent):
    │
    ├─ wxToolBar* toolbar = parent->CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT);
    │
    ├─ For each category in toolbarOrder ("file", "edit", "format"):
    │   │
    │   ├─ addSeparator(toolbar);  // Between categories
    │   │
    │   └─ addToolsFromCategory(toolbar, registry, category, window);
    │       │
    │       ├─ std::vector<Command> commands = registry.getCommandsByCategory(category);
    │       │
    │       └─ For each command in commands:
    │           │
    │           ├─ Skip if command.showInToolbar == false
    │           │
    │           └─ createToolItem(toolbar, command, window, iconSize);
    │               │
    │               ├─ int itemId = wxWindow::NewControlId();
    │               ├─ wxBitmap icon = getIcon(command, iconSize);
    │               ├─ toolbar->AddTool(itemId, label, icon, tooltip);
    │               │
    │               └─ window->Bind(wxEVT_TOOL, [commandId](wxCommandEvent&) {
    │                       CommandRegistry::getInstance().executeCommand(commandId);
    │                   }, itemId);
    │
    ├─ toolbar->Realize();
    └─ Return wxToolBar
```

**Key differences from MenuBuilder:**
- Filters commands with `showInToolbar == true` (not `showInMenu`)
- Uses `icons.icon24` or `icons.icon32` (not `icon16`)
- Adds separators between categories
- Calls `toolbar->Realize()` at the end

---

## ShortcutManager Integration

**ShortcutManager** is a separate singleton that manages keyboard shortcut bindings:

```cpp
class ShortcutManager {
public:
    static ShortcutManager& getInstance();

    // Binding
    bool bindShortcut(const KeyboardShortcut& shortcut, const std::string& commandId);
    void unbindShortcut(const KeyboardShortcut& shortcut);
    bool isShortcutBound(const KeyboardShortcut& shortcut) const;

    // Execution
    bool executeShortcut(const KeyboardShortcut& shortcut);

    // Query
    std::optional<std::string> getCommandIdForShortcut(const KeyboardShortcut& shortcut) const;
    std::optional<KeyboardShortcut> getShortcutForCommand(const std::string& commandId) const;

    // Persistence
    bool saveToFile(const std::string& filename);
    bool loadFromFile(const std::string& filename);
};
```

**Storage:**
- `std::map<KeyboardShortcut, std::string>` (shortcut → commandId)

**Integration with CommandRegistry:**

```cpp
bool ShortcutManager::executeShortcut(const KeyboardShortcut& shortcut) {
    auto it = m_bindings.find(shortcut);
    if (it == m_bindings.end()) {
        return false;  // Shortcut not bound
    }

    std::string commandId = it->second;
    CommandExecutionResult result = CommandRegistry::getInstance().executeCommand(commandId);
    return result == CommandExecutionResult::Success;
}
```

**Workflow:**

1. User presses keyboard combination (e.g., Ctrl+S)
2. wxAcceleratorTable catches event → calls handler
3. Handler creates `KeyboardShortcut` from event
4. Calls `ShortcutManager::getInstance().executeShortcut(shortcut)`
5. ShortcutManager looks up commandId in bindings map
6. Calls `CommandRegistry::getInstance().executeCommand(commandId)`
7. Command executes (same path as menu/toolbar)

**Example binding:**

```cpp
void MainWindow::setupShortcuts() {
    ShortcutManager& shortcuts = ShortcutManager::getInstance();

    // Bind shortcuts from Command descriptors
    CommandRegistry& registry = CommandRegistry::getInstance();
    for (const auto& command : registry.getAllCommands()) {
        if (!command.shortcut.isEmpty()) {
            shortcuts.bindShortcut(command.shortcut, command.id);
        }
    }
}
```

---

## Plugin Integration Guide

### How Plugins Register Commands

Plugins register commands using the same API as core application:

**1. In plugin initialization:**

```cpp
// Plugin: ExportPlugin (exports documents to PDF, DOCX, etc.)
class ExportPlugin : public IExporter {
public:
    void onLoad() override {
        CommandRegistry& registry = CommandRegistry::getInstance();

        // Register export commands
        registerExportPDFCommand(registry);
        registerExportDOCXCommand(registry);
        registerExportEPUBCommand(registry);
    }

    void onUnload() override {
        CommandRegistry& registry = CommandRegistry::getInstance();

        // Unregister all plugin commands
        registry.unregisterCommand("plugin.export.pdf");
        registry.unregisterCommand("plugin.export.docx");
        registry.unregisterCommand("plugin.export.epub");
    }

private:
    void registerExportPDFCommand(CommandRegistry& registry) {
        Command cmd;
        cmd.id = "plugin.export.pdf";  // Plugin prefix
        cmd.label = "Export to PDF";
        cmd.tooltip = "Export current document to PDF format";
        cmd.category = "Export";  // Custom category (creates new menu)
        cmd.icons = IconSet("plugins/export/icons/pdf.png");
        cmd.shortcut = KeyboardShortcut('P', true, true);  // Ctrl+Alt+P
        cmd.showInMenu = true;
        cmd.showInToolbar = false;
        cmd.execute = [this]() {
            exportToPDF();
        };
        cmd.isEnabled = [this]() {
            return hasActiveDocument();
        };
        cmd.isPluginCommand = true;
        cmd.pluginId = "export-plugin";
        cmd.apiVersion = 1;

        registry.registerCommand(cmd);
    }

    void exportToPDF() {
        // Plugin-specific export logic
        m_logger->info("Exporting to PDF...");
        // ...
    }

    bool hasActiveDocument() {
        // Check if there's an active document
        return DocumentManager::getInstance().getActiveDocument() != nullptr;
    }
};
```

**2. MenuBuilder automatically adds plugin commands:**

When MenuBuilder calls `registry.getCommandsByCategory("Export")`, it will return plugin commands too!

**Result:**
- Main menu bar gets new "Export" menu (from category)
- Menu contains "Export to PDF", "Export to DOCX", "Export to EPUB"
- Clicking menu item calls `CommandRegistry::executeCommand("plugin.export.pdf")`
- Command executes plugin's `exportToPDF()` method

### Plugin Command Naming Convention

**Core commands:**
- `"category.action"` (e.g., `"file.save"`, `"edit.undo"`)
- Category: file, edit, format, view, help

**Plugin commands:**
- `"plugin.pluginName.action"` (e.g., `"plugin.export.pdf"`)
- Prefix `"plugin."` to avoid conflicts with core
- Plugin name: short identifier (e.g., `"export"`, `"grammar"`)

### Plugin Categories

Plugins can:
- **Extend existing categories** (e.g., add to "Edit" menu)
- **Create new categories** (e.g., "Export", "AI", "Research")

MenuBuilder dynamically builds menus based on registered categories.

**Example: Adding to existing "Edit" category:**

```cpp
Command cmd;
cmd.id = "plugin.grammar.check";
cmd.label = "Check Grammar";
cmd.category = "Edit";  // Extends existing Edit menu
cmd.execute = [this]() { runGrammarCheck(); };
registry.registerCommand(cmd);
```

Result: "Check Grammar" appears in existing Edit menu.

### Plugin Command Lifecycle

```
Plugin Load:
1. Plugin.onLoad() called
2. Plugin registers commands via CommandRegistry::registerCommand()
3. MenuBuilder/ToolbarBuilder already have pointers to CommandRegistry
4. Next menu rebuild: Plugin commands appear automatically

Plugin Unload:
1. Plugin.onUnload() called
2. Plugin unregisters commands via CommandRegistry::unregisterCommand()
3. Next menu rebuild: Plugin commands disappear
4. Shortcuts cleared automatically (ShortcutManager cleanup)
```

**Important:** Plugins must unregister all commands in `onUnload()` to avoid dangling function pointers!

---

## Execution Flow Examples

### Example 1: User Clicks "File → Save"

```
1. User clicks menu item
   └─> wxEVT_MENU event fired with itemId = 1234

2. Event handler (lambda bound to itemId 1234)
   └─> std::string commandId = "file.save";  // Captured in lambda
   └─> CommandRegistry::getInstance().executeCommand("file.save");

3. CommandRegistry::executeCommand("file.save")
   ├─> Find command: auto* cmd = getCommand("file.save");
   ├─> Check exists: if (!cmd) return CommandNotFound;
   ├─> Check can execute: if (!cmd->canExecute()) return NoExecuteCallback;
   ├─> Check enabled: if (!cmd->checkEnabled()) return CommandDisabled;
   ├─> Execute: cmd->execute();  // Calls registered lambda
   │   └─> Lambda: [this]() {
   │           Document* doc = getActiveDocument();
   │           if (doc) doc->save();
   │           m_statusBar->SetStatusText("Saved");
   │       }
   └─> Return Success

4. Document saved, status bar updated
```

### Example 2: User Presses Ctrl+S

```
1. User presses Ctrl+S
   └─> wxAcceleratorTable catches event
   └─> Calls shortcut handler

2. Shortcut handler
   └─> KeyboardShortcut shortcut('S', true, false, false);
   └─> ShortcutManager::getInstance().executeShortcut(shortcut);

3. ShortcutManager::executeShortcut(shortcut)
   ├─> Look up commandId: auto it = m_bindings.find(shortcut);
   ├─> Found: commandId = "file.save"
   └─> CommandRegistry::getInstance().executeCommand("file.save");

4. (Same as Example 1, step 3-4)
```

### Example 3: Command Disabled (No Active Document)

```
1. User clicks "File → Save"
   └─> executeCommand("file.save") called

2. CommandRegistry checks:
   ├─> Command exists: ✅
   ├─> Has execute callback: ✅
   └─> Check enabled: cmd->checkEnabled()
       └─> Calls isEnabled(): return doc && doc->isModified();
       └─> Returns false (no document open)

3. CommandRegistry returns: CommandDisabled

4. No execution, menu item should be grayed out
   (MenuBuilder updates enable state separately)
```

---

## Dynamic State Management

### Enable/Disable Commands

Commands can be dynamically enabled/disabled based on application state:

```cpp
cmd.isEnabled = [this]() {
    // Enable Save only if document exists and is modified
    Document* doc = getActiveDocument();
    return doc && doc->isModified();
};
```

**UI Update:**
MenuBuilder and ToolbarBuilder should periodically call `cmd->checkEnabled()` to update UI state.

**Recommended approach:**
```cpp
void MainWindow::updateCommandStates() {
    CommandRegistry& registry = CommandRegistry::getInstance();

    for (const auto& command : registry.getAllCommands()) {
        bool enabled = command.checkEnabled();

        // Update menu item
        if (wxMenuItem* item = m_menuBar->FindItem(command.id)) {
            item->Enable(enabled);
        }

        // Update toolbar button
        if (m_toolBar) {
            m_toolBar->EnableTool(toolId, enabled);
        }
    }
}
```

**Call `updateCommandStates()` when:**
- Document opened/closed
- Document modified/saved
- Selection changed
- Focus changed

### Toggle Commands (Checked State)

Commands can have checked state (for toggle menu items):

```cpp
cmd.isChecked = [this]() {
    // Check if bold formatting is active
    return m_editor->isBoldActive();
};
```

**UI Update:**
```cpp
void MainWindow::updateCommandStates() {
    for (const auto& command : registry.getAllCommands()) {
        if (wxMenuItem* item = m_menuBar->FindItem(command.id)) {
            item->Check(command.checkChecked());
        }
    }
}
```

---

## Error Handling

### CommandExecutionResult

```cpp
enum class CommandExecutionResult {
    Success,           // Command executed successfully
    CommandNotFound,   // Command ID not registered
    CommandDisabled,   // Command exists but is disabled
    NoExecuteCallback, // Command has no execute callback
    ExecutionFailed    // Execution threw exception
};
```

### Custom Error Handler

```cpp
// Set error handler (once, at startup)
CommandRegistry::getInstance().setErrorHandler(
    [](const std::string& commandId, const std::string& errorMessage) {
        wxMessageBox(
            wxString::Format("Command '%s' failed: %s", commandId, errorMessage),
            "Command Execution Error",
            wxOK | wxICON_ERROR
        );
    }
);

// Execute command (error handler called automatically on failure)
CommandExecutionResult result = registry.executeCommand("file.save");
if (result == CommandExecutionResult::Success) {
    m_statusBar->SetStatusText("Saved");
}
```

---

## Best Practices

### 1. Command ID Naming

**Core commands:**
```
category.action
file.new, file.open, file.save, file.saveAs
edit.undo, edit.redo, edit.cut, edit.copy, edit.paste
format.bold, format.italic, format.underline
```

**Plugin commands:**
```
plugin.pluginName.action
plugin.export.pdf, plugin.grammar.check, plugin.ai.complete
```

### 2. Command Registration

**Register at startup:**
```cpp
void MainWindow::initializeCommands() {
    registerFileCommands();
    registerEditCommands();
    registerFormatCommands();
    registerViewCommands();
    registerHelpCommands();
}
```

**Register in separate methods by category:**
```cpp
void MainWindow::registerFileCommands() {
    CommandRegistry& registry = CommandRegistry::getInstance();

    // file.new
    {
        Command cmd;
        cmd.id = "file.new";
        // ...
        registry.registerCommand(cmd);
    }

    // file.open
    {
        Command cmd;
        cmd.id = "file.open";
        // ...
        registry.registerCommand(cmd);
    }

    // ...
}
```

### 3. Lambda Context Capture

**Capture `this` by value (safe):**
```cpp
cmd.execute = [this]() {
    Document* doc = getActiveDocument();
    doc->save();
};
```

**Avoid capturing local variables by reference (dangling reference!):**
```cpp
// BAD - document reference may be invalid when command executes
Document& doc = getActiveDocument();
cmd.execute = [&doc]() {  // ❌ Dangling reference!
    doc.save();
};

// GOOD - capture this, get document at execution time
cmd.execute = [this]() {  // ✅ Safe
    Document* doc = getActiveDocument();
    if (doc) doc->save();
};
```

### 4. Check Enabled State

Always provide `isEnabled` callback for commands that depend on application state:

```cpp
cmd.isEnabled = [this]() {
    Document* doc = getActiveDocument();
    return doc != nullptr;  // Enable only if document exists
};
```

### 5. Shortcut Conflicts

Avoid shortcut conflicts:
- Core commands use standard shortcuts (Ctrl+S, Ctrl+C, Ctrl+V)
- Plugins use Ctrl+Alt+X or Ctrl+Shift+X combinations
- Check `ShortcutManager::isShortcutBound()` before registering

---

## Thread-Safety Notes

### Singleton Initialization

**Thread-safe (C++11 guarantee):**
```cpp
CommandRegistry& registry = CommandRegistry::getInstance();
```

### Command Registration

**NOT thread-safe:**
- Register commands in main thread only
- Typically at startup or plugin load (main thread)

### Command Execution

**Thread-safe (with caveats):**
- `executeCommand()` can be called from any thread
- Callbacks must handle threading (e.g., use wxPostEvent for GUI updates)

**Example thread-safe execution:**
```cpp
// Background thread
void WorkerThread::run() {
    // Safe to call executeCommand
    CommandRegistry::getInstance().executeCommand("plugin.ai.complete");
}

// Command callback - post event to GUI thread
cmd.execute = [this]() {
    // This lambda runs in calling thread (worker thread)
    // Post event to GUI thread for UI updates
    wxCommandEvent event(wxEVT_COMMAND_MENU_SELECTED);
    event.SetString("AI completion done");
    wxPostEvent(m_mainWindow, event);
};
```

---

## Performance Considerations

### Lookup Performance

- `getCommand(id)`: **O(1)** (unordered_map lookup)
- `getCommandsByCategory(category)`: **O(n)** (iterates all commands)
- `getAllCommands()`: **O(n)** (copies all commands)

**Recommendation:**
- Cache results of `getCommandsByCategory()` if called frequently
- Use `getCommand()` for individual lookups (fastest)

### Memory Footprint

- Each command: ~200-300 bytes (depends on lambda captures)
- 100 commands ≈ 20-30 KB
- IconSet bitmaps: ~10-50 KB per command (3 sizes)

---

## Future Enhancements (Phase 2+)

### 1. Command Groups

Group related commands (e.g., "Text Formatting" group with Bold, Italic, Underline):

```cpp
struct CommandGroup {
    std::string id;
    std::string label;
    std::vector<std::string> commandIds;
};
```

### 2. Conditional Visibility

Hide/show commands based on context:

```cpp
cmd.isVisible = [this]() {
    // Show export commands only if document has content
    Document* doc = getActiveDocument();
    return doc && !doc->isEmpty();
};
```

### 3. Command History

Track command execution for undo/redo:

```cpp
struct CommandHistoryEntry {
    std::string commandId;
    std::chrono::time_point<std::chrono::system_clock> timestamp;
    std::any undoData;  // Command-specific undo information
};
```

### 4. Command Macros

Record and replay command sequences:

```cpp
CommandMacro macro;
macro.recordCommand("edit.selectAll");
macro.recordCommand("format.bold");
macro.recordCommand("edit.copy");
macro.save("BoldCopyMacro");

// Later
macro.load("BoldCopyMacro");
macro.replay();
```

---

## References

**Source files:**
- `include/kalahari/gui/command.h` - Command structures
- `include/kalahari/gui/command_registry.h` - CommandRegistry interface
- `src/gui/command_registry.cpp` - CommandRegistry implementation
- `src/gui/menu_builder.cpp` - MenuBuilder implementation
- `src/gui/toolbar_builder.cpp` - ToolbarBuilder implementation
- `include/kalahari/gui/shortcut_manager.h` - ShortcutManager interface
- `src/gui/shortcut_manager.cpp` - ShortcutManager implementation

**Tasks:**
- Task #00031 - MenuBuilder Class (2025-11-13)
- Task #00032 - ToolbarBuilder Class (2025-11-13)
- Task #00033 - Settings Command Integration (2025-11-13)
- Task #00034 - Command Registry Architecture Documentation (2025-11-13)

**Related documentation:**
- `project_docs/03_architecture.md` - Overall application architecture
- `project_docs/04_plugin_system.md` - Plugin system architecture
- `project_docs/08_gui_design.md` - GUI design patterns

---

**Document Version:** 1.0
**Last Updated:** 2025-11-13
**Author:** Claude (AI Assistant)
