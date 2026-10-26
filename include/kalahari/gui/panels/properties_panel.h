/// @file properties_panel.h
/// @brief Contextual properties panel for project and chapter properties (Qt6)
///
/// This file defines the PropertiesPanel class, displaying different content
/// based on context: project properties (default), chapter properties (when
/// chapter selected), editor statistics (when editing), or placeholder
/// (when no project open).
///
/// OpenSpec #00033 Phase G: Full implementation with QStackedWidget.
/// OpenSpec #00042 Task 7.4: PropertiesPanel Integration with BookEditor.

#pragma once

#include "kalahari/core/book_project.h"

#include <QWidget>
#include <QString>

class QDateTime;
class QStackedWidget;
class QLabel;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QTextEdit;
class QFormLayout;
class QGroupBox;
class QTimer;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari {
namespace gui {

class EditorPanel;
class SectionNamesEdit;
class SectionsComboBox;

/// @brief Contextual properties panel with three views
///
/// Shows different content based on current context:
/// - NoProject: Placeholder message when no project is open
/// - Project: Project metadata (title, author, language, genre, sections, statistics)
/// - Chapter: Chapter properties (title, word count, status, notes)
/// - Editor: Text selection statistics (word/char count); the paragraph at the cursor is
///   the Paragraph and Styles panel's (ParagraphPanel)
///
/// Connects to ProjectManager signals for automatic updates.
/// Connects to BookEditor for real-time selection statistics.
class PropertiesPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Page indices for QStackedWidget
    enum class Page {
        NoProject = 0,  ///< Placeholder when no project open
        Project = 1,    ///< Project properties view
        Chapter = 2,    ///< Chapter properties view
        Section = 3,    ///< Section aggregate statistics view
        Part = 4,       ///< Part aggregate statistics view
        Editor = 5      ///< Editor statistics view (selection/document stats)
    };

    /// @brief Constructor
    /// @param parent Parent widget
    explicit PropertiesPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~PropertiesPanel() override;

    /// @brief The page the panel shows
    Page currentPage() const;

public slots:
    /// @brief Show project properties view
    ///
    /// Switches to project properties page and populates fields from
    /// the current project in ProjectManager.
    void showProjectProperties();

    /// @brief Show the book's properties with the keys in the first field, the title (Book >
    /// Book Properties...)
    void editProjectProperties();

    /// @brief Show chapter properties view
    /// @param elementId Element ID of the chapter to display
    ///
    /// Switches to chapter properties page and populates fields from
    /// the specified chapter element.
    void showChapterProperties(const QString& elementId);

    /// @brief Show no project placeholder
    ///
    /// Switches to placeholder page when no project is open.
    void showNoProject();

    /// @brief Show section properties view (aggregate statistics)
    /// @param sectionType Section type ("section_frontmatter", "section_body", "section_backmatter")
    void showSectionProperties(const QString& sectionType);

    /// @brief Show part properties view (aggregate statistics)
    /// @param partId Part ID
    void showPartProperties(const QString& partId);

    /// @brief Show editor statistics view
    ///
    /// Switches to editor page showing text statistics.
    /// Called when user starts editing in BookEditor.
    void showEditorProperties();

    /// @brief Connect to an EditorPanel for statistics updates
    /// @param editorPanel EditorPanel to track (nullptr to disconnect)
    ///
    /// Connects to BookEditor's selectionChanged signal for real-time
    /// updates of selection statistics. Without an editor (another tab in front, or none),
    /// the page of the editor gives way to the page of the book, or to the page without one.
    void setActiveEditor(EditorPanel* editorPanel);

    /// @brief Refresh current view with latest data
    ///
    /// Re-reads data from ProjectManager and updates displayed values.
    void refresh();

    /// @brief Show the words ProjectManager counts now on the current page, also of the
    /// changes that are not saved; the fields the writer edits stay as they are
    void updateWordCounts();

private slots:
    /// @brief Handle project opened signal
    /// @param projectPath Path to opened project
    void onProjectOpened(const QString& projectPath);

    /// @brief Handle project closed signal
    void onProjectClosed();

    /// @brief Handle project title changed
    void onProjectTitleChanged();

    /// @brief Handle project author changed
    void onProjectAuthorChanged();

    /// @brief Handle project language changed
    void onProjectLanguageChanged(int index);

    /// @brief Handle project genre changed
    void onProjectGenreChanged();

    /// @brief The writer chose another item of the Sections field
    void onProjectSectionsChanged();

    /// @brief The writer changed the own names of the sections
    void onProjectSectionNamesChanged();

    /// @brief Handle chapter title changed
    void onChapterTitleChanged();

    /// @brief Handle chapter status changed
    void onChapterStatusChanged(int index);

    /// @brief The writer set whether the text element starts on a left page
    void onChapterFirstPageChanged(bool left);

    /// @brief Handle chapter notes changed (called on focus lost)
    void onChapterNotesChanged();

    /// @brief Handle selection changed in BookEditor
    void onEditorSelectionChanged();

    /// @brief Handle cursor position changed in BookEditor
    void onEditorCursorChanged();

protected:
    /// @brief Event filter for handling focus events
    /// @param obj Object that received the event
    /// @param event The event
    /// @return true if event was handled
    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    /// @brief Emitted when chapter title or status is changed in the panel
    /// @param elementId Element ID of the chapter
    /// @note Used to notify Navigator to refresh the item's display title (status suffix)
    void chapterStatusChanged(const QString& elementId);

    /// @brief Whether a text element starts on a left page changed in the panel: its open
    ///        editor places the margins of its pages anew
    void chapterFirstPageChanged(const QString& elementId);

    /// @brief The writer chose how the Navigator divides the book: sections @p sections
    /// @note The Navigator's coordinator saves them, and refreshes the Navigator and the panel
    void requestSections(const kalahari::core::BookSections& sections);

    /// @brief The title or the language of the book changed in the panel: the Navigator shows
    /// the title, and the names of the sections in the language of the book
    void bookChanged();

    /// @brief Emitted when the book's language is changed in the panel
    /// @param language Its code (pl, en...)
    void bookLanguageChanged(const QString& language);

private:
    /// @brief Setup UI components
    void setupUI();

    /// @brief Setup "No Project" page
    /// @return Created widget
    QWidget* createNoProjectPage();

    /// @brief Setup "Project Properties" page
    /// @return Created widget
    QWidget* createProjectPage();

    /// @brief Setup "Chapter Properties" page
    /// @return Created widget
    QWidget* createChapterPage();

    /// @brief Setup "Section Properties" page (aggregate statistics)
    /// @return Created widget
    QWidget* createSectionPage();

    /// @brief Setup "Part Properties" page (aggregate statistics)
    /// @return Created widget
    QWidget* createPartPage();

    /// @brief Setup "Editor Properties" page (selection/document statistics)
    /// @return Created widget
    QWidget* createEditorPage();

    /// @brief Connect signals to ProjectManager
    void connectSignals();

    /// @brief Disconnect from current editor
    void disconnectFromEditor();

    /// @brief Update editor statistics from current BookEditor
    void updateEditorStatistics();

    /// @brief Populate project fields from current project
    void populateProjectFields();

    /// @brief Populate chapter fields from specified element
    /// @param elementId Element ID of the chapter
    void populateChapterFields(const QString& elementId);

    /// @brief Populate section fields (aggregate statistics)
    /// @param sectionType Section type
    void populateSectionFields(const QString& sectionType);

    /// @brief Populate part fields (aggregate statistics)
    /// @param partId Part ID
    void populatePartFields(const QString& partId);

    /// @brief Update project statistics
    void updateProjectStatistics();

    /// @brief Fill the Sections field and the own names of the sections of book @p book
    void populateBookSections(const core::ProjectBook& book);

    /// @brief Show the row of own names when the Sections field says own names
    void showSectionNamesRow();

    /// @brief Format date for display
    /// @param dateTime Date and time; invalid when unknown
    /// @return Formatted date string, in local time
    QString formatDate(const QDateTime& dateTime) const;

    /// @brief Select @p status in the chapter's status combo; "draft" when it is not there
    void selectChapterStatus(const QString& status);

    /// @brief Show the type of the open book @p project: its icon and its name, greyed out
    /// when the program does not have it, as the Dashboard shows it
    void showBookType(const core::BookProject& project, const core::ProjectBook& book);

    // Main widget
    QStackedWidget* m_stackedWidget;

    // No Project Page
    QLabel* m_noProjectLabel;

    // Project Page widgets
    QLabel* m_projectTypeIcon = nullptr;   ///< Icon of the book's type (showBookType())
    QLabel* m_projectTypeLabel = nullptr;  ///< Name of the book's type: "Novel", "Novel (not in
                                           ///< the program)"
    QLineEdit* m_projectTitleEdit;
    QLineEdit* m_projectAuthorEdit;
    QComboBox* m_projectLanguageCombo;
    QLineEdit* m_projectGenreEdit;
    QFormLayout* m_projectInfoLayout;          ///< Rows of the book's information
    SectionsComboBox* m_projectSectionsCombo;  ///< The sections of the book, or none
    SectionNamesEdit* m_projectSectionNames;   ///< Own names of the sections
    QLabel* m_projectChaptersTitle;  ///< "Total Chapters:", "Total Stories:": the main texts
    QLabel* m_projectChaptersLabel;
    QLabel* m_projectWordsLabel;
    QLabel* m_projectCreatedLabel;
    QLabel* m_projectModifiedLabel;
    QLabel* m_projectDraftCountLabel;
    QLabel* m_projectRevisionCountLabel;
    QLabel* m_projectFinalCountLabel;

    // Chapter Page widgets: a text element of any kind, or a file of the book outside its
    // text, which has only its title and its words
    QGroupBox* m_chapterInfoGroup;  ///< "Chapter Information", "Story Information"
    QFormLayout* m_chapterInfoLayout;  ///< Rows of the element's information
    QLineEdit* m_chapterTitleEdit;
    QLabel* m_chapterWordCountLabel;
    QComboBox* m_chapterStatusCombo;
    QCheckBox* m_chapterFirstPageLeft;  ///< The text starts on a left page (mirror margins)
    QGroupBox* m_chapterNotesGroup;
    QTextEdit* m_chapterNotesEdit;

    // Section Page widgets
    QLabel* m_sectionTitleLabel;
    QLabel* m_sectionChapterCountTitle;  ///< "Chapters:", "Stories:", or "Elements:" outside
                                         ///< the main one
    QLabel* m_sectionChapterCountLabel;
    QLabel* m_sectionWordCountLabel;
    QLabel* m_sectionDraftCountLabel;
    QLabel* m_sectionRevisionCountLabel;
    QLabel* m_sectionFinalCountLabel;

    // Part Page widgets
    QLabel* m_partTitleLabel;
    QLabel* m_partChapterCountTitle;  ///< "Chapters:", "Stories:": the main texts of the book
    QLabel* m_partChapterCountLabel;
    QLabel* m_partWordCountLabel;
    QLabel* m_partDraftCountLabel;
    QLabel* m_partRevisionCountLabel;
    QLabel* m_partFinalCountLabel;

    // Editor Page widgets (OpenSpec #00042 Task 7.4)
    QLabel* m_editorTitleLabel;           ///< "Selection" or "Document"
    QLabel* m_editorWordCountLabel;       ///< Word count
    QLabel* m_editorCharCountLabel;       ///< Character count (with spaces)
    QLabel* m_editorCharNoSpaceLabel;     ///< Character count (without spaces)
    QLabel* m_editorParagraphCountLabel;  ///< Paragraph count
    QLabel* m_editorReadingTimeLabel;     ///< Estimated reading time

    // State tracking
    QString m_currentChapterId;    ///< Currently displayed chapter ID
    QString m_currentSectionType;  ///< Currently displayed section type
    QString m_currentPartId;       ///< Currently displayed part ID
    bool m_isUpdating;             ///< Flag to prevent recursive updates

    // Active editor tracking (OpenSpec #00042 Task 7.4)
    EditorPanel* m_activeEditorPanel{nullptr};  ///< Currently tracked editor panel

    // OpenSpec #00043: Debounce timer for cursor change updates
    QTimer* m_cursorDebounceTimer{nullptr};     ///< Debounce rapid cursor changes

    int m_settingsListener{0};  ///< SettingsManager::subscribe() id: the rules of counting
};

} // namespace gui
} // namespace kalahari
