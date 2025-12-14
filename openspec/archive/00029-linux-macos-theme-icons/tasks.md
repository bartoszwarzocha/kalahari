# Tasks for #00029

## Investigation
- [x] Analyze current resource path resolution in IconRegistry
- [x] Analyze current resource path resolution in ThemeManager
- [x] Check if resources are embedded (QRC) or loaded from filesystem
- [x] Identify platform-specific path handling differences
- [x] Check CMakeLists.txt for resource installation rules

## Implementation
- [x] Create ResourcePaths singleton class for centralized resource discovery
  - Created: `include/kalahari/core/resource_paths.h`
  - Created: `src/core/resource_paths.cpp`
  - Multi-path search with platform-specific locations
  - Caches first valid resources directory
  - Logs search results via Logger
- [x] Integrate ResourcePaths into IconRegistry
  - Modified: `src/core/icon_registry.cpp`
  - Added include for `resource_paths.h`
  - Updated `loadSVGFromFile()` to use ResourcePaths for icon discovery
  - Maintains fallback to old behavior for compatibility
- [x] Integrate ResourcePaths into ThemeManager
  - Modified: `src/core/theme_manager.cpp`
  - Added include for `resource_paths.h`
  - Updated `loadThemeFile()` to use ResourcePaths for theme discovery
  - Updated `getAvailableThemes()` to use ResourcePaths for themes directory
- [x] Ensure consistent path separators (use QDir::cleanPath)
- [x] Add fallback paths for different installation layouts
  - Windows: next to executable
  - macOS Bundle: ../Resources/resources
  - Linux FHS: /usr/share, /usr/local/share, ~/.local/share
  - Development: ../../resources, ../../../resources
- [x] Add diagnostic logging for resource loading failures

## Testing
- [x] Test icon loading on Linux (CI)
- [x] Test theme loading on Linux (CI)
- [x] Test icon loading on macOS (manual or CI)
- [x] Test theme loading on macOS (manual or CI)
- [x] Verify Windows still works correctly

## Documentation
- [x] Update CHANGELOG.md
- [x] Document cross-platform resource path handling (in resource_paths.h header)

## Completion
- **Date:** 2025-12-10
- **Status:** DEPLOYED
- **Tested:** Manual testing on all platforms passed
