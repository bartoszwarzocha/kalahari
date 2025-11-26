# Design: Advanced Theme System with Plugin Architecture

## Context

Kalahari is a Writer's IDE where users spend extended periods writing. Visual comfort is critical. The theming system must:
- Support system-wide light/dark mode switching
- Provide distinctive branded themes (African naming)
- Allow themes to be distributed as installable plugins
- Work consistently across Windows, macOS, Linux
- **Never fail to start** - emergency fallback required

**Constraint:** Qt6 provides multiple theming mechanisms (QPalette, QSS, QStyle). Themes must be loadable from Python plugins while maintaining C++ performance for rendering.

## Goals / Non-Goals

### Goals
- Full control over application colors through JSON configuration
- Plugin-based theme and icon distribution
- Core plugins that cannot be accidentally removed
- Emergency fallback when plugins fail
- System theme detection and following
- Hot-reload themes without restart
- Cross-platform visual consistency

### Non-Goals
- Runtime QSS editor (out of scope)
- Theme marketplace integration (future)
- Per-panel theme overrides (single app-wide theme)
- Animated theme transitions
- Theme creation wizard

## Decisions

### Decision 1: Three-Level Theme Hierarchy

**What:** Implement three distinct levels of theme sources with fallback chain.

```
┌─────────────────────────────────────────────────────────────┐
│  LEVEL 3: User Plugins (~/.kalahari/plugins/)              │
│  └── Removable, user-installable                            │
│  └── african-themes, vintage-icons, etc.                    │
├─────────────────────────────────────────────────────────────┤
│  LEVEL 2: Core Plugins (app/plugins/core/)                  │
│  └── Pre-installed with application                         │
│  └── Protected: cannot uninstall from UI                    │
│  └── kalahari-default-themes, kalahari-default-icons        │
├─────────────────────────────────────────────────────────────┤
│  LEVEL 1: Emergency Fallback (hardcoded C++)                │
│  └── ~100 LOC minimal Light/Dark palettes                   │
│  └── ~16 essential icons as embedded SVG strings            │
│  └── Used ONLY when plugins fail                            │
└─────────────────────────────────────────────────────────────┘
```

**Why:**
- Level 1 guarantees application always starts
- Level 2 provides quality defaults that survive user mistakes
- Level 3 enables ecosystem growth

**Alternatives considered:**
- All themes as plugins: Risk of app not starting if corrupted
- All themes hardcoded: No extensibility, large binary
- Themes in resources/: No plugin ecosystem

### Decision 2: IThemeProvider Extension Point

**What:** Define Python extension point for theme plugins.

```python
# kalahari_api/extensions/theme_provider.py
from abc import ABC, abstractmethod
from pathlib import Path
from typing import List, Dict, Any

class IThemeProvider(ABC):
    """Extension point for theme plugins."""

    @abstractmethod
    def get_theme_directory(self) -> Path:
        """Return path to directory containing theme JSON files."""
        pass

    @abstractmethod
    def get_themes(self) -> List[Dict[str, Any]]:
        """Return list of theme metadata.

        Each dict contains:
        - name: str - Theme display name
        - file: str - JSON filename
        - description: str - Theme description
        - preview: str - Preview image filename (optional)
        """
        pass

    def get_qss_directory(self) -> Path:
        """Return path to optional QSS files. Default: same as themes."""
        return self.get_theme_directory()
```

```python
# Example plugin implementation
class AfricanThemesPlugin(IThemeProvider):
    def __init__(self, plugin_dir: Path):
        self.plugin_dir = plugin_dir

    def get_theme_directory(self) -> Path:
        return self.plugin_dir / "themes"

    def get_themes(self) -> List[Dict[str, Any]]:
        return [
            {"name": "Serengeti", "file": "serengeti.json",
             "description": "Warm savanna sunset"},
            {"name": "Savannah", "file": "savannah.json",
             "description": "Golden afternoon hues"},
        ]
```

**Why:**
- Simple interface - just point to JSON files
- No Python involved in actual theme rendering (C++ handles it)
- Plugin only provides discovery, not execution
- Easy to implement for plugin authors

### Decision 3: IIconSetProvider Extension Point

**What:** Define Python extension point for icon set plugins.

```python
# kalahari_api/extensions/icon_set_provider.py
from abc import ABC, abstractmethod
from pathlib import Path
from typing import List, Dict, Any

class IIconSetProvider(ABC):
    """Extension point for icon set plugins."""

    @abstractmethod
    def get_icon_sets(self) -> List[Dict[str, Any]]:
        """Return list of icon set metadata.

        Each dict contains:
        - name: str - Icon set display name (e.g., "Outlined")
        - id: str - Icon set identifier (e.g., "outlined")
        - directory: Path - Directory containing SVG files
        - description: str - Icon set description
        """
        pass

    def get_icon_mapping(self) -> Dict[str, str]:
        """Return mapping of logical icon names to filenames.

        Optional. If not provided, filenames are used as-is.
        Example: {"file.new": "note_add.svg", "file.save": "save.svg"}
        """
        return {}
```

**Why:**
- Separates icon sets from themes (can mix and match)
- Supports icon name mapping (Material Icons → Kalahari names)
- Simple directory-based discovery

### Decision 4: Protected Plugin Mechanism

**What:** Mark core plugins as protected to prevent accidental removal.

```json
// plugins/core/kalahari-default-themes/manifest.json
{
    "id": "org.kalahari.themes.default",
    "name": "Default Themes",
    "version": "1.0.0",
    "author": "Kalahari Team",
    "description": "Light and Dark default themes",
    "core": true,
    "protected": true,
    "extension_points": ["theme_provider"],
    "provides": {
        "themes": ["Light", "Dark"]
    }
}
```

**Implementation:**
```cpp
// PluginManager checks protected flag
bool PluginManager::canUninstall(const QString& pluginId) const {
    auto plugin = getPlugin(pluginId);
    if (!plugin) return false;

    // Core protected plugins cannot be uninstalled via UI
    if (plugin->manifest.core && plugin->manifest.protected) {
        return false;
    }
    return true;
}
```

**UI Behavior:**
- Protected plugins shown in list but "Uninstall" disabled
- Tooltip explains why
- Update still allowed (if newer version available)

### Decision 5: Emergency Fallback Implementation

**What:** Hardcode minimal themes in C++ for guaranteed startup.

```cpp
// src/core/fallback_theme.h
namespace kalahari::core {

struct FallbackTheme {
    static Theme getLightTheme();
    static Theme getDarkTheme();
    static bool isUsingFallback();
};

} // namespace kalahari::core
```

```cpp
// src/core/fallback_theme.cpp
Theme FallbackTheme::getLightTheme() {
    Theme theme;
    theme.meta.name = "Light (Fallback)";
    theme.meta.version = "1.0";
    theme.meta.description = "Emergency fallback theme";

    // Minimal QPalette colors
    theme.palette.window = QColor("#FFFFFF");
    theme.palette.windowText = QColor("#000000");
    theme.palette.base = QColor("#FFFFFF");
    theme.palette.text = QColor("#000000");
    theme.palette.button = QColor("#E0E0E0");
    theme.palette.buttonText = QColor("#000000");
    theme.palette.highlight = QColor("#0078D4");
    theme.palette.highlightedText = QColor("#FFFFFF");

    // Minimal icon colors
    theme.icons.primary = QColor("#333333");
    theme.icons.secondary = QColor("#666666");

    return theme;
}
```

**Fallback Icons:**
```cpp
// src/core/fallback_icons.cpp
// Essential icons embedded as SVG strings
const std::map<QString, QString> FALLBACK_ICONS = {
    {"file.new", R"(<svg viewBox="0 0 24 24">...</svg>)"},
    {"file.open", R"(<svg viewBox="0 0 24 24">...</svg>)"},
    {"file.save", R"(<svg viewBox="0 0 24 24">...</svg>)"},
    {"edit.undo", R"(<svg viewBox="0 0 24 24">...</svg>)"},
    {"edit.redo", R"(<svg viewBox="0 0 24 24">...</svg>)"},
    {"edit.cut", R"(<svg viewBox="0 0 24 24">...</svg>)"},
    {"edit.copy", R"(<svg viewBox="0 0 24 24">...</svg>)"},
    {"edit.paste", R"(<svg viewBox="0 0 24 24">...</svg>)"},
    // ... ~16 essential icons
};
```

### Decision 6: Theme Discovery Flow

**What:** ThemeManager discovers themes from all sources at startup.

```cpp
void ThemeManager::discoverAllThemes() {
    m_availableThemes.clear();

    // 1. Always add fallback themes (Level 1)
    addFallbackThemes();

    // 2. Discover from core plugins (Level 2)
    discoverPluginThemes(PluginType::Core);

    // 3. Discover from user plugins (Level 3)
    discoverPluginThemes(PluginType::User);

    // Log results
    Logger::info("ThemeManager: Discovered {} themes from {} sources",
                 m_availableThemes.size(), m_themeSources.size());
}

void ThemeManager::discoverPluginThemes(PluginType type) {
    auto& registry = ExtensionPointRegistry::getInstance();
    auto providers = registry.getExtensions<IThemeProvider>(type);

    for (auto& provider : providers) {
        QString pluginId = provider->getPluginId();
        Path themeDir = provider->getThemeDirectory();

        for (auto& themeMeta : provider->getThemes()) {
            ThemeInfo info;
            info.name = themeMeta["name"];
            info.source = pluginId;
            info.path = themeDir / themeMeta["file"];
            info.description = themeMeta.value("description", "");
            info.isCore = (type == PluginType::Core);

            m_availableThemes.push_back(info);
        }
    }
}
```

### Decision 7: Application Startup Sequence

**What:** Define exact order of theme system initialization.

```
Application Start
       │
       ▼
┌──────────────────────────────────────┐
│ 1. Load Emergency Fallback           │
│    └── FallbackTheme::getLightTheme()│
│    └── Apply minimal palette         │
│    └── App is now visible/usable     │
└──────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 2. Initialize Plugin System          │
│    └── Load core plugins first       │
│    └── Load user plugins second      │
└──────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 3. Discover Themes from Plugins      │
│    └── IThemeProvider extensions     │
│    └── IIconSetProvider extensions   │
└──────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 4. Load User's Preferred Theme       │
│    └── From SettingsManager          │
│    └── Fallback to "Light" if gone   │
│    └── Apply full theme (4 layers)   │
└──────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ 5. Load User's Preferred Icon Set    │
│    └── From SettingsManager          │
│    └── Fallback to "outlined"        │
│    └── Register icons with IconReg   │
└──────────────────────────────────────┘
       │
       ▼
     Ready
```

**Key Points:**
- Step 1 happens BEFORE any plugins load
- If Step 2-5 fail, app still works with fallback
- User sees themed UI immediately (fallback), then upgrade

### Decision 8: Four-Layer Theme Application

**What:** Apply themes in four layers for maximum flexibility.

```cpp
void ThemeManager::applyTheme(const Theme& theme) {
    // Layer 1: Set Fusion as base style
    QApplication::setStyle("Fusion");

    // Layer 2: Apply QPalette
    QPalette palette = buildPalette(theme.palette);
    QApplication::setPalette(palette);

    // Layer 3: Apply component QSS
    QString componentQss = generateComponentQSS(theme);

    // Layer 4: Apply full QSS (if provided)
    QString fullQss;
    if (!theme.qssPath.isEmpty()) {
        fullQss = loadQSSFile(theme.qssPath);
    }

    // Combine and apply
    QString finalQss = componentQss + "\n" + fullQss;
    qApp->setStyleSheet(finalQss);

    // Update IconRegistry
    IconRegistry::getInstance().setThemeColors(
        theme.icons.primary, theme.icons.secondary);

    // Save and emit
    m_currentTheme = theme;
    SettingsManager::getInstance().setTheme(theme.meta.name);
    emit themeChanged(theme);
}
```

### Decision 9: Core Plugin Directory Structure

**What:** Define standard structure for core theme/icon plugins.

```
plugins/
└── core/
    ├── kalahari-default-themes/
    │   ├── manifest.json
    │   ├── main.py
    │   └── themes/
    │       ├── light.json
    │       └── dark.json
    │
    └── kalahari-default-icons/
        ├── manifest.json
        ├── main.py
        ├── mapping.json          # Icon name → filename mapping
        └── icons/
            ├── outlined/
            │   ├── note_add.svg
            │   ├── folder_open.svg
            │   └── ...
            ├── rounded/
            │   └── ...
            └── twotone/
                └── ...
```

**Mapping file example:**
```json
{
    "file.new": "note_add.svg",
    "file.open": "folder_open.svg",
    "file.save": "save.svg",
    "file.saveAs": "save_as.svg",
    "edit.undo": "undo.svg",
    "edit.redo": "redo.svg"
}
```

## Risks / Trade-offs

### Risk 1: Plugin Loading Performance
**Risk:** Loading themes from plugins slower than hardcoded.
**Mitigation:**
- Fallback shown immediately
- Plugin discovery async after first paint
- Cache discovered themes

### Risk 2: Plugin API Stability
**Risk:** Changes to IThemeProvider break existing plugins.
**Mitigation:**
- Version extension point interfaces
- Maintain backward compatibility
- Deprecation warnings before removal

### Risk 3: Icon Mapping Complexity
**Risk:** Different icon sets use different names.
**Mitigation:**
- Standard mapping.json in each plugin
- Fallback to filename if no mapping
- Document standard icon names

### Risk 4: Core Plugin Corruption
**Risk:** User manually deletes core plugin files.
**Mitigation:**
- App detects missing core plugin at startup
- Offers to reinstall from embedded backup
- Falls back to emergency theme

## Migration Plan

### Phase 1: Emergency Fallback (Week 1)
1. Implement FallbackTheme class
2. Implement FallbackIcons with 16 essential icons
3. Update main.cpp to load fallback before plugins
4. Test app starts without any plugins

### Phase 2: Extension Points (Week 1-2)
1. Define IThemeProvider in kalahari_api
2. Define IIconSetProvider in kalahari_api
3. Register extension points in ExtensionPointRegistry
4. Add pybind11 bindings

### Phase 3: Theme Discovery (Week 2)
1. Update ThemeManager with discoverAllThemes()
2. Add ThemeInfo struct with source tracking
3. Update getAvailableThemes() to return ThemeInfo list
4. Add isCore/isProtected flags

### Phase 4: Icon Discovery (Week 2-3)
1. Update IconRegistry with plugin-based loading
2. Implement icon name mapping
3. Support multiple icon sets
4. Add getAvailableIconSets()

### Phase 5: Core Plugins (Week 3)
1. Create kalahari-default-themes plugin
2. Create kalahari-default-icons plugin
3. Move existing themes/icons to plugins
4. Mark as protected in manifest

### Phase 6: Settings UI (Week 3-4)
1. Add theme dropdown with source indication
2. Add icon set dropdown
3. Add "Follow system theme" checkbox
4. Show protected status for core plugins

### Phase 7: Testing (Week 4)
1. Test startup without plugins
2. Test startup with corrupt plugins
3. Test theme switching
4. Test icon set switching
5. Test on Windows, macOS, Linux

## Open Questions

1. **Should fallback icons be SVG or PNG?**
   - Recommendation: SVG (scalable, smaller)
   - Embedded as C++ string literals

2. **How to handle theme updates?**
   - Recommendation: Plugin update mechanism (future task)

3. **Should we allow mixing themes and icon sets?**
   - Recommendation: Yes, they are independent
   - User can use "Dark" theme with "Rounded" icons

4. **Theme preview in settings?**
   - Recommendation: Small preview image from theme meta
   - Live preview is complex (defer to future)
