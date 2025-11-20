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
    explicit SettingsDialog(QWidget* parent = nullptr);

    /// @brief Destructor
    ~SettingsDialog() override = default;

private slots:
    /// @brief Apply button clicked - save settings without closing
    void onApply();

    /// @brief OK button clicked - save settings and close
    void onAccept();

    /// @brief Cancel button clicked - discard changes and close
    void onReject();

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
};

} // namespace gui
} // namespace kalahari
