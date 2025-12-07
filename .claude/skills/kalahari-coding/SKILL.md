---
name: kalahari-coding
description: Core coding patterns and conventions for Kalahari project. MUST be used by all code-related agents.
---

# Kalahari Coding Standards

## 1. Icons

### ALWAYS
```cpp
core::ArtProvider::getInstance().getIcon("cmd_id")
core::ArtProvider::getInstance().createAction("cmd_id", parent)  // for QAction with auto-refresh
```

### NEVER
```cpp
QIcon("path/to/icon.svg")  // hardcoded path
```

### Available icons
- Location: `resources/icons/`
- Registered in: `main_window.cpp` (IconRegistry::registerIcon)

## 2. Icon Colors

### ALWAYS
```cpp
QColor primary = core::ArtProvider::getInstance().getPrimaryColor();
QColor secondary = core::ArtProvider::getInstance().getSecondaryColor();
core::ArtProvider::getInstance().setPrimaryColor(QColor("#hex"));
core::ArtProvider::getInstance().setSecondaryColor(QColor("#hex"));
```

### NEVER
```cpp
QColor(255, 0, 0)  // hardcoded color
Theme::instance().getColor()  // DOES NOT EXIST!
```

## 3. Configuration

### ALWAYS
```cpp
auto& settings = core::SettingsManager::getInstance();
std::string value = settings.getValue("key", "default");
settings.setValue("key", "value");
```

### NEVER
```cpp
// hardcoded configuration values
const int MAX_SIZE = 100;  // should be in settings
```

## 4. UI Strings

### ALWAYS
```cpp
tr("User visible text")
tr("Format: %1").arg(value)
```

### NEVER
```cpp
"Hardcoded string"  // not translatable
```

## 5. Themes

### ALWAYS
```cpp
const core::Theme& theme = core::ThemeManager::getInstance().getCurrentTheme();
// Access theme colors:
theme.colors.primary
theme.colors.secondary
theme.palette.toQPalette()
theme.log.info  // log panel colors
```

## 6. Logging

### ALWAYS
```cpp
core::Logger::getInstance().info("Message: {}", value);
core::Logger::getInstance().debug("Debug: {}", value);
core::Logger::getInstance().warn("Warning: {}", value);
core::Logger::getInstance().error("Error: {}", value);
```

### Levels
- trace, debug, info, warn, error, critical

## 7. Layouts (Qt6)

### Basic patterns
```cpp
QVBoxLayout* mainLayout = new QVBoxLayout(this);
QHBoxLayout* rowLayout = new QHBoxLayout();
QGroupBox* group = new QGroupBox(tr("Title"));
```

### Stretch factors
- 0 = fixed size
- 1+ = flexible, fills available space

## 8. Build

### Windows
```bash
scripts/build_windows.bat Debug
```

### Linux
```bash
scripts/build_linux.sh
```

### NEVER
- cmake directly
- WSL for Windows builds

## 9. Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Files | snake_case | `character_card.cpp` |
| Classes | PascalCase | `CharacterCard` |
| Methods | camelCase | `getTitle()` |
| Members | m_prefix | `m_title` |
| Constants | UPPER_SNAKE_CASE | `MAX_CHAPTERS` |
| Namespaces | lowercase | `kalahari::core` |

## 10. Singletons Reference

| Class | Access Method | Namespace |
|-------|---------------|-----------|
| ArtProvider | `getInstance()` | `kalahari::core` |
| SettingsManager | `getInstance()` | `kalahari::core` |
| ThemeManager | `getInstance()` | `kalahari::core` |
| IconRegistry | `getInstance()` | `kalahari::core` |
| Logger | `getInstance()` | `kalahari::core` |
| CommandRegistry | `getInstance()` | `kalahari::gui` |
