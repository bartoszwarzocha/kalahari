# Tasks: Centralized Theme Color Configuration

**Status:** IN_PROGRESS

**Created:** 2025-11-27
**Last Updated:** 2025-12-07

---

## CURRENT STATUS (2025-12-07) - ENVIRONMENT FIXED

### ABI Mismatch - RESOLVED

**Problem NIE byl w kodzie #00027!** Problem byl w srodowisku build po reinstalacji systemu.

**Root Cause:**
- Qt bylo zbudowane z: `MSVC 14.44.35207` (stary kompilator z cache)
- Aplikacja byla budowana z: `MSVC 14.50.35717` (nowy VS 2026)
- **ABI mismatch** powodowal Qt assertion "Format_Mono" w `qpixmap_win.cpp:200`

**Resolution (COMPLETE):**
1. Usunieto stare buildy: `vcpkg/buildtrees/*` i `build-windows/vcpkg_installed/*`
2. Naprawiono `scripts/build_windows.bat` - dodano detekcje VS 2026 (folder "18")
3. Naprawiono `tests/catch2_windows_compat.cpp` - dodano `#if _MSC_VER < 1950` (nowy MSVC ma te symbole)
4. Dodano brakujaca metode `MenuBuilder::getMenu()`
5. Przebudowano wszystko z MSVC 14.50.35717
6. **SUKCES:** Aplikacja uruchamia sie bez bledow Format_Mono!

### Next Steps
- Mozna kontynuowac implementacje #00027 (Theme Color Configuration)
- Commit `4be9896` z zmianami #00027 jest gotowy do zastosowania

---

## Summary

Create a centralized UI for configuring all theme colors in one place (Appearance -> Theme), with per-theme storage in settings.json and "Reset to Theme Defaults" functionality.

## Phase 1: Color Storage Architecture

- [x] Extend SettingsManager with per-theme log color methods
  - `getLogColorForTheme(themeName, colorKey, defaultColor)`
  - `setLogColorForTheme(themeName, colorKey, color)`
  - Keys: trace, debug, info, warning, error, critical, background

- [x] Update ThemeManager to load user color overrides
  - Check settings.json for theme-specific overrides
  - Fall back to Theme JSON defaults

- [x] Add log colors to SettingsData struct
  - QColor fields for each log level
  - Include in operator!= comparison

## Phase 2: ColorConfigWidget

- [x] Create reusable color picker widget
  - QPushButton showing current color
  - Click opens QColorDialog
  - Displays hex code next to button
  - Emits colorChanged(QColor) signal

- [x] Add to `include/kalahari/gui/widgets/color_config_widget.h`
- [x] Add to `src/gui/widgets/color_config_widget.cpp`

## Phase 3: Theme Color Editor UI

- [x] Redesign Appearance/Theme page
  - Theme selector at top (existing)
  - Dynamic color groups below

- [x] Build groups from Theme struct
  - "Icon Colors" group (primary, secondary)
  - "Log Panel Colors" group (6 levels + background)

## Phase 4: Reset to Defaults

- [x] Add "Reset to Theme Defaults" button
- [x] Implement reset logic

## Phase 5: Integration & Testing

- [x] **UNBLOCKED** - Environment issue fixed (ABI mismatch resolved)
- [ ] Test theme switching
- [ ] Test persistence
- [ ] Apply commit `4be9896` with #00027 changes

## ISSUES FIXED

1. ~~**Qt assertion** - Format_Mono error w qpixmap_win.cpp~~ - **FIXED** (ABI mismatch)
2. ~~**Build system** - triplet/vcpkg changes caused problems~~ - **FIXED** (VS 2026 detection)
3. ~~**Linker error** - catch2_windows_compat.cpp duplicate symbols~~ - **FIXED** (MSVC version check)
4. ~~**Missing method** - MenuBuilder::getMenu()~~ - **FIXED** (added method)

## REMAINING ISSUES (to verify after #00027 changes applied)

1. **Missing tab close buttons** - zakładki nie mają X
2. **Missing toolbar icons** - jeden toolbar pusty

## Files Created

| File | Status |
|------|--------|
| `include/kalahari/gui/widgets/color_config_widget.h` | Created |
| `src/gui/widgets/color_config_widget.cpp` | Created |

## Files Modified

| File | Status |
|------|--------|
| `include/kalahari/core/settings_manager.h` | Modified |
| `src/core/settings_manager.cpp` | Modified |
| `src/core/theme_manager.cpp` | Modified |
| `include/kalahari/gui/settings_data.h` | Modified |
| `include/kalahari/gui/settings_dialog.h` | Modified |
| `src/gui/settings_dialog.cpp` | Modified |
| `src/CMakeLists.txt` | Modified |

## Dependencies

- OpenSpec #00022 (Theme System Foundation) - DEPLOYED
- OpenSpec #00025 (Theme-Icon Integration) - DEPLOYED
- OpenSpec #00024 (Log Panel Enhanced) - DEPLOYED
