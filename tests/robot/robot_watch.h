// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file robot_watch.h
/// @brief What the robot watches in each step besides the windows: the program's log, the
/// warnings of Qt, what the program asks the system to open, the settings it changes, and
/// whether it still responds

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace kalahari::robot {

class Report;

/// @brief The warnings and the errors of the program's log
///
/// The tests start without the log: the robot starts it with the file @p logFile, and the
/// console shows only its warnings and errors.
class LogWatcher {
public:
    /// @brief Watch the log from now on
    /// @param logFile The file of the log, when the test run has no log yet
    explicit LogWatcher(const QString& logFile);

    /// @brief Stops watching
    ~LogWatcher();

    LogWatcher(const LogWatcher&) = delete;
    LogWatcher& operator=(const LogWatcher&) = delete;
    LogWatcher(LogWatcher&&) = delete;
    LogWatcher& operator=(LogWatcher&&) = delete;

    /// @brief The lines logged since the last take(), each with its level ("error: ...")
    QStringList take();

private:
    class Sink;
    std::shared_ptr<Sink> m_sink;  ///< Among the sinks of the log while it watches
};

/// @brief The warnings of Qt ("QWidget::setLayout: ...")
///
/// The messages still reach the console. Messages that only say what a platform of the tests
/// cannot do (the offscreen platform cannot raise a window) are not warnings of the program.
class QtMessageWatcher {
public:
    /// @brief Watch Qt's messages from now on
    QtMessageWatcher();

    /// @brief Gives Qt its previous handler back
    ~QtMessageWatcher();

    QtMessageWatcher(const QtMessageWatcher&) = delete;
    QtMessageWatcher& operator=(const QtMessageWatcher&) = delete;
    QtMessageWatcher(QtMessageWatcher&&) = delete;
    QtMessageWatcher& operator=(QtMessageWatcher&&) = delete;

    /// @brief The warnings since the last take()
    static QStringList take();
};

/// @brief The system in place of the system: the browser, the file manager, the programs that
/// open files, the Trash and the window that chooses the file of a material. Nothing starts or
/// opens; the robot sees what the program asked for.
class OutsideWorld : public QObject {
    Q_OBJECT

public:
    /// @brief Stand in for the system from now on
    OutsideWorld();

    /// @brief Gives the program the system back
    ~OutsideWorld() override;

    OutsideWorld(const OutsideWorld&) = delete;
    OutsideWorld& operator=(const OutsideWorld&) = delete;
    OutsideWorld(OutsideWorld&&) = delete;
    OutsideWorld& operator=(OutsideWorld&&) = delete;

    /// @brief What the program asked for since the last take()
    QStringList take();

public slots:
    /// @brief An address QDesktopServices::openUrl() got (the handler of every scheme)
    void openUrl(const QUrl& url);

private:
    /// @brief Note what the program asked for
    void add(const QString& what);

    std::mutex m_mutex;    ///< The program may ask from another thread
    QStringList m_opened;  ///< What it asked for since the last take()
};

/// @brief The settings the program changes
class SettingsWatcher {
public:
    /// @brief Watch the settings from now on
    SettingsWatcher();

    /// @brief Stops watching
    ~SettingsWatcher();

    SettingsWatcher(const SettingsWatcher&) = delete;
    SettingsWatcher& operator=(const SettingsWatcher&) = delete;
    SettingsWatcher(SettingsWatcher&&) = delete;
    SettingsWatcher& operator=(SettingsWatcher&&) = delete;

    /// @brief The keys of the settings changed since the last take(), each once, in order
    QStringList take();

private:
    std::shared_ptr<std::mutex> m_mutex = std::make_shared<std::mutex>();  ///< Shared with the listener
    std::shared_ptr<QStringList> m_keys = std::make_shared<QStringList>();  ///< Changed keys
    int m_subscription = -1;  ///< The id of the listener
};

/// @brief Notices that the program stopped responding, and ends the run then: a program that
/// does not respond cannot be closed from inside. The step's record says so, and the robot ends
/// the process, so a run on GitHub does not wait for hours.
class Watchdog {
public:
    /// @brief Watch from now on
    /// @param report Where a hang is recorded
    /// @param silence How long the program may not respond
    /// @param stepLimit How long a step may take, the program responding or not
    Watchdog(Report& report, std::chrono::milliseconds silence, std::chrono::milliseconds stepLimit);

    /// @brief Stops watching
    ~Watchdog();

    Watchdog(const Watchdog&) = delete;
    Watchdog& operator=(const Watchdog&) = delete;
    Watchdog(Watchdog&&) = delete;
    Watchdog& operator=(Watchdog&&) = delete;

    /// @brief The program responds (the main thread calls it in its event loop)
    void beat();

    /// @brief Step @p index with command @p command begins
    void stepBegins(int index, const QString& command);

    /// @brief The step ended
    void stepEnds();

private:
    /// @brief The watching thread
    void watch();

    /// @brief Record the hang and end the process
    [[noreturn]] void giveUp(const QString& message);

    Report& m_report;                           ///< Where a hang is recorded
    const std::chrono::milliseconds m_silence;  ///< How long the program may not respond
    const std::chrono::milliseconds m_stepLimit;  ///< How long a step may take
    std::atomic<std::int64_t> m_lastBeat;       ///< When the program responded, ms
    std::atomic<std::int64_t> m_stepStart{-1};  ///< When the step began, ms; -1: no step
    std::atomic<int> m_step{0};                 ///< The index of the step
    std::mutex m_mutex;                         ///< Guards m_command and m_stop
    std::condition_variable m_wake;             ///< Wakes the thread to stop
    QString m_command;                          ///< The command of the step
    bool m_stop = false;                        ///< The thread ends
    std::thread m_thread;                       ///< The watching thread
};

}  // namespace kalahari::robot
