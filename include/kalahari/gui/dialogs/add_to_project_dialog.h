/// @file add_to_project_dialog.h
/// @brief Dialog for adding standalone files to an open project
///
/// AddToProjectDialog allows users to add a standalone file (not part of the
/// project) to the current project structure. Users can choose the target
/// section (front, main, back, with the book's names; in a book without sections, the place
/// in the book), the part in the main section, the kind of the new element and whether to copy
/// or move the file.
///
/// OpenSpec #00033: Project File System - Phase F

#pragma once

#include "kalahari/core/book_type_registry.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QLabel>
#include <QList>
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
    core::BookPlace place = core::BookPlace::Main;  ///< Section: front, main or back
    QString groupId;         ///< Part ID in the body; empty: the body itself or another section
    core::KindRef kind;      ///< Kind of the new element (a chapter, a prologue...)
    QString newTitle;        ///< Display title for the file in the project
    bool copyFile = true;    ///< true = copy file, false = move file
};

// ============================================================================
// AddToProjectDialog - Main dialog class
// ============================================================================

/// @brief Dialog for adding standalone files to an open project
///
/// AddToProjectDialog allows users to integrate standalone files into the
/// current project structure. The user can:
/// - Select a target section (front, main, back), named as the book names them; in a book
///   without sections, the place in the book (its beginning, its content, its end)
/// - Select a target part (only when the main section is selected and the book has parts)
/// - Select the kind of the new element: a text kind the project offers there
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
/// |  Section: [Body          v]                   |
/// |  Part:    [Part 1: Intro v]                   |
/// |  Kind:    [Chapter       v]                   |
/// |  Title:   [Research Notes_________]           |
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

    /// @brief Handle part combo selection change: the kinds follow the part
    void onPartChanged();

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

    /// @brief Populate kinds combo with the text kinds the project offers in the chosen
    /// section and part
    void populateKinds();

    /// @brief Section chosen in the section combo
    core::BookPlace currentPlace() const;

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

    /// @brief Target section selection (front, main, back)
    QComboBox* m_sectionCombo;

    /// @brief Label for section combo: "Section:", or "Place:" in a book without sections
    QLabel* m_sectionLabel = nullptr;

    /// @brief Target part selection (shown only for the main section of a book with parts)
    QComboBox* m_partCombo;

    /// @brief Label for part combo (to hide when not applicable)
    QLabel* m_partLabel;

    /// @brief Kind of the new element
    QComboBox* m_kindCombo;

    /// @brief Kinds of the kind combo, in its order
    QList<core::KindRef> m_kinds;

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
