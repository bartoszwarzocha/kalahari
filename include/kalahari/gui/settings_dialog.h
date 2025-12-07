/// @file settings_dialog.h
/// @brief Settings dialog for Kalahari application
///
/// Architecture:
/// - SettingsDialog collects and validates settings (data collection)
/// - MainWindow applies settings with BusyIndicator (data application)
/// - SettingsData transfers data between dialog and main window
///
/// Flow:
/// 1. User edits settings in dialog
/// 2. On Apply/OK, dialog emits settingsApplyRequested(SettingsData)
/// 3. MainWindow receives signal, shows BusyIndicator, applies settings
/// 4. Dialog stays open (Apply) or closes (OK)

#pragma once

#include <QDialog>
#include <QMap>
#include "kalahari/gui/settings_data.h"

class QTreeWidget;
class QTreeWidgetItem;
class QStackedWidget;
class QScrollArea;
class QDialogButtonBox;
class QComboBox;
class QSpinBox;
class QFontComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QHBoxLayout;

namespace kalahari {
namespace gui {
class ColorConfigWidget;
}
}

namespace kalahari {
namespace gui {

/// @brief Settings dialog with hierarchical tree navigation
///
/// Modal dialog for configuring Kalahari application settings.
/// Uses QTreeWidget for category navigation and QStackedWidget
/// for displaying settings panels.
///
/// This dialog only collects data - it does NOT apply settings directly.
/// Instead, it emits settingsApplyRequested signal with SettingsData.
/// MainWindow is responsible for applying settings with proper UI feedback.
///
/// Example usage:
/// @code
/// SettingsDialog dialog(this);
/// connect(&dialog, &SettingsDialog::settingsApplyRequested,
///         this, &MainWindow::onApplySettings);
/// dialog.exec();
/// @endcode
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget (usually MainWindow)
    /// @param currentSettings Current application settings
    explicit SettingsDialog(QWidget* parent, const SettingsData& currentSettings);

    /// @brief Destructor
    ~SettingsDialog() override = default;

    /// @brief Collect current settings from UI controls
    /// @return SettingsData structure with all current values
    SettingsData collectSettings() const;

signals:
    /// @brief Emitted AFTER settings have been applied successfully
    /// @param settings Applied settings data
    /// @note MainWindow can react to this (e.g., update diagnostic mode)
    void settingsApplied(const SettingsData& settings);

private slots:
    /// @brief Tree item selection changed
    void onTreeItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

    /// @brief Apply button clicked - emit signal, keep dialog open
    void onApply();

    /// @brief OK button clicked - emit signal, close dialog
    void onAccept();

    /// @brief Cancel button clicked - close without saving
    void onReject();

    /// @brief Diagnostic mode checkbox toggled
    void onDiagModeCheckboxToggled(bool checked);

    /// @brief Theme combo box changed - update color widgets
    void onThemeComboChanged(int index);

    /// @brief Icon theme combo box changed - update preview
    void onIconThemeComboChanged(int index);

private:
    // ========================================================================
    // UI Creation
    // ========================================================================

    void createUI();
    void createNavigationTree();
    void createSettingsPages();
    QWidget* createAppearanceGeneralPage();
    QWidget* createAppearanceThemePage();
    QWidget* createAppearanceIconsPage();
    QWidget* createEditorGeneralPage();
    QWidget* createAdvancedGeneralPage();
    QWidget* createAdvancedLogPage();
    QWidget* createPlaceholderPage(const QString& title, const QString& description);

    // ========================================================================
    // Settings Management
    // ========================================================================

    /// @brief Populate UI controls from settings data
    void populateFromSettings(const SettingsData& settings);

    /// @brief Update icon preview with current theme and colors
    void updateIconPreview();

    /// @brief Apply settings with BusyIndicator overlay on this dialog
    /// @param settings Settings to apply
    /// @note Shows spinner, applies all settings, emits settingsApplied signal
    void applySettingsWithSpinner(const SettingsData& settings);

    // ========================================================================
    // Member Variables - Navigation
    // ========================================================================

    QTreeWidget* m_navTree;
    QStackedWidget* m_pageStack;
    QDialogButtonBox* m_buttonBox;
    QMap<QTreeWidgetItem*, int> m_itemToPage;

    // ========================================================================
    // Member Variables - Appearance/General
    // ========================================================================

    QComboBox* m_languageComboBox;
    QSpinBox* m_uiFontSizeSpinBox;

    // ========================================================================
    // Member Variables - Appearance/Theme
    // ========================================================================

    QComboBox* m_themeComboBox;
    ColorConfigWidget* m_primaryColorWidget;
    ColorConfigWidget* m_secondaryColorWidget;
    ColorConfigWidget* m_logTraceColorWidget;
    ColorConfigWidget* m_logDebugColorWidget;
    ColorConfigWidget* m_logInfoColorWidget;
    ColorConfigWidget* m_logWarningColorWidget;
    ColorConfigWidget* m_logErrorColorWidget;
    ColorConfigWidget* m_logCriticalColorWidget;
    ColorConfigWidget* m_logBackgroundColorWidget;
    QLabel* m_themePreviewLabel;

    // ========================================================================
    // Member Variables - Appearance/Icons
    // ========================================================================

    QComboBox* m_iconThemeComboBox;
    QSpinBox* m_toolbarIconSizeSpinBox;
    QSpinBox* m_menuIconSizeSpinBox;
    QSpinBox* m_treeViewIconSizeSpinBox;
    QSpinBox* m_tabBarIconSizeSpinBox;
    QSpinBox* m_statusBarIconSizeSpinBox;
    QSpinBox* m_buttonIconSizeSpinBox;
    QSpinBox* m_comboBoxIconSizeSpinBox;
    QLabel* m_iconPreviewLabel;
    QHBoxLayout* m_iconPreviewLayout;

    // ========================================================================
    // Member Variables - Editor/General
    // ========================================================================

    QFontComboBox* m_fontFamilyComboBox;
    QSpinBox* m_editorFontSizeSpinBox;
    QSpinBox* m_tabSizeSpinBox;
    QCheckBox* m_lineNumbersCheckBox;
    QCheckBox* m_wordWrapCheckBox;

    // ========================================================================
    // Member Variables - Advanced/General
    // ========================================================================

    QCheckBox* m_diagModeCheckbox;

    // ========================================================================
    // Member Variables - Advanced/Log
    // ========================================================================

    QSpinBox* m_logBufferSizeSpinBox;

    // ========================================================================
    // Original settings (for comparison)
    // ========================================================================

    SettingsData m_originalSettings;

    // ========================================================================
    // Page Indices (for QStackedWidget)
    // ========================================================================

    enum PageIndex {
        PAGE_APPEARANCE_GENERAL = 0,
        PAGE_APPEARANCE_THEME = 1,
        PAGE_APPEARANCE_ICONS = 2,
        PAGE_EDITOR_GENERAL = 3,
        PAGE_EDITOR_SPELLING = 4,
        PAGE_EDITOR_AUTOCORRECT = 5,
        PAGE_EDITOR_COMPLETION = 6,
        PAGE_FILES_BACKUP = 7,
        PAGE_FILES_AUTOSAVE = 8,
        PAGE_FILES_IMPORT_EXPORT = 9,
        PAGE_NETWORK_CLOUD_SYNC = 10,
        PAGE_NETWORK_UPDATES = 11,
        PAGE_ADVANCED_GENERAL = 12,
        PAGE_ADVANCED_PERFORMANCE = 13,
        PAGE_ADVANCED_LOG = 14
    };
};

} // namespace gui
} // namespace kalahari
