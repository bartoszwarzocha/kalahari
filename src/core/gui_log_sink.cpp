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
    // Format the message using the sink's formatter
    spdlog::memory_buf_t formatted;
    this->formatter_->format(msg, formatted);

    // Convert to std::string
    std::string log_message = fmt::to_string(formatted);

    if (!m_panel) {
        // Buffered mode: Store in history for later backfill
        m_messageHistory.push_back(log_message);

        // Trim buffer if it exceeds max size (keep most recent messages)
        if (m_messageHistory.size() > m_maxHistorySize) {
            m_messageHistory.pop_front();
        }
        return;
    }

    // Active mode: Forward to LogPanel immediately
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

template<typename Mutex>
void GuiLogSinkImpl<Mutex>::setPanel(gui::LogPanel* panel) {
    std::lock_guard<Mutex> lock(this->mutex_);

    if (!panel) {
        return;  // Ignore null panel
    }

    // Backfill panel with limited history (respect panel's ring buffer size)
    if (!m_messageHistory.empty() && wxTheApp) {
        // Get panel's max buffer size to avoid overwhelming it
        size_t panelMaxSize = panel->getMaxBufferSize();
        size_t backfillCount = std::min(m_messageHistory.size(), panelMaxSize);

        // Copy last N messages to temporary buffer
        std::deque<std::string> history;
        auto start_it = m_messageHistory.end() - static_cast<std::deque<std::string>::difference_type>(backfillCount);
        history.assign(start_it, m_messageHistory.end());

        // Backfill on main thread
        wxTheApp->CallAfter([panel, history]() {
            if (panel && !panel->IsBeingDeleted()) {
                for (const auto& msg : history) {
                    panel->appendLog(msg);
                }
            }
        });

        // Clear history buffer after backfill
        m_messageHistory.clear();
    }

    // Switch to active mode
    m_panel = panel;
}

template<typename Mutex>
void GuiLogSinkImpl<Mutex>::clearPanel() {
    std::lock_guard<Mutex> lock(this->mutex_);

    // Return to buffered mode
    m_panel = nullptr;
}

// ============================================================================
// Explicit Template Instantiations
// ============================================================================

template class GuiLogSinkImpl<std::mutex>;
template class GuiLogSinkImpl<spdlog::details::null_mutex>;

} // namespace core
} // namespace kalahari
