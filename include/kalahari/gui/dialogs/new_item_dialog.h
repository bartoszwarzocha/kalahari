/// @file new_item_dialog.h
/// @brief Dual-purpose dialog for creating new projects and files
///
/// NewItemDialog provides a unified interface for creating both new projects
/// (.klh files) and new files within a project. The dialog adapts its UI based
/// on the mode selected at construction time.
///
/// OpenSpec #00033: Project File System - Phase C

#pragma once

#include "kalahari/gui/dialogs/item_templates.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>

namespace kalahari {
namespace gui {
namespace dialogs {

// ============================================================================
// NewItemMode - Dialog mode enumeration
// ============================================================================

/// @brief Dialog mode - Project or File creation
enum class NewItemMode {
    Project,    ///< Creating a new project (.klh)
    File        ///< Creating a new file within a project
};

// ============================================================================
// NewItemResult - Result data structure
// ============================================================================

/// @brief Result data from NewItemDialog
///
/// Contains all information needed to create the selected item type.
/// For File mode, author/language/location fields are empty.
struct NewItemResult {
    NewItemMode mode;        ///< Dialog mode (Project or File)
    QString templateId;      ///< Selected template ID (e.g., "template.novel")
    QString title;           ///< Project title or file name
    QString author;          ///< Author (project mode only)
    QString language;        ///< Language code (project mode only, e.g., "en")
    QString location;        ///< Project folder path (project mode only)
    bool createSubfolder;    ///< Create subfolder with project name (project mode only)
};

// ============================================================================
// NewItemDialog - Main dialog class
// ============================================================================

/// @brief Dual-purpose dialog for creating projects and files
///
/// NewItemDialog adapts its interface based on the mode:
/// - Project mode: Shows author, language, location fields
/// - File mode: Shows only name field
///
/// Layout structure:
/// - LEFT panel (250px): Description area with icon and text
/// - RIGHT panel (flexible): Template grid with icons
/// - BOTTOM: Details group with input fields
///
/// Usage:
/// @code
/// NewItemDialog dialog(NewItemMode::Project, this);
/// if (dialog.exec() == QDialog::Accepted) {
///     NewItemResult result = dialog.result();
///     // Create project using result.templateId, result.title, etc.
/// }
/// @endcode
///
/// @see TemplateRegistry
/// @see NewItemResult
class NewItemDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructs the NewItemDialog
    /// @param mode Dialog mode (Project or File creation)
    /// @param parent Parent widget (typically MainWindow)
    explicit NewItemDialog(NewItemMode mode, QWidget* parent = nullptr);

    /// @brief Destructor
    ~NewItemDialog() override = default;

    /// @brief Get the dialog result data
    /// @return Populated NewItemResult structure
    /// @note Only valid after dialog is accepted
    NewItemResult result() const;

private slots:
    /// @brief Handle template selection change
    /// @param current Currently selected item
    /// @param previous Previously selected item
    void onTemplateSelected(QListWidgetItem* current, QListWidgetItem* previous);

    /// @brief Handle browse button click for location
    void onBrowseLocation();

    /// @brief Handle title/name text change
    /// @param text New text value
    void onTitleChanged(const QString& text);

    /// @brief Handle Create button click
    void onAccept();

    /// @brief Refresh icons when theme changes
    void onThemeChanged();

private:
    // ========================================================================
    // UI Setup
    // ========================================================================

    /// @brief Create and configure all UI elements
    void setupUI();

    /// @brief Create the description panel (LEFT)
    /// @return Widget containing description area
    QWidget* createDescriptionPanel();

    /// @brief Create the template grid (RIGHT)
    /// @return Widget containing template list
    QWidget* createTemplateGrid();

    /// @brief Create the details input group (BOTTOM)
    /// @return Widget containing input fields
    QWidget* createDetailsGroup();

    /// @brief Create connections between signals and slots
    void createConnections();

    /// @brief Populate template list from TemplateRegistry
    void populateTemplates();

    /// @brief Update description panel for selected template
    /// @param templateId Template ID to display
    void updateDescription(const QString& templateId);

    /// @brief Validate input and update Create button state
    void validateInput();

    /// @brief Load default values from SettingsManager
    void loadDefaults();

    // ========================================================================
    // State
    // ========================================================================

    /// @brief Dialog mode (Project or File)
    NewItemMode m_mode;

    /// @brief Result data (populated on accept)
    NewItemResult m_result;

    // ========================================================================
    // Description Panel Widgets (LEFT)
    // ========================================================================

    /// @brief Large template icon (64x64)
    QLabel* m_iconLabel;

    /// @brief Template name (bold)
    QLabel* m_titleLabel;

    /// @brief Rich text description with features
    QLabel* m_descriptionLabel;

    // ========================================================================
    // Template Grid Widgets (RIGHT)
    // ========================================================================

    /// @brief Template list with icon mode
    QListWidget* m_templateList;

    /// @brief Optional search filter (future use)
    QLineEdit* m_searchEdit;

    // ========================================================================
    // Details Group Widgets (BOTTOM)
    // ========================================================================

    /// @brief Project title / file name input
    QLineEdit* m_nameEdit;

    /// @brief Author input (project mode only)
    QLineEdit* m_authorEdit;

    /// @brief Language selection (project mode only)
    QComboBox* m_languageCombo;

    /// @brief Project location input (project mode only)
    QLineEdit* m_locationEdit;

    /// @brief Browse folder button (project mode only)
    QPushButton* m_browseBtn;

    /// @brief Create subfolder checkbox (project mode only)
    QCheckBox* m_subfolderCheck;

    /// @brief Label for author row
    QLabel* m_authorLabel;

    /// @brief Label for language row
    QLabel* m_languageLabel;

    /// @brief Label for location row
    QLabel* m_locationLabel;

    // ========================================================================
    // Dialog Buttons
    // ========================================================================

    /// @brief Standard dialog buttons (Create, Cancel)
    QDialogButtonBox* m_buttonBox;

    /// @brief Create button reference (for enable/disable)
    QPushButton* m_createBtn;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
