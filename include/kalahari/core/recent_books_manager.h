/// @file recent_books_manager.h
/// @brief Manager for recent books list
///
/// OpenSpec #00030: Manages the list of recently opened .klh files. The File > Recent
/// Books submenu is gui::RecentBooksMenu.

#pragma once

#include <QObject>
#include <QStringList>
#include <QString>

namespace kalahari {
namespace core {

/// @brief Manager for recent books list
///
/// Singleton class that maintains a list of recently opened .klh files.
///
/// Features:
/// - Stores up to 10 recent files
/// - Persists the list in the settings
/// - Emits recentFilesChanged() when the list changes
///
/// Example usage:
/// @code
/// auto& manager = RecentBooksManager::getInstance();
/// manager.addRecentFile("/path/to/book.klh");
/// @endcode
class RecentBooksManager : public QObject {
    Q_OBJECT

public:
    /// @brief Get singleton instance
    /// @return Reference to the singleton instance
    static RecentBooksManager& getInstance();

    /// @brief Add a file to the recent list
    /// @param filePath Full path to the .klh file
    /// @note Moves file to top of list if already present
    /// @note Removes oldest entry if list exceeds MAX_RECENT_FILES
    void addRecentFile(const QString& filePath);

    /// @brief Remove a file from the recent list
    /// @param filePath Full path to remove
    void removeRecentFile(const QString& filePath);

    /// @brief Clear all recent files
    void clearRecentFiles();

    /// @brief Get list of recent files
    /// @return List of file paths (most recent first)
    QStringList getRecentFiles() const;

    /// @brief Check if recent files list is empty
    /// @return true if no recent files, false otherwise
    bool isEmpty() const;

    /// @brief Maximum number of recent files to store
    static constexpr int MAX_RECENT_FILES = 10;

signals:
    /// @brief Emitted when recent files list changes
    void recentFilesChanged();

private:
    /// @brief Private constructor (singleton)
    RecentBooksManager();

    /// @brief Destructor
    ~RecentBooksManager() override = default;

    /// @brief Prevent copying
    RecentBooksManager(const RecentBooksManager&) = delete;
    RecentBooksManager& operator=(const RecentBooksManager&) = delete;

    /// @brief Load recent files from the settings
    void loadRecentFiles();

    /// @brief Save recent files to the settings
    void saveRecentFiles();

    QStringList m_recentFiles;    ///< List of recent file paths
};

} // namespace core
} // namespace kalahari
