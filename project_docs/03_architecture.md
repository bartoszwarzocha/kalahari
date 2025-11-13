# Architecture Design

> **System Architecture** - Core design patterns, component structure, and technical decisions

**Status:** ✅ Complete
**Version:** 1.0
**Last Updated:** 2025-10-25

---

## Overview

### Architectural Vision

Kalahari's architecture is designed around **five core principles**:

1. **Separation of Concerns** - MVP pattern for GUI, clear layer boundaries
2. **Extensibility** - Plugin architecture from day zero
3. **Performance** - Lazy loading, async operations, efficient threading
4. **Reliability** - Hybrid error handling, comprehensive testing
5. **Maintainability** - Modern C++20, clear conventions, documentation

### Technology Stack

**Core Technologies:**
- **Language:** C++20 (GCC 10+, Clang 10+, MSVC 2019+)
- **GUI:** wxWidgets 3.2+ with wxAUI (dockable panels)
- **Build:** CMake 3.21+ with vcpkg (manifest mode)
- **Plugins:** Python 3.11 embedded via pybind11
- **Testing:** Catch2 v3 (BDD style)
- **Logging:** spdlog (fast, structured)
- **JSON:** nlohmann_json
- **Compression:** libzip (for .klh files)
- **Database:** SQLite3 (Phase 2+ for full-text search)

### Key Architectural Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **GUI Pattern** | MVP (Model-View-Presenter) | Clear separation, testable presenters, wxWidgets-friendly |
| **Error Handling** | Hybrid (exceptions + error codes + wxLog*) | Pragmatic for desktop app, clear guidelines |
| **Dependency Management** | Hybrid (Singletons infrastructure + DI business logic) | Infrastructure stable, business logic testable |
| **Threading** | Dynamic thread pool (2-4 workers) | CPU-aware, prevents over-subscription |
| **Memory** | Lazy loading from Phase 1 + smart pointers | Large document support, RAII principles |
| **Undo/Redo** | Command pattern (100 commands, configurable) | Industry standard, merge consecutive edits |
| **Document Model** | Composite pattern (Book → Parts → Chapters) | Flexible nested structure, tree operations |
| **Plugin System** | Python 3.11 embedded, .kplugin format | Flexibility, ecosystem, easy iteration |

---

## Design Principles

### 1. Modern C++ (C++20)

**Use modern features:**
- Smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- RAII everywhere
- Concepts (C++20 type constraints)
- Ranges (functional iteration)
- No raw `new`/`delete`

**Example - RAII file handle:**
```cpp
class FileHandle {
    std::unique_ptr<std::FILE, decltype(&std::fclose)> m_file;
    
public:
    FileHandle(const std::string& path) 
        : m_file(std::fopen(path.c_str(), "rb"), &std::fclose) {
        if (!m_file) {
            throw FileNotFoundException(path);
        }
    }
    // Automatic cleanup via destructor
};
```

### 2. Testability First

**Design for testing:**
- Dependency Injection for business logic
- Interfaces for mocking
- Pure functions where possible

### 3. Convention Over Configuration

**Sensible defaults:**
- Auto-save: 5 minutes
- Undo stack: 100 commands
- Worker threads: CPU cores (clamped 2-4)

### 4. Fail Fast in Debug, Graceful in Release

**Debug builds:** Extensive assertions, verbose logging, sanitizers

**Release builds:** Graceful degradation, user-friendly errors

---

## High-Level Architecture

### Layer Model

```
┌────────────────────────────────────────┐
│     Presentation Layer (wxWidgets)     │
│  Views (Passive) + Presenters (Logic)  │
├────────────────────────────────────────┤
│       Business Logic Layer (C++)       │
│   Models + Services + Domain Logic     │
├────────────────────────────────────────┤
│      Plugin Layer (Python + C++)       │
│  PluginManager + Extension Points      │
├────────────────────────────────────────┤
│    Infrastructure Layer (Libraries)    │
│  File I/O + Logging + Database         │
└────────────────────────────────────────┘
```

**Key interactions:**
- View → Presenter (user events)
- Presenter → Model (business operations)
- Model → EventBus (change notifications)
- EventBus → Plugins (extension points)

---

## Design Patterns

### MVP (Model-View-Presenter)

**Why MVP not MVC:**
- Testable presenters (no wxWidgets dependencies)
- Passive views (minimal logic)
- Clear separation of concerns

**Example:**
```cpp
// Model - Pure C++, no GUI
class DocumentModel {
    std::string m_text;
public:
    const std::string& getText() const { return m_text; }
    void setText(const std::string& text) { m_text = text; }
    int getWordCount() const;
};

// View - wxWidgets, passive
class DocumentView : public wxPanel {
    wxRichTextCtrl* m_editor;
public:
    std::string getText() const { return m_editor->GetValue().ToStdString(); }
    void setText(const std::string& text) { m_editor->SetValue(text); }
};

// Presenter - Mediator
class DocumentPresenter {
    DocumentModel* m_model;
    DocumentView* m_view;
public:
    DocumentPresenter(DocumentModel* model, DocumentView* view);
    void onTextChanged(const std::string& text) {
        m_model->setText(text);
        EventBus::instance().emit(TextChangedEvent{text});
    }
};
```

### Command Pattern (Undo/Redo)

**Configuration:**
- Default: 100 commands
- Configurable via settings
- Merge consecutive typing

**Implementation:**
```cpp
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string description() const = 0;
    virtual bool canMerge(const Command* other) const { return false; }
    virtual void mergeWith(Command* other) {}
};

class UndoStack {
    std::vector<std::unique_ptr<Command>> m_commands;
    size_t m_currentIndex = 0;
    size_t m_maxSize = 100;  // Configurable
public:
    void execute(std::unique_ptr<Command> cmd);
    void undo();
    void redo();
};
```

### Composite Pattern (Document Model)

**Structure:** Book → Parts → Chapters

```cpp
class BookElement {
public:
    virtual std::string getId() const = 0;
    virtual std::string getTitle() const = 0;
    virtual int getWordCount() const = 0;
    virtual bool isContainer() const { return false; }
};

class Chapter : public BookElement {
    std::string m_content;
public:
    int getWordCount() const override;
};

class Part : public BookElement {
    std::vector<std::shared_ptr<Chapter>> m_chapters;
public:
    bool isContainer() const override { return true; }
    int getWordCount() const override {
        int total = 0;
        for (auto& ch : m_chapters) total += ch->getWordCount();
        return total;
    }
};
```

### Command Registry System

**Purpose:** Unified command execution system for menu items, toolbar buttons, and keyboard shortcuts.

**Implementation:**
```cpp
// Command descriptor
struct Command {
    std::string id;                    // "file.save", "edit.undo"
    std::string label;                 // "Save", "Undo"
    std::string category;              // "File", "Edit"
    std::function<void()> execute;     // Execution callback
    std::function<bool()> isEnabled;   // Dynamic enable/disable
    KeyboardShortcut shortcut;         // Ctrl+S, etc.
    bool showInMenu = true;
    bool showInToolbar = false;
};

// Central registry (Singleton)
class CommandRegistry {
    std::unordered_map<std::string, Command> m_commands;
public:
    static CommandRegistry& getInstance();
    void registerCommand(const Command& command);
    CommandExecutionResult executeCommand(const std::string& commandId);
    std::vector<Command> getCommandsByCategory(const std::string& category) const;
};
```

**Benefits:**
- Single source of truth for all commands
- Automatic UI generation (MenuBuilder, ToolbarBuilder)
- Plugin-friendly (plugins register commands like core)
- Dynamic state management (enable/disable based on app state)
- Unified execution path (menu/toolbar/keyboard all route through registry)

**See:** [project_docs/18_command_registry_architecture.md](18_command_registry_architecture.md) for complete documentation.

---

## Error Handling Strategy

### Decision: Hybrid Approach

**Use exceptions for:**
- Programmer errors (`std::invalid_argument`, `std::logic_error`)
- Unexpected errors (`std::bad_alloc`)
- Unrecoverable errors (Python init failure)

**Use error codes for:**
- Expected failures (file not found)
- User input validation
- I/O operations

**Use wxLog* for:**
- User-facing error messages
- Warnings and info messages

**Example:**
```cpp
// Exceptions for programmer errors
void setText(const std::string& text) {
    if (text.empty()) {
        throw std::invalid_argument("Text cannot be empty");
    }
}

// Error codes for expected failures
std::optional<Document> loadDocument(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return std::nullopt;  // Expected failure
    }
    return Document::fromFile(path);
}

// wxLog for user messages
void onSaveError() {
    wxLogError("Failed to save document");
}
```

---

## Dependency Management

### Decision: Hybrid (Singletons + DI)

**Infrastructure services → Singletons:**
- EventBus
- CommandRegistry
- PluginManager
- SettingsManager
- LoggingService

**Business logic → Dependency Injection:**
- Models (DocumentModel, ChapterModel)
- Presenters (DocumentPresenter)
- Services (ExportService)

**Example:**
```cpp
// Singleton (Meyer's pattern - thread-safe C++11+)
class EventBus {
public:
    static EventBus& instance() {
        static EventBus inst;
        return inst;
    }
private:
    EventBus() = default;
};

// Dependency Injection
class DocumentPresenter {
    DocumentModel* m_model;  // Injected
    DocumentView* m_view;    // Injected
public:
    DocumentPresenter(DocumentModel* model, DocumentView* view)
        : m_model(model), m_view(view) {}
};
```

---

## Threading Model

### Decision: Dynamic Thread Pool (2-4 workers)

**Configuration:**
- Detect CPU cores via `std::thread::hardware_concurrency()`
- Workers: cores clamped to [2, 4] range
- Example: 8-core CPU → 4 workers (not 8, avoids over-subscription)

**Thread safety guidelines:**

1. **GUI operations MUST run on main thread:**
```cpp
// WRONG - will crash
threadPool.submit([]() {
    wxLogMessage("Done");  // CRASH
});

// CORRECT - marshal to GUI thread
threadPool.submit([]() {
    auto result = heavyWork();
    wxTheApp->CallAfter([result]() {
        wxLogMessage("Done: %d", result);
    });
});
```

2. **Python GIL handling:**
```cpp
threadPool.submit([=]() {
    py::gil_scoped_acquire acquire;  // Get GIL
    {
        py::gil_scoped_release release;  // Release during work
        plugin.execute(command);
    }
    // Marshal result to GUI
    wxTheApp->CallAfter([=]() {
        EventBus::instance().emit(PluginComplete{});
    });
});
```

---

## Memory Management

### Decision: Lazy Loading from Phase 1

**Strategy:**
- Load chapter metadata (titles, word counts) eagerly
- Load chapter content on-demand
- Unload inactive chapters

**Example:**
```cpp
class DocumentModel {
    std::vector<ChapterMetadata> m_chapterMetadata;  // Always loaded
    std::unordered_map<std::string, std::unique_ptr<ChapterData>> m_loadedChapters;  // Lazy
    
public:
    // Fast - metadata only
    size_t getChapterCount() const { 
        return m_chapterMetadata.size(); 
    }
    
    // Lazy load
    const std::string& getChapterContent(const std::string& id) {
        if (m_loadedChapters.find(id) == m_loadedChapters.end()) {
            m_loadedChapters[id] = loadChapter(id);
        }
        return m_loadedChapters[id]->getContent();
    }
};
```

---

## Event System

**EventBus (Singleton):**
- Thread-safe (std::mutex)
- Async emit (marshalled to GUI thread)
- Type-safe subscriptions

**Common events:**
- `DocumentCreated`, `DocumentOpened`, `DocumentClosed`
- `TextChanged`, `SelectionChanged`
- `ChapterAdded`, `ChapterDeleted`, `ChapterRenamed`
- `PluginLoaded`, `PluginUnloaded`
- `GoalReached`, `StreakAchieved`

**Example:**
```cpp
// Subscribe
int id = EventBus::instance().subscribe<TextChangedEvent>(
    [](const TextChangedEvent& e) {
        spdlog::debug("Text changed: {}", e.newText);
    }
);

// Emit (async - GUI thread)
EventBus::instance().emitAsync(TextChangedEvent{newText});
```

---

## File Format

### .klh Structure (ZIP Container)

```
project.klh
├── manifest.json         # Metadata (title, author, word count)
├── structure.json        # Chapter/Part hierarchy
├── chapters/
│   ├── ch1.rtf          # Rich Text Format
│   ├── ch2.rtf
│   └── ch3.rtf
├── metadata/
│   ├── characters.json  # Character bank
│   ├── locations.json   # Location bank
│   └── notes.json       # Notes
└── settings.json         # Project-specific settings
```

### .kplugin Structure (ZIP Container)

```
plugin.kplugin
├── manifest.json         # Plugin metadata (id, version, dependencies)
├── plugin.py            # Main entry point
├── requirements.txt     # Python dependencies
├── assets/              # Images, icons
└── locale/              # Translations (en.json, pl.json)
```

---

## Plugin Architecture Overview

**Full details in 04_plugin_system.md**

**Key concepts:**
- Extension Points (IExportFormat, IPanelProvider, ICommandProvider)
- Python 3.11 embedded (pybind11)
- .kplugin format (ZIP with manifest)
- Event-driven communication
- Thread-safe operations

---

## Testing Strategy

### Testing Pyramid

```
      /     /E2E\      10% - End-to-end (GUI automation)
    /______   /          /Integration\ 30% - Integration (plugins, file I/O)
 /____________/              /   Unit Tests  \ 60% - Unit (models, presenters, algorithms)
/________________```

**Framework:** Catch2 v3 (BDD style)

**Example:**
```cpp
TEST_CASE("DocumentModel manages chapters", "[document][model]") {
    DocumentModel doc;
    
    SECTION("Adding a chapter") {
        Chapter ch("ch1", "Chapter One");
        doc.addChapter(ch);
        
        REQUIRE(doc.getChapterCount() == 1);
        REQUIRE(doc.getChapter(0).getTitle() == "Chapter One");
    }
    
    SECTION("Word count calculation") {
        Chapter ch("ch1", "Test");
        ch.setContent("Hello world, this is a test.");
        doc.addChapter(ch);
        
        REQUIRE(doc.getWordCount() == 6);
    }
}
```

---

## Performance Considerations

1. **Lazy loading:** Load 100-chapter novel in <2 seconds (metadata only)
2. **Async operations:** UI never freezes (auto-save, export on background thread)
3. **Text rendering:** Edit 10,000-word chapter smoothly (wxRichTextCtrl optimizations)
4. **Search (Phase 2+):** SQLite FTS5 full-text index (~10ms for 100-chapter search)

---

## Security Considerations

1. **Plugin sandboxing (Phase 2+):** Restricted file system access, explicit permissions
2. **Backup encryption (Phase 3+):** Optional AES-256 with user password
3. **API key storage:** OS keychain integration (Windows Credential Manager, macOS Keychain, Linux Secret Service)

---

## Summary

This architecture defines:

✅ **MVP Pattern** - Clear GUI separation (Model/View/Presenter)
✅ **Hybrid Error Handling** - Exceptions + error codes + wxLog* with clear guidelines
✅ **Hybrid Dependencies** - Singletons (infrastructure) + DI (business logic)
✅ **Dynamic Threading** - 2-4 worker pool, CPU-aware, GIL-safe
✅ **Lazy Loading** - Metadata eager, content on-demand
✅ **Command Pattern** - 100-command undo/redo, mergeable
✅ **Composite Model** - Book → Parts → Chapters
✅ **Event System** - Thread-safe EventBus, async GUI marshalling
✅ **File Formats** - .klh (ZIP+JSON+RTF), .kplugin (ZIP+manifest)
✅ **Testing** - 60/30/10 pyramid, Catch2 BDD style
✅ **Performance** - Lazy, async, FTS5 (Phase 2+)
✅ **Security** - Sandboxing, encryption, keychain

**Next Steps:**
1. Finalize 04_plugin_system.md (detailed Extension Points API)
2. Update 07_mvp_tasks.md (week-by-week implementation breakdown)
3. Start Phase 0 - Foundation 🚀

---

**Document Version:** 1.0
**Last Updated:** 2025-10-25
**Next Review:** Start of Phase 0
