/// @file log_panel_sink.cpp
/// @brief Implementation of LogPanelSink

#include "kalahari/core/log_panel_sink.h"
#include <spdlog/pattern_formatter.h>

namespace kalahari {
namespace core {

LogPanelSink::LogPanelSink(QObject* parent)
    : QObject(parent)
{
    // Format: [HH:MM:SS.mmm] [LEVEL] message
    // %l = level name (trace, debug, info, warning, error, critical)
    // %^...%$ = color markers (ignored in our case, we handle colors separately)
    set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    // Set sink level to trace - accept ALL messages from logger
    // LogPanel::shouldDisplayLevel() handles filtering based on diagnostic mode
    set_level(spdlog::level::trace);
}

void LogPanelSink::sink_it_(const spdlog::details::log_msg& msg) {
    // Format the message using the pattern
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);

    // Convert to QString (spdlog uses UTF-8)
    QString message = QString::fromUtf8(formatted.data(), static_cast<int>(formatted.size()));

    // Remove trailing newline if present
    if (message.endsWith('\n')) {
        message.chop(1);
    }

    // Emit signal with level and message
    // Note: This is called from logging thread, receiver should use QueuedConnection
    emit logMessage(static_cast<int>(msg.level), message);
}

void LogPanelSink::flush_() {
    // No buffering, nothing to flush
}

} // namespace core
} // namespace kalahari
