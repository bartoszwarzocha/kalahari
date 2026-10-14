/// @file project_lock.cpp
/// @brief Implementation of ProjectLock
///
/// OpenSpec #00041: SQLite Project Database

#include "kalahari/core/project_lock.h"
#include "kalahari/core/logger.h"

#include <QFile>
#include <QFileInfo>
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
#ifdef Q_OS_MACOS
#include <libproc.h>
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

bool ProjectLock::isHeldByOther(const QString& projectPath)
{
    const QString path = lockFilePath(projectPath);
    if (!QFile::exists(path)) {
        return false;
    }
    const qint64 pid = readPidFromLock(path);
    return pid != 0 && pid != QCoreApplication::applicationPid() && isProcessAlive(pid);
}

bool ProjectLock::isProcessAlive(qint64 pid)
{
    // The PID of a program that has ended (e.g. after a crash) can be another program's now:
    // only a Kalahari program, or its tests, holds a lock. A running program whose name
    // cannot be read counts as one.
    const auto isKalahari = [](const QString& program) {
        return QFileInfo(program).fileName().startsWith(QLatin1String("kalahari"),
                                                        Qt::CaseInsensitive);
    };
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr) {
        return false;
    }

    DWORD exitCode;
    bool alive = GetExitCodeProcess(process, &exitCode) && (exitCode == STILL_ACTIVE);
    if (alive) {
        wchar_t program[MAX_PATH] = {};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(process, 0, program, &size)) {
            alive = isKalahari(QString::fromWCharArray(program, static_cast<qsizetype>(size)));
        }
    }
    CloseHandle(process);
    return alive;
#else
    // POSIX: send signal 0 to check if process exists
    if (kill(static_cast<pid_t>(pid), 0) != 0) {
        return false;
    }
#ifdef Q_OS_MACOS
    char program[256] = {};
    if (proc_name(static_cast<int>(pid), program, sizeof(program)) > 0) {
        return isKalahari(QString::fromLocal8Bit(program));
    }
#else
    QFile name(QStringLiteral("/proc/%1/comm").arg(pid));
    if (name.open(QIODevice::ReadOnly)) {
        return isKalahari(QString::fromLocal8Bit(name.readAll().trimmed()));
    }
#endif
    return true;
#endif
}

} // namespace core
} // namespace kalahari
