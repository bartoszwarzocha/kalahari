/// @file log_panel.h
/// @brief Log panel placeholder (Qt6)
///
/// This file defines the LogPanel class, a placeholder for the
/// log output viewer. Will be enhanced in Phase 1 with colored output.

#pragma once

#include <QWidget>

class QPlainTextEdit;

namespace kalahari {
namespace gui {

/// @brief Log panel (placeholder)
///
/// Displays a QPlainTextEdit for log messages.
/// This is a placeholder - full implementation comes in Phase 1 (colored output, filtering).
class LogPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit LogPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~LogPanel() override = default;

private:
    QPlainTextEdit* m_logEdit;
};

} // namespace gui
} // namespace kalahari
