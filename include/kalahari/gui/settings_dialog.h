/// @file settings_dialog.h
/// @brief Settings dialog for Kalahari application
///
/// Provides a tabbed dialog for configuring application settings.
/// Structure:
/// - Appearance tab: Theme, font, language settings (Task #00005)
/// - Editor tab: Font, line numbers, word wrap, etc. (Task #00006)

#pragma once

#include <QDialog>

class QTabWidget;
class QDialogButtonBox;
class QComboBox;
class QSpinBox;
class QFontComboBox;
class QCheckBox;
class QPushButton;

namespace kalahari {
namespace gui {

/// @brief Settings dialog with tabbed panels
///
/// Modal dialog for configuring Kalahari application settings.
/// Settings are loaded from SettingsManager on open and saved
/// when OK/Apply is clicked.
///
/// Example usage:
/// @code
/// SettingsDialog dialog(this);
/// if (dialog.exec() == QDialog::Accepted) {
///     // Settings were saved
/// }
/// @endcode
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget (usually MainWindow)
    /// @param diagnosticModeEnabled Current diagnostic mode state from MainWindow (Task #00018)
    explicit SettingsDialog(QWidget* parent = nullptr, bool diagnosticModeEnabled = false);

    /// @brief Destructor
    ~SettingsDialog() override = default;

signals:
    /// @brief Emitted when diagnostic mode checkbox is toggled
    /// @param enabled true if diagnostic mode enabled, false otherwise
    void diagnosticModeChanged(bool enabled);

private slots:
    /// @brief Apply button clicked - save settings without closing
    void onApply();

    /// @brief OK button clicked - save settings and close
    void onAccept();

    /// @brief Cancel button clicked - discard changes and close
    void onReject();

    /// @brief Diagnostic mode checkbox toggled
    /// @param checked true if checked, false otherwise
    void onDiagModeCheckboxToggled(bool checked);

    /// @brief Primary icon color button clicked (Task #00020)
    void onPrimaryColorButtonClicked();

    /// @brief Secondary icon color button clicked (Task #00020)
    void onSecondaryColorButtonClicked();

private:
    /// @brief Create dialog UI
    ///
    /// Creates QTabWidget with placeholder tabs and QDialogButtonBox.
    void createUI();

    /// @brief Load settings from SettingsManager
    ///
    /// Called when dialog opens. Placeholder for Tasks #00005-00006.
    void loadSettings();

    /// @brief Save settings to SettingsManager
    ///
    /// Called when OK or Apply is clicked. Placeholder for Tasks #00005-00006.
    void saveSettings();

    // Widgets
    QTabWidget* m_tabWidget;
    QDialogButtonBox* m_buttonBox;

    // Placeholder tabs (will be replaced with actual panels in Tasks #00005, #00006)
    QWidget* m_appearanceTab;
    QWidget* m_editorTab;
    QWidget* m_advancedTab;

    // Appearance tab controls (Task #00005)
    QComboBox* m_themeComboBox;
    QComboBox* m_languageComboBox;
    QSpinBox* m_fontSizeSpinBox;
    QPushButton* m_primaryColorButton;    ///< Primary icon color picker button (Task #00020)
    QPushButton* m_secondaryColorButton;  ///< Secondary icon color picker button (Task #00020)

    // Editor tab controls (Task #00006)
    QFontComboBox* m_fontFamilyComboBox;
    QSpinBox* m_editorFontSizeSpinBox;
    QSpinBox* m_tabSizeSpinBox;
    QCheckBox* m_lineNumbersCheckBox;
    QCheckBox* m_wordWrapCheckBox;

    // Advanced tab controls (Task #00018)
    QCheckBox* m_diagModeCheckbox;
    bool m_initialDiagMode;  ///< Initial diagnostic mode state (Task #00018)
};

} // namespace gui
} // namespace kalahari
