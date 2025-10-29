/// @file logger.h
/// @brief Thread-safe logging singleton wrapper for spdlog
///
/// This file provides a global logger singleton that wraps spdlog functionality.
/// The logger is thread-safe and provides convenient methods for different log levels.
///
/// Example usage:
/// @code
/// Logger::getInstance().init("/path/to/logs/kalahari.log");
/// Logger::getInstance().info("Application started");
/// Logger::getInstance().warn("Low memory: {} MB", availableMemory);
/// Logger::getInstance().error("Failed to load file: {}", filename);
/// @endcode

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>

namespace kalahari {
namespace core {

/// @brief Thread-safe logging singleton
///
/// Provides a global access point for logging throughout the application.
/// Uses spdlog internally for high-performance, thread-safe logging.
/// Supports both console and file output.
class Logger {
public:
    /// @brief Get the singleton instance
    /// @return Reference to the global Logger instance
    static Logger& getInstance();

    /// @brief Initialize the logger with file output
    /// @param logFilePath Absolute path to the log file
    /// @throws std::runtime_error if logger initialization fails
    ///
    /// This method should be called once during application startup.
    /// It creates both console and file sinks for output.
    void init(const std::string& logFilePath);

    /// @brief Check if logger has been initialized
    /// @return true if init() has been called, false otherwise
    bool isInitialized() const;

    /// @brief Log a debug message (only in Debug builds)
    /// @tparam Args Variadic template arguments for format string
    /// @param fmt Format string (fmt library syntax)
    /// @param args Arguments to format
    template<typename... Args>
    void debug(fmt::format_string<Args...> fmt, Args&&... args) {
        if (m_logger) {
            m_logger->debug(fmt, std::forward<Args>(args)...);
        }
    }

    /// @brief Log an informational message
    /// @tparam Args Variadic template arguments for format string
    /// @param fmt Format string (fmt library syntax)
    /// @param args Arguments to format
    template<typename... Args>
    void info(fmt::format_string<Args...> fmt, Args&&... args) {
        if (m_logger) {
            m_logger->info(fmt, std::forward<Args>(args)...);
        }
    }

    /// @brief Log a warning message
    /// @tparam Args Variadic template arguments for format string
    /// @param fmt Format string (fmt library syntax)
    /// @param args Arguments to format
    template<typename... Args>
    void warn(fmt::format_string<Args...> fmt, Args&&... args) {
        if (m_logger) {
            m_logger->warn(fmt, std::forward<Args>(args)...);
        }
    }

    /// @brief Log an error message
    /// @tparam Args Variadic template arguments for format string
    /// @param fmt Format string (fmt library syntax)
    /// @param args Arguments to format
    template<typename... Args>
    void error(fmt::format_string<Args...> fmt, Args&&... args) {
        if (m_logger) {
            m_logger->error(fmt, std::forward<Args>(args)...);
        }
    }

    /// @brief Log a critical error message
    /// @tparam Args Variadic template arguments for format string
    /// @param fmt Format string (fmt library syntax)
    /// @param args Arguments to format
    template<typename... Args>
    void critical(fmt::format_string<Args...> fmt, Args&&... args) {
        if (m_logger) {
            m_logger->critical(fmt, std::forward<Args>(args)...);
        }
    }

    /// @brief Flush all pending log messages
    ///
    /// Forces all buffered log messages to be written immediately.
    /// Automatically called on program exit, but can be called manually
    /// for critical logging points.
    void flush();

private:
    /// @brief Private constructor (singleton pattern)
    Logger() = default;

    /// @brief Deleted copy constructor (non-copyable)
    Logger(const Logger&) = delete;

    /// @brief Deleted copy assignment (non-copyable)
    Logger& operator=(const Logger&) = delete;

    /// @brief The underlying spdlog logger instance
    std::shared_ptr<spdlog::logger> m_logger;
};

} // namespace core
} // namespace kalahari
