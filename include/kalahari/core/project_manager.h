/// @file project_manager.h
/// @brief The book project open in the program: its .klh file, its files and their state
///
/// ProjectManager is a singleton that creates, opens, saves and closes book projects. The
/// project is a BookProject (book_project.h): the .klh file with the type, the books and the
/// Workshop. The text of each element is in a chapter file of its own (.kchapter) in the
/// book's folder; the program keeps the text of open chapters, their word counts and notes
/// while it works, and saves them to these files.
///
/// @example
/// @code
/// auto& pm = ProjectManager::getInstance();
///
/// // Create a new novel and open it
/// pm.createProject("E:/Books", "My Novel", "John Doe", "en", true, "kalahari.novel");
///
/// // Open an existing project
/// pm.openProject("E:/Books/My Novel/My Novel.klh");
///
/// // Add a chapter and save its text
/// const QString id = pm.addElement(pm.chapterKindFor(BookPlace::Main), "Chapter 1",
///                                  BookPlace::Main);
/// pm.setChapterContent(id, kml);
/// pm.saveChapterContent(id);
///
/// pm.closeProject();
/// @endcode

#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
// OpenSpec #00041: SQLite Project Database
#include "kalahari/core/project_database.h"
#include "kalahari/core/project_lock.h"
#include "kalahari/core/backup_manager.h"

namespace kalahari {
namespace core {

/// @brief Work mode enumeration for application state
///
/// Defines what type of document is currently active:
/// - NoDocument: Nothing open, welcome screen
/// - ProjectMode: .klh project open with full features
/// - StandaloneMode: Single file without project context
enum class WorkMode {
    NoDocument,      ///< Nothing open - show welcome/dashboard
    ProjectMode,     ///< Full project open (.klh manifest)
    StandaloneMode   ///< Single file without project - limited features
};

/// @brief Words and statuses of the text elements of a list, with those inside its groups
struct TextStatistics {
    int elements = 0;                 ///< Text elements (chapters, prefaces, poems...)
    int words = 0;                    ///< Their words
    std::map<QString, int> statuses;  ///< Status ("draft", "revision", "final") -> elements
};

/// @brief Singleton manager of the book project open in the program
///
/// Signals:
/// - projectOpened: Emitted when project is successfully opened (also a new one)
/// - projectAboutToClose: Emitted before the project database is closed
/// - projectClosed: Emitted when project is closed
/// - workModeChanged: Emitted when work mode changes
/// - dirtyStateChanged: Emitted when project dirty state changes
class ProjectManager : public QObject {
    Q_OBJECT

public:
    /// @brief Get singleton instance
    /// @return Reference to the singleton ProjectManager
    static ProjectManager& getInstance();

    // Prevent copy and move
    ProjectManager(const ProjectManager&) = delete;
    ProjectManager& operator=(const ProjectManager&) = delete;
    ProjectManager(ProjectManager&&) = delete;
    ProjectManager& operator=(ProjectManager&&) = delete;

    // =========================================================================
    // Project Lifecycle
    // =========================================================================

    /// @brief Create a new project and open it
    /// @param parentDir Folder of the project, or the folder it is made in (createSubfolder)
    /// @param title Title of the book; it also names the folder and the .klh file
    /// @param author Author of the book
    /// @param language Language code of the text (e.g., "en", "pl")
    /// @param createSubfolder Make the project's folder, named after the title, in parentDir
    /// @param typeId Package of the book type, e.g. "kalahari.novel"; empty: a user project
    ///        with the kinds of the base package
    /// @param problems Gets what is wrong when the project cannot be created, one line each
    /// @return false when the folder exists and is not empty, the type is not installed or a
    ///         file cannot be written; the open project stays open when the folder is wrong
    ///
    /// A book of a type starts with the elements the type names, titled in the language of
    /// the book: a novel with a title page and Chapter 1, a screenplay with a title page and
    /// Act I. A kind's template gives a new element its text, with the book's title and author
    /// in place of "{title}" and "{author}". A user project starts empty. The folders of the
    /// other files are made when they are needed.
    bool createProject(const QString& parentDir,
                       const QString& title,
                       const QString& author,
                       const QString& language,
                       bool createSubfolder = true,
                       const QString& typeId = QString(),
                       QStringList* problems = nullptr);

    /// @brief Folder that createProject() makes the project in
    /// @param parentDir Folder of the project, or the folder it is made in (createSubfolder)
    /// @param title Title of the book, which names the subfolder
    /// @param createSubfolder The project's folder is named after the title, in parentDir
    /// @return The folder; empty when the title gives no name, so no project can be made
    static QString newProjectFolder(const QString& parentDir, const QString& title,
                                    bool createSubfolder);

    /// @brief Whether createProject() can make a project in @p folder: the folder does not
    /// exist yet or it is empty
    static bool canHoldNewProject(const QString& folder);

    /// @brief Open an existing project from its .klh file
    /// @param manifestPath Path to the .klh file
    /// @param problems Gets what is wrong when the project cannot be opened, one line each
    /// @return true if project opened successfully, false otherwise
    bool openProject(const QString& manifestPath, QStringList* problems = nullptr);

    /// @brief Close the current project
    /// @param promptSave If true, saves the .klh file when it has unsaved changes
    /// @return true if project closed (or no project open), false if user cancelled
    bool closeProject(bool promptSave = true);

    /// @brief Save the project to its .klh file
    /// @return true if the file was written
    bool saveManifest();

    /// @brief Check if a project is currently open
    /// @return true if a project is open (ProjectMode), false otherwise
    bool isProjectOpen() const;

    // =========================================================================
    // Paths
    // =========================================================================

    /// @brief Get project root path
    /// @return Absolute path to project folder, empty if no project open
    QString getProjectPath() const;

    /// @brief Get path to .klh manifest file
    /// @return Absolute path to manifest file, empty if no project open
    QString getManifestPath() const;

    /// @brief Absolute path of the file of @p element; empty when it has none
    QString filePathOf(const ProjectElement& element) const;

    // =========================================================================
    // State Accessors
    // =========================================================================

    /// @brief Get current work mode
    /// @return Current WorkMode (NoDocument, ProjectMode, StandaloneMode)
    WorkMode getWorkMode() const;

    /// @brief The open project, or nullptr
    BookProject* project();
    const BookProject* project() const;

    /// @brief The first book of the open project, or nullptr
    ProjectBook* book();
    const ProjectBook* book() const;

    /// @brief Check if the project's .klh file has unsaved changes
    ///
    /// This tracks the structure and the data of the project ONLY (add/rename/move/delete
    /// of elements and changes of the book's data). It is reset by saveManifest(). Unsaved
    /// text of chapters is tracked per element (getDirtyElements()).
    bool isDirty() const;

    /// @brief Set the dirty state of the .klh file
    /// @param dirty true to mark it as dirty, false to mark it as clean
    void setDirty(bool dirty);

    // =========================================================================
    // Book Types
    // =========================================================================

    /// @brief Packages of book types and kinds the program has
    ///
    /// The built-in packages of the resources folder, loaded the first time they are needed.
    const BookTypeRegistry& bookTypes() const;

    /// @brief Load the packages in @p folders instead of the built-in ones
    ///
    /// Kinds of the packages loaded before (KindRef) are no longer valid.
    void loadBookTypes(const QStringList& folders);

    /// @brief Kind of @p element; none when its package is not installed
    KindRef kindOf(const ProjectElement& element) const;

    /// @brief Form of @p element: of its kind, or, when its package is not installed, a text
    /// element for a chapter file, a group for no file and a window element for other files
    ElementForm formOf(const ProjectElement& element) const;

    /// @brief Status of @p element, "draft" when it has none
    static QString statusOf(const ProjectElement& element);

    /// @brief Kinds that can be added to @p place of the book, or inside group @p groupId
    ///
    /// The kinds the project offers there, without those whose elements reached the kind's
    /// limit in the book.
    QList<KindRef> kindsFor(BookPlace place, const QString& groupId = QString()) const;

    /// @brief Text kinds of kindsFor()
    QList<KindRef> textKindsFor(BookPlace place, const QString& groupId = QString()) const;

    /// @brief Kind of a new chapter in @p place, or inside group @p groupId: the main text kind
    /// of the type when it can be there, else the first of textKindsFor()
    KindRef chapterKindFor(BookPlace place, const QString& groupId = QString()) const;

    /// @brief Kind of a new part: the first group kind the project offers in the main part
    KindRef partKind() const;

    /// @brief Title of a new element of @p kind: its default title in the language of the
    /// book, numbered after the elements of the kind the project has
    QString defaultTitle(const KindRef& kind) const;

    // =========================================================================
    // Elements
    // =========================================================================

    /// @brief Element @p elementId of the project, or nullptr
    ProjectElement* findElement(const QString& elementId);
    const ProjectElement* findElement(const QString& elementId) const;

    /// @brief Add a new element of @p kind with @p title to @p place of the book, or to group
    /// @p groupId
    ///
    /// A text element gets a chapter file of its own in the book's folder, with the text of
    /// its kind's template, and the status "draft". Saves the .klh file.
    /// @param index Place of the element in its list; -1: the place of its kind, newIndexIn()
    /// @return The new element's id; empty when the kind cannot be there or is a window kind,
    ///         the project has no group @p groupId, @p index is beyond the list, or a file
    ///         cannot be written
    QString addElement(const KindRef& kind, const QString& title, BookPlace place,
                       const QString& groupId = QString(), qsizetype index = -1);

    /// @brief Place in @p elements that a new element of @p kind takes when the writer does not
    /// choose it
    ///
    /// A kind of the start (a prologue) goes first, a kind of the end (an epilogue) last, and
    /// any other kind last before the elements of the kinds of the end: a new chapter goes
    /// before the epilogue.
    qsizetype newIndexIn(const QList<ProjectElement>& elements, const KindRef& kind) const;

    /// @brief Add a chapter or text file as a new text element of @p kind
    ///
    /// The file goes to the book's folder, named after its kind and a number; a text file
    /// (.txt) becomes a chapter, its lines the paragraphs. The element takes the place of its
    /// kind in its list, newIndexIn(). Saves the .klh file.
    /// @param sourcePath The chapter (.kchapter) or text file
    /// @param copy true to copy the file, false to move it
    /// @return The new element's id; empty when it cannot be added
    QString addFile(const QString& sourcePath, bool copy, const KindRef& kind,
                    const QString& title, BookPlace place, const QString& groupId = QString());

    /// @brief Give element @p elementId the title @p title and save the .klh file
    /// @return false when the project has no such element or the file cannot be written
    bool renameElement(const QString& elementId, const QString& title);

    /// @brief Take element @p elementId, with the elements inside it, out of the project and
    /// save the .klh file
    ///
    /// Their files stay in the project's folder.
    /// @return The element; nullopt when the project has no such element or the file cannot be
    ///         written
    std::optional<ProjectElement> removeElement(const QString& elementId);

    /// @brief Move element @p elementId to place @p index of its list and save the .klh file
    /// @return false when the element or the place does not exist or the file cannot be written
    bool moveElement(const QString& elementId, qsizetype index);

    /// @brief Place in its list that Move to Start gives element @p elementId
    ///
    /// The first place; an element of a kind that is not of the start goes after the elements
    /// of the kinds of the start, so a chapter moved to the start stays after the prologue.
    /// @return The index; -1 when the project has no such element
    qsizetype startIndexOf(const QString& elementId) const;

    /// @brief Place in its list that Move to End gives element @p elementId
    ///
    /// The last place; an element of a kind that is not of the end goes before the elements of
    /// the kinds of the end, so a chapter moved to the end stays before the epilogue.
    /// @return The index; -1 when the project has no such element
    qsizetype endIndexOf(const QString& elementId) const;

    /// @brief Set the status of a text element ("draft", "revision" or "final")
    ///
    /// Saves the .klh file and writes the status to the element's chapter file.
    bool setStatus(const QString& elementId, const QString& status);

    /// @brief Notes of a text element, from its chapter file
    QString notes(const QString& elementId) const;

    /// @brief Set the notes of a text element and write them to its chapter file
    bool setNotes(const QString& elementId, const QString& notes);

    /// @brief Words of a text element, as its chapter file counts them
    int wordCount(const QString& elementId) const;

    /// @brief Words and statuses of the text elements of @p elements and of their groups
    TextStatistics statisticsOf(const QList<ProjectElement>& elements) const;

    // =========================================================================
    // Text of Chapters
    // =========================================================================

    /// @brief Read the text (KML) of a text element from its chapter file
    /// @return The text; empty when the element or its file cannot be read
    QString loadChapterContent(const QString& elementId);

    /// @brief Keep @p kml as the unsaved text of a text element
    void setChapterContent(const QString& elementId, const QString& kml);

    /// @brief Forget the unsaved text of a text element; it is read from its file again
    void discardChapterContent(const QString& elementId);

    /// @brief Write the unsaved text of a text element to its chapter file
    /// @return true when it is written or there is nothing to write
    bool saveChapterContent(const QString& elementId);

    /// @brief Get all dirty elements
    /// @return IDs of the text elements whose text is not saved
    std::vector<QString> getDirtyElements() const;

    /// @brief Save all dirty elements
    /// @return true if all saved successfully
    bool saveAllDirty();

    /// @brief Get text elements whose status is not "final"
    /// @return Pairs (element ID, status), in the reading order of the book
    std::vector<std::pair<QString, QString>> getIncompleteElements() const;

    /// @brief Get the text elements of the book by status
    /// @return Map of status -> count
    std::map<QString, int> getStatusStatistics() const;

    // =========================================================================
    // Archive Operations
    // =========================================================================

    /// @brief Write the open project to a .klh.zip archive
    ///
    /// The archive has the files of the project's folder, with "/" in their names, so that it
    /// opens on every system. The database is brought up to date first, so its log files stay
    /// out (a log that cannot be written to the database goes with it), and so do the lock of
    /// the open project and the backups of its database: they serve the project only here.
    /// @param outputPath Path of the .klh.zip file
    /// @param progressCallback Optional callback for progress (0-100)
    /// @param problems What went wrong, one problem per line; may be nullptr
    /// @return true if the archive was written
    bool exportArchive(const QString& outputPath,
                       std::function<void(int)> progressCallback = nullptr,
                       QStringList* problems = nullptr);

    /// @brief Unpack a .klh.zip archive to a new folder and open the project in it
    ///
    /// The project goes to targetDir/<archiveProjectName()>, which must not exist yet. Entries
    /// that would land outside that folder are skipped, and so are the lock and the database's
    /// shared memory file that archives of earlier versions have. When the project cannot be
    /// unpacked or opened, the new folder is removed.
    /// @param archivePath Path of the .klh.zip file
    /// @param targetDir Folder in which the project's folder is made
    /// @param progressCallback Optional callback for progress (0-100)
    /// @param problems What went wrong, one problem per line; may be nullptr
    /// @return true if the project was unpacked and opened
    bool importArchive(const QString& archivePath,
                       const QString& targetDir,
                       std::function<void(int)> progressCallback = nullptr,
                       QStringList* problems = nullptr);

    /// @brief Name of the folder that importArchive() makes for @p archivePath: the archive's
    /// name without ".klh.zip"
    static QString archiveProjectName(const QString& archivePath);

signals:
    /// @brief Emitted when a project is successfully opened
    /// @param projectPath Absolute path to the project folder
    void projectOpened(const QString& projectPath);

    /// @brief Emitted when a project is about to close, while its database is still open
    ///
    /// Services holding the project database detach from it here.
    void projectAboutToClose();

    /// @brief Emitted when a project is closed
    void projectClosed();

    /// @brief Emitted when work mode changes
    /// @param mode New work mode
    void workModeChanged(WorkMode mode);

    /// @brief Emitted when dirty state changes
    /// @param dirty true if project has unsaved changes
    void dirtyStateChanged(bool dirty);

private:
    /// @brief What the program keeps of a text element while it works, outside the .klh file
    struct ElementState {
        QString content;       ///< Text (KML) read from the file or set by the editor
        bool loaded = false;   ///< content holds the text
        bool dirty = false;    ///< content is not saved
        int wordCount = 0;     ///< Words, as the chapter file counts them
        QString notes;         ///< Notes of the chapter file
    };

    /// @brief Private constructor (singleton)
    ProjectManager();

    /// @brief Private destructor
    ~ProjectManager();

    /// @brief Set work mode and emit signal
    /// @param mode New work mode
    void setWorkMode(WorkMode mode);

    /// @brief Read the word counts and notes of the text elements from their chapter files
    void loadElementStates();

    /// @brief State of text element @p elementId; nullptr when it is not one
    ElementState* stateOf(const QString& elementId);

    /// @brief Write the title, status and notes of @p element to its chapter file, and
    /// @p kml when given; the file keeps its other data
    bool writeChapterFile(const ProjectElement& element, const std::optional<QString>& kml);

    /// @brief A free chapter file for a new element of @p kind in @p place, relative to the
    /// project: <book folder>/<kind>_001.kchapter, or workshop/<kind>_001.kchapter
    QString newChapterFile(const KindRef& kind, BookPlace place) const;

    /// @brief The text a new element of @p kind starts with: its template's, with the title
    /// and the author of @p book in place of "{title}" and "{author}"; empty without a template
    static QString startingText(const KindRef& kind, const ProjectBook& book);

    /// @brief List that a new element of @p kind goes to: @p place of the book, or group
    /// @p groupId; nullptr, with the reason in the log, when the kind cannot be there
    QList<ProjectElement>* listForNew(const KindRef& kind, BookPlace place,
                                      const QString& groupId);

    // =========================================================================
    // Member Variables
    // =========================================================================

    WorkMode m_workMode;                          ///< Current work mode
    std::unique_ptr<BookProject> m_project;       ///< Open project
    QHash<QString, ElementState> m_elementStates; ///< Text element id -> its state
    std::filesystem::path m_projectPath;          ///< Project root folder path
    std::filesystem::path m_manifestPath;         ///< Path to .klh manifest file
    bool m_isDirty;                               ///< The .klh file has unsaved changes

    mutable BookTypeRegistry m_bookTypes;         ///< Packages of book types and kinds
    mutable bool m_bookTypesLoaded = false;       ///< m_bookTypes was loaded

    // OpenSpec #00041: SQLite Project Database
    std::unique_ptr<ProjectLock> m_projectLock;      ///< Project lock (prevents multiple instances)
    std::unique_ptr<ProjectDatabase> m_database;     ///< SQLite database for metadata
    std::unique_ptr<BackupManager> m_backupManager;  ///< Database backup manager

public:
    /// @brief Get project database (OpenSpec #00041)
    /// @return Pointer to database, nullptr if no project open
    ProjectDatabase* getDatabase() { return m_database.get(); }
    const ProjectDatabase* getDatabase() const { return m_database.get(); }
};

} // namespace core
} // namespace kalahari
