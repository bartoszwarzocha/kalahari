/// @file log_panel.cpp
/// @brief Enhanced Log Panel implementation

#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/core/log_panel_sink.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/art_provider.h"

#include <QTextEdit>
#include <QToolBar>
#include <QHBoxLayout>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QScrollBar>
#include <QShowEvent>
#include <QTime>
#include <algorithm>

namespace kalahari {
namespace gui {

// ============================================================================
// Constructor / Destructor
// ============================================================================

LogPanel::LogPanel(QWidget* parent, bool diagnosticMode)
    : QWidget(parent)
    , m_logEdit(nullptr)
    , m_toolBar(nullptr)
    , m_sink(std::make_shared<core::LogPanelSink>())
    , m_diagnosticMode(diagnosticMode)
    , m_isDarkTheme(false)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("LogPanel: Creating enhanced log panel (diagnosticMode={})", diagnosticMode);

    // Detect current theme
    const auto& theme = core::ThemeManager::getInstance().getCurrentTheme();
    m_isDarkTheme = (theme.name == "Dark");

    // Setup UI
    setupLayout();

    // Connect sink signal to appendLog slot (queued for thread safety)
    connect(m_sink.get(), &core::LogPanelSink::logMessage,
            this, &LogPanel::appendLog, Qt::QueuedConnection);

    // Connect to theme changes
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged,
            this, [this](const core::Theme& theme) {
                m_isDarkTheme = (theme.name == "Dark");
                applyThemeColors();
            });

    // Initial welcome message
    appendLog(2, tr("[%1] Log panel initialized").arg(QTime::currentTime().toString("HH:mm:ss.zzz")));
    if (diagnosticMode) {
        appendLog(1, tr("[%1] Diagnostic mode: showing all log levels").arg(QTime::currentTime().toString("HH:mm:ss.zzz")));
    } else {
        appendLog(2, tr("[%1] Normal mode: showing INFO and above").arg(QTime::currentTime().toString("HH:mm:ss.zzz")));
    }

    logger.debug("LogPanel: Initialized");
}

// ============================================================================
// Public Methods
// ============================================================================

void LogPanel::clear() {
    m_logBuffer.clear();
    if (m_logEdit) {
        m_logEdit->clear();
    }
    core::Logger::getInstance().debug("LogPanel: Cleared");
}

void LogPanel::setDiagnosticMode(bool enabled) {
    if (m_diagnosticMode != enabled) {
        m_diagnosticMode = enabled;
        rebuildDisplay();  // Refilter existing logs
        core::Logger::getInstance().info("LogPanel: Diagnostic mode {}", enabled ? "enabled" : "disabled");
    }
}

void LogPanel::setMaxBufferSize(size_t size) {
    m_maxBufferSize = std::clamp(size, size_t(1), size_t(1000));

    // Trim buffer if needed
    while (m_logBuffer.size() > m_maxBufferSize) {
        m_logBuffer.pop_front();
    }

    rebuildDisplay();
}

void LogPanel::applyThemeColors() {
    // Update background color from SettingsManager (user customizable, Task #00027)
    if (m_logEdit) {
        auto& settings = core::SettingsManager::getInstance();
        std::string themeName = m_isDarkTheme ? "Dark" : "Light";
        std::string defBackground = m_isDarkTheme ? "#252525" : "#F5F5F5";
        QString bgColor = QString::fromStdString(
            settings.getLogColorForTheme(themeName, "background", defBackground));
        m_logEdit->setStyleSheet(QString("QTextEdit { background-color: %1; }").arg(bgColor));
    }
    rebuildDisplay();
}

// ============================================================================
// Public Slots
// ============================================================================

void LogPanel::appendLog(int level, const QString& message) {
    // Store in buffer (always, regardless of filter or visibility)
    LogEntry entry{level, message};
    m_logBuffer.push_back(entry);

    // Trim buffer if exceeds max size
    while (m_logBuffer.size() > m_maxBufferSize) {
        m_logBuffer.pop_front();
    }

    // OPTIMIZATION: Skip UI update if panel is hidden
    // Set flag so showEvent() can rebuild when visible again
    if (!isVisible()) {
        m_needsRebuild = true;
        return;
    }

    // Check if this level should be displayed
    if (!shouldDisplayLevel(level)) {
        return;  // Filtered out
    }

    // Get color for this level
    QColor color = getColorForLevel(level);

    // Append to display with color
    if (m_logEdit) {
        // Build HTML for colored text
        QString html = QString("<span style=\"color: %1;\">%2</span><br>")
                           .arg(color.name())
                           .arg(message.toHtmlEscaped());
        m_logEdit->insertHtml(html);

        // Scroll to bottom
        m_logEdit->verticalScrollBar()->setValue(m_logEdit->verticalScrollBar()->maximum());
    }
}

// ============================================================================
// Private Slots
// ============================================================================

void LogPanel::onOptions() {
    core::Logger::getInstance().info("LogPanel: Options clicked - opening Settings Dialog");
    emit openSettingsRequested();
}

void LogPanel::onOpenLogFolder() {
    core::Logger::getInstance().info("LogPanel: Open Log Folder clicked");

    // Log file is in application's current directory (kalahari.log)
    QString logDir = QDir::currentPath();
    QString logFile = QDir(logDir).filePath("kalahari.log");

    // Check if log file exists
    if (!QFile::exists(logFile)) {
        core::Logger::getInstance().warn("LogPanel: Log file does not exist: {}", logFile.toStdString());
        QMessageBox::warning(this, tr("Open Log Folder"),
                             tr("Log file not found:\n%1").arg(logFile));
        return;
    }

    // Open folder containing log file in file explorer
    QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
    core::Logger::getInstance().info("LogPanel: Opened log folder: {}", logDir.toStdString());
}

void LogPanel::onCopyToClipboard() {
    // Build text from buffer - only visible lines
    QString logText;
    int visibleCount = 0;

    for (const auto& entry : m_logBuffer) {
        if (shouldDisplayLevel(entry.level)) {
            logText += entry.message + "\n";
            ++visibleCount;
        }
    }

    // Copy to clipboard
    QApplication::clipboard()->setText(logText);

    core::Logger::getInstance().info("LogPanel: Copied {} visible lines to clipboard", visibleCount);
    QMessageBox::information(this, tr("Copy to Clipboard"),
                             tr("Copied %1 log lines to clipboard.").arg(visibleCount));
}

void LogPanel::onClearLog() {
    clear();
    appendLog(2, tr("[%1] Log cleared by user").arg(QTime::currentTime().toString("HH:mm:ss.zzz")));
}

// ============================================================================
// Private Methods
// ============================================================================

void LogPanel::setupLayout() {
    // Main horizontal layout: [TextEdit (flex) | ToolBar (fixed)]
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Log display (QTextEdit for rich text colors)
    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setAcceptRichText(true);
    m_logEdit->setLineWrapMode(QTextEdit::WidgetWidth);

    // Set monospace font
    QFont monoFont("Consolas", 10);
    monoFont.setStyleHint(QFont::Monospace);
    m_logEdit->setFont(monoFont);

    // Apply initial theme colors
    QString bgColor = m_isDarkTheme ? "#1E1E1E" : "#FFFFFF";
    m_logEdit->setStyleSheet(QString("QTextEdit { background-color: %1; }").arg(bgColor));

    mainLayout->addWidget(m_logEdit, 1);  // Stretch factor 1

    // Create toolbar
    createToolbar();
    mainLayout->addWidget(m_toolBar, 0);  // Fixed width

    setLayout(mainLayout);
}

void LogPanel::createToolbar() {
    m_toolBar = new QToolBar(this);
    m_toolBar->setOrientation(Qt::Vertical);
    m_toolBar->setIconSize(QSize(20, 20));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    auto& artProvider = core::ArtProvider::getInstance();

    // Options button (uses log.options icon registered in MainWindow)
    QAction* optionsAction = artProvider.createAction("log.options", tr("Options"), m_toolBar);
    optionsAction->setToolTip(tr("Open Log Settings"));
    connect(optionsAction, &QAction::triggered, this, &LogPanel::onOptions);
    m_toolBar->addAction(optionsAction);

    // Open Folder button
    QAction* folderAction = artProvider.createAction("log.openFolder", tr("Open Folder"), m_toolBar);
    folderAction->setToolTip(tr("Open log directory"));
    connect(folderAction, &QAction::triggered, this, &LogPanel::onOpenLogFolder);
    m_toolBar->addAction(folderAction);

    // Copy button
    QAction* copyAction = artProvider.createAction("log.copy", tr("Copy"), m_toolBar);
    copyAction->setToolTip(tr("Copy log to clipboard"));
    connect(copyAction, &QAction::triggered, this, &LogPanel::onCopyToClipboard);
    m_toolBar->addAction(copyAction);

    // Clear button
    QAction* clearAction = artProvider.createAction("log.clear", tr("Clear"), m_toolBar);
    clearAction->setToolTip(tr("Clear log"));
    connect(clearAction, &QAction::triggered, this, &LogPanel::onClearLog);
    m_toolBar->addAction(clearAction);
}

void LogPanel::rebuildDisplay() {
    if (!m_logEdit) return;

    // Block signals during rebuild
    m_logEdit->blockSignals(true);

    // Build ALL HTML at once for performance (single DOM update)
    // This is CRITICAL - multiple insertHtml() calls are extremely slow
    QString fullHtml;
    fullHtml.reserve(m_logBuffer.size() * 100);  // Pre-allocate for performance

    for (const auto& entry : m_logBuffer) {
        if (shouldDisplayLevel(entry.level)) {
            QColor color = getColorForLevel(entry.level);
            fullHtml += QString("<span style=\"color: %1;\">%2</span><br>")
                            .arg(color.name())
                            .arg(entry.message.toHtmlEscaped());
        }
    }

    // Single setHtml() is MUCH faster than multiple insertHtml()
    m_logEdit->setHtml(fullHtml);

    m_logEdit->blockSignals(false);

    // Scroll to bottom
    m_logEdit->verticalScrollBar()->setValue(m_logEdit->verticalScrollBar()->maximum());
}

QColor LogPanel::getColorForLevel(int level) const {
    // spdlog levels: 0=trace, 1=debug, 2=info, 3=warn, 4=error, 5=critical
    //
    // Colors are loaded from SettingsManager (user customizable, Task #00027)
    // Falls back to theme defaults if not customized
    //
    // Color scheme:
    // - TRACE/DEBUG: Magenta (diagnostic mode only)
    // - INFO: Default text color
    // - WARN: Orange
    // - ERROR/CRITICAL: Red (same color)

    auto& settings = core::SettingsManager::getInstance();
    std::string themeName = m_isDarkTheme ? "Dark" : "Light";

    // Theme defaults
    std::string defTrace = m_isDarkTheme ? "#FF66FF" : "#CC00CC";
    std::string defDebug = m_isDarkTheme ? "#FF66FF" : "#CC00CC";
    std::string defInfo = m_isDarkTheme ? "#FFFFFF" : "#000000";
    std::string defWarning = m_isDarkTheme ? "#FFA500" : "#FF8C00";
    std::string defError = m_isDarkTheme ? "#FF4444" : "#CC0000";
    std::string defCritical = m_isDarkTheme ? "#FF4444" : "#CC0000";

    switch (level) {
        case 0: return QColor(QString::fromStdString(
                    settings.getLogColorForTheme(themeName, "trace", defTrace)));
        case 1: return QColor(QString::fromStdString(
                    settings.getLogColorForTheme(themeName, "debug", defDebug)));
        case 2: return QColor(QString::fromStdString(
                    settings.getLogColorForTheme(themeName, "info", defInfo)));
        case 3: return QColor(QString::fromStdString(
                    settings.getLogColorForTheme(themeName, "warning", defWarning)));
        case 4: return QColor(QString::fromStdString(
                    settings.getLogColorForTheme(themeName, "error", defError)));
        case 5: return QColor(QString::fromStdString(
                    settings.getLogColorForTheme(themeName, "critical", defCritical)));
        default: return QColor(QString::fromStdString(
                    settings.getLogColorForTheme(themeName, "info", defInfo)));
    }
}

bool LogPanel::shouldDisplayLevel(int level) const {
    // spdlog levels: 0=trace, 1=debug, 2=info, 3=warn, 4=error, 5=critical

    if (m_diagnosticMode) {
        // Diagnostic mode: show ALL levels (trace, debug, info, warn, error, critical)
        return true;
    } else {
        // Normal mode: show INFO (2), WARN (3), ERROR (4), CRITICAL (5)
        // Hide: TRACE (0), DEBUG (1)
        return level >= 2;
    }
}

// ============================================================================
// Protected Events
// ============================================================================

void LogPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);

    // Sync UI with buffer when becoming visible (if needed)
    if (m_needsRebuild) {
        m_needsRebuild = false;
        rebuildDisplay();
        core::Logger::getInstance().debug("LogPanel: Rebuilt display on show ({} entries)", m_logBuffer.size());
    }
}

} // namespace gui
} // namespace kalahari
