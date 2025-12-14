/// @file properties_panel.h
/// @brief Contextual properties panel for project and chapter properties (Qt6)
///
/// This file defines the PropertiesPanel class, displaying different content
/// based on context: project properties (default), chapter properties (when
/// chapter selected), or placeholder (when no project open).
///
/// OpenSpec #00033 Phase G: Full implementation with QStackedWidget.

#pragma once

#include <QWidget>
#include <QString>

class QStackedWidget;
class QLabel;
class QLineEdit;
class QComboBox;
class QTextEdit;
class QFormLayout;

namespace kalahari {
namespace gui {

/// @brief Contextual properties panel with three views
///
/// Shows different content based on current context:
/// - NoProject: Placeholder message when no project is open
/// - Project: Project metadata (title, author, language, genre, statistics)
/// - Chapter: Chapter properties (title, word count, status, notes)
///
/// Connects to ProjectManager signals for automatic updates.
class PropertiesPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Page indices for QStackedWidget
    enum class Page {
        NoProject = 0,  ///< Placeholder when no project open
        Project = 1,    ///< Project properties view
        Chapter = 2,    ///< Chapter properties view
        Section = 3,    ///< Section aggregate statistics view
        Part = 4        ///< Part aggregate statistics view
    };

    /// @brief Constructor
    /// @param parent Parent widget
    explicit PropertiesPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~PropertiesPanel() override = default;

public slots:
    /// @brief Show project properties view
    ///
    /// Switches to project properties page and populates fields from
    /// the current project in ProjectManager.
    void showProjectProperties();

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

    /// @brief Refresh current view with latest data
    ///
    /// Re-reads data from ProjectManager and updates displayed values.
    void refresh();

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

    /// @brief Handle chapter title changed
    void onChapterTitleChanged();

    /// @brief Handle chapter status changed
    void onChapterStatusChanged(int index);

    /// @brief Handle chapter notes changed
    void onChapterNotesChanged();

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

    /// @brief Connect signals to ProjectManager
    void connectSignals();

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

    /// @brief Format date for display
    /// @param timePoint Chrono time point
    /// @return Formatted date string
    QString formatDate(const std::chrono::system_clock::time_point& timePoint) const;

    // Main widget
    QStackedWidget* m_stackedWidget;

    // No Project Page
    QLabel* m_noProjectLabel;

    // Project Page widgets
    QLineEdit* m_projectTitleEdit;
    QLineEdit* m_projectAuthorEdit;
    QComboBox* m_projectLanguageCombo;
    QLineEdit* m_projectGenreEdit;
    QLabel* m_projectChaptersLabel;
    QLabel* m_projectWordsLabel;
    QLabel* m_projectCreatedLabel;
    QLabel* m_projectModifiedLabel;
    QLabel* m_projectDraftCountLabel;
    QLabel* m_projectRevisionCountLabel;
    QLabel* m_projectFinalCountLabel;

    // Chapter Page widgets
    QLineEdit* m_chapterTitleEdit;
    QLabel* m_chapterWordCountLabel;
    QComboBox* m_chapterStatusCombo;
    QTextEdit* m_chapterNotesEdit;

    // Section Page widgets
    QLabel* m_sectionTitleLabel;
    QLabel* m_sectionChapterCountLabel;
    QLabel* m_sectionWordCountLabel;
    QLabel* m_sectionDraftCountLabel;
    QLabel* m_sectionRevisionCountLabel;
    QLabel* m_sectionFinalCountLabel;

    // Part Page widgets
    QLabel* m_partTitleLabel;
    QLabel* m_partChapterCountLabel;
    QLabel* m_partWordCountLabel;
    QLabel* m_partDraftCountLabel;
    QLabel* m_partRevisionCountLabel;
    QLabel* m_partFinalCountLabel;

    // State tracking
    QString m_currentChapterId;    ///< Currently displayed chapter ID
    QString m_currentSectionType;  ///< Currently displayed section type
    QString m_currentPartId;       ///< Currently displayed part ID
    bool m_isUpdating;             ///< Flag to prevent recursive updates
};

} // namespace gui
} // namespace kalahari
