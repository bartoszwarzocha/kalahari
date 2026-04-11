# Spec: Icon Theming System

## Overview

The Icon Theming System connects IconRegistry to ThemeManager, enabling automatic icon color updates when themes change.

## Architecture

```
┌─────────────────┐    themeChanged(theme)    ┌─────────────────┐
│  ThemeManager   │ ────────────────────────► │  IconRegistry   │
│  (Singleton)    │                           │  (QObject)      │
└────────┬────────┘                           └────────┬────────┘
         │                                             │
         │ getCurrentTheme()                           │ setThemeColors()
         │                                             │ clearCache()
         ▼                                             ▼
┌─────────────────┐                           ┌─────────────────┐
│  Theme JSON     │                           │  SVG Files      │
│  - colors       │                           │  - {PRIMARY}    │
│  - palette      │                           │  - {SECONDARY}  │
└─────────────────┘                           └─────────────────┘
```

## Data Flow

### On Application Startup

```
1. main.cpp: ThemeManager::getInstance() (loads theme from settings)
2. main.cpp: IconRegistry::getInstance().initialize()
   └── Gets colors from ThemeManager::getCurrentTheme()
   └── Sets m_theme.primaryColor = theme.colors.primary
   └── Sets m_theme.secondaryColor = theme.colors.secondary
   └── Connects to ThemeManager::themeChanged signal
3. MainWindow: registerIcons() - registers all 100+ icons
4. MainWindow: builds toolbars/menus using IconRegistry::getIcon()
   └── IconRegistry loads SVG, replaces placeholders with m_theme colors
   └── Renders to QPixmap, caches result
```

### On Theme Change

```
1. User: Settings → Appearance → Theme → "Dark"
2. SettingsDialog: ThemeManager::switchTheme("Dark")
3. ThemeManager: loads Dark.json, applies QPalette
4. ThemeManager: emit themeChanged(darkTheme)
5. IconRegistry::onThemeChanged(darkTheme)
   └── setThemeColors(theme.colors.primary, theme.colors.secondary, "Dark")
   └── clearCache() - invalidates all cached QPixmaps
6. [On next getIcon() call]
   └── Cache miss → reload SVG with new colors
   └── Re-render and cache
```

## SVG Template Format

All SVG icons must use color placeholders:

```xml
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24">
  <path d="M0 0h24v24H0V0z" fill="none"/>
  <!-- Secondary color: background/accent shapes -->
  <path d="M5 5v14h14V7.83L16.17 5H5z..." fill="{COLOR_SECONDARY}" opacity=".3"/>
  <!-- Primary color: main icon shape -->
  <path d="M17 3H5c-1.11 0-2 .9-2 2v14..." fill="{COLOR_PRIMARY}"/>
</svg>
```

### SvgConverter Rules

1. Elements with `opacity > 0.5` → `{COLOR_PRIMARY}`
2. Elements with `opacity <= 0.5` → `{COLOR_SECONDARY}`
3. Supported elements: `path`, `circle`, `rect`, `polygon`, `polyline`, `ellipse`, `line`

## Color Mapping

### Light Theme (resources/themes/Light.json)

```json
{
  "colors": {
    "primary": "#424242",    // Dark gray (main icon)
    "secondary": "#757575"   // Medium gray (accent)
  }
}
```

### Dark Theme (resources/themes/Dark.json)

```json
{
  "colors": {
    "primary": "#E0E0E0",    // Light gray (main icon)
    "secondary": "#B0B0B0"   // Medium-light gray (accent)
  }
}
```

## API Changes

### IconRegistry (icon_registry.h)

```cpp
class IconRegistry : public QObject {
    Q_OBJECT

public:
    static IconRegistry& getInstance();
    void initialize();  // Connects to ThemeManager

public slots:
    /// Called when ThemeManager emits themeChanged
    void onThemeChanged(const Theme& theme);

private:
    // Removed: static const ThemeConfig DEFAULT_LIGHT;
    // Removed: static const ThemeConfig DEFAULT_DARK;
};
```

### ThemeManager Signal (already exists)

```cpp
class ThemeManager : public QObject {
    Q_OBJECT

signals:
    void themeChanged(const Theme& theme);
};
```

## Cache Invalidation

When theme changes:
1. `clearCache()` removes all entries from `m_pixmapCache`
2. Next `getIcon()` call triggers re-render with new colors
3. Lazy re-rendering (icons rendered on demand, not all at once)

Cache key format: `{actionId}_{theme}_{size}_{primaryColor}_{secondaryColor}`

## Testing

### Manual Testing

1. Start application with Light theme
2. Verify toolbar icons are dark gray (#424242)
3. Open Settings → Appearance → Theme
4. Switch to Dark theme
5. Verify toolbar icons change to light gray (#E0E0E0)
6. Verify pets icon has same color as save icon
7. Restart application
8. Verify Dark theme persists

### Automated Verification

```bash
# All icons must have placeholders
count=$(grep -l "COLOR_PRIMARY" resources/icons/twotone/*.svg | wc -l)
total=$(ls resources/icons/twotone/*.svg | wc -l)
[ "$count" -eq "$total" ] && echo "PASS" || echo "FAIL: $count/$total"
```

## Dependencies

- ThemeManager (Task #00023)
- IconRegistry (Task #00021)
- SvgConverter (Task #00020)
- Qt6::Core (QObject, signals/slots)
