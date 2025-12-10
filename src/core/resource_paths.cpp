/// @file resource_paths.cpp
/// @brief Implementation of ResourcePaths singleton

#include "kalahari/core/resource_paths.h"
#include "kalahari/core/logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace kalahari {
namespace core {

// ============================================================================
// Singleton Instance
// ============================================================================

ResourcePaths& ResourcePaths::getInstance() {
    // Thread-safe since C++11 (magic statics)
    static ResourcePaths instance;
    return instance;
}

// ============================================================================
// Constructor
// ============================================================================

ResourcePaths::ResourcePaths() {
    initSearchPaths();
    m_foundResourcesDir = findResourcesDir();

    if (!m_foundResourcesDir.isEmpty()) {
        Logger::getInstance().info("ResourcePaths: Resources found at: {}",
                                   m_foundResourcesDir.toStdString());
    } else {
        Logger::getInstance().error("ResourcePaths: Resources NOT found!");
        Logger::getInstance().error("ResourcePaths: Searched locations:");
        for (const QString& path : m_searchPaths) {
            Logger::getInstance().error("  - {}", path.toStdString());
        }
    }
}

// ============================================================================
// Path Initialization
// ============================================================================

void ResourcePaths::initSearchPaths() {
    QString appDir = QCoreApplication::applicationDirPath();

    // Clear any existing paths
    m_searchPaths.clear();

    // Priority 1: Next to executable (most common for development and Windows installs)
    m_searchPaths << QDir::cleanPath(appDir + "/resources");

#ifdef Q_OS_MACOS
    // Priority 2a: macOS Bundle - Resources folder inside .app bundle
    // Structure: Kalahari.app/Contents/MacOS/kalahari (executable)
    //            Kalahari.app/Contents/Resources/resources (our resources)
    m_searchPaths << QDir::cleanPath(appDir + "/../Resources/resources");
    // Alternative: resources directly in Resources folder
    m_searchPaths << QDir::cleanPath(appDir + "/../Resources");
#endif

#ifdef Q_OS_LINUX
    // Priority 2b: Linux FHS standard locations
    // System-wide installation
    m_searchPaths << "/usr/share/kalahari/resources";
    m_searchPaths << "/usr/local/share/kalahari/resources";
    // User-local installation
    QString homeDir = QDir::homePath();
    m_searchPaths << QDir::cleanPath(homeDir + "/.local/share/kalahari/resources");
#endif

    // Priority 3: Development fallbacks (building from source)
    // When running from build/bin/Debug/kalahari, resources are at ../../resources
    m_searchPaths << QDir::cleanPath(appDir + "/../../resources");
    // When running from build/Debug/kalahari (some generators)
    m_searchPaths << QDir::cleanPath(appDir + "/../../../resources");
    // When running from build/bin/kalahari (single-config generators)
    m_searchPaths << QDir::cleanPath(appDir + "/../resources");
}

QString ResourcePaths::findResourcesDir() const {
    for (const QString& path : m_searchPaths) {
        QDir dir(path);
        // Check if directory exists and contains expected subdirectories
        if (dir.exists()) {
            // Validate by checking for icons and themes subdirectories
            bool hasIcons = QFileInfo::exists(path + "/icons");
            bool hasThemes = QFileInfo::exists(path + "/themes");

            if (hasIcons && hasThemes) {
                return QDir::cleanPath(path);
            }
        }
    }
    return QString();
}

// ============================================================================
// Public API
// ============================================================================

QString ResourcePaths::getResourcesDir() const {
    return m_foundResourcesDir;
}

QString ResourcePaths::getIconPath(const QString& iconTheme, const QString& iconName) const {
    if (m_foundResourcesDir.isEmpty()) {
        return QString();
    }
    return QDir::cleanPath(m_foundResourcesDir + "/icons/" + iconTheme + "/" + iconName);
}

QString ResourcePaths::getThemePath(const QString& themeName) const {
    if (m_foundResourcesDir.isEmpty()) {
        return QString();
    }
    return QDir::cleanPath(m_foundResourcesDir + "/themes/" + themeName + ".json");
}

QString ResourcePaths::getThemesDir() const {
    if (m_foundResourcesDir.isEmpty()) {
        return QString();
    }
    return QDir::cleanPath(m_foundResourcesDir + "/themes");
}

QString ResourcePaths::getIconsDir() const {
    if (m_foundResourcesDir.isEmpty()) {
        return QString();
    }
    return QDir::cleanPath(m_foundResourcesDir + "/icons");
}

bool ResourcePaths::resourcesFound() const {
    return !m_foundResourcesDir.isEmpty();
}

QStringList ResourcePaths::getSearchPaths() const {
    return m_searchPaths;
}

} // namespace core
} // namespace kalahari
