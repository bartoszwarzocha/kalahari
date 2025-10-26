# Kalahari Source Code Structure

This directory contains all C++ source code for Kalahari Writer's IDE, organized according to the **MVP (Model-View-Presenter)** architecture pattern.

## Directory Structure

```
src/
├── core/               # Model layer - Business logic & data
│   ├── model/          # Data models (Document, Chapter, Book, Project)
│   └── utils/          # Utilities, helpers, common functions
├── gui/                # View layer - wxWidgets GUI
│   ├── views/          # Main windows, panels, dialogs
│   └── widgets/        # Custom wxWidgets controls
├── presenters/         # Presenter layer - MVP logic bridge
├── services/           # Application services (singletons)
│   ├── PluginManager   # Plugin system management
│   ├── EventBus        # Event system
│   ├── CommandRegistry # Undo/redo system
│   └── SettingsManager # Application settings
└── main.cpp            # Application entry point
```

## Architecture Layers

### Core (Model)
- **Purpose:** Pure business logic, data structures
- **Dependencies:** NO GUI dependencies (wxWidgets-free)
- **Testability:** Fully unit testable without GUI
- **Examples:** `Document`, `Chapter`, `Book`, `Project`

### GUI (View)
- **Purpose:** wxWidgets user interface, visual components
- **Dependencies:** wxWidgets, presenters (for delegation)
- **Testability:** Integration tests with wxUIActionSimulator
- **Examples:** `MainWindow`, `EditorView`, `ProjectNavigatorPanel`

### Presenters
- **Purpose:** Coordinates Model ↔ View interaction
- **Dependencies:** Models, View interfaces
- **Testability:** Unit testable with mock views
- **Examples:** `DocumentPresenter`, `ProjectPresenter`

### Services
- **Purpose:** Application-wide infrastructure (singletons)
- **Dependencies:** Varies (plugins, events, commands)
- **Testability:** Integration tests
- **Examples:** `PluginManager`, `EventBus`, `CommandRegistry`

## Coding Conventions

- **File names:** `snake_case` (e.g., `document_presenter.cpp`)
- **Class names:** `PascalCase` (e.g., `DocumentPresenter`)
- **Methods:** `camelCase` (e.g., `saveDocument()`)
- **Members:** `m_` prefix (e.g., `m_document`)
- **Constants:** `UPPER_SNAKE_CASE` (e.g., `MAX_CHAPTERS`)
- **Namespaces:** `namespace kalahari { namespace core { } }`

## Build System

- **CMake 3.21+** with vcpkg
- **C++20 standard** (required)
- **Precompiled headers** for wxWidgets (Phase 1+)
- **See:** Root `CMakeLists.txt` for configuration

## Related Documentation

- **Architecture:** `project_docs/03_architecture.md`
- **MVP Pattern:** `project_docs/03_architecture.md` (GUI Pattern section)
- **Plugin System:** `project_docs/04_plugin_system.md`
- **GUI Design:** `project_docs/08_gui_design.md`

---

**Last Updated:** 2025-10-26
**Phase:** 0 (Foundation)
