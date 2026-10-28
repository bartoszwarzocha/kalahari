/// @file new_item_dialog.h
/// @brief Dual-purpose dialog for creating new projects and files
///
/// NewItemDialog provides a unified interface for creating both new projects
/// (.klh files) and new files within a project. The dialog adapts its UI based
/// on the mode selected at construction time.
///
/// OpenSpec #00033: Project File System - Phase C

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/core/custom_book_type.h"
#include "kalahari/gui/dialogs/item_templates.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/widgets/sections_field.h"

#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>

#include <functional>
#include <optional>

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
    /// Selected template: in project mode the package of a book type ("kalahari.novel"), a
    /// type of the writer's own (NewItemDialog::CUSTOM_PREFIX and its id) or
    /// NewItemDialog::USER_PROJECT; in file mode a file template ("template.chapter")
    QString templateId;
    /// Package of the book type of the new book (project mode only); empty: a user project
    QString typeId;
    /// The writer's own type the book is made of (project mode only); none: a book type or a
    /// user project
    std::optional<kalahari::core::CustomBookType> customType;
    QString title;           ///< Project title or file name
    QString author;          ///< Author (project mode only)
    QString language;        ///< Language code (project mode only, e.g., "en")
    QString location;        ///< Project folder path (project mode only)
    bool createSubfolder;    ///< Create subfolder with project name (project mode only)
    /// Whether the Navigator shows the sections of the book, and their names (project mode
    /// only)
    std::optional<kalahari::core::BookSections> sections;
    /// Whether the new book opens (project mode only): false when the writer chose to create it
    /// without opening, and the open book stays open
    bool open = true;
};

// ============================================================================
// The question about the open book
// ============================================================================

/// @brief Whether a new book opens: the answer to the question about the open book
enum class NewBookOpening {
    Open,      ///< The new book opens in the place of the open one
    DontOpen,  ///< The open book stays; the new one goes to the Dashboard and to Recent Books
    Cancel     ///< Nothing is created; the window that asked stays as it was
};

/// @brief Asks in @p window about the open book before book @p title is created
using NewBookQuestion = std::function<NewBookOpening(QWidget* window, const QString& title)>;

// ============================================================================
// NewItemDialog - Main dialog class
// ============================================================================

/// @brief Dual-purpose dialog for creating projects and files
///
/// NewItemDialog adapts its interface based on the mode:
/// - Project mode: Shows author, language, location fields
/// - File mode: Shows only name field
///
/// One of the program's own dialogs: its heading says what the window makes, and Create Book
/// and Cancel are at the bottom. Layout structure of its content:
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
class NewItemDialog : public KalahariDialog {
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

    /// @brief Ask @p question about the open book before a book of a type is created; without
    /// a question the new book opens. A user project asks in its next window
    /// (UserProjectDialog).
    void setOpeningQuestion(NewBookQuestion question);

    /// @brief Template of a user project, the last one in project mode
    static constexpr const char* USER_PROJECT = "user-project";

    /// @brief Start of the templates of the writer's own types: "custom:<id of the type>"
    static constexpr const char* CUSTOM_PREFIX = "custom:";

    /// @brief Whether template @p templateId makes a user project: a book without a type,
    /// whose kinds the writer chooses in the next window
    static bool isUserProject(const QString& templateId);

    /// @brief The description of the chosen template, at the left: in project mode what a
    /// book of it starts with and what can be added to each section and to the Workshop
    QLabel* descriptionLabel() const { return m_descriptionLabel; }

    /// @brief Create Book: the result, and for a book of a type the question about the open
    /// book; the window closes unless the question is cancelled
    void accept() override;

protected:
    /// @brief Shown again (Back from the window of the kinds of a user project): the list
    /// has the types saved in the meantime
    void showEvent(QShowEvent* event) override;

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

    /// @brief Refresh icons when theme changes
    void onThemeChanged();

    /// @brief The Settings window of the gear was closed: a new folder of the books is
    /// suggested, unless the writer typed another
    void onSettingsClosed();

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

    /// @brief Populate the template list: in project mode the book types of the installed
    /// packages, the writer's own types and the user project; in file mode the file templates
    /// of TemplateRegistry. The chosen template stays chosen.
    void populateTemplates();

    /// @brief Add the template @p id to the list, with @p name, its tooltip and its icon
    void addTemplate(const QString& id, const QString& name, const QString& tooltip,
                     const QString& iconId);

    /// @brief The writer's own type of template @p templateId, or nullptr
    const kalahari::core::CustomBookType* customTypeOf(const QString& templateId) const;

    /// @brief Package of the book type of template @p templateId: the type itself, or the
    /// type a type of the writer's own is based on; empty for a user project
    QString typeIdOf(const QString& templateId) const;

    /// @brief The description of a book of template @p templateId, as rich text: the type's
    /// sentence, what the book starts with, the kinds of each section and of the Workshop
    /// (those the program cannot make yet in a line of their own) and what cannot change
    QString describe(const QString& templateId) const;

    /// @brief Name of @p place in the description: the name of its section that the Sections
    /// field gives, its place in a book without sections ("Content"), or the Workshop
    QString placeName(kalahari::core::BookPlace place) const;

    /// @brief Ask whether to delete the writer's own type of the chosen template, and delete
    /// its file
    void removeChosenCustomType();

    /// @brief Update description panel for selected template
    /// @param templateId Template ID to display
    void updateDescription(const QString& templateId);

    /// @brief Validate input and update Create button state
    ///
    /// In project mode the folder of the new book must be new or empty; the folder line
    /// names it, or says what to change when it is not.
    void validateInput();

    /// @brief Load default values from SettingsManager
    void loadDefaults();

    /// @brief Fill the Sections field: the sets of names in the language of the book, own
    /// names and no sections; keeps the chosen item
    void populateSections();

    /// @brief Whether a book of template @p templateId shows its sections when the writer does
    /// not choose otherwise: the template has a type, and the type shows them
    bool showsSections(const QString& templateId) const;

    /// @brief Show the choice of the selected template's group in the Sections field: the
    /// writer's last choice for a template that shows sections, no sections for the others
    void showSectionsChoice();

    /// @brief The writer chose an item of the Sections field
    void onSectionsChosen();

    /// @brief Fill the fields of own names with the names of the set chosen before, in the
    /// language of the book, until the writer types in them
    void fillSectionNames();

    /// @brief What the Sections field says
    kalahari::core::BookSections chosenSections() const;

    /// @brief Remember the writer's choice for the next book, for templates that show sections
    void saveSectionsChoice() const;

    /// @brief Name the create button after what it does: "Next" for a user project, which has
    /// a next window, else "Create Book"
    void updateCreateButton();

    // ========================================================================
    // State
    // ========================================================================

    /// @brief Dialog mode (Project or File)
    NewItemMode m_mode;

    /// @brief Result data (populated on accept)
    NewItemResult m_result;

    /// @brief Item of the Sections field for templates that show sections: the writer's last
    /// choice
    QString m_sectionsShown;

    /// @brief Item of the Sections field for templates that do not show sections
    QString m_sectionsHidden;

    /// @brief Set of names whose names fill the fields of own names
    QString m_namesSet;

    /// @brief The writer typed in the fields of own names, or they come from the last book
    bool m_sectionNamesTyped = false;

    /// @brief Asks about the open book before a book of a type is created
    NewBookQuestion m_openingQuestion;

    /// @brief The writer's own types, as the list shows them
    QList<kalahari::core::CustomBookType> m_customTypes;

    /// @brief The folder the Location field suggested (native separators)
    QString m_suggestedLocation;

    /// @brief The window was shown before
    bool m_shown = false;

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

    /// @brief Sections: a set of names, own names or no sections (project mode only)
    SectionsComboBox* m_sectionsCombo = nullptr;

    /// @brief Label of the row of own names of the sections
    QLabel* m_sectionNamesLabel = nullptr;

    /// @brief Own names of the front, main and back section
    SectionNamesEdit* m_sectionNames = nullptr;

    /// @brief Label for location row
    QLabel* m_locationLabel;

    /// @brief Icon of the folder line: a warning when the book cannot be made in the folder
    QLabel* m_folderIcon;

    /// @brief Folder line: the folder the book will be in, or why it cannot be made there
    QLabel* m_folderLabel;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
