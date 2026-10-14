/// @file other_program.h
/// @brief Another Kalahari program that runs, for the tests of a book that it has open

#pragma once

#include <kalahari/core/project_lock.h>

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QStringList>

namespace kalahari {
namespace test {

/// @brief Another Kalahari program that runs until stop() or the end of the object: a copy of
/// the test program, which waits for a key before its tests, so it runs none
class OtherProgram {
public:
    OtherProgram() {
        m_process.start(QCoreApplication::applicationFilePath(),
                        {QStringLiteral("--wait-for-keypress"), QStringLiteral("start"),
                         QStringLiteral("[none]")});
        m_process.waitForStarted();
    }
    ~OtherProgram() { stop(); }
    OtherProgram(const OtherProgram&) = delete;
    OtherProgram& operator=(const OtherProgram&) = delete;

    /// @brief Its PID; 0 when it does not run
    qint64 pid() const { return m_process.processId(); }

    /// @brief End it at once, as a program that crashes
    void stop() {
        if (m_process.state() != QProcess::NotRunning) {
            m_process.kill();
            m_process.waitForFinished();
        }
    }

private:
    QProcess m_process;
};

/// @brief The lock of the book in @p folder as the program @p pid holds it
/// @return false when the lock cannot be written
inline bool lockBook(const QString& folder, qint64 pid) {
    QFile lock(core::ProjectLock::lockFilePath(folder));
    return lock.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           lock.write(QByteArray::number(pid)) > 0;
}

} // namespace test
} // namespace kalahari
