/// @file project_lock.h
/// @brief Project lock mechanism to prevent concurrent access
///
/// ProjectLock ensures only one instance of Kalahari can open a project at a time.
/// Uses a PID-based lock file that can detect and clean up stale locks from crashed instances.
///
/// Usage:
/// @code
/// ProjectLock lock("/path/to/project");
/// if (!lock.tryAcquire()) {
///     // Project is already open elsewhere
///     showError("Project is already open in another instance");
///     return;
/// }
/// // ... work with project ...
/// // Lock is released automatically in destructor
/// @endcode
///
/// OpenSpec #00041: SQLite Project Database

#pragma once

#include <QString>

namespace kalahari {
namespace core {

/// @brief RAII-style project lock using PID-based lock file
///
/// Lock file location: {projectPath}/.kalahari.lock
/// Lock file format: Single line with PID of owning process
///
/// Features:
/// - Automatic cleanup on destruction (RAII)
/// - Stale lock detection (process no longer running)
/// - Cross-platform PID handling (Windows/Linux/macOS)
class ProjectLock {
public:
    /// @brief Construct lock for project path
    /// @param projectPath Path to project directory
    explicit ProjectLock(const QString& projectPath);

    /// @brief Destructor - releases lock if acquired
    ~ProjectLock();

    // Non-copyable, non-movable (RAII resource)
    ProjectLock(const ProjectLock&) = delete;
    ProjectLock& operator=(const ProjectLock&) = delete;
    ProjectLock(ProjectLock&&) = delete;
    ProjectLock& operator=(ProjectLock&&) = delete;

    /// @brief Attempt to acquire the lock
    /// @return true if lock acquired, false if project is already locked
    ///
    /// If a stale lock is detected (owning process no longer exists),
    /// it is automatically removed and lock is acquired.
    bool tryAcquire();

    /// @brief Release the lock
    ///
    /// Safe to call multiple times or if lock wasn't acquired.
    void release();

    /// @brief Check if this instance holds the lock
    /// @return true if lock is currently acquired
    bool isAcquired() const { return m_acquired; }

    /// @brief Check if a lock file exists and is stale
    /// @param lockFilePath Path to lock file
    /// @return true if lock file exists but owning process is dead
    static bool isStale(const QString& lockFilePath);

    /// @brief Get path to lock file for a project
    /// @param projectPath Path to project directory
    /// @return Full path to lock file
    static QString lockFilePath(const QString& projectPath);

private:
    /// @brief Write current PID to lock file
    void writeLockFile();

    /// @brief Read PID from lock file
    /// @param path Path to lock file
    /// @return PID from file, or 0 if file doesn't exist or is invalid
    static qint64 readPidFromLock(const QString& path);

    /// @brief Check if process with given PID is running
    /// @param pid Process ID to check
    /// @return true if process exists
    static bool isProcessAlive(qint64 pid);

    QString m_projectPath;   ///< Project directory path
    QString m_lockFile;      ///< Full path to lock file
    bool m_acquired = false; ///< Whether lock is currently held
};

} // namespace core
} // namespace kalahari
