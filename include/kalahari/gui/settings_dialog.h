/// @file settings_dialog.h
/// @brief Settings dialog for Kalahari application
///
/// Task #00024: Refactored to use QTreeWidget + QStackedWidget
/// for hierarchical navigation with scrollable panels.
///
/// Structure:
/// - Appearance: General, Theme, Icons
/// - Editor: General (future: Spelling, Auto-correct)
/// - Files: (future: Backup, Auto-save)
/// - Network: (future: Cloud Sync, Updates)
/// - Advanced: General (diagnostic mode, log config)

#pragma once

#include <QDialog>
#include <QMap>

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

/// @brief Settings dialog with hierarchical tree navigation
///
/// Modal dialog for configuring Kalahari application settings.
/// Uses QTreeWidget for category navigation and QStackedWidget
/// for displaying settings panels.
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
    /// @param diagnosticModeEnabled Current diagnostic mode state
    explicit SettingsDialog(QWidget* parent = nullptr, bool diagnosticModeEnabled = false);

    /// @brief Destructor
    ~SettingsDialog() override = default;

signals:
    /// @brief Emitted when diagnostic mode checkbox is toggled
    void diagnosticModeChanged(bool enabled);

    /// @brief Emitted when theme is changed
    void themeChanged(const QString& themeName);

    /// @brief Emitted when icon theme is changed
    void iconThemeChanged(const QString& iconTheme);

private slots:
    /// @brief Tree item selection changed
    void onTreeItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

    /// @brief Apply button clicked
    void onApply();

    /// @brief OK button clicked
    void onAccept();

    /// @brief Cancel button clicked
    void onReject();

    /// @brief Diagnostic mode checkbox toggled
    void onDiagModeCheckboxToggled(bool checked);

    /// @brief Primary icon color button clicked
    void onPrimaryColorButtonClicked();

    /// @brief Secondary icon color button clicked
    void onSecondaryColorButtonClicked();

    /// @brief Theme combo box changed
    void onThemeComboChanged(int index);

    /// @brief Icon theme combo box changed
    void onIconThemeComboChanged(int index);

private:
    // ========================================================================
    // UI Creation
    // ========================================================================

    /// @brief Create main dialog layout
    void createUI();

    /// @brief Create navigation tree (left panel)
    void createNavigationTree();

    /// @brief Create all settings pages (right panel)
    void createSettingsPages();

    /// @brief Create Appearance/General page
    QWidget* createAppearanceGeneralPage();

    /// @brief Create Appearance/Theme page
    QWidget* createAppearanceThemePage();

    /// @brief Create Appearance/Icons page
    QWidget* createAppearanceIconsPage();

    /// @brief Create Editor/General page
    QWidget* createEditorGeneralPage();

    /// @brief Create Advanced/General page
    QWidget* createAdvancedGeneralPage();

    /// @brief Create placeholder page for future features
    QWidget* createPlaceholderPage(const QString& title, const QString& description);

    // ========================================================================
    // Settings Management
    // ========================================================================

    /// @brief Load settings from SettingsManager
    void loadSettings();

    /// @brief Save settings to SettingsManager
    void saveSettings();

    /// @brief Update color button appearance
    void updateColorButton(QPushButton* button, const QColor& color);

    /// @brief Update icon preview with current theme
    void updateIconPreview();

    // ========================================================================
    // Member Variables - Navigation
    // ========================================================================

    QTreeWidget* m_navTree;              ///< Navigation tree (left panel)
    QStackedWidget* m_pageStack;         ///< Settings pages (right panel)
    QDialogButtonBox* m_buttonBox;       ///< OK/Cancel/Apply buttons

    /// @brief Map tree items to page indices
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
    QPushButton* m_primaryColorButton;
    QPushButton* m_secondaryColorButton;
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
    bool m_initialDiagMode;

    // ========================================================================
    // Page Indices (for QStackedWidget)
    // ========================================================================

    enum PageIndex {
        // Appearance (0-2)
        PAGE_APPEARANCE_GENERAL = 0,
        PAGE_APPEARANCE_THEME = 1,
        PAGE_APPEARANCE_ICONS = 2,
        // Editor (3-6)
        PAGE_EDITOR_GENERAL = 3,
        PAGE_EDITOR_SPELLING = 4,
        PAGE_EDITOR_AUTOCORRECT = 5,
        PAGE_EDITOR_COMPLETION = 6,
        // Files (7-9)
        PAGE_FILES_BACKUP = 7,
        PAGE_FILES_AUTOSAVE = 8,
        PAGE_FILES_IMPORT_EXPORT = 9,
        // Network (10-11)
        PAGE_NETWORK_CLOUD_SYNC = 10,
        PAGE_NETWORK_UPDATES = 11,
        // Advanced (12-13)
        PAGE_ADVANCED_GENERAL = 12,
        PAGE_ADVANCED_PERFORMANCE = 13
    };
};

} // namespace gui
} // namespace kalahari
