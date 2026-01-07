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

#include <QWidget>
#include <QString>

class QStackedWidget;
class QLabel;
class QLineEdit;
class QComboBox;
class QTextEdit;
class QFormLayout;
class QTimer;

namespace kalahari::editor {
class BookEditor;
class StyleResolver;
}

namespace kalahari {
namespace gui {

class EditorPanel;

/// @brief Contextual properties panel with three views
///
/// Shows different content based on current context:
/// - NoProject: Placeholder message when no project is open
/// - Project: Project metadata (title, author, language, genre, statistics)
/// - Chapter: Chapter properties (title, word count, status, notes)
/// - Editor: Text selection statistics (word/char count, paragraph style)
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

    /// @brief Show editor statistics view
    ///
    /// Switches to editor page showing text statistics.
    /// Called when user starts editing in BookEditor.
    void showEditorProperties();

    /// @brief Connect to an EditorPanel for statistics updates
    /// @param editorPanel EditorPanel to track (nullptr to disconnect)
    ///
    /// Connects to BookEditor's selectionChanged signal for real-time
    /// updates of selection statistics.
    void setActiveEditor(EditorPanel* editorPanel);

    /// @brief Set the style resolver for style operations
    /// @param resolver StyleResolver to use for style resolution (nullptr to disconnect)
    ///
    /// When set, the properties panel can resolve style IDs to display names
    /// and update styles via the database. Connected to project's StyleResolver.
    void setStyleResolver(editor::StyleResolver* resolver);

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

    /// @brief Handle chapter notes changed (called on focus lost)
    void onChapterNotesChanged();

    /// @brief Handle selection changed in BookEditor
    void onEditorSelectionChanged();

    /// @brief Handle cursor position changed in BookEditor
    void onEditorCursorChanged();

    /// @brief Handle paragraph style changed from combo box
    void onEditorStyleChanged(int index);

protected:
    /// @brief Event filter for handling focus events
    /// @param obj Object that received the event
    /// @param event The event
    /// @return true if event was handled
    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    /// @brief Emitted when chapter status is changed via combo box
    /// @param elementId Element ID of the chapter
    /// @note Used to notify Navigator to refresh the item's display title (status suffix)
    void chapterStatusChanged(const QString& elementId);

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

    // Editor Page widgets (OpenSpec #00042 Task 7.4)
    QLabel* m_editorTitleLabel;           ///< "Selection" or "Document"
    QLabel* m_editorWordCountLabel;       ///< Word count
    QLabel* m_editorCharCountLabel;       ///< Character count (with spaces)
    QLabel* m_editorCharNoSpaceLabel;     ///< Character count (without spaces)
    QLabel* m_editorParagraphCountLabel;  ///< Paragraph count
    QLabel* m_editorReadingTimeLabel;     ///< Estimated reading time
    QComboBox* m_editorStyleCombo;        ///< Paragraph style selector
    QLabel* m_editorStyleLabel;           ///< Current style display

    // State tracking
    QString m_currentChapterId;    ///< Currently displayed chapter ID
    QString m_currentSectionType;  ///< Currently displayed section type
    QString m_currentPartId;       ///< Currently displayed part ID
    bool m_isUpdating;             ///< Flag to prevent recursive updates

    // Active editor tracking (OpenSpec #00042 Task 7.4)
    EditorPanel* m_activeEditorPanel{nullptr};  ///< Currently tracked editor panel

    // OpenSpec #00043: Debounce timer for cursor change updates
    QTimer* m_cursorDebounceTimer{nullptr};     ///< Debounce rapid cursor changes

    // Style resolver (OpenSpec #00042 Task 7.6)
    editor::StyleResolver* m_styleResolver{nullptr};  ///< Style resolver for the project

    /// @brief Populate style combo with styles from StyleResolver
    void populateStyleComboFromResolver();

    /// @brief Add default built-in styles to combo (fallback when no database)
    void addDefaultStylesToCombo();

    /// @brief Handle style combo selection to update style from resolver
    void applyStyleFromCombo();
};

} // namespace gui
} // namespace kalahari
