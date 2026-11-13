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

class ProjectManager;

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

    /// @brief Every book @p book opens, or creates and opens, goes to the top of the list and is
    /// the book opened at startup; each book window gives its book
    void follow(const ProjectManager& book);

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

    /// @brief The book to open at startup: the book opened last, while it is on the list
    ///
    /// A book created or imported without opening goes to the top of the list, yet the book
    /// the writer opened last stays the one opened at startup.
    /// @return Its .klh file; without one on the list the first of the list, empty for none
    QString lastOpenedBook() const;

    /// @brief Which books open at startup, with the option of the last book: "all" (the books
    /// of the last work, each in its window) or "last" (the book opened last)
    static constexpr const char* STARTUP_BOOKS_KEY = "startup.books";

    /// @brief The books open when the program ended last (their .klh files, the one in front
    /// last), for STARTUP_BOOKS_KEY "all"
    static constexpr const char* OPEN_BOOKS_KEY = "startup.openBooks";

    /// @brief Keep @p manifests as the books open when the program ends (OPEN_BOOKS_KEY)
    void setOpenBooks(const QStringList& manifests);

    /// @brief The books to open at startup, as STARTUP_BOOKS_KEY says: those of the last work
    /// that are still there, in their order, or without them (or with "last") the book opened
    /// last (lastOpenedBook())
    /// @return Their .klh files; empty for none
    QStringList booksToOpenAtStart() const;

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
