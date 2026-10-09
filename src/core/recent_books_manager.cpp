/// @file recent_books_manager.cpp
/// @brief Implementation of RecentBooksManager
///
/// OpenSpec #00030: Manages recent books list with QSettings persistence

#include "kalahari/core/recent_books_manager.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include <QFileInfo>
#include <vector>

namespace kalahari {
namespace core {

RecentBooksManager& RecentBooksManager::getInstance() {
    static RecentBooksManager instance;
    return instance;
}

RecentBooksManager::RecentBooksManager()
    : QObject(nullptr)
{
    loadRecentFiles();

    // Every book opened or just created goes to the top of the list (the Dashboard and
    // the Recent Books menu follow recentFilesChanged)
    auto& projectManager = ProjectManager::getInstance();
    connect(&projectManager, &ProjectManager::projectOpened, this, [this, &projectManager]() {
        addRecentFile(projectManager.getManifestPath());
    });
}

void RecentBooksManager::addRecentFile(const QString& filePath) {
    auto& logger = Logger::getInstance();

    if (filePath.isEmpty()) {
        return;
    }

    // Normalize path
    QString normalizedPath = QFileInfo(filePath).absoluteFilePath();

    // Remove if already in list (will be re-added at top)
    m_recentFiles.removeAll(normalizedPath);

    // Add to beginning of list
    m_recentFiles.prepend(normalizedPath);

    // Trim to max size
    while (m_recentFiles.size() > MAX_RECENT_FILES) {
        m_recentFiles.removeLast();
    }

    saveRecentFiles();

    logger.debug("RecentBooksManager: Added '{}' to recent files ({} total)",
        normalizedPath.toStdString(), m_recentFiles.size());

    emit recentFilesChanged();
}

void RecentBooksManager::removeRecentFile(const QString& filePath) {
    QString normalizedPath = QFileInfo(filePath).absoluteFilePath();

    if (m_recentFiles.removeAll(normalizedPath) > 0) {
        saveRecentFiles();
            emit recentFilesChanged();
    }
}

void RecentBooksManager::clearRecentFiles() {
    auto& logger = Logger::getInstance();

    if (!m_recentFiles.isEmpty()) {
        m_recentFiles.clear();
        saveRecentFiles();
            logger.info("RecentBooksManager: Cleared all recent files");
        emit recentFilesChanged();
    }
}

QStringList RecentBooksManager::getRecentFiles() const {
    return m_recentFiles;
}

bool RecentBooksManager::isEmpty() const {
    return m_recentFiles.isEmpty();
}

void RecentBooksManager::loadRecentFiles() {
    m_recentFiles.clear();
    const auto files = SettingsManager::getInstance().get<std::vector<std::string>>("recent_files");
    for (const std::string& file : files) {
        m_recentFiles.append(QString::fromStdString(file));
    }

    // Validate paths - remove non-existent files
    QStringList validFiles;
    for (const QString& path : m_recentFiles) {
        if (QFileInfo::exists(path)) {
            validFiles.append(path);
        }
    }

    if (validFiles.size() != m_recentFiles.size()) {
        m_recentFiles = validFiles;
        saveRecentFiles();
    }
}

void RecentBooksManager::saveRecentFiles() {
    std::vector<std::string> files;
    files.reserve(static_cast<size_t>(m_recentFiles.size()));
    for (const QString& file : m_recentFiles) {
        files.push_back(file.toStdString());
    }
    SettingsManager::getInstance().set("recent_files", files);
}

} // namespace core
} // namespace kalahari
