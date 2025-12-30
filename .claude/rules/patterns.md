# C++ Code Patterns

Applies to: `src/**/*.cpp`, `src/**/*.h`, `include/**/*`

## Mandatory Patterns (CORRECT API)

```cpp
// Icons - ALWAYS through ArtProvider
core::ArtProvider::getInstance().getIcon("file.new")
core::ArtProvider::getInstance().createAction("file.new", parent)

// Icon Colors - ALWAYS through ArtProvider
core::ArtProvider::getInstance().getPrimaryColor()
core::ArtProvider::getInstance().getSecondaryColor()

// Config - ALWAYS through SettingsManager
core::SettingsManager::getInstance().getValue("key", "default")

// Themes - through ThemeManager
core::ThemeManager::getInstance().getCurrentTheme()

// UI Strings - ALWAYS through tr()
tr("User visible text")

// Logging
core::Logger::getInstance().info("Message: {}", value)
```

## NEVER Use

```cpp
Theme::instance()            // Does NOT exist!
ArtProvider::instance()      // Wrong! Use getInstance()
QIcon("path/to/icon.svg")    // Use ArtProvider
QColor(255, 0, 0)            // Use ArtProvider colors
"Hardcoded string"           // Use tr()
```
