# 00029: Fix Theme and Icon Loading on Linux/macOS

## Status
DEPLOYED

## Goal
Fix the theme system and icon loading on Linux and macOS platforms. Currently, the application works correctly on Windows but fails to display icons and apply theme styling on Linux/macOS.

## Problem Description
On Linux/macOS the following issues occur:
- Empty spaces in menus and toolbar where icons should appear
- Default dark background instead of proper theme styling
- Debug log shows ArtProvider creates actions (e.g., "ArtProvider: created action 'toggleai' (ctrl/)"), but icons are not visible
- No CSS styles applied to the interface

## Root Cause Analysis (Hypotheses)
1. **Resource path resolution** - Windows vs Unix path differences for:
   - Icon files: `resources/icons/`
   - Theme files: `resources/themes/`
2. **QRC resource compilation** - Resources may not be properly embedded or paths differ
3. **Platform-specific path separators** - Using `\` instead of `/` in resource paths
4. **Application directory detection** - `QCoreApplication::applicationDirPath()` may return different relative structure
5. **QStandardPaths differences** - Platform-specific standard locations

## Scope

### Included
- Investigation of resource path resolution on Linux/macOS
- Fix icon loading in ArtProvider/IconRegistry for all platforms
- Fix theme file loading in ThemeManager for all platforms
- Cross-platform path handling with proper separators
- Testing on Linux (CI) and macOS (if available)

### Excluded
- New features or UI changes
- Windows-specific fixes (already working)
- Changes to icon design or theme colors

## Acceptance Criteria
- [x] Icons display correctly in menus on Linux
- [x] Icons display correctly in toolbar on Linux
- [x] Theme CSS styles are applied on Linux
- [x] Icons display correctly in menus on macOS
- [x] Icons display correctly in toolbar on macOS
- [x] Theme CSS styles are applied on macOS
- [x] Windows functionality remains unchanged
- [x] CI build and tests pass on all platforms

## Files to Investigate
- `src/core/icon_registry.cpp` - Icon path resolution
- `src/core/theme_manager.cpp` - Theme file loading
- `src/core/art_provider.cpp` - Icon creation and caching
- `src/main.cpp` - Resource initialization
- `src/gui/main_window.cpp` - Resource path usage
- `CMakeLists.txt` - Resource file handling

## Design

### Constraint: No QRC (Qt Resource System)
Motywy graficzne beda ladowane jako pluginy Python, wiec zasoby MUSZA byc w systemie plikow (nie wbudowane w executable).

### Zidentyfikowane problemy

1. **CMake: `file(COPY ...)` wykonuje sie tylko podczas configure**
   - Zasoby sa kopiowane raz, podczas `cmake` configure
   - Nowe ikony/motywy wymagaja ponownego `cmake` configure
   - Na CI zasoby moga nie byc w oczekiwanym miejscu

2. **Rozne struktury katalogow na platformach**
   | Platforma | `applicationDirPath()` | Oczekiwana sciezka resources |
   |-----------|------------------------|------------------------------|
   | Windows | `build/bin/` | `build/bin/resources/` |
   | Linux | `build/bin/` | `build/bin/resources/` (moze brakowac) |
   | macOS Bundle | `App.app/Contents/MacOS/` | `App.app/Contents/Resources/` (brak!) |

3. **Brak multi-path fallback**
   - Kod szuka zasobow tylko w jednym miejscu
   - Nie ma fallbacku na inne standardowe lokalizacje

### Rozwiazanie

#### 1. CMake: Kopiowanie zasobow przy kazdym buildzie (nie tylko configure)

```cmake
# Zamiast file(COPY ...) uzyc add_custom_command z POST_BUILD
add_custom_command(TARGET kalahari POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:kalahari>/resources"
    COMMENT "Copying resources to output directory"
)
```

#### 2. macOS Bundle: Kopiowanie do Contents/Resources

```cmake
if(APPLE)
    set_target_properties(kalahari PROPERTIES
        MACOSX_BUNDLE TRUE
        RESOURCE "${CMAKE_SOURCE_DIR}/resources"
    )
    # Lub custom command do Contents/Resources
endif()
```

#### 3. Nowa klasa ResourcePaths - centralna logika wyszukiwania zasobow

Zamiast duplikowac logike w IconRegistry i ThemeManager, stworzyc centralna klase:

```cpp
// src/core/resource_paths.h
namespace kalahari::core {

class ResourcePaths {
public:
    static ResourcePaths& getInstance();

    // Get base resource directory (first found)
    QString getResourcesDir() const;

    // Get specific resource paths
    QString getIconPath(const QString& iconTheme, const QString& iconName) const;
    QString getThemePath(const QString& themeName) const;
    QString getThemesDir() const;

    // Check if resources are available
    bool resourcesFound() const;

    // Get all search paths (for debugging)
    QStringList getSearchPaths() const;

private:
    ResourcePaths();
    void initSearchPaths();

    QStringList m_searchPaths;
    QString m_foundResourcesDir;  // Cached first valid path
};

} // namespace kalahari::core
```

**Implementacja:**

```cpp
// src/core/resource_paths.cpp
void ResourcePaths::initSearchPaths() {
    QString appDir = QCoreApplication::applicationDirPath();

    // 1. Obok executable (Windows/Linux standard)
    m_searchPaths << appDir + "/resources";

    // 2. macOS Bundle: App.app/Contents/MacOS/../Resources
    #ifdef Q_OS_MACOS
    m_searchPaths << appDir + "/../Resources/resources";
    m_searchPaths << appDir + "/../Resources";
    #endif

    // 3. Linux FHS standard locations
    #ifdef Q_OS_LINUX
    m_searchPaths << "/usr/share/kalahari/resources";
    m_searchPaths << "/usr/local/share/kalahari/resources";
    m_searchPaths << QDir::homePath() + "/.local/share/kalahari/resources";
    #endif

    // 4. Development: wzgledem source (build-*/bin/ -> resources/)
    m_searchPaths << appDir + "/../../resources";
    m_searchPaths << appDir + "/../../../resources";

    // Find first valid path
    for (const QString& path : m_searchPaths) {
        QDir dir(path);
        if (dir.exists()) {
            m_foundResourcesDir = QDir::cleanPath(path);
            Logger::getInstance().info("ResourcePaths: Found resources at: {}",
                m_foundResourcesDir.toStdString());
            return;
        }
    }

    Logger::getInstance().error("ResourcePaths: No resources directory found!");
    Logger::getInstance().debug("ResourcePaths: Searched paths:");
    for (const QString& path : m_searchPaths) {
        Logger::getInstance().debug("  - {}", path.toStdString());
    }
}

QString ResourcePaths::getIconPath(const QString& iconTheme, const QString& iconName) const {
    if (m_foundResourcesDir.isEmpty()) return QString();
    return m_foundResourcesDir + "/icons/" + iconTheme + "/" + iconName;
}

QString ResourcePaths::getThemePath(const QString& themeName) const {
    if (m_foundResourcesDir.isEmpty()) return QString();
    return m_foundResourcesDir + "/themes/" + themeName + ".json";
}

QString ResourcePaths::getThemesDir() const {
    if (m_foundResourcesDir.isEmpty()) return QString();
    return m_foundResourcesDir + "/themes";
}
```

#### 4. Uzycie ResourcePaths w IconRegistry

```cpp
// icon_registry.cpp - zmiana w loadSVGFromFile()
QString IconRegistry::loadSVGFromFile(const QString& filePath) const {
    QString resolvedPath;

    if (QDir::isAbsolutePath(filePath)) {
        resolvedPath = filePath;
    } else {
        // Use ResourcePaths for relative paths like "resources/icons/twotone/save.svg"
        // Extract icon theme and name from path
        if (filePath.startsWith("resources/icons/")) {
            QString relativePart = filePath.mid(16);  // Remove "resources/icons/"
            int slashPos = relativePart.indexOf('/');
            if (slashPos > 0) {
                QString iconTheme = relativePart.left(slashPos);
                QString iconName = relativePart.mid(slashPos + 1);
                resolvedPath = ResourcePaths::getInstance().getIconPath(iconTheme, iconName);
            }
        }

        // Fallback to old behavior if ResourcePaths didn't resolve
        if (resolvedPath.isEmpty()) {
            resolvedPath = QCoreApplication::applicationDirPath() + "/" + filePath;
        }
    }

    // ... rest of loading code
}
```

#### 5. Uzycie ResourcePaths w ThemeManager

```cpp
// theme_manager.cpp - zmiana w loadThemeFile()
std::optional<nlohmann::json> ThemeManager::loadThemeFile(const QString& themeName) {
    QString themePath = ResourcePaths::getInstance().getThemePath(themeName);

    if (themePath.isEmpty()) {
        Logger::getInstance().error("ThemeManager: Resources not found, cannot load theme '{}'",
            themeName.toStdString());
        return std::nullopt;
    }

    QFile file(themePath);
    // ... rest of loading code
}

// theme_manager.cpp - zmiana w getAvailableThemes()
QStringList ThemeManager::getAvailableThemes() const {
    QStringList themes;

    QString themesPath = ResourcePaths::getInstance().getThemesDir();
    if (themesPath.isEmpty()) {
        Logger::getInstance().warn("ThemeManager: Resources not found");
        return themes;
    }

    QDir themesDir(themesPath);
    // ... rest of code
}
```

### Pliki do modyfikacji

| Plik | Zmiana |
|------|--------|
| `CMakeLists.txt` | POST_BUILD copy zamiast file(COPY) |
| `src/CMakeLists.txt` | macOS bundle resources, dodanie resource_paths.cpp |
| `src/core/icon_registry.cpp` | Uzycie ResourcePaths zamiast hardcoded path |
| `src/core/theme_manager.cpp` | Uzycie ResourcePaths zamiast hardcoded path |

### Nowe pliki

| Plik | Opis |
|------|------|
| `include/kalahari/core/resource_paths.h` | Header dla klasy ResourcePaths |
| `src/core/resource_paths.cpp` | Implementacja multi-path resource search |

## Notes
- Debug logging shows that ArtProvider::createAction is being called, so the action creation logic works
- The issue is specifically with loading the actual icon files and theme CSS
- Need to verify if resources are compiled into QRC or loaded from filesystem
- Linux CI exists and can be used for testing
- **2025-12-10:** Task completed and manually tested. All platforms working correctly.
