/// @file project_lock.cpp
/// @brief Implementation of ProjectLock
///
/// OpenSpec #00041: SQLite Project Database

#include "kalahari/core/project_lock.h"
#include "kalahari/core/logger.h"

#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

namespace kalahari {
namespace core {

ProjectLock::ProjectLock(const QString& projectPath)
    : m_projectPath(projectPath)
    , m_lockFile(lockFilePath(projectPath))
{
}

ProjectLock::~ProjectLock()
{
    release();
}

QString ProjectLock::lockFilePath(const QString& projectPath)
{
    return QDir(projectPath).filePath(".kalahari.lock");
}

bool ProjectLock::tryAcquire()
{
    if (m_acquired) {
        return true;  // Already have the lock
    }

    // Check if lock file exists
    if (QFile::exists(m_lockFile)) {
        // Check if it's stale
        if (isStale(m_lockFile)) {
            Logger::getInstance().info("ProjectLock: Removing stale lock file");
            QFile::remove(m_lockFile);
        } else {
            qint64 pid = readPidFromLock(m_lockFile);
            Logger::getInstance().warn("ProjectLock: Project is locked by PID {}", pid);
            return false;
        }
    }

    // Create lock file
    writeLockFile();
    m_acquired = true;
    Logger::getInstance().info("ProjectLock: Lock acquired for {}", m_projectPath.toStdString());
    return true;
}

void ProjectLock::release()
{
    if (!m_acquired) {
        return;
    }

    if (QFile::exists(m_lockFile)) {
        QFile::remove(m_lockFile);
    }

    m_acquired = false;
    Logger::getInstance().info("ProjectLock: Lock released");
}

void ProjectLock::writeLockFile()
{
    QFile file(m_lockFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << QCoreApplication::applicationPid();
        file.close();
    }
}

qint64 ProjectLock::readPidFromLock(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }

    QTextStream in(&file);
    QString content = in.readLine().trimmed();
    file.close();

    bool ok;
    qint64 pid = content.toLongLong(&ok);
    return ok ? pid : 0;
}

bool ProjectLock::isStale(const QString& lockFilePath)
{
    qint64 pid = readPidFromLock(lockFilePath);
    if (pid == 0) {
        return true;  // Invalid lock file
    }

    // Check if it's our own PID
    if (pid == QCoreApplication::applicationPid()) {
        return false;  // We own this lock
    }

    return !isProcessAlive(pid);
}

bool ProjectLock::isProcessAlive(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr) {
        return false;
    }

    DWORD exitCode;
    bool alive = GetExitCodeProcess(process, &exitCode) && (exitCode == STILL_ACTIVE);
    CloseHandle(process);
    return alive;
#else
    // POSIX: send signal 0 to check if process exists
    return kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

} // namespace core
} // namespace kalahari
