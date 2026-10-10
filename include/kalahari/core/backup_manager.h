/// @file backup_manager.h
/// @brief Project database backup management
///
/// BackupManager handles backup creation, rotation, and restoration
/// for project.db files.
///
/// OpenSpec #00041: SQLite Project Database

#pragma once

#include <QString>
#include <QStringList>

namespace kalahari {
namespace core {

/// @brief Manages project database backups
///
/// Backup location: {projectPath}/.backups/, or a folder of the book in one common folder of
/// the backups of all books (setCommonFolder()). That folder is named after the folder of the
/// book and holds a file (BOOK_FILE) that names the folder of the book, so that two books with
/// the same name keep their backups apart. A book moved to another folder gets a new folder of
/// backups, and the old one keeps its copies.
/// Backup naming: project_YYYYMMDD_HHMMSS.db
class BackupManager {
public:
    /// @brief Setting of the common folder of the backups of all books; empty: the .backups
    /// folder of each book
    static constexpr const char* FOLDER_SETTING = "project.backupFolder";
    /// @brief Setting of the number of the newest backups kept of each book
    static constexpr const char* COUNT_SETTING = "project.backupCount";
    /// @brief File in the folder of a book in the common folder: the folder of the book and
    /// the id of its project
    static constexpr const char* BOOK_FILE = "kalahari-book.json";

    /// @brief Construct backup manager for project
    /// @param projectPath Path to project directory
    explicit BackupManager(const QString& projectPath);

    /// @brief Keep the backups in the folder of the book in common folder @p folder, or in
    /// the book's own .backups folder when @p folder is empty
    /// @param folder The common folder of the backups of all books
    /// @param projectId Id of the project of the book
    void setCommonFolder(const QString& folder, const QString& projectId);

    /// @brief Create backup of project database
    ///
    /// A common folder that cannot be created or written takes the backup to the book's own
    /// .backups folder, so that it is not lost.
    /// @return Path to created backup, or empty string on failure
    QString createBackup();

    /// @brief Rotate backups, keeping only the most recent N
    /// @param keepCount Number of backups to keep (default: 5)
    void rotateBackups(int keepCount = 5);

    /// @brief Get list of available backups (newest first)
    /// @return List of backup file paths
    QStringList availableBackups() const;

    /// @brief Restore database from backup
    /// @param backupPath Path to backup file
    /// @return true on success
    bool restoreFromBackup(const QString& backupPath);

    /// @brief Get backup directory path
    QString backupDir() const { return m_backupDir; }

    /// @brief Get last error message
    QString lastError() const { return m_lastError; }

private:
    /// @brief Ensure backup directory exists
    bool ensureBackupDirExists();

    /// @brief The folder of the book in common folder @p folder: the one whose BOOK_FILE
    /// names the folder of the book, else a new one named after the folder of the book
    QString bookFolderIn(const QString& folder) const;

    /// @brief Write BOOK_FILE into the folder of the book in the common folder
    bool writeBookFile() const;

    /// @brief Generate backup filename with timestamp
    QString generateBackupName() const;

    /// @brief Get path to project database
    QString databasePath() const;

    QString m_projectPath;
    QString m_backupDir;
    QString m_commonFolder;  ///< Empty: the backups are in the book's own folder
    QString m_projectId;
    QString m_lastError;
};

} // namespace core
} // namespace kalahari
