/// @file log_panel_sink.h
/// @brief Custom spdlog sink for LogPanel (Qt signal-based)
///
/// LogPanelSink is a thread-safe spdlog sink that emits Qt signals
/// when log messages arrive. This enables real-time log display
/// in the GUI without blocking the logging thread.
///
/// Usage:
/// @code
/// auto sink = std::make_shared<LogPanelSink>();
/// connect(sink.get(), &LogPanelSink::logMessage,
///         logPanel, &LogPanel::appendLog);
/// Logger::getInstance().addSink(sink);
/// @endcode

#pragma once

#include <QObject>
#include <QString>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>
#include <mutex>

namespace kalahari {
namespace core {

/// @brief spdlog sink that emits Qt signals for GUI integration
///
/// Thread-safe sink that converts spdlog messages to Qt signals.
/// The signal is emitted from the logging thread, so the receiving
/// slot should use Qt::QueuedConnection for thread safety.
///
/// Features:
/// - Thread-safe (uses std::mutex internally via base_sink)
/// - Emits signal with log level and formatted message
/// - Minimal overhead on logging path
class LogPanelSink : public QObject, public spdlog::sinks::base_sink<std::mutex> {
    Q_OBJECT

public:
    /// @brief Constructor
    explicit LogPanelSink(QObject* parent = nullptr);

    /// @brief Destructor
    ~LogPanelSink() override = default;

signals:
    /// @brief Emitted when a log message is received
    /// @param level spdlog level (trace=0, debug=1, info=2, warn=3, error=4, critical=5)
    /// @param message Formatted log message (without level prefix)
    void logMessage(int level, const QString& message);

protected:
    /// @brief Called by spdlog when a message is logged
    /// @param msg Log message details
    void sink_it_(const spdlog::details::log_msg& msg) override;

    /// @brief Called by spdlog to flush the sink
    void flush_() override;
};

} // namespace core
} // namespace kalahari
