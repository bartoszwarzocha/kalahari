/// @file log_panel.h
/// @brief Enhanced Log Panel with colored output and mode-based visibility
///
/// LogPanel displays real-time application logs with:
/// - Colored output per log level (theme-aware)
/// - Ring buffer for memory management
/// - Mode-based visibility (normal vs --diag/--dev)
/// - Vertical toolbar (Options, Open Folder, Copy, Clear)
///
/// Normal mode: Panel hidden by default, shows only INFO+
/// Diagnostic mode (--diag/--dev): Panel shown, all log levels

#pragma once

#include <QWidget>
#include <QColor>
#include <deque>

class QTextEdit;
class QToolBar;

namespace kalahari {
namespace core {
class LogPanelSink;
}

namespace gui {

/// @brief Log entry for ring buffer
struct LogEntry {
    int level;          ///< spdlog level (0=trace, 1=debug, 2=info, 3=warn, 4=error, 5=critical)
    QString message;    ///< Formatted message with timestamp
};

/// @brief Enhanced diagnostic log panel
///
/// Features:
/// - Real-time log display via spdlog custom sink
/// - Colored output per log level (theme-aware)
/// - Ring buffer (configurable 1-1000 lines, default 500)
/// - Vertical toolbar: Options, Open Folder, Copy, Clear
/// - Mode-based filtering:
///   - Normal: INFO, WARN, ERROR, CRITICAL only
///   - Diagnostic: All levels including TRACE, DEBUG
class LogPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    /// @param diagnosticMode If true, show all log levels; if false, INFO+ only
    explicit LogPanel(QWidget* parent = nullptr, bool diagnosticMode = false);

    /// @brief Destructor
    ~LogPanel() override = default;

    /// @brief Clear log panel content
    void clear();

    /// @brief Get the log panel sink for registration with spdlog
    /// @return Shared pointer to LogPanelSink
    std::shared_ptr<core::LogPanelSink> getSink() const { return m_sink; }

    /// @brief Set diagnostic mode (affects log level filtering)
    /// @param enabled true = show all levels, false = INFO+ only
    void setDiagnosticMode(bool enabled);

    /// @brief Check if diagnostic mode is enabled
    bool isDiagnosticMode() const { return m_diagnosticMode; }

    /// @brief Get current ring buffer size
    size_t getBufferSize() const { return m_logBuffer.size(); }

    /// @brief Get maximum ring buffer capacity
    size_t getMaxBufferSize() const { return m_maxBufferSize; }

    /// @brief Set maximum ring buffer capacity
    /// @param size Maximum lines (clamped to 1-1000)
    void setMaxBufferSize(size_t size);

    /// @brief Apply current theme colors
    /// Call when theme changes to update log colors
    void applyThemeColors();

signals:
    /// @brief Emitted when user clicks Options button
    /// MainWindow should connect this to open Settings Dialog
    void openSettingsRequested();

public slots:
    /// @brief Append a log message (called by LogPanelSink)
    /// @param level spdlog level (0-5)
    /// @param message Formatted log message
    void appendLog(int level, const QString& message);

private slots:
    /// @brief Handle Options toolbar button
    void onOptions();

    /// @brief Handle Open Log Folder toolbar button
    void onOpenLogFolder();

    /// @brief Handle Copy to Clipboard toolbar button
    void onCopyToClipboard();

    /// @brief Handle Clear Log toolbar button
    void onClearLog();

private:
    /// @brief Setup panel layout (log display + toolbar)
    void setupLayout();

    /// @brief Create toolbar with action buttons
    void createToolbar();

    /// @brief Rebuild display from buffer (after theme/settings change)
    void rebuildDisplay();

    /// @brief Get color for log level based on current theme
    /// @param level spdlog level (0-5)
    /// @return Color for the level
    QColor getColorForLevel(int level) const;

    /// @brief Check if log level should be displayed
    /// @param level spdlog level (0-5)
    /// @return true if level passes current filter
    bool shouldDisplayLevel(int level) const;

    // ========================================================================
    // UI Components
    // ========================================================================

    QTextEdit* m_logEdit;           ///< Log display (rich text for colors)
    QToolBar* m_toolBar;            ///< Vertical toolbar

    // ========================================================================
    // Log Sink
    // ========================================================================

    std::shared_ptr<core::LogPanelSink> m_sink;  ///< spdlog custom sink

    // ========================================================================
    // Ring Buffer
    // ========================================================================

    std::deque<LogEntry> m_logBuffer;   ///< Ring buffer for log entries
    size_t m_maxBufferSize = 500;       ///< Max buffer size (default 500)

    // ========================================================================
    // Mode & Theme
    // ========================================================================

    bool m_diagnosticMode;              ///< Show all levels vs INFO+ only
    bool m_isDarkTheme;                 ///< Current theme (for colors)

    // ========================================================================
    // Cached Colors (Task #00027 - performance fix)
    // ========================================================================

    QColor m_colorTrace;                ///< Cached TRACE color
    QColor m_colorDebug;                ///< Cached DEBUG color
    QColor m_colorInfo;                 ///< Cached INFO color
    QColor m_colorWarning;              ///< Cached WARNING color
    QColor m_colorError;                ///< Cached ERROR color
    QColor m_colorCritical;             ///< Cached CRITICAL color
    QColor m_colorBackground;           ///< Cached background color

    /// @brief Refresh cached colors from SettingsManager
    void refreshColorCache();

    // ========================================================================
    // Visibility Optimization
    // ========================================================================

    bool m_needsRebuild = false;        ///< Flag: UI needs sync when visible

protected:
    /// @brief Called when widget becomes visible - syncs UI if needed
    void showEvent(QShowEvent* event) override;
};

} // namespace gui
} // namespace kalahari
