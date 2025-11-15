/// @file log_panel.h
/// @brief Diagnostic Log Panel (wxAUI panel, diagnostic mode only)
///
/// In-app log viewer with wxTextCtrl display area and vertical toolbar.
/// Only visible when diagnostic mode is enabled.
/// Supports real-time log display with configurable ring buffer.

#pragma once

#include <wx/wx.h>
#include <wx/toolbar.h>
#include <deque>
#include <string>

namespace kalahari {
namespace gui {

/// @brief Log panel for in-app diagnostic log viewing
///
/// Displays application logs in a read-only wxTextCtrl with vertical toolbar.
/// Features:
/// - Ring buffer (configurable 1-1000 lines, default 500)
/// - Configurable colors (background, text)
/// - Configurable monospace font
/// - Toolbar: Options, Open Log Folder, Copy to Clipboard
/// - Thread-safe log appending via spdlog custom sink
///
/// Only shown when diagnostic mode is enabled.
class LogPanel : public wxPanel {
public:
    /// @brief Constructor
    /// @param parent Parent window (usually MainWindow)
    explicit LogPanel(wxWindow* parent);

    /// @brief Destructor
    virtual ~LogPanel() = default;

    /// @brief Append a log message to the display
    ///
    /// Thread-safe method called by spdlog custom sink.
    /// Manages ring buffer (removes oldest line if buffer full).
    ///
    /// @param message Log message to append
    void appendLog(const std::string& message);

    /// @brief Get current ring buffer size
    /// @return Number of log lines in buffer
    size_t getBufferSize() const { return m_logBuffer.size(); }

    /// @brief Get maximum ring buffer capacity
    /// @return Maximum number of lines (default 500)
    size_t getMaxBufferSize() const { return m_maxBufferSize; }

    /// @brief Set maximum ring buffer capacity
    /// @param size Maximum lines (clamped to 1-1000)
    void setMaxBufferSize(size_t size);

    /// @brief Clear all log messages
    void clearLog();

    /// @brief Apply settings from SettingsManager
    ///
    /// Called when settings are changed in Settings Dialog.
    /// Updates colors, font, ring buffer size.
    void applySettings();

private:
    // ========================================================================
    // UI Components
    // ========================================================================

    /// @brief Log display area (read-only, multiline)
    wxTextCtrl* m_logDisplay = nullptr;

    /// @brief Vertical toolbar (Options, Open Folder, Copy)
    wxToolBar* m_toolBar = nullptr;

    // ========================================================================
    // Ring Buffer
    // ========================================================================

    /// @brief Ring buffer storing recent log messages
    std::deque<wxString> m_logBuffer;

    /// @brief Maximum ring buffer size (default 500)
    size_t m_maxBufferSize = 500;

    // ========================================================================
    // Event Handlers
    // ========================================================================

    /// @brief Handle Options toolbar button
    ///
    /// Opens Settings Dialog and selects "Log" tree item.
    ///
    /// @param event Command event from toolbar
    void onOptions(wxCommandEvent& event);

    /// @brief Handle Open Log Folder toolbar button
    ///
    /// Opens the log directory in system file explorer.
    ///
    /// @param event Command event from toolbar
    void onOpenLogFolder(wxCommandEvent& event);

    /// @brief Handle Copy to Clipboard toolbar button
    ///
    /// Copies entire log buffer to system clipboard.
    ///
    /// @param event Command event from toolbar
    void onCopyToClipboard(wxCommandEvent& event);

    /// @brief Handle Clear Log toolbar button
    ///
    /// Clears entire log buffer and display.
    ///
    /// @param event Command event from toolbar
    void onClearLog(wxCommandEvent& event);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /// @brief Setup panel layout
    ///
    /// Creates horizontal sizer with wxTextCtrl (left, proportion=1)
    /// and wxToolBar (right, vertical orientation).
    void setupLayout();

    /// @brief Rebuild log display from buffer
    ///
    /// Clears wxTextCtrl and re-appends all buffered messages.
    /// Called after settings changes (color/font).
    void rebuildDisplay();

    wxDECLARE_EVENT_TABLE();
};

} // namespace gui
} // namespace kalahari
