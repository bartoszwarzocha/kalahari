/// @file editor_panel.h
/// @brief Editor panel placeholder (Qt6)
///
/// This file defines the EditorPanel class, a placeholder for the
/// text editor widget. Will be enhanced in Phase 1 with real editing.

#pragma once

#include <QWidget>

class QPlainTextEdit;

namespace kalahari {
namespace gui {

/// @brief Editor panel (placeholder)
///
/// Displays a simple QPlainTextEdit for text editing.
/// This is a placeholder - full editor implementation comes in Phase 1.
class EditorPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit EditorPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~EditorPanel() override = default;

private:
    QPlainTextEdit* m_textEdit;
};

} // namespace gui
} // namespace kalahari
