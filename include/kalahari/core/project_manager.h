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
#include <QTemporaryDir>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "kalahari/core/archive_problem.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/package_card.h"
// OpenSpec #00041: SQLite Project Database
#include "kalahari/core/project_database.h"
#include "kalahari/core/project_lock.h"
#include "kalahari/core/backup_manager.h"
#include "kalahari/core/text_statistics.h"

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

/// @brief Why a book cannot be opened
enum class OpenFailure {
    None,           ///< Nothing keeps it from opening
    Missing,        ///< There is no such .klh file
    NotBook,        ///< The file is not a .klh file
    Newer,          ///< A newer version of the program saved it
    Older,          ///< An older version of the program saved it, in a format it cannot read
    Unreadable,     ///< The .klh file is damaged or incomplete
    OpenElsewhere,  ///< Another program that is running has it open
    Database,       ///< The database of the book cannot be opened
    CloseFailed     ///< The open book cannot be closed for it
};

/// @brief Words and statuses of the text elements of a list, with those inside its groups
struct TextStatistics {
    int elements = 0;                 ///< Text elements (chapters, prefaces, poems...)
    int words = 0;                    ///< Their words
    std::map<QString, int> statuses;  ///< Status ("draft", "revision", "final") -> elements
    std::map<QString, int> kinds;     ///< Kind ("kalahari.base:chapter", KindRef::reference())
                                      ///< -> elements
};

/// @brief A chapter file in the folder of the book or of the Workshop that no element has: the
/// file of an element deleted from the book without its file, or a file the writer put there
struct UnassignedFile {
    QString file;   ///< File, relative to the project folder: "book/chapter_004.kchapter"
    QString title;  ///< Title the file keeps; empty when it has none or cannot be read
};

/// @brief What the writer chose in the palette for a user project: the kinds the project
/// offers and the elements it starts with
struct PaletteChoice {
    QList<KindRef> kinds;          ///< Kinds of the project, in the order of the palette
    QList<StartElement> elements;  ///< Elements it starts with ("Create now"), each of one of
                                   ///< the kinds, in a place of the book the kind allows
};

/// @brief A new project, as ProjectManager::createProject() makes it
struct NewProject {
    QString parentDir;            ///< Folder of the project, or the folder it is made in
    QString title;                ///< Title of the book; it also names the folder and the file
    QString author;               ///< Author of the book
    QString language;             ///< Language code of the text (e.g., "en", "pl")
    bool createSubfolder = true;  ///< Make the project's folder, named after the title, in
                                  ///< parentDir
    QString typeId;               ///< Package of the book type, e.g. "kalahari.novel"; empty: a
                                  ///< user project
    /// Whether the Navigator shows the sections of the book, and their names; none: as the
    /// type says (a user project shows none), with the first set of names
    std::optional<BookSections> sections;
    /// A user project's kinds and the elements it starts with; none: the kinds of the base
    /// package, and no elements
    std::optional<PaletteChoice> palette;
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

    /// @brief Package whose kinds a user project has when the writer skips the palette
    static constexpr const char* BASE_PACKAGE = "kalahari.base";

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
    /// @param sections Whether the Navigator shows the sections of the book, and their names;
    ///        none: as the type says (a user project shows none), with the first set of names
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
                       QStringList* problems = nullptr,
                       const std::optional<BookSections>& sections = std::nullopt);

    /// @brief Create a new project and open it, as the other createProject() does, also a user
    /// project with the kinds and the elements of the palette
    /// @param project The project; a book of a type takes no palette
    /// @param problems Gets what is wrong when the project cannot be created, one line each
    /// @return false when the project cannot be created; the open project stays open when the
    ///         project is wrong (newProjectProblems())
    ///
    /// The elements of the palette are made in its order: the groups first, then the text
    /// elements, each in its place of the book where its kind puts a new element (a prologue
    /// at the start). A text element that can be inside a group made with it goes into the
    /// first such group of its place ("Part I" with "Chapter 1"); one that stands at the start
    /// or at the end of its place (a prologue, an epilogue) does not. Elements of kinds that
    /// open in a window of their own wait until the program has the window.
    bool createProject(const NewProject& project, QStringList* problems = nullptr);

    /// @brief Create a new project without opening it: the open project stays open
    /// @param project The project, as createProject() takes it
    /// @param problems Gets what is wrong when the project cannot be created, one line each
    /// @return The project's .klh file; empty when the project cannot be created, and then
    ///         nothing of it is left in the folder
    QString createProjectWithoutOpening(const NewProject& project,
                                        QStringList* problems = nullptr) const;

    /// @brief What keeps @p project from being created: the book has no title, the type is
    /// not installed, the palette is wrong or the folder exists and is not empty
    /// @return One line per problem; empty when the project can be created
    QStringList newProjectProblems(const NewProject& project) const;

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

    /// @brief What keeps openProject() from reading the project file @p manifestPath: the
    /// file does not exist, it is not a .klh file or its content is wrong (e.g. an older
    /// format)
    /// @return One line per problem; empty when the file can be read
    static QStringList projectFileProblems(const QString& manifestPath);

    /// @brief What keeps openProject() from opening the book @p manifestPath, checked before
    /// the open book is closed for it: the problems of its file (projectFileProblems()), or
    /// another program that is running and has the book open
    /// @param problems Gets what is wrong, one line each
    static OpenFailure openFailure(const QString& manifestPath, QStringList* problems = nullptr);

    /// @brief Open an existing project from its .klh file
    /// @param manifestPath Path to the .klh file
    /// @param problems Gets what is wrong when the project cannot be opened, one line each
    /// @return true if project opened successfully, false otherwise (lastOpenFailure() says
    ///         why); the open project stays open when the file cannot be read or another
    ///         program has the book open (openFailure())
    bool openProject(const QString& manifestPath, QStringList* problems = nullptr);

    /// @brief Why the last openProject() did not open the book; None after it opened it
    OpenFailure lastOpenFailure() const { return m_lastOpenFailure; }

    /// @brief Convert the book @p manifestPath, saved by an older version of Kalahari in the
    /// first format (OpenFailure::Older), to the format of this version (OldBookConversion)
    ///
    /// Only the .klh file changes, and the old one stays beside it as a copy
    /// (OldBookConversion::copyPathFor()). Nothing changes when the book cannot be converted
    /// or another program that is running has it open. The book is not opened.
    /// @param copyPath Gets the copy of the old .klh file
    /// @param otherFiles Gets the titles of the elements that went among the files of the
    ///        book outside its text
    /// @param problems Gets why the book cannot be converted, one line each
    /// @return true when the book is converted
    bool convertOlderBook(const QString& manifestPath, QString* copyPath = nullptr,
                          QStringList* otherFiles = nullptr,
                          QStringList* problems = nullptr) const;

    /// @brief Close the current project
    /// @param promptSave If true, saves the .klh file when it has unsaved changes
    /// @return true if project closed (or no project open), false if user cancelled
    bool closeProject(bool promptSave = true);

    /// @brief Save the project to its .klh file
    /// @return true if the file was written; false also when the book is open read-only
    bool saveManifest();

    /// @brief Check if a project is currently open
    /// @return true if a project is open (ProjectMode), false otherwise
    bool isProjectOpen() const;

    /// @brief Whether the open book is open read-only: the program does not have a package it
    /// uses (missingPackages())
    ///
    /// The book can be read and exported (exportArchive()), but not changed: the methods that
    /// change it do nothing and report so, and nothing is written into its folder, neither
    /// its .klh file nor its chapters nor the cards of its packages.
    bool isReadOnly() const;

    /// @brief Packages of the open book that the program does not have, or has without a kind
    /// the book names (BookProject::missingPackages()), sorted; empty when it has them all or
    /// no book is open
    const QStringList& missingPackages() const;

    /// @brief The card of package @p packageId that the open book keeps (package_card.h), or
    /// nullptr when it has none or no book is open
    const PackageCard* packageCard(const QString& packageId) const;

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

    /// @brief Packages of book types and kinds the program has, which the open book works with:
    /// installedBookTypes()
    ///
    /// Where the program does not have a package of the book, the book opens read-only
    /// (isReadOnly()) and shows its type and kinds after its cards (packageCard()).
    const BookTypeRegistry& bookTypes() const;

    /// @brief Packages of book types and kinds installed with the program
    ///
    /// The built-in packages of the resources folder, loaded the first time they are needed.
    /// The New Book window, the palette, new books and the open book take their types and
    /// kinds from here.
    const BookTypeRegistry& installedBookTypes() const;

    /// @brief Load the packages in @p folders instead of the built-in ones
    ///
    /// Kinds of the packages loaded before (KindRef) are no longer valid. Whether the open book
    /// is read-only is decided when it opens.
    void loadBookTypes(const QStringList& folders);

    /// @brief Kind of @p element; none when its package is not installed
    KindRef kindOf(const ProjectElement& element) const;

    /// @brief Form of @p element: of its kind, or, when its package is not installed, a text
    /// element for a chapter file, a group for no file and a window element for other files
    ElementForm formOf(const ProjectElement& element) const;

    /// @brief Status of @p element, "draft" when it has none
    static QString statusOf(const ProjectElement& element);

    /// @brief Whether @p status is a status of text elements ("draft", "revision", "final"):
    /// an element added from a chapter file takes the file's status only then
    static bool isTextStatus(const QString& status);

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

    /// @brief The main text kind of the book (chapter, story, poem, act): the main text kind of
    /// its type; in a project without a type, the first text kind it offers in the main part
    ///
    /// The program's sentences name the texts of the book after it ("Stories: 12").
    KindRef mainTextKind() const;

    /// @brief Title of a new element of @p kind: its default title in the language of the
    /// book, numbered after the elements of the kind the project has
    QString defaultTitle(const KindRef& kind) const;

    /// @brief A free chapter file for a new element of @p kind in @p place, relative to the
    /// project: <book folder>/<kind>_001.kchapter, or workshop/<kind>_001.kchapter; the file
    /// that addElement() gives it
    QString newChapterFile(const KindRef& kind, BookPlace place) const;

    /// @brief A free chapter file for a new file of the book outside its text, relative to the
    /// project: <book folder>/file_001.kchapter; the file that addBookFile() gives it
    QString newBookFile() const;

    // =========================================================================
    // Elements
    // =========================================================================

    /// @brief Element @p elementId of the project, or nullptr
    ProjectElement* findElement(const QString& elementId);
    const ProjectElement* findElement(const QString& elementId) const;

    /// @brief The element of the project whose file is @p path (absolute), also when the path
    /// names it in another way (another case of letters on Windows, a link); nullptr when no
    /// element has it
    const ProjectElement* elementWithFile(const QString& path) const;

    /// @brief Add a new element of @p kind with @p title to @p place of the book, or to group
    /// @p groupId
    ///
    /// A text element gets a chapter file of its own in the book's folder, with @p text or
    /// else the text of its kind's template, and the status "draft". A group can take elements
    /// of the book inside it, at its end, in the order given: a new part takes the epilogue
    /// from the end of the last part, so that the chapters added to the new part go before it.
    /// Saves the .klh file.
    /// @param index Place of the element in its list; -1: the place of its kind, newPlaceOf(),
    ///        which can be in a group of the body of the book
    /// @param takeInside Ids of elements that the new group takes inside it
    /// @param text The text of a text element (KML), such as the text of a new file that the
    ///        writer adds to the book; none: the text of its kind's template
    /// @return The new element's id; empty when the kind cannot be there or is a window kind,
    ///         the project has no group @p groupId, @p index is beyond the list, an element of
    ///         @p takeInside cannot be inside the group, or a file cannot be written
    QString addElement(const KindRef& kind, const QString& title, BookPlace place,
                       const QString& groupId = QString(), qsizetype index = -1,
                       const QStringList& takeInside = {},
                       const std::optional<QString>& text = std::nullopt);

    /// @brief Whether @p elementId is a file of the book outside its text (ProjectBook::files)
    bool isBookFile(const QString& elementId) const;

    /// @brief Add an empty text file with @p title to the files of the book outside its text
    ///
    /// The file, such as notes or a text pasted from elsewhere, gets a chapter file of its own
    /// in the book's folder (newBookFile()), but no kind and no status: it is not an element of
    /// the text, and the finished book does not have it. Saves the .klh file.
    /// @return The new file's id; empty when no project is open or a file cannot be written
    QString addBookFile(const QString& title);

    /// @brief Place in @p elements that a new element of @p kind takes when the writer does not
    /// choose it
    ///
    /// A kind of the start (a prologue) goes first, a kind of the end (an epilogue) last, and
    /// any other kind last before the elements of the kinds of the end: a new chapter goes
    /// before the epilogue.
    qsizetype newIndexIn(const QList<ProjectElement>& elements, const KindRef& kind) const;

    /// @brief Place that a new element of @p kind takes in @p place of the book, or in group
    /// @p groupId, when the writer does not choose it
    ///
    /// The place of its kind in the list, newIndexIn(). In the body of the book, an element of
    /// a kind with no position that is not a group goes before the elements that close the
    /// body, also when they are at the end of a part: a new chapter goes before the epilogue
    /// that ends the last part, if the chapter can be in that part.
    ElementPlace newPlaceOf(const KindRef& kind, BookPlace place,
                            const QString& groupId = QString()) const;

    /// @brief Add a chapter or text file as a new text element of @p kind
    ///
    /// The file goes to the book's folder, named after its kind and a number; a text file
    /// (.txt) becomes a chapter, its lines the paragraphs. Saves the .klh file.
    /// @param sourcePath The chapter (.kchapter) or text file
    /// @param copy true to copy the file, false to move it
    /// @param index Place of the element in its list; -1: the place of its kind, newPlaceOf(),
    ///        which can be in a group of the body of the book
    /// @return The new element's id; empty when it cannot be added
    QString addFile(const QString& sourcePath, bool copy, const KindRef& kind,
                    const QString& title, BookPlace place, const QString& groupId = QString(),
                    qsizetype index = -1);

    /// @brief Give element @p elementId the title @p title and save the .klh file
    /// @return false when the project has no such element or the file cannot be written
    bool renameElement(const QString& elementId, const QString& title);

    /// @brief Show the sections of the book in the Navigator or not, and name them
    /// (ProjectBook::setSections()); saves the .klh file
    /// @return false when no project is open or the file cannot be written; the book keeps
    ///         its sections then
    bool setSections(const BookSections& sections);

    /// @brief Take element @p elementId, with the elements inside it, out of the project and
    /// save the .klh file
    ///
    /// Their files stay in the project's folder: unassignedFiles() has their chapter files,
    /// and filesOf() of the element gives them to whoever deletes them.
    /// @return The element; nullopt when the project has no such element or the file cannot be
    ///         written
    std::optional<ProjectElement> removeElement(const QString& elementId);

    /// @brief Files of @p element and of the elements inside it that are on the disk, as
    /// absolute paths in reading order
    ///
    /// A file that an element outside them also has is left out, so deleting the files
    /// deletes no file of another element.
    QStringList filesOf(const ProjectElement& element) const;

    /// @brief Chapter files (.kchapter) in the folders of the book and of the Workshop that no
    /// element of the project has, by folder and name
    ///
    /// Other files there (pictures, materials) are the writer's, and so are the folders in
    /// them: they are left out.
    QList<UnassignedFile> unassignedFiles() const;

    /// @brief Whether unassignedFiles() lists file @p path (absolute) when no element has it:
    /// a chapter file in the folder of the book or of the Workshop
    bool listsAsUnassigned(const QString& path) const;

    /// @brief Add the chapter file @p file, which no element has, as a new text element of
    /// @p kind with @p title in @p place of the book, or in group @p groupId
    ///
    /// The file stays where it is and takes the title; the element takes the status and the
    /// notes of the file. Saves the .klh file.
    /// @param file The file, relative to the project folder (UnassignedFile::file)
    /// @param index Place of the element in its list; -1: the place of its kind, newPlaceOf(),
    ///        which can be in a group of the body of the book
    /// @return The new element's id; empty when the file is not one of unassignedFiles() or
    ///         cannot be read, the kind cannot be there or is not a text kind, or the .klh file
    ///         cannot be written. The file is never removed.
    QString addUnassignedFile(const QString& file, const KindRef& kind, const QString& title,
                              BookPlace place, const QString& groupId = QString(),
                              qsizetype index = -1);

    /// @brief Move element @p elementId to place @p index of its list and save the .klh file
    /// @return false when the element or the place does not exist or the file cannot be written
    bool moveElement(const QString& elementId, qsizetype index);

    /// @brief Places that element @p elementId can be moved to, in reading order
    /// (BookProject::movePlacesOf()): wherever its kind can be in the book, in the book without
    /// the element; the place it has now is one of them
    QList<ElementPlace> movePlacesOf(const QString& elementId) const;

    /// @brief Whether element @p elementId can be moved to @p to, one of movePlacesOf()
    bool canMoveTo(const QString& elementId, const ElementPlace& to) const;

    /// @brief Move element @p elementId, with the elements inside it, to @p to and save the
    /// .klh file
    ///
    /// The element goes to another section of the book or into another group, or to another
    /// place of its list, wherever its kind can be. Its files stay where they are.
    /// @param to A place in the book without the element, one of movePlacesOf()
    /// @return false when the element cannot go there or the file cannot be written; the
    ///         project stays as it was then
    bool moveElementTo(const QString& elementId, const ElementPlace& to);

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
    /// Saves the .klh file and writes the status to the element's chapter file. A file of the
    /// book outside its text has no status.
    bool setStatus(const QString& elementId, const QString& status);

    /// @brief Notes of a text element, from its chapter file
    QString notes(const QString& elementId) const;

    /// @brief Set the notes of a text element and write them to its chapter file
    bool setNotes(const QString& elementId, const QString& notes);

    /// @brief Words of a text element under the rules of the settings (wordCountRules()):
    /// of its text in the editor while its changes are not saved (setEditorWords()), else of
    /// the text of its chapter file; new rules count at once, without reading the file again
    int wordCount(const QString& elementId) const;

    /// @brief Words of the text of a text element in its editor, with changes that are not
    /// saved: the counts of the book show them until the text is saved or its changes are
    /// discarded
    void setEditorWords(const QString& elementId, const WordTally& words);

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

    /// @brief Write the title, the status and the notes of a text element to its chapter file
    /// when the file has others, keeping its text
    ///
    /// A book made before Kalahari wrote them to the files has others there; the file that
    /// stays when the element leaves the book takes the element's own.
    /// @return true when they are written, the file has them or the element has no file
    bool saveChapterDetails(const QString& elementId);

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

    /// @brief The backups of the database of the open project, newest first: those where
    /// the Folders page of the Settings keeps them, and those that stayed in the project's own
    /// .backups folder before a common folder was chosen
    QStringList databaseBackups() const;

    /// @brief Write the open project to a .klh.zip archive
    ///
    /// The archive has the files of the project's folder, with "/" in their names, so that it
    /// opens on every system. The database is brought up to date first, so its log files stay
    /// out (a log that cannot be written to the database goes with it), and so does the lock
    /// of the open project, which serves the project only here. The backups of the database
    /// go only when @p withBackups says so, to the .backups folder of the archive, wherever
    /// they are kept (databaseBackups()).
    /// @param outputPath Path of the .klh.zip file
    /// @param progressCallback Optional callback for progress (0-100)
    /// @param problem Gets what went wrong; may be nullptr
    /// @param withBackups Whether the backups of the database go to the archive
    /// @return true if the archive was written
    bool exportArchive(const QString& outputPath,
                       const std::function<void(int)>& progressCallback = nullptr,
                       ArchiveProblem* problem = nullptr, bool withBackups = false);

    /// @brief Read the project of a .klh.zip archive without unpacking it: its .klh file, the
    /// one unpackArchive() finds at the top of the project
    /// @param archivePath Path of the .klh.zip file
    /// @param problem Gets what went wrong, e.g. that the book of the archive is of a newer
    /// format, saved by a newer version of Kalahari; may be nullptr
    /// @param backups Gets the number of the backups of the database in the archive; may be
    /// nullptr
    /// @return The project, or nullopt when the archive or its .klh file cannot be read
    static std::optional<BookProject> readArchive(const QString& archivePath,
                                                  ArchiveProblem* problem = nullptr,
                                                  int* backups = nullptr);

    /// @brief Unpack the cards of the packages of the book of a .klh.zip archive (its folder
    /// types/) to folder @p targetDir, as the folder of the book has them
    ///
    /// The book of the archive is shown before it is imported with these cards where the
    /// program does not have its packages (PackageCards::read()). An archive of an earlier
    /// version has none. Entries that would land outside @p targetDir are skipped.
    /// @return false when the archive cannot be read or a file cannot be written
    static bool unpackArchiveCards(const QString& archivePath, const QString& targetDir);

    /// @brief Unpack a .klh.zip archive to a new folder, without opening the project in it
    ///
    /// The project goes to targetDir/<archiveProjectName()>, which must not exist yet. Entries
    /// that would land outside that folder are skipped, and so are the lock and the database's
    /// shared memory file that archives of earlier versions have. When the project cannot be
    /// unpacked or its .klh file cannot be read, the new folder is removed. The backups of the
    /// database in the archive keep their times and go where the Folders page of the Settings
    /// keeps the backups of the project.
    /// @param archivePath Path of the .klh.zip file
    /// @param targetDir Folder in which the project's folder is made
    /// @param progressCallback Optional callback for progress (0-100)
    /// @param problem Gets what went wrong; may be nullptr
    /// @return Path of the .klh file of the unpacked project; empty when it was not unpacked
    static QString unpackArchive(const QString& archivePath,
                                 const QString& targetDir,
                                 const std::function<void(int)>& progressCallback = nullptr,
                                 ArchiveProblem* problem = nullptr);

    /// @brief Unpack a .klh.zip archive to a new folder and open the project in it
    ///
    /// The project is unpacked as unpackArchive() does it. When it cannot be opened, the new
    /// folder is removed.
    /// @param archivePath Path of the .klh.zip file
    /// @param targetDir Folder in which the project's folder is made
    /// @param progressCallback Optional callback for progress (0-100)
    /// @param problem Gets what went wrong; may be nullptr
    /// @return true if the project was unpacked and opened
    bool importArchive(const QString& archivePath,
                       const QString& targetDir,
                       const std::function<void(int)>& progressCallback = nullptr,
                       ArchiveProblem* problem = nullptr);

    /// @brief Name of the folder that unpackArchive() makes for @p archivePath: the archive's
    /// name without ".klh.zip"
    static QString archiveProjectName(const QString& archivePath);

    /// @brief Extension of the archives of books
    static constexpr const char* ARCHIVE_EXTENSION = ".klh.zip";

    /// @brief Name of the archive of book @p title: the title, without the characters that file
    /// systems do not allow, and ".klh.zip"
    static QString archiveFileName(const QString& title);

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
        WordTally words;       ///< Words of the text of the chapter file
        /// Words of the text in its editor, with changes that are not saved
        std::optional<WordTally> editorWords;
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

    /// @brief Words of text element @p elementId under @p rules, as wordCount() gives them
    int wordsUnder(const QString& elementId, const WordCountRules& rules) const;

    /// @brief Write the title, status and notes of @p element to its chapter file, and
    /// @p kml when given; the file keeps its other data
    bool writeChapterFile(const ProjectElement& element, const std::optional<QString>& kml);

    /// @brief Folders of the files of unassignedFiles(): of the book and of the Workshop, as
    /// absolute paths
    QStringList unassignedFolders() const;

    /// @brief Absolute paths of the files of unassignedFiles()
    QStringList unassignedPaths() const;

    /// @brief The text a new element of @p kind starts with: its template's, with the title
    /// and the author of @p book in place of "{title}" and "{author}"; empty without a template
    static QString startingText(const KindRef& kind, const ProjectBook& book);

    /// @brief Remove what was made of a new project that cannot be created: its folder goes
    /// back to how it was, empty when it existed (@p folderExisted), else it is removed
    static void removeNewProject(const QString& projectDir, bool folderExisted);

    /// @brief Whether the open book is read-only, so that @p what does not change it; the log
    /// says so
    bool refusesChange(const char* what) const;

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
    OpenFailure m_lastOpenFailure = OpenFailure::None;  ///< Of the last openProject()

    mutable BookTypeRegistry m_bookTypes;         ///< Installed packages of book types and kinds
    mutable bool m_bookTypesLoaded = false;       ///< m_bookTypes was loaded
    QStringList m_missingPackages;                ///< Of the open book: it is read-only
    QHash<QString, PackageCard> m_packageCards;   ///< Cards of the open book, by package id

    // OpenSpec #00041: SQLite Project Database. A book open read-only has no lock and no
    // backups, and its database is a copy in a temporary folder: nothing is written into its
    // folder (the temporary folder goes after the database, which is declared after it)
    std::unique_ptr<ProjectLock> m_projectLock;      ///< Project lock (prevents multiple instances)
    std::unique_ptr<QTemporaryDir> m_databaseCopy;   ///< Of a book open read-only: its database
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
