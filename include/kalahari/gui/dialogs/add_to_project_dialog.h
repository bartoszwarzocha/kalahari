/// @file add_to_project_dialog.h
/// @brief Dialog for adding standalone files to an open project
///
/// AddToProjectDialog allows users to add a standalone file (not part of the
/// project) to the current project structure. Users can choose the target
/// section (frontmatter, body, backmatter, mindmaps, timelines) and whether
/// to copy or move the file.
///
/// OpenSpec #00033: Project File System - Phase F

#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QLabel>
#include <QString>

namespace kalahari {
namespace gui {
namespace dialogs {

// ============================================================================
// AddToProjectResult - Result data structure
// ============================================================================

/// @brief Result data from AddToProjectDialog
///
/// Contains all information needed to add a file to the project structure.
struct AddToProjectResult {
    QString targetSection;   ///< "frontmatter", "body", "backmatter", "mindmaps", "timelines"
    QString targetPart;      ///< Part ID if body section selected, empty otherwise
    QString newTitle;        ///< Display title for the file in the project
    bool copyFile;           ///< true = copy file, false = move file
};

// ============================================================================
// AddToProjectDialog - Main dialog class
// ============================================================================

/// @brief Dialog for adding standalone files to an open project
///
/// AddToProjectDialog allows users to integrate standalone files into the
/// current project structure. The user can:
/// - Select a target section (frontmatter, body, backmatter, mindmaps, timelines)
/// - Select a target part (only when body section is selected)
/// - Set a display title for the file
/// - Choose whether to copy or move the file
///
/// Layout structure:
/// @code
/// +----------------------------------------------+
/// |  Add File to Project                          |
/// +----------------------------------------------+
/// |  File: notes.rtf                              |
/// |                                               |
/// |  Target Section: [Body          v]            |
/// |  Target Part:    [Part 1: Intro v]            |
/// |  Title:          [Research Notes_________]    |
/// |                                               |
/// |  Action: (*) Copy file to project             |
/// |          ( ) Move file to project             |
/// |                                               |
/// |              [Add to Project]    [Cancel]     |
/// +----------------------------------------------+
/// @endcode
///
/// Usage:
/// @code
/// AddToProjectDialog dialog("E:/notes.rtf", this);
/// if (dialog.exec() == QDialog::Accepted) {
///     AddToProjectResult result = dialog.result();
///     // Add file to project using result data
/// }
/// @endcode
class AddToProjectDialog : public QDialog {
    Q_OBJECT

public:
    /// @brief Constructs the AddToProjectDialog
    /// @param filePath Path to the file being added to the project
    /// @param parent Parent widget (typically MainWindow)
    explicit AddToProjectDialog(const QString& filePath, QWidget* parent = nullptr);

    /// @brief Destructor
    ~AddToProjectDialog() override = default;

    /// @brief Get the dialog result data
    /// @return Populated AddToProjectResult structure
    /// @note Only valid after dialog is accepted
    AddToProjectResult result() const;

private slots:
    /// @brief Handle section combo selection change
    /// @param index New selection index
    void onSectionChanged(int index);

    /// @brief Handle title text change
    /// @param text New text value
    void onTitleChanged(const QString& text);

    /// @brief Handle Accept button click
    void onAccept();

private:
    // ========================================================================
    // UI Setup
    // ========================================================================

    /// @brief Create and configure all UI elements
    void setupUI();

    /// @brief Create connections between signals and slots
    void createConnections();

    /// @brief Populate section combo with available sections
    void populateSections();

    /// @brief Populate parts combo based on current project structure
    void populateParts();

    /// @brief Validate input and update Add button state
    void validateInput();

    /// @brief Extract file name from path (without extension)
    /// @param filePath Full file path
    /// @return File name without extension
    QString extractFileName(const QString& filePath) const;

    // ========================================================================
    // State
    // ========================================================================

    /// @brief Path to file being added
    QString m_filePath;

    /// @brief Result data (populated on accept)
    AddToProjectResult m_result;

    // ========================================================================
    // File Info Widgets
    // ========================================================================

    /// @brief Label showing file name
    QLabel* m_fileLabel;

    // ========================================================================
    // Form Widgets
    // ========================================================================

    /// @brief Target section selection (frontmatter, body, backmatter, etc.)
    QComboBox* m_sectionCombo;

    /// @brief Target part selection (enabled only for Body section)
    QComboBox* m_partCombo;

    /// @brief Label for part combo (to hide when not applicable)
    QLabel* m_partLabel;

    /// @brief Display title input
    QLineEdit* m_titleEdit;

    /// @brief Copy file radio button
    QRadioButton* m_copyRadio;

    /// @brief Move file radio button
    QRadioButton* m_moveRadio;

    // ========================================================================
    // Dialog Buttons
    // ========================================================================

    /// @brief Standard dialog buttons (Add to Project, Cancel)
    QDialogButtonBox* m_buttonBox;

    /// @brief Add button reference (for enable/disable)
    QPushButton* m_addBtn;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
