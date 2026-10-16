/// @file document_coordinator.h
/// @brief Document lifecycle and file operations coordination for MainWindow
///
/// OpenSpec #00038 - Phase 7: Extract Document Operations from MainWindow
/// This class manages document lifecycle, project operations, and archive import/export.

#pragma once

#include "kalahari/core/standalone_file.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

namespace kalahari::core {
enum class OpenFailure;
}

namespace kalahari::editor {
class StyleResolver;
class StatisticsCollector;
}

class QMainWindow;
class QTabWidget;
class QStatusBar;
class QWidget;

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
///     [this]() { updateWindowTitle(); },
///     [this]() { return hasUnsavedChanges(); },
///     this
/// );
/// @endcode
class DocumentCoordinator : public QObject {
    Q_OBJECT

public:
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
    /// @param updateTitle Callback to update window title
    /// @param hasUnsavedChanges Callback that tells whether anything has unsaved changes
    /// @param parent Parent QObject
    explicit DocumentCoordinator(QMainWindow* mainWindow,
                                  QTabWidget* centralTabs,
                                  NavigatorPanel* navigatorPanel,
                                  PropertiesPanel* propertiesPanel,
                                  DashboardPanel* dashboardPanel,
                                  NavigatorCoordinator* navigatorCoordinator,
                                  StandaloneInfoBar* standaloneInfoBar,
                                  QStatusBar* statusBar,
                                  WindowTitleUpdater updateTitle,
                                  HasUnsavedChangesGetter hasUnsavedChanges,
                                  QObject* parent = nullptr);

    /// @brief Destructor
    ~DocumentCoordinator() override = default;

    // =========================================================================
    // Document state accessors
    // =========================================================================

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
    /// @param editor Editor tab: a chapter of the book or a file outside it
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

    /// @brief Why Save has nothing to save now: neither a book is open nor a file outside it
    /// is in front
    /// @return The reason, for its tooltip and the status bar; empty while Save saves
    [[nodiscard]] QString whyNotSave() const;

    /// @brief Why Save As has nothing to save under another name now: no file outside the
    /// book is in front, or a chapter of the book is, which is saved with the book
    /// @return The reason, for its tooltip and the status bar; empty while Save As saves
    [[nodiscard]] QString whyNotSaveAs() const;

public slots:
    // =========================================================================
    // Document operations
    // =========================================================================

    /// @brief New File: with a book open, a new element of the book or an empty text file in
    /// the New File window, with the book (in "Other Files") or outside it; without one, an
    /// empty file outside a book. A file outside the book is listed in the Navigator at once
    /// and saved where and as the writer chooses when it is saved for the first time.
    void onNewDocument();

    /// @brief Create new project
    void onNewProject();

    /// @brief Open document via file dialog
    void onOpenDocument();

    /// @brief Open a recent file: a book, or another file outside a book
    /// @param filePath Path to the file to open
    void onOpenRecentFile(const QString& filePath);

    /// @brief Save current document
    void onSaveDocument();

    /// @brief Save the file of the current tab under a name the writer chooses; a chapter of
    /// the book is saved with the book, and the status bar says so
    void onSaveAsDocument();

    /// @brief Save the book: its changed chapters and its data; what cannot be saved is
    /// reported in a window
    void onSaveBook();

    /// @brief Save all: the book and every open file outside it that has unsaved changes
    ///
    /// A file that cannot be saved is reported and does not stop the others; with nothing to
    /// save, the status bar says so.
    void onSaveAll();

    /// @brief Close the book: its chapters close with it, and the files open outside it stay
    /// open with their changes, so only the book's unsaved changes are asked about
    void onCloseDocument();

    // =========================================================================
    // Standalone file operations
    // =========================================================================

    /// @brief Open standalone file via file dialog
    void onOpenStandaloneFile();

    /// @brief Open a chapter (.kchapter) or text file (.txt) outside the project
    /// @param path Absolute path to the file
    ///
    /// A file already open in a tab is shown there, and the file of an element of the open
    /// book opens as the element; other files get a message.
    void openStandaloneFile(const QString& path);

    /// @brief Add the standalone file of the info bar (the current tab's) to the project: a new
    /// file that has no file yet at once, any other file in the Add to Book window
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

    /// @brief Export the open book to an archive, in the Export Project Archive window
    void onExportArchive();

    /// @brief Import a book from an archive, in the Import Project Archive window
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
    /// @brief Before a command that puts another book in the place of the open one: asks
    /// whether to save the book's changes or, without changes, whether to close it
    /// @param saveQuestion The question with unsaved changes; "%1" is the book's title
    /// @param closeQuestion The question without them; "%1" is the book's title
    /// @return true when no book is open or the writer agreed; false: the command stops
    bool agreeToCloseBook(const QString& saveQuestion, const QString& closeQuestion);

    /// @brief What the writer chose to do with the open book when another book comes
    enum class OpenBookChoice {
        Close,     ///< The open book closes, with its changes saved or not, as the writer said
        KeepOpen,  ///< The open book stays open, and the other book does not open
        Cancel     ///< Nothing happens; the writer returns to the window that asked
    };

    /// @brief Before another book comes while a book is open: asks whether the open book
    /// closes and the other one opens, or the open book stays and the other one does not open;
    /// before the open book closes, asks about its unsaved changes
    /// @param parent The window that asks
    /// @param description Sentence under the heading: what can happen to the other book
    /// @param note Where the book that does not open is found, and where Cancel returns
    /// @param closeText Text of the button that closes the open book and opens the other one
    /// @param keepText Text of the button that keeps the open book open
    /// @param saveQuestion The question about the unsaved changes; "%1" is the book's title
    /// @return The choice; Close when no book is open
    OpenBookChoice askAboutOpenBook(QWidget* parent, const QString& description,
                                    const QString& note, const QString& closeText,
                                    const QString& keepText, const QString& saveQuestion);

    /// @brief Whether the open book has unsaved changes: the text of its chapters open in
    /// tabs, or the book's data; the standalone files are not the book's
    [[nodiscard]] bool bookHasUnsavedChanges() const;

    /// @brief Save the text of the book's chapters open in tabs and the book's data
    /// @return false when some changes could not be saved
    bool saveBook();

    /// @brief Tell the writer why the book of @p path cannot be opened, in plain words, with
    /// the path and @p problems under Show Details. A book that another program has open
    /// can be tried again, and a book of an older version converted (offerConversion()).
    void showOpenError(const QString& path, const QStringList& problems,
                       core::OpenFailure failure);

    /// @brief Ask the writer whether to convert the book of @p path, saved by an older version
    /// of Kalahari, to the format of this version, and open it when it is converted
    /// (core::ProjectManager::convertOlderBook())
    void offerConversion(const QString& path);

    /// @brief Tell the writer why the book of @p path cannot be opened, when something keeps
    /// it from opening (core::ProjectManager::openFailure()), before the open book is closed
    /// for it
    /// @return true when the book cannot be opened
    bool reportOpenFailure(const QString& path);

    /// @brief Open the first text of the book's body in the editor, e.g. Chapter 1 of a new
    /// book; nothing when the body has no text
    void openFirstText();

    /// @brief Get currently active EditorPanel tab
    /// @return Active EditorPanel or nullptr if not an editor tab
    EditorPanel* getCurrentEditor() const;

    /// @brief Tab of a standalone file, or nullptr when the file is not open
    /// @param path The file, or the key of a new file in the Navigator (openNewFile())
    EditorPanel* findStandaloneEditor(const QString& path) const;

    /// @brief Show the info bar for the current tab's standalone file, or hide it
    void updateStandaloneInfoBar();

    /// @brief List the standalone files in the navigator again after it was cleared, with the
    /// new files that have no file yet
    void relistStandaloneFiles();

    /// @brief Write a standalone file tab's content in the file's own format
    ///
    /// A plain text file keeps the text only: a content with formatting or annotations is
    /// written only after the writer agrees (agreeToSaveAsText()), and then the tab shows
    /// the text of the file (showTextOfFile()).
    /// @param path The tab's file (Save) or a new file the tab moves to (Save As)
    /// @return true when the file was written
    bool writeStandaloneFile(EditorPanel* editor, const QString& path);

    /// @brief Whether to write to the plain text file @p path a content that loses @p lost
    /// there: the writer is asked
    bool agreeToSaveAsText(const core::StandaloneFile::TextLoss& lost, const QString& path);

    /// @brief Show in the tab of @p editor the text of the plain text file @p path it was
    /// saved in, without the formatting and the annotations the file does not keep
    ///
    /// One edit, which brings them back when undone. The cursor stays at its place in the
    /// text, also when a line break became the end of a paragraph, and the view at the text
    /// it showed.
    void showTextOfFile(EditorPanel* editor, const QString& path);

    /// @brief Save a standalone file tab under a name the user chooses (chapter or text file);
    /// a new file starts in the folder of Open File
    /// @return true when the file was written
    bool saveStandaloneFileAs(EditorPanel* editor);

    /// @brief Add the text of the new file of @p editor, which has no file yet, to the open
    /// book, asking nothing: an element of the main text kind of the book, at the place of its
    /// kind and with the title of a new element of it, whose file is in the book's folder; the
    /// element takes the place of the file's tab
    void addNewFileToProject(EditorPanel* editor);

    /// @brief Mark a standalone file tab and the file in the Navigator as changed, with "*"
    /// before its name, while its text differs from the saved text
    void trackStandaloneChanges(EditorPanel* editor);

    /// @brief An empty file outside a book in a new tab named @p title, listed at once in the
    /// Navigator under "Files Outside the Book"; its first save asks where to save it, as a
    /// Kalahari document or a text file
    void openNewFile(const QString& title);

    /// @brief The files of the open book that are open outside it become the tabs of their
    /// elements, in their places and with their unsaved changes: one file is edited in one tab
    void adoptBookFiles();

    QMainWindow* m_mainWindow;
    QTabWidget* m_centralTabs;
    NavigatorPanel* m_navigatorPanel;
    PropertiesPanel* m_propertiesPanel;
    DashboardPanel* m_dashboardPanel;
    NavigatorCoordinator* m_navigatorCoordinator;
    StandaloneInfoBar* m_standaloneInfoBar;
    bool m_infoBarAllowed{true};    ///< The info bar may show (not in Distraction-Free)
    QStatusBar* m_statusBar;

    WindowTitleUpdater m_updateWindowTitle;
    HasUnsavedChangesGetter m_hasUnsavedChanges;

    /// @brief Files of the navigator's "Other Files" (opened outside the project)
    QStringList m_standaloneFilePaths;

    /// @brief Empty text files made so far, outside a book or with it, for the number in the
    /// names they are given ("Untitled 2")
    int m_newFiles{0};

    /// @brief New files that got a key in the Navigator so far ("untitled:3", openNewFile())
    int m_newFileKeys{0};

    /// @brief Style resolver for the current project (OpenSpec #00042 Task 7.6)
    std::unique_ptr<editor::StyleResolver> m_styleResolver;

    /// @brief Statistics collector for session tracking (OpenSpec #00042 Task 7.7)
    std::unique_ptr<editor::StatisticsCollector> m_statisticsCollector;
};

} // namespace gui
} // namespace kalahari
