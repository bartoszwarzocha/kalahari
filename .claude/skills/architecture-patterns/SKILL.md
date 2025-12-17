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
| CommandRegistry | gui/command_registry.h | Singleton, action/command registration (thread-safe) |
| Logger | core/logger.h | Singleton, spdlog wrapper |
| TrustedKeys | core/trusted_keys.h | Singleton, plugin publisher key management |

### MainWindow Coordinators (OpenSpec #00038)
| Class | Location | Role |
|-------|----------|------|
| MainWindow | gui/main_window.h | Thin orchestrator (~805 lines) |
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

```
include/kalahari/
├── core/           # business logic, singletons
│   ├── art_provider.h
│   ├── settings_manager.h
│   ├── theme_manager.h
│   ├── icon_registry.h
│   ├── logger.h
│   ├── book.h
│   ├── document.h
│   ├── plugin_signature.h    # Ed25519 verification
│   └── trusted_keys.h        # Publisher key management
├── gui/            # UI components
│   ├── main_window.h         # Thin orchestrator
│   ├── icon_registrar.h      # Icon registration
│   ├── command_registrar.h   # Command registration
│   ├── dock_coordinator.h    # Panel management
│   ├── document_coordinator.h
│   ├── navigator_coordinator.h
│   ├── diagnostic_controller.h
│   ├── settings_coordinator.h
│   ├── command_registry.h
│   ├── settings_dialog.h
│   ├── panels/
│   │   ├── editor_panel.h
│   │   ├── navigator_panel.h
│   │   └── log_panel.h
│   └── utils/
│       └── layout_utils.h    # clearLayout() helper
└── utils/          # utilities
    └── ...

src/
├── core/
├── gui/
└── utils/
```

## 4. Adding New Components

### New Panel (QDockWidget)
1. Create header: `include/kalahari/gui/panels/my_panel.h`
2. Create source: `src/gui/panels/my_panel.cpp`
3. Inherit from QDockWidget
4. Register in MainWindow::createDockWidgets()
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

```cmake
set(KALAHARI_GUI_SOURCES
    ...
    src/gui/my_new_file.cpp
)

set(KALAHARI_GUI_HEADERS
    ...
    include/kalahari/gui/my_new_file.h
)
```

## 8. Analyzing Existing Code

1. `get_symbols_overview("path/to/file.cpp")` - see class structure
2. `find_symbol("ClassName")` - find class definition
3. `find_referencing_symbols("ClassName")` - find usages

### Key files to check
- `main_window.cpp` - thin orchestrator, coordinator creation
- `dock_coordinator.cpp` - panel/dock widget patterns
- `document_coordinator.cpp` - document lifecycle patterns
- `settings_dialog.cpp` - dialog patterns
- `art_provider.cpp` - icon/color handling
- `plugin_signature.cpp` - Ed25519 verification patterns
