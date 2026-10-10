/// @file document_coordinator.h
/// @brief Document lifecycle and file operations coordination for MainWindow
///
/// OpenSpec #00038 - Phase 7: Extract Document Operations from MainWindow
/// This class manages document lifecycle, project operations, and archive import/export.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <optional>
#include <filesystem>
#include <memory>
#include "kalahari/core/document.h"

namespace kalahari::editor {
class StyleResolver;
class StatisticsCollector;
}

class QMainWindow;
class QTabWidget;
class QStatusBar;

namespace kalahari {
namespace gui {

class NavigatorPanel;
class PropertiesPanel;
class DashboardPanel;
class NavigatorCoordinator;
class StandaloneInfoBar;
class EditorPanel;

/// @brief Coordinates document lifecycle and file operations
///
/// Manages:
/// - New document/project creation
/// - Open/Save/SaveAs operations
/// - Recent files handling
/// - Standalone file operations
/// - Archive import/export
/// - Project open/close lifecycle
///
/// Example usage:
/// @code
/// auto coordinator = new DocumentCoordinator(
///     mainWindow, centralTabs, navigatorPanel, propertiesPanel,
///     dashboardPanel, navigatorCoordinator, standaloneInfoBar,
///     statusBar,
///     [this]() { return m_isDirty; },
///     [this](bool d) { setDirty(d); },
///     [this]() { updateWindowTitle(); },
///     this
/// );
/// @endcode
class DocumentCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Callback type for checking dirty state
    using DirtyStateGetter = std::function<bool()>;

    /// @brief Callback type for setting dirty state
    using DirtySetter = std::function<void(bool)>;

    /// @brief Callback type for updating window title
    using WindowTitleUpdater = std::function<void()>;

    /// @brief Callback type for the single unsaved-changes predicate
    ///
    /// Returns true if there are ANY unsaved changes anywhere (content, structure,
    /// standalone tabs). Provided by MainWindow::hasUnsavedChanges() so every save
    /// prompt path in this coordinator consults one coherent source of truth.
    using HasUnsavedChangesGetter = std::function<bool()>;

    /// @brief Constructor
    /// @param mainWindow Parent QMainWindow for dialogs
    /// @param centralTabs Central tab widget for editor tabs
    /// @param navigatorPanel Navigator panel for document structure
    /// @param propertiesPanel Properties panel (unused, kept for future)
    /// @param dashboardPanel Dashboard panel for refresh after project changes
    /// @param navigatorCoordinator NavigatorCoordinator for dirty chapter tracking
    /// @param standaloneInfoBar Info bar for standalone files
    /// @param statusBar Status bar for feedback messages
    /// @param isDirty Callback to check if document is dirty
    /// @param setDirty Callback to set dirty state
    /// @param updateTitle Callback to update window title
    /// @param parent Parent QObject
    explicit DocumentCoordinator(QMainWindow* mainWindow,
                                  QTabWidget* centralTabs,
                                  NavigatorPanel* navigatorPanel,
                                  PropertiesPanel* propertiesPanel,
                                  DashboardPanel* dashboardPanel,
                                  NavigatorCoordinator* navigatorCoordinator,
                                  StandaloneInfoBar* standaloneInfoBar,
                                  QStatusBar* statusBar,
                                  DirtyStateGetter isDirty,
                                  DirtySetter setDirty,
                                  WindowTitleUpdater updateTitle,
                                  HasUnsavedChangesGetter hasUnsavedChanges,
                                  QObject* parent = nullptr);

    /// @brief Destructor
    ~DocumentCoordinator() override = default;

    // =========================================================================
    // Document state accessors
    // =========================================================================

    /// @brief Get current document (if loaded)
    [[nodiscard]] std::optional<core::Document>& currentDocument() { return m_currentDocument; }
    [[nodiscard]] const std::optional<core::Document>& currentDocument() const { return m_currentDocument; }

    /// @brief Get current file path
    [[nodiscard]] const std::filesystem::path& currentFilePath() const { return m_currentFilePath; }

    /// @brief Set current file path
    void setCurrentFilePath(const std::filesystem::path& path) { m_currentFilePath = path; }

    /// @brief Get the files of the navigator's "Other Files" (opened outside the project)
    [[nodiscard]] const QStringList& standaloneFilePaths() const { return m_standaloneFilePaths; }

    /// @brief Let the info bar of a standalone file show, or keep it hidden
    ///
    /// Distraction-Free writing keeps it hidden with the window's other bars.
    /// @param allowed false to hide it until it is allowed again
    void setInfoBarAllowed(bool allowed);

    // =========================================================================
    // Per-editor save state
    // =========================================================================

    /// @brief Check whether one editor tab has unsaved changes
    /// @param editor Editor tab: project chapter, standalone file or single-file document
    [[nodiscard]] bool isEditorDirty(const EditorPanel* editor) const;

    /// @brief Save one editor tab
    /// @param editor Editor tab to save
    /// @return true when its content is saved; false when saving failed, was cancelled or
    ///         is not possible - the tab then keeps its unsaved changes
    bool saveEditor(EditorPanel* editor);

    /// @brief Drop one editor tab's unsaved changes (it is about to close)
    /// @param editor Editor tab
    void discardEditorChanges(EditorPanel* editor);

    /// @brief Save all unsaved changes: the project and every editor tab
    /// @return true when nothing is left unsaved
    bool saveAllChanges();

    /// @brief Names of what has unsaved changes, for the question whether to save them
    /// @return The book's title (for its chapters and structure) and the name of every
    ///         other editor tab with unsaved changes; empty when nothing is known
    [[nodiscard]] QStringList unsavedDocumentNames() const;

public slots:
    // =========================================================================
    // Document operations
    // =========================================================================

    /// @brief Create new document
    void onNewDocument();

    /// @brief Create new project
    void onNewProject();

    /// @brief Open document via file dialog
    void onOpenDocument();

    /// @brief Open a recent file
    /// @param filePath Path to the file to open
    void onOpenRecentFile(const QString& filePath);

    /// @brief Save current document
    void onSaveDocument();

    /// @brief Save document with new name
    void onSaveAsDocument();

    /// @brief Save all modified files
    void onSaveAll();

    /// @brief Close current document
    void onCloseDocument();

    // =========================================================================
    // Standalone file operations
    // =========================================================================

    /// @brief Open standalone file via file dialog
    void onOpenStandaloneFile();

    /// @brief Open a chapter (.kchapter) or text file (.txt) outside the project
    /// @param path Absolute path to the file
    ///
    /// A file already open in a tab is shown there; other files get a message.
    void openStandaloneFile(const QString& path);

    /// @brief Add the standalone file of the info bar (the current tab's) to the project
    void onAddToProject();

    /// @brief Add a standalone file to the project, saving its tab's changes first
    /// @param filePath Absolute path to the file
    void addToProject(const QString& filePath);

    /// @brief Remove a file from the navigator's "Other Files" (its tab stays open)
    /// @param path Absolute path to the file
    void removeStandaloneFile(const QString& path);

    // =========================================================================
    // Archive operations
    // =========================================================================

    /// @brief Export current project to archive
    void onExportArchive();

    /// @brief Import project from archive
    void onImportArchive();

    // =========================================================================
    // Project lifecycle
    // =========================================================================

    /// @brief Handle project opened event
    /// @param projectPath Path to the opened project
    void onProjectOpened(const QString& projectPath);

    /// @brief Prepare services for project close (before database is destroyed)
    ///
    /// Connected to ProjectManager::projectAboutToClose, so StatisticsCollector
    /// and other services flush data before the database closes.
    void prepareForProjectClose();

    /// @brief Handle project closed event
    void onProjectClosed();

    // =========================================================================
    // Style Resolver Access (OpenSpec #00042 Task 7.6)
    // =========================================================================

    /// @brief Get the project's style resolver
    /// @return Pointer to StyleResolver (nullptr if no project open)
    ///
    /// The StyleResolver is connected to the project's database when a project
    /// is opened. It provides paragraph and character style resolution with
    /// inheritance support. When styles change in the database, the resolver
    /// emits stylesChanged() which can be connected to refresh editors.
    editor::StyleResolver* styleResolver() const { return m_styleResolver.get(); }

    // =========================================================================
    // Statistics Collector Access (OpenSpec #00042 Task 7.7)
    // =========================================================================

    /// @brief Get the project's statistics collector
    /// @return Pointer to StatisticsCollector (nullptr if no project open)
    ///
    /// The StatisticsCollector tracks writing statistics (words written/deleted,
    /// active time) and saves them to the project database. It automatically
    /// starts a session when a project is opened and ends it when closed.
    editor::StatisticsCollector* statisticsCollector() const { return m_statisticsCollector.get(); }

signals:
    /// @brief Emitted when a document is opened
    void documentOpened();

    /// @brief Emitted when a document is closed
    void documentClosed();

    /// @brief Emitted when document is modified
    void documentModified();

    /// @brief Emitted when recent files list is updated
    void recentFilesUpdated();

    /// @brief Emitted when window title should be updated
    /// @param title New window title
    void windowTitleChanged(const QString& title);

private:
    /// @brief Ask user to save if document is dirty
    /// @return true if operation should continue, false if cancelled
    bool maybeSave();

    /// @brief Before a command that puts another book in the place of the open one: asks
    /// whether to save the book's changes or, without changes, whether to close it
    /// @param saveQuestion The question with unsaved changes; "%1" is the book's title
    /// @param closeQuestion The question without them; "%1" is the book's title
    /// @return true when no book is open or the writer agreed; false: the command stops
    bool agreeToCloseBook(const QString& saveQuestion, const QString& closeQuestion);

    /// @brief Open the first text of the book's body in the editor, e.g. Chapter 1 of a new
    /// book; nothing when the body has no text
    void openFirstText();

    /// @brief Get currently active EditorPanel tab
    /// @return Active EditorPanel or nullptr if not an editor tab
    EditorPanel* getCurrentEditor() const;

    /// @brief Get text from first chapter metadata (Phase 0 temporary hack)
    /// @param doc Document to extract text from
    /// @return Editor text content, or empty string if no content
    QString getPhase0Content(const core::Document& doc) const;

    /// @brief Set text in first chapter metadata (Phase 0 temporary hack)
    /// @param doc Document to update
    /// @param text Editor text content
    void setPhase0Content(core::Document& doc, const QString& text);

    /// @brief Tab of a standalone file, or nullptr when the file is not open
    EditorPanel* findStandaloneEditor(const QString& path) const;

    /// @brief Show the info bar for the current tab's standalone file, or hide it
    void updateStandaloneInfoBar();

    /// @brief List the standalone files in the navigator again after it was cleared
    void relistStandaloneFiles();

    /// @brief Write a standalone file tab's content in the file's own format
    /// @param path The tab's file (Save) or a new file the tab moves to (Save As)
    /// @return true when the file was written
    bool writeStandaloneFile(EditorPanel* editor, const QString& path);

    /// @brief Save a standalone file tab under a name the user chooses (chapter or text file)
    /// @return true when the file was written
    bool saveStandaloneFileAs(EditorPanel* editor);

    /// @brief Save an editor's text as the single-file (Phase 0) document
    /// @param editor Editor whose text is saved
    /// @param askForPath Ask for a new file name (Save As) instead of using the current one
    /// @return true when the document was written
    bool saveSingleDocument(EditorPanel* editor, bool askForPath);

    QMainWindow* m_mainWindow;
    QTabWidget* m_centralTabs;
    NavigatorPanel* m_navigatorPanel;
    PropertiesPanel* m_propertiesPanel;
    DashboardPanel* m_dashboardPanel;
    NavigatorCoordinator* m_navigatorCoordinator;
    StandaloneInfoBar* m_standaloneInfoBar;
    bool m_infoBarAllowed{true};    ///< The info bar may show (not in Distraction-Free)
    QStatusBar* m_statusBar;

    DirtyStateGetter m_isDirty;
    DirtySetter m_setDirty;
    WindowTitleUpdater m_updateWindowTitle;
    HasUnsavedChangesGetter m_hasUnsavedChanges;

    /// @brief Current loaded document
    std::optional<core::Document> m_currentDocument;

    /// @brief Current .klh file path
    std::filesystem::path m_currentFilePath;

    /// @brief Files of the navigator's "Other Files" (opened outside the project)
    QStringList m_standaloneFilePaths;

    /// @brief Style resolver for the current project (OpenSpec #00042 Task 7.6)
    std::unique_ptr<editor::StyleResolver> m_styleResolver;

    /// @brief Statistics collector for session tracking (OpenSpec #00042 Task 7.7)
    std::unique_ptr<editor::StatisticsCollector> m_statisticsCollector;
};

} // namespace gui
} // namespace kalahari
