---
name: architecture-patterns
description: Kalahari architecture patterns and key classes. Use for code analysis and design.
---

# Architecture Patterns

## 1. Key Classes

### Core Singletons
| Class | Location | Role |
|-------|----------|------|
| SettingsManager | core/settings_manager.h | Singleton, JSON config persistence |
| ArtProvider | core/art_provider.h | Singleton, icons, colors, QAction creation |
| IconRegistry | core/icon_registry.h | Singleton, icon registration and caching |
| ThemeManager | core/theme_manager.h | Singleton, theme loading, palette management |
| CommandRegistry | gui/command_registry.h | Singleton, central QAction owner, getAction(), updateActionState() |
| Logger | core/logger.h | Singleton, spdlog wrapper |
| TrustedKeys | core/trusted_keys.h | Singleton, plugin publisher key management |

### MainWindow Coordinators
| Class | Location | Role |
|-------|----------|------|
| MainWindow | gui/main_window.h | Orchestrator delegating to the coordinators below |
| IconRegistrar | gui/icon_registrar.h | Icon registration with IconRegistry |
| CommandRegistrar | gui/command_registrar.h | Command registration with callbacks |
| DockCoordinator | gui/dock_coordinator.h | Panel and dock widget management |
| DocumentCoordinator | gui/document_coordinator.h | Document lifecycle, open/save/close |
| NavigatorCoordinator | gui/navigator_coordinator.h | Navigator panel interaction handlers |
| DiagnosticController | gui/diagnostic_controller.h | Diagnostic and dev mode management |
| SettingsCoordinator | gui/settings_coordinator.h | Settings dialog integration |

### Plugin Security
| Class | Location | Role |
|-------|----------|------|
| PluginSignature | core/plugin_signature.h | Ed25519 signature verification |
| TrustedKeys | core/trusted_keys.h | Trusted publisher key management |

## 2. Design Patterns Used

### Singleton
- SettingsManager, ArtProvider, ThemeManager, IconRegistry, Logger
- Access: `ClassName::getInstance()`

### Command Pattern
- CommandRegistry stores CommandDef entries
- Actions created via ArtProvider::createAction()

### Observer (Qt Signals/Slots)
- ThemeManager::themeChanged signal
- ArtProvider::resourcesChanged signal
- Components connect to update on changes

### Composite
- Book → Part → Chapter (Document)
- BookElement hierarchy

## 3. Source Structure

Headers in `include/kalahari/<module>/`, sources in `src/<module>/` (same layout).

```
include/kalahari/
├── core/           # business logic, singletons
│   ├── art_provider.h, icon_registry.h, theme_manager.h, theme.h, fallback_theme.h
│   ├── settings_manager.h, logger.h, event_bus.h
│   ├── book.h, part.h, book_element.h, document.h, chapter_document.h
│   ├── project_manager.h, project_database.h, project_lock.h, backup_manager.h
│   ├── plugin_manager.h, plugin_manifest.h, plugin_archive.h, python_interpreter.h
│   ├── plugin_signature.h, trusted_keys.h   # Ed25519 plugin security
│   └── utils/      # icon_downloader.h, svg_converter.h
├── editor/         # BookEditor and its document model
│   ├── book_editor.h                         # the editor widget
│   ├── kml_*.h                               # KML reader (kml_document_model.h), serializer, format registry
│   ├── kalahari_text_document_layout.h, viewport_manager.h, search_engine.h
│   ├── editor_render_pipeline.h, render_context.h, text_source_adapter.h, style_resolver.h
│   ├── buffer_commands.h, clipboard_handler.h, snapshot_manager.h
│   └── *_service.h                           # spell/grammar check, TTS
└── gui/            # UI components
    ├── main_window.h + coordinators (icon_registrar, command_registrar,
    │   dock_coordinator, document_coordinator, navigator_coordinator,
    │   diagnostic_controller, settings_coordinator)
    ├── command_registry.h, menu_builder.h, toolbar_builder.h, toolbar_manager.h
    ├── settings_dialog.h, settings_data.h
    ├── dialogs/    # about, new item, add to project, toolbar manager, ...
    ├── panels/     # editor, navigator, log, properties, search, tags, comments, ...
    ├── widgets/    # color_config_widget, standalone_info_bar
    └── utils/      # layout_utils.h (clearLayout)

src/bindings/       # pybind11 Python bindings
tests/              # core/, editor/, gui/, benchmarks/ (Catch2 v3)
```

## 4. Adding New Components

### New Panel (QDockWidget)
1. Create header: `include/kalahari/gui/panels/my_panel.h`
2. Create source: `src/gui/panels/my_panel.cpp`
3. Inherit from QDockWidget
4. Register in `DockCoordinator::createDocks()` via a `create<Name>Dock()` helper
5. Add to CMakeLists.txt

### New Dialog (QDialog)
1. Create header: `include/kalahari/gui/my_dialog.h`
2. Create source: `src/gui/my_dialog.cpp`
3. Inherit from QDialog
4. Add action in MainWindow or menu
5. Add to CMakeLists.txt

### New Widget (QWidget)
1. Create header: `include/kalahari/gui/my_widget.h`
2. Create source: `src/gui/my_widget.cpp`
3. Inherit from QWidget
4. Use in panel or dialog
5. Add to CMakeLists.txt

### New Core Class
1. Create header: `include/kalahari/core/my_class.h`
2. Create source: `src/core/my_class.cpp`
3. Use `kalahari::core` namespace
4. Add to CMakeLists.txt

## 5. Signal/Slot Connections

### Theme changes
```cpp
connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged,
        this, &MyClass::onThemeChanged);
```

### Icon/color changes
```cpp
connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
        this, &MyClass::onResourcesChanged);
```

## 6. File Naming

| Component Type | Header | Source |
|----------------|--------|--------|
| Panel | `my_panel.h` | `my_panel.cpp` |
| Dialog | `my_dialog.h` | `my_dialog.cpp` |
| Widget | `my_widget.h` | `my_widget.cpp` |
| Core class | `my_class.h` | `my_class.cpp` |

## 7. CMakeLists.txt Integration

Sources are listed in `src/CMakeLists.txt` (paths relative to `src/`). Headers of `Q_OBJECT` classes
built into the executable must also be listed there (`${CMAKE_SOURCE_DIR}/include/...`) so AUTOMOC sees them:

```cmake
set(KALAHARI_CORE_SOURCES   # -> kalahari_core shared library (also used by Python bindings)
    ...
    core/my_class.cpp
)

set(KALAHARI_SOURCES        # -> kalahari executable (GUI, Q_OBJECT singletons)
    ...
    gui/my_new_file.cpp
)
```

Rule of thumb: model and logic without `Q_OBJECT` (including the KML editor model) go to `KALAHARI_CORE_SOURCES`;
GUI code, the editor widget and `Q_OBJECT` classes (ThemeManager, IconRegistry, ArtProvider, BookEditor) go to `KALAHARI_SOURCES`.
Check where similar files are listed before adding a new one. New test files go to `tests/CMakeLists.txt`.

## 8. Analyzing Existing Code

1. `mcp__serena__get_symbols_overview("path/to/file.cpp")` - see class structure
2. `mcp__serena__find_symbol("ClassName")` - find class definition
3. `mcp__serena__find_referencing_symbols("ClassName")` - find usages

(Serena = semantic C++ engine. Fallback if unavailable: Grep / Glob / Read.)

### Key files to check
- `main_window.cpp` - thin orchestrator, coordinator creation
- `dock_coordinator.cpp` - panel/dock widget patterns
- `document_coordinator.cpp` - document lifecycle patterns
- `settings_dialog.cpp` - dialog patterns
- `art_provider.cpp` - icon/color handling
- `plugin_signature.cpp` - Ed25519 verification patterns
