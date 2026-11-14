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
}

void RecentBooksManager::follow(const ProjectManager& book) {
    // Every book opened or just created goes to the top of the list (the Dashboard and
    // the Recent Books menu follow recentFilesChanged), and is the book opened at startup
    connect(&book, &ProjectManager::projectOpened, this, [this, &book]() {
        const QString manifest = book.getManifestPath();
        if (manifest.isEmpty()) {
            return;
        }
        addRecentFile(manifest);
        SettingsManager::getInstance().set(
            "startup.lastBook", QFileInfo(manifest).absoluteFilePath().toStdString());
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

QString RecentBooksManager::lastOpenedBook() const {
    const QString last = QString::fromStdString(
        SettingsManager::getInstance().get<std::string>("startup.lastBook"));
    if (!last.isEmpty()) {
        const QString path = QFileInfo(last).absoluteFilePath();
        if (m_recentFiles.contains(path)) {
            return path;
        }
    }
    return m_recentFiles.value(0);
}

void RecentBooksManager::setOpenBooks(const QStringList& manifests) {
    std::vector<std::string> books;
    for (const QString& manifest : manifests) {
        if (!manifest.isEmpty()) {
            books.push_back(QFileInfo(manifest).absoluteFilePath().toStdString());
        }
    }
    SettingsManager::getInstance().set<std::vector<std::string>>(OPEN_BOOKS_KEY, books);
}

QStringList RecentBooksManager::booksToOpenAtStart() const {
    auto& settings = SettingsManager::getInstance();
    QStringList books;
    if (settings.get<std::string>(STARTUP_BOOKS_KEY, "all") == "all") {
        for (const std::string& book : settings.get<std::vector<std::string>>(OPEN_BOOKS_KEY)) {
            const QString path = QFileInfo(QString::fromStdString(book)).absoluteFilePath();
            // A book moved or deleted since is left out, saying nothing
            if (QFileInfo::exists(path) && !books.contains(path)) {
                books.append(path);
            }
        }
    }
    if (books.isEmpty()) {
        const QString last = lastOpenedBook();
        if (!last.isEmpty()) {
            books.append(last);
        }
    }
    return books;
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
