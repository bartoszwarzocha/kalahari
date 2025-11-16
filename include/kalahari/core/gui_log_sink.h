/// @file gui_log_sink.h
/// @brief Custom spdlog sink for GUI log panel
///
/// Thread-safe spdlog sink that forwards log messages to LogPanel.
/// Uses wxTheApp->CallAfter() to marshal GUI calls to main thread.

#pragma once

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>
#include <memory>
#include <mutex>
#include <deque>
#include <string>

namespace kalahari {
namespace gui {
    class LogPanel;  // Forward declaration
}

namespace core {

/// @brief Thread-safe spdlog sink for GUI LogPanel
///
/// Forwards formatted log messages to LogPanel::appendLog() on main thread.
/// Stores raw pointer to LogPanel (owned by wxWidgets parent).
///
/// **Buffered Mode (panel == nullptr):**
/// - Messages are stored in internal buffer (m_messageHistory)
/// - Max buffer size: 1000 messages (configurable)
/// - When panel is attached via setPanel(), history is backfilled
///
/// **Active Mode (panel != nullptr):**
/// - Messages are forwarded to LogPanel::appendLog() immediately
/// - No buffering occurs
///
/// Thread Safety:
/// - sink_it_() can be called from any thread (protected by base_sink mutex)
/// - GUI operations marshalled to main thread via wxTheApp->CallAfter()
/// - Raw pointer is safe: wxTheApp->CallAfter() won't execute if app is shutting down
///
/// Lifetime Safety:
/// - LogPanel is owned by wxAUI/MainWindow
/// - Sink is destroyed when logger is destroyed (app shutdown)
/// - If LogPanel is destroyed first, CallAfter() lambda will safely do nothing
///
/// Usage:
/// @code
/// // Early startup (before LogPanel exists)
/// auto gui_sink = std::make_shared<GuiLogSink>(nullptr);
/// logger->sinks().push_back(gui_sink);
///
/// // Later (when LogPanel is created)
/// gui_sink->setPanel(logPanel);  // Backfills history automatically
/// @endcode
template<typename Mutex>
class GuiLogSinkImpl : public spdlog::sinks::base_sink<Mutex> {
public:
    /// @brief Constructor
    /// @param panel Raw pointer to LogPanel (can be nullptr initially)
    explicit GuiLogSinkImpl(gui::LogPanel* panel = nullptr);

    /// @brief Destructor
    virtual ~GuiLogSinkImpl() = default;

    /// @brief Attach LogPanel and backfill with message history
    ///
    /// If panel was nullptr, this backfills the panel with buffered messages.
    /// Thread-safe: Protected by base_sink mutex.
    ///
    /// @param panel Raw pointer to LogPanel (owned by wxWidgets parent)
    void setPanel(gui::LogPanel* panel);

    /// @brief Detach LogPanel (return to buffered mode)
    ///
    /// Future messages will be buffered instead of forwarded.
    void clearPanel();

protected:
    /// @brief Handle incoming log message
    ///
    /// If panel is nullptr: Store in buffer (m_messageHistory)
    /// If panel is set: Forward to LogPanel::appendLog() on main thread
    /// Uses wxTheApp->CallAfter() for thread-safe GUI marshalling.
    ///
    /// @param msg Log message from spdlog
    void sink_it_(const spdlog::details::log_msg& msg) override;

    /// @brief Flush the sink (no-op for GUI sink)
    void flush_() override;

private:
    /// @brief Raw pointer to LogPanel (owned by wxWidgets, not ref-counted)
    gui::LogPanel* m_panel;

    /// @brief Message history buffer (used when panel is nullptr)
    std::deque<std::string> m_messageHistory;

    /// @brief Maximum history buffer size (default 1000 messages)
    size_t m_maxHistorySize = 1000;
};

/// @brief Thread-safe GUI log sink (mutex-protected)
using GuiLogSink = GuiLogSinkImpl<std::mutex>;

/// @brief Non-thread-safe GUI log sink (for single-threaded use)
using GuiLogSinkSt = GuiLogSinkImpl<spdlog::details::null_mutex>;

} // namespace core
} // namespace kalahari
