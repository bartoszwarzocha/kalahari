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
/// Backup location: {projectPath}/.backups/
/// Backup naming: project_YYYYMMDD_HHMMSS.db
class BackupManager {
public:
    /// @brief Construct backup manager for project
    /// @param projectPath Path to project directory
    explicit BackupManager(const QString& projectPath);

    /// @brief Create backup of project database
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

    /// @brief Generate backup filename with timestamp
    QString generateBackupName() const;

    /// @brief Get path to project database
    QString databasePath() const;

    QString m_projectPath;
    QString m_backupDir;
    QString m_lastError;
};

} // namespace core
} // namespace kalahari
