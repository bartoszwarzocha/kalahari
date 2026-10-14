/// @file logger.cpp
/// @brief Implementation of Logger singleton

#include <kalahari/core/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

namespace kalahari {
namespace core {

namespace {

/// Every line: [timestamp] [level] message
constexpr const char* LINE_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v";

/// The messages that wait for the log file at most; a start logs a few dozen before it
constexpr std::size_t WAITING_MESSAGES = 2000;

} // anonymous namespace

Logger& Logger::getInstance() {
    // Thread-safe since C++11 (magic statics)
    static Logger instance;
    return instance;
}

spdlog::sink_ptr Logger::openFileSink(const std::string& logFilePath, std::string& openedPath) {
    std::vector<std::string> candidates{logFilePath};
    std::error_code ec;
    const auto tempDir = std::filesystem::temp_directory_path(ec);
    if (!ec) {
        candidates.push_back((tempDir / "kalahari.log").string());
    }

    for (const auto& path : candidates) {
        try {
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true);
            openedPath = path;
            return sink;
        } catch (const spdlog::spdlog_ex& e) {
            // Logger is not set up yet, so report to stderr and try the next location
            std::cerr << "Cannot open log file " << path << ": " << e.what() << '\n';
        }
    }
    openedPath.clear();
    return nullptr;
}

void Logger::init(const std::string& logFilePath) {
    initWithoutFile();
    openFile(logFilePath);
}

void Logger::initWithoutFile() {
    if (m_logger) {
        // Already initialized - just log a warning
        m_logger->warn("Logger::init() called twice - ignoring");
        return;
    }

    // Console sink (color output to stdout/stderr)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);

    // The messages wait here for the log file
    m_waitingSink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(WAITING_MESSAGES);
    m_waitingSink->set_level(spdlog::level::trace);

    // Create logger with both sinks
    m_logger = std::make_shared<spdlog::logger>(
        "kalahari", spdlog::sinks_init_list{console_sink, m_waitingSink});

    // Set log level based on build type
#ifdef NDEBUG
    m_logger->set_level(spdlog::level::info);  // Release: info and above
#else
    m_logger->set_level(spdlog::level::debug); // Debug: all messages
#endif

    // Set pattern: [timestamp] [level] message
    m_logger->set_pattern(LINE_PATTERN);

    // Register as default logger
    spdlog::set_default_logger(m_logger);

    // Flush on every message (safer, minimal performance impact)
    m_logger->flush_on(spdlog::level::trace);

    // spdlog statics created above (e.g. the console mutex) are destroyed
    // before singletons that were constructed earlier. Handlers registered
    // now run before those statics go away, so destructors that run later
    // (PluginManager, PythonInterpreter) log nothing instead of crashing.
    std::atexit([] { Logger::getInstance().shutdown(); });
}

void Logger::openFile(const std::string& logFilePath) {
    if (!m_logger || !m_waitingSink) {
        return;  // not initialized, or the file is open already
    }

    // File sink (write to log file, with a fallback when it is not writable): first the
    // messages that waited for it, then the later ones
    auto& sinks = m_logger->sinks();
    auto file_sink = openFileSink(logFilePath, m_logFilePath);
    if (file_sink) {
        file_sink->set_level(spdlog::level::trace);
        file_sink->set_pattern(LINE_PATTERN);
        for (const auto& message : m_waitingSink->last_raw()) {
            file_sink->log(message);
        }
        file_sink->flush();
        sinks.push_back(file_sink);
    }
    sinks.erase(std::remove(sinks.begin(), sinks.end(), m_waitingSink), sinks.end());
    m_waitingSink.reset();

    if (m_logFilePath.empty()) {
        m_logger->warn("Cannot write log file {} or a temp-directory fallback - logging to console only",
                       logFilePath);
    } else {
        if (m_logFilePath != logFilePath) {
            m_logger->warn("Cannot write log file {} - using {}", logFilePath, m_logFilePath);
        }
        m_logger->info("Logger initialized (log file: {})", m_logFilePath);
    }
}

void Logger::shutdown() {
    if (m_logger) {
        m_logger->flush();
        m_logger.reset();
    }
    m_waitingSink.reset();
}

bool Logger::isInitialized() const {
    return m_logger != nullptr;
}

void Logger::flush() {
    if (m_logger) {
        m_logger->flush();
    }
}

void Logger::setLevel(spdlog::level::level_enum level) {
    if (m_logger) {
        m_logger->set_level(level);
        m_logger->info("Logger level changed to: {}", spdlog::level::to_string_view(level));
    }
}

spdlog::level::level_enum Logger::getLevel() const {
    if (m_logger) {
        return m_logger->level();
    }
    return spdlog::level::info;  // Default if not initialized
}

} // namespace core
} // namespace kalahari
