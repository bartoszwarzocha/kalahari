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
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>

namespace kalahari {
namespace core {

namespace {

/// @brief A folder the way the folders of backups compare it: absolute, cleaned
QString cleanFolder(const QString& folder) {
    return folder.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(folder).absoluteFilePath());
}

/// @brief Whether two paths name the same folder (on Windows letters of either case are the
/// same)
bool sameFolder(const QString& first, const QString& second) {
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity CASE = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity CASE = Qt::CaseSensitive;
#endif
    return !first.isEmpty() && cleanFolder(first).compare(cleanFolder(second), CASE) == 0;
}

/// @brief The folder of the book that the folder of backups @p folder belongs to; empty when
/// it is not a folder of backups of a book
QString bookFolderOf(const QString& folder) {
    QFile file(QDir(folder).filePath(QString::fromLatin1(BackupManager::BOOK_FILE)));
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const QJsonObject book = QJsonDocument::fromJson(file.readAll()).object();
    return book.value(QStringLiteral("folder")).toString();
}

}  // namespace

BackupManager::BackupManager(const QString& projectPath)
    : m_projectPath(projectPath)
    , m_backupDir(QDir(projectPath).filePath(".backups"))
{
}

void BackupManager::setCommonFolder(const QString& folder, const QString& projectId)
{
    const QString common = folder.trimmed();
    m_commonFolder =
        common.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(common));
    m_projectId = projectId;
    m_backupDir = m_commonFolder.isEmpty() ? QDir(m_projectPath).filePath(".backups")
                                           : bookFolderIn(m_commonFolder);
}

QString BackupManager::bookFolderIn(const QString& folder) const
{
    const QDir common(folder);
    for (const QFileInfo& entry :
         common.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (sameFolder(bookFolderOf(entry.absoluteFilePath()), m_projectPath)) {
            return entry.absoluteFilePath();
        }
    }

    // A new folder named after the folder of the book; a name another book took gets a number
    QString name = QDir(cleanFolder(m_projectPath)).dirName();
    if (name.isEmpty()) {
        name = QStringLiteral("book");  // a book at the root of a drive
    }
    QString candidate = common.filePath(name);
    for (int number = 2; QFileInfo::exists(candidate); ++number) {
        candidate = common.filePath(QStringLiteral("%1 (%2)").arg(name).arg(number));
    }
    return candidate;
}

bool BackupManager::writeBookFile() const
{
    const QJsonObject book{{QStringLiteral("folder"), cleanFolder(m_projectPath)},
                           {QStringLiteral("project"), m_projectId}};
    QSaveFile file(QDir(m_backupDir).filePath(QString::fromLatin1(BOOK_FILE)));
    return file.open(QIODevice::WriteOnly) &&
           file.write(QJsonDocument(book).toJson()) >= 0 && file.commit();
}

QString BackupManager::databasePath() const
{
    return QDir(m_projectPath).filePath("project.db");
}

bool BackupManager::ensureBackupDirExists()
{
    if (!m_commonFolder.isEmpty() && (!QDir().mkpath(m_backupDir) || !writeBookFile())) {
        // The backup is not lost: it goes to the book's own folder
        Logger::getInstance().warn(
            "BackupManager: Cannot write to {}, the backup goes to the folder of the book",
            m_backupDir.toStdString());
        m_backupDir = QDir(m_projectPath).filePath(".backups");
    }

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
