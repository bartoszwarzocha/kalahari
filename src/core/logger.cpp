/// @file logger.cpp
/// @brief Implementation of Logger singleton

#include "logger.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vector>
#include <stdexcept>

namespace kalahari {
namespace core {

Logger& Logger::getInstance() {
    // Thread-safe since C++11 (magic statics)
    static Logger instance;
    return instance;
}

void Logger::init(const std::string& logFilePath) {
    if (m_logger) {
        // Already initialized - just log a warning
        m_logger->warn("Logger::init() called twice - ignoring");
        return;
    }

    try {
        // Create sinks (console + file)
        std::vector<spdlog::sink_ptr> sinks;

        // Console sink (color output to stdout/stderr)
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);
        sinks.push_back(console_sink);

        // File sink (write to log file)
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath, true);
        file_sink->set_level(spdlog::level::trace);
        sinks.push_back(file_sink);

        // Create logger with both sinks
        m_logger = std::make_shared<spdlog::logger>("kalahari", sinks.begin(), sinks.end());

        // Set log level based on build type
#ifdef NDEBUG
        m_logger->set_level(spdlog::level::info);  // Release: info and above
#else
        m_logger->set_level(spdlog::level::debug); // Debug: all messages
#endif

        // Set pattern: [timestamp] [level] message
        m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

        // Register as default logger
        spdlog::set_default_logger(m_logger);

        // Flush on every message (safer, minimal performance impact)
        m_logger->flush_on(spdlog::level::trace);

        m_logger->info("Logger initialized (log file: {})", logFilePath);

    } catch (const spdlog::spdlog_ex& ex) {
        // If we can't initialize logger, throw exception
        throw std::runtime_error(std::string("Failed to initialize logger: ") + ex.what());
    }
}

bool Logger::isInitialized() const {
    return m_logger != nullptr;
}

void Logger::flush() {
    if (m_logger) {
        m_logger->flush();
    }
}

} // namespace core
} // namespace kalahari
