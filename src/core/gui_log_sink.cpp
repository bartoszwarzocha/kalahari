/// @file gui_log_sink.cpp
/// @brief Implementation of GuiLogSink template

#include <kalahari/core/gui_log_sink.h>
#include <kalahari/gui/panels/log_panel.h>
#include <wx/app.h>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/pattern_formatter.h>

namespace kalahari {
namespace core {

// ============================================================================
// Template Implementation
// ============================================================================

template<typename Mutex>
GuiLogSinkImpl<Mutex>::GuiLogSinkImpl(gui::LogPanel* panel)
    : m_panel(panel)
{
}

template<typename Mutex>
void GuiLogSinkImpl<Mutex>::sink_it_(const spdlog::details::log_msg& msg) {
    if (!m_panel) {
        // No panel registered - skip
        return;
    }

    // Format the message using the sink's formatter
    spdlog::memory_buf_t formatted;
    this->formatter_->format(msg, formatted);

    // Convert to std::string
    std::string log_message = fmt::to_string(formatted);

    // Marshal to main GUI thread using wxTheApp->CallAfter()
    // This is thread-safe - wxWidgets queues the lambda for execution on main thread
    if (wxTheApp) {
        // Capture raw pointer (safe: CallAfter won't execute if app is shutting down)
        gui::LogPanel* panel = m_panel;

        wxTheApp->CallAfter([panel, log_message]() {
            // On main thread: check if panel is still valid
            // wxWidgets handles deleted windows gracefully
            if (panel && !panel->IsBeingDeleted()) {
                panel->appendLog(log_message);
            }
        });
    }
}

template<typename Mutex>
void GuiLogSinkImpl<Mutex>::flush_() {
    // No buffering in GUI sink - each message is appended immediately
    // Nothing to flush
}

// ============================================================================
// Explicit Template Instantiations
// ============================================================================

template class GuiLogSinkImpl<std::mutex>;
template class GuiLogSinkImpl<spdlog::details::null_mutex>;

} // namespace core
} // namespace kalahari
