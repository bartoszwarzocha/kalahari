/// @file backup_manager.cpp
/// @brief Implementation of BackupManager
///
/// OpenSpec #00041: SQLite Project Database

#include "kalahari/core/backup_manager.h"
#include "kalahari/core/logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <algorithm>

namespace kalahari {
namespace core {

BackupManager::BackupManager(const QString& projectPath)
    : m_projectPath(projectPath)
    , m_backupDir(QDir(projectPath).filePath(".backups"))
{
}

QString BackupManager::databasePath() const
{
    return QDir(m_projectPath).filePath("project.db");
}

bool BackupManager::ensureBackupDirExists()
{
    QDir dir(m_backupDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            m_lastError = "Cannot create backup directory";
            Logger::getInstance().error("BackupManager: {}", m_lastError.toStdString());
            return false;
        }
    }
    return true;
}

QString BackupManager::generateBackupName() const
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("project_%1.db").arg(timestamp);
}

QString BackupManager::createBackup()
{
    QString dbPath = databasePath();

    if (!QFile::exists(dbPath)) {
        m_lastError = "Database file does not exist";
        Logger::getInstance().warn("BackupManager: {}", m_lastError.toStdString());
        return QString();
    }

    if (!ensureBackupDirExists()) {
        return QString();
    }

    QString backupName = generateBackupName();
    QString backupPath = QDir(m_backupDir).filePath(backupName);

    if (!QFile::copy(dbPath, backupPath)) {
        m_lastError = "Failed to copy database file";
        Logger::getInstance().error("BackupManager: {}", m_lastError.toStdString());
        return QString();
    }

    Logger::getInstance().info("BackupManager: Created backup {}", backupName.toStdString());
    return backupPath;
}

void BackupManager::rotateBackups(int keepCount)
{
    QStringList backups = availableBackups();

    if (backups.size() <= keepCount) {
        return;  // Nothing to remove
    }

    // Remove oldest backups (list is sorted newest first)
    for (int i = keepCount; i < backups.size(); ++i) {
        QFile::remove(backups[i]);
        Logger::getInstance().info("BackupManager: Removed old backup {}",
                                  QFileInfo(backups[i]).fileName().toStdString());
    }
}

QStringList BackupManager::availableBackups() const
{
    QDir dir(m_backupDir);
    if (!dir.exists()) {
        return QStringList();
    }

    QStringList filters;
    filters << "project_*.db";

    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);

    QStringList result;
    for (const QFileInfo& fi : files) {
        result << fi.absoluteFilePath();
    }

    return result;
}

bool BackupManager::restoreFromBackup(const QString& backupPath)
{
    if (!QFile::exists(backupPath)) {
        m_lastError = "Backup file does not exist";
        Logger::getInstance().error("BackupManager: {}", m_lastError.toStdString());
        return false;
    }

    QString dbPath = databasePath();

    // Remove current database if exists
    if (QFile::exists(dbPath)) {
        if (!QFile::remove(dbPath)) {
            m_lastError = "Cannot remove current database";
            Logger::getInstance().error("BackupManager: {}", m_lastError.toStdString());
            return false;
        }
    }

    // Copy backup to database location
    if (!QFile::copy(backupPath, dbPath)) {
        m_lastError = "Failed to restore from backup";
        Logger::getInstance().error("BackupManager: {}", m_lastError.toStdString());
        return false;
    }

    Logger::getInstance().info("BackupManager: Restored from {}",
                              QFileInfo(backupPath).fileName().toStdString());
    return true;
}

} // namespace core
} // namespace kalahari
