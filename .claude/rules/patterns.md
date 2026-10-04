---
paths:
  - "src/**/*.cpp"
  - "src/**/*.h"
  - "include/**/*.h"
---

# C++ Code Patterns

## Mandatory Patterns (CORRECT API)

```cpp
// Icons - ALWAYS through ArtProvider
core::ArtProvider::getInstance().getIcon("file.new")
core::ArtProvider::getInstance().createAction("file.new", tr("New"), parent)

// Icon Colors - ALWAYS through ArtProvider
core::ArtProvider::getInstance().getPrimaryColor()
core::ArtProvider::getInstance().getSecondaryColor()

// Config - ALWAYS through SettingsManager (typed get/set with JSON-pointer-like keys)
core::SettingsManager::getInstance().get<int>("editor.fontSize", 12)
core::SettingsManager::getInstance().set<bool>("editor.showRuler", true)

// Themes - through ThemeManager
core::ThemeManager::getInstance().getCurrentTheme()

// UI Strings - ALWAYS through tr()
tr("User visible text")

// Logging
core::Logger::getInstance().info("Message: {}", value)
```

## NEVER Use

```cpp
settings.getValue("key")     // Does NOT exist! Use get<T>()
Theme::instance()            // Does NOT exist!
ArtProvider::instance()      // Wrong! Use getInstance()
QIcon("path/to/icon.svg")    // Use ArtProvider
QColor(255, 0, 0)            // In UI code use ArtProvider/ThemeManager colors
"Hardcoded string"           // In UI use tr()
```

## Clarifications

- **Constants:** named `constexpr` constants (`UPPER_SNAKE_CASE`) are fine for invariants
  (limits, protocol values). Values a user may want to change belong in SettingsManager.
- **Colors:** literal colors are allowed only where theme defaults are defined
  (`resources/themes/*.json`, `fallback_theme.cpp`, `theme.cpp`). Add new theme colors with
  `scripts/add_theme_color.py`.
- **TODO/FIXME:** do not add new ones without a reference to a tracked issue or plan.
