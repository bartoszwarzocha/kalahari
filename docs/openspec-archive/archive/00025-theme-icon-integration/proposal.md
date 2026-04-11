# Change: Theme-Icon Integration

## Why

Currently, Kalahari has two separate systems that don't communicate:
1. **ThemeManager** - Manages themes (Light/Dark), emits `themeChanged` signal, sets QPalette
2. **IconRegistry** - Manages icons with color placeholders, has hardcoded DEFAULT_LIGHT/DEFAULT_DARK colors

**Critical Issues Discovered:**

1. **167 out of 193 icons (86%)** lack `{COLOR_PRIMARY}/{COLOR_SECONDARY}` placeholders
   - Downloaded directly without SvgConverter processing
   - Render with default black fill (won't theme properly)
   - `pets.svg` has different color than other icons (raw black)

2. **IconRegistry doesn't listen to ThemeManager**
   - No connection to `themeChanged` signal
   - Hardcoded colors: `#424242/#757575` (Light), `#E0E0E0/#B0B0B0` (Dark)
   - Theme switch doesn't update icon colors

3. **Colors not synchronized**
   - Theme JSON has `colors.primary: #424242` and `colors.secondary: #757575`
   - IconRegistry has separate `ThemeConfig::DEFAULT_LIGHT` with same values
   - Duplication leads to inconsistency

**User-Visible Problems:**
- Most icons appear black (no theming)
- Pets icon has different color than others
- Theme switch doesn't change icon colors

## What Changes

### 1. Fix SVG Icons (167 files)

Re-process all downloaded SVG files through SvgConverter to add color placeholders:
- Delete existing non-templated icons
- Re-download via IconDownloader (uses SvgConverter automatically)
- Verify all icons have `{COLOR_PRIMARY}` and `{COLOR_SECONDARY}` placeholders

### 2. Connect IconRegistry to ThemeManager

```cpp
// In IconRegistry::initialize()
connect(&ThemeManager::getInstance(), &ThemeManager::themeChanged,
        this, &IconRegistry::onThemeChanged);

// New slot
void IconRegistry::onThemeChanged(const Theme& theme) {
    setThemeColors(theme.colors.primary, theme.colors.secondary,
                   QString::fromStdString(theme.name));
}
```

### 3. Remove Hardcoded Colors from IconRegistry

Replace:
```cpp
const ThemeConfig ThemeConfig::DEFAULT_LIGHT = {
    QColor("#424242"),  // Hardcoded!
    QColor("#757575"),  // Hardcoded!
    "Light"
};
```

With dynamic loading from ThemeManager on startup:
```cpp
void IconRegistry::initialize() {
    // Get colors from current theme
    const Theme& theme = ThemeManager::getInstance().getCurrentTheme();
    m_theme.primaryColor = theme.colors.primary;
    m_theme.secondaryColor = theme.colors.secondary;
    m_theme.name = QString::fromStdString(theme.name);

    // Connect for future changes
    // ...
}
```

### 4. Update download_all_icons.sh

Ensure script uses proper SvgConverter pipeline:
- Delete raw SVGs
- Use CLI mode: `kalahari --cli --dev --get-icons "..." --themes "twotone"`
- Verify each icon has placeholders after download

## Impact

### Affected Specs
- **icon-system** - IconRegistry connects to ThemeManager
- **theme-system** - Theme colors flow to icons

### Affected Code

**Modified Files:**
| File | Changes |
|------|---------|
| `include/kalahari/core/icon_registry.h` | Add QObject inheritance, slot declaration |
| `src/core/icon_registry.cpp` | Connect to themeChanged, dynamic color init |
| `scripts/download_all_icons.sh` | Fix SvgConverter pipeline |
| `resources/icons/twotone/*.svg` | 167 files re-processed |

**New Files:**
- None (refactoring existing code)

### Dependencies
- ThemeManager (existing, Task #00023)
- IconRegistry (existing, Task #00021)
- SvgConverter (existing, Task #00020)
- IconDownloader (existing, Task #00020)

### Breaking Changes
- None (internal refactoring)

## Acceptance Criteria

1. **All icons have placeholders**: `grep -c "COLOR_PRIMARY" *.svg` = 193
2. **Theme switch updates icons**: Change theme in Settings → icons change color
3. **No hardcoded colors**: IconRegistry uses ThemeManager colors only
4. **Pets icon matches others**: Same color as other icons in toolbar/menu

## Estimated Effort

- Fix SVG icons: ~30 min (batch re-download)
- Connect IconRegistry to ThemeManager: ~30 min
- Remove hardcoded colors: ~15 min
- Testing: ~15 min
- **Total: ~90 minutes** (atomic task)

## Current Status

**Status:** ✅ COMPLETE (2025-11-26)

**Builds on:**
- Task #00021 (IconRegistry Runtime System) - COMPLETE
- Task #00023 (Theme System Foundation) - COMPLETE
- Task #00024 (Settings Dialog Refactor) - COMPLETE

**Next:**
- Task #00026 (Centralized Icon Refresh) - will improve architecture
