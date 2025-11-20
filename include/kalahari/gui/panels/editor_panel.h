/// @file editor_panel.h
/// @brief Editor panel with settings integration (Task #00007)
///
/// This file defines the EditorPanel class - a text editor widget
/// with settings integration (font, tabs, word wrap).
/// Full syntax highlighting and line numbers come in Phase 1.

#pragma once

#include <QWidget>
#include <QString>

class QPlainTextEdit;

namespace kalahari {
namespace gui {

/// @brief Editor panel with settings integration
///
/// Displays QPlainTextEdit for text editing with:
/// - Font settings (family, size)
/// - Tab size configuration
/// - Word wrap mode
/// - Public setText/getText API for Document integration (Task #00008)
///
/// Phase 1 will add: syntax highlighting, line numbers, custom features
class EditorPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit EditorPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~EditorPanel() override = default;

    /// @brief Set editor text
    /// @param text Text to display in editor
    ///
    /// Used by Document load operations (Task #00008)
    void setText(const QString& text);

    /// @brief Get editor text
    /// @return Current editor content
    ///
    /// Used by Document save operations (Task #00008)
    QString getText() const;

private:
    /// @brief Apply settings from SettingsManager
    ///
    /// Reads and applies:
    /// - editor.fontFamily (default: "Consolas")
    /// - editor.fontSize (default: 12)
    /// - editor.tabSize (default: 4)
    /// - editor.wordWrap (default: false)
    /// - editor.lineNumbers (default: true) - logged only, display not implemented
    ///
    /// Called on construction and when settings change.
    void applySettings();

    QPlainTextEdit* m_textEdit;
};

} // namespace gui
} // namespace kalahari
