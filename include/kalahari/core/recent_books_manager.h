/// @file recent_books_manager.h
/// @brief Manager for recent books list
///
/// OpenSpec #00030: Manages the list of recently opened .klh files
/// and provides dynamic menu integration.

#pragma once

#include <QObject>
#include <QMenu>
#include <QStringList>
#include <QString>
#include <functional>

namespace kalahari {
namespace core {

/// @brief Manager for recent books list
///
/// Singleton class that maintains a list of recently opened .klh files.
/// Integrates with FILE menu to provide dynamic "Recent Books" submenu.
///
/// Features:
/// - Stores up to 10 recent files
/// - Persists list to QSettings
/// - Provides dynamic menu population
/// - Emits signal when file should be opened
///
/// Example usage:
/// @code
/// auto& manager = RecentBooksManager::getInstance();
/// manager.addRecentFile("/path/to/book.klh");
/// manager.createRecentBooksMenu(fileMenu, this, &MainWindow::onOpenRecentFile);
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

    /// @brief Create or update the Recent Books submenu
    /// @param parentMenu FILE menu to add submenu to
    /// @param receiver Object to receive file open signals
    /// @param slot Slot to call with file path when item clicked
    /// @note Call this in createMenus() after FILE menu is created
    void createRecentBooksMenu(QMenu* parentMenu);

    /// @brief Maximum number of recent files to store
    static constexpr int MAX_RECENT_FILES = 10;

signals:
    /// @brief Emitted when user clicks a recent file item
    /// @param filePath Path to the file to open
    void recentFileClicked(const QString& filePath);

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

    /// @brief Load recent files from QSettings
    void loadRecentFiles();

    /// @brief Save recent files to QSettings
    void saveRecentFiles();

    /// @brief Update menu items based on current list
    void updateMenu();

    /// @brief Extract display name from file path
    /// @param filePath Full path to file
    /// @return Display name (filename without extension)
    QString getDisplayName(const QString& filePath) const;

    QStringList m_recentFiles;    ///< List of recent file paths
    QMenu* m_recentMenu;          ///< Recent Books submenu
    QAction* m_clearAction;       ///< "Clear Recent Files" action
};

} // namespace core
} // namespace kalahari
