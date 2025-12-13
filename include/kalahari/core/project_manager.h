/// @file project_manager.h
/// @brief Project management system for Solution-like architecture
///
/// ProjectManager is a singleton that manages the project lifecycle,
/// including creating project folder structures, loading/saving .klh manifests,
/// and tracking work modes (Project vs Standalone).
///
/// OpenSpec #00033: Project File System - Solution-like Architecture
///
/// @example
/// @code
/// auto& pm = ProjectManager::getInstance();
///
/// // Create new project
/// pm.createProject("E:/Books/MyNovel", "My Novel", "John Doe", "en");
///
/// // Open existing project
/// pm.openProject("E:/Books/MyNovel/MyNovel.klh");
///
/// // Get paths
/// QString contentPath = pm.getContentPath();
/// QString metadataPath = pm.getMetadataPath();
///
/// // Close project
/// pm.closeProject();
/// @endcode

#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <memory>
#include <filesystem>
#include <vector>
#include <functional>

namespace kalahari {
namespace core {

// Forward declarations
class Document;
class Book;
class Part;
class BookElement;

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

/// @brief Singleton project manager for Solution-like architecture
///
/// Manages the project lifecycle:
/// - Creating new projects (folder structure + manifest)
/// - Opening existing projects (reading .klh JSON manifest)
/// - Closing projects (cleanup, save prompts)
/// - Path resolution (content/, metadata/, .kalahari/)
///
/// Signals:
/// - projectOpened: Emitted when project is successfully opened
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

    /// @brief Create a new project with folder structure
    /// @param parentDir Parent directory where project folder will be created
    /// @param title Project/book title (used for folder and manifest name)
    /// @param author Author name
    /// @param language ISO 639-1 language code (e.g., "en", "pl")
    /// @param createSubfolder If true, creates title subfolder in parentDir
    /// @return true if project created successfully, false otherwise
    ///
    /// Creates folder structure:
    /// - content/frontmatter/, content/body/, content/backmatter/
    /// - metadata/
    /// - mindmaps/, timelines/, resources/
    /// - .kalahari/cache/, .kalahari/backup/, .kalahari/recovery/
    /// - ProjectName.klh (JSON manifest)
    bool createProject(const QString& parentDir,
                       const QString& title,
                       const QString& author,
                       const QString& language,
                       bool createSubfolder = true);

    /// @brief Open an existing project from .klh manifest
    /// @param manifestPath Path to the .klh manifest file
    /// @return true if project opened successfully, false otherwise
    ///
    /// Reads JSON manifest, validates structure, sets work mode to ProjectMode.
    bool openProject(const QString& manifestPath);

    /// @brief Close the current project
    /// @param promptSave If true, prompts user to save unsaved changes
    /// @return true if project closed (or no project open), false if user cancelled
    ///
    /// Clears project state, sets work mode to NoDocument.
    bool closeProject(bool promptSave = true);

    /// @brief Save the project manifest to .klh file
    /// @return true if manifest saved successfully, false otherwise
    ///
    /// Writes Document::toJson() to the .klh file.
    bool saveManifest();

    /// @brief Check if a project is currently open
    /// @return true if a project is open (ProjectMode), false otherwise
    bool isProjectOpen() const;

    // =========================================================================
    // Path Helpers
    // =========================================================================

    /// @brief Get project root path
    /// @return Absolute path to project folder, empty if no project open
    QString getProjectPath() const;

    /// @brief Get path to .klh manifest file
    /// @return Absolute path to manifest file, empty if no project open
    QString getManifestPath() const;

    /// @brief Get path to content folder
    /// @return Absolute path to content/, empty if no project open
    QString getContentPath() const;

    /// @brief Get path to frontmatter folder
    /// @return Absolute path to content/frontmatter/, empty if no project open
    QString getFrontmatterPath() const;

    /// @brief Get path to body folder
    /// @return Absolute path to content/body/, empty if no project open
    QString getBodyPath() const;

    /// @brief Get path to backmatter folder
    /// @return Absolute path to content/backmatter/, empty if no project open
    QString getBackmatterPath() const;

    /// @brief Get path to metadata folder
    /// @return Absolute path to metadata/, empty if no project open
    QString getMetadataPath() const;

    /// @brief Get path to mindmaps folder
    /// @return Absolute path to mindmaps/, empty if no project open
    QString getMindmapsPath() const;

    /// @brief Get path to timelines folder
    /// @return Absolute path to timelines/, empty if no project open
    QString getTimelinesPath() const;

    /// @brief Get path to resources folder
    /// @return Absolute path to resources/, empty if no project open
    QString getResourcesPath() const;

    /// @brief Get path to .kalahari IDE folder
    /// @return Absolute path to .kalahari/, empty if no project open
    QString getKalahariPath() const;

    /// @brief Get path to cache folder
    /// @return Absolute path to .kalahari/cache/, empty if no project open
    QString getCachePath() const;

    /// @brief Get path to backup folder
    /// @return Absolute path to .kalahari/backup/, empty if no project open
    QString getBackupPath() const;

    /// @brief Get path to recovery folder
    /// @return Absolute path to .kalahari/recovery/, empty if no project open
    QString getRecoveryPath() const;

    // =========================================================================
    // State Accessors
    // =========================================================================

    /// @brief Get current work mode
    /// @return Current WorkMode (NoDocument, ProjectMode, StandaloneMode)
    WorkMode getWorkMode() const;

    /// @brief Get current document
    /// @return Pointer to current Document, nullptr if no document open
    Document* getDocument();

    /// @brief Get current document (const)
    /// @return Const pointer to current Document, nullptr if no document open
    const Document* getDocument() const;

    /// @brief Check if project has unsaved changes
    /// @return true if dirty, false otherwise
    bool isDirty() const;

    /// @brief Set dirty state
    /// @param dirty true to mark as dirty, false to mark as clean
    void setDirty(bool dirty);

    // =========================================================================
    // Book Structure Management
    // =========================================================================

    /// @brief Load book structure from manifest JSON
    /// @param structureObj The "structure" object from manifest
    /// @return true if successful
    ///
    /// Parses the structure section of manifest:
    /// - "frontmatter" array -> BookElements in Book::frontMatter
    /// - "body" array -> Parts with chapters in Book::body
    /// - "backmatter" array -> BookElements in Book::backMatter
    bool loadStructureFromManifest(const QJsonObject& structureObj);

    /// @brief Serialize book structure to manifest JSON
    /// @return "structure" QJsonObject for manifest
    ///
    /// Creates JSON structure:
    /// - "frontmatter": array of element objects
    /// - "body": array of part objects with chapters
    /// - "backmatter": array of element objects
    QJsonObject saveStructureToManifest() const;

    /// @brief Load chapter content from RTF file
    /// @param elementId Chapter/element ID
    /// @return Content string, empty if failed
    ///
    /// Loads RTF content from the file specified in element's file path.
    /// Uses project path to resolve relative paths.
    QString loadChapterContent(const QString& elementId);

    /// @brief Save chapter content to RTF file
    /// @param elementId Chapter/element ID
    /// @return true if saved successfully
    ///
    /// Saves element's cached content to RTF file.
    /// Creates parent directories if needed.
    bool saveChapterContent(const QString& elementId);

    /// @brief Find element by ID across all sections
    /// @param elementId Element ID to find
    /// @return Pointer to element, nullptr if not found
    ///
    /// Searches frontmatter, body (all parts), and backmatter.
    BookElement* findElement(const QString& elementId);

    /// @brief Find part by ID
    /// @param partId Part ID to find
    /// @return Pointer to part, nullptr if not found
    Part* findPart(const QString& partId);

    /// @brief Get all dirty elements
    /// @return Vector of IDs of elements with unsaved changes
    ///
    /// Returns IDs of all elements where isDirty() == true.
    std::vector<QString> getDirtyElements() const;

    /// @brief Save all dirty elements
    /// @return true if all saved successfully
    ///
    /// Iterates all dirty elements and calls saveChapterContent().
    bool saveAllDirty();

    // =========================================================================
    // Archive Operations
    // =========================================================================

    /// @brief Export project to .klh.zip archive
    /// @param outputPath Path to output .klh.zip file
    /// @param progressCallback Optional callback for progress (0-100)
    /// @return true if export succeeded, false otherwise
    ///
    /// Creates ZIP archive containing entire project folder structure.
    /// Excludes .kalahari/ cache folder (backup, cache, recovery).
    bool exportArchive(const QString& outputPath,
                       std::function<void(int)> progressCallback = nullptr);

    /// @brief Import project from .klh.zip archive
    /// @param archivePath Path to .klh.zip file
    /// @param targetDir Directory where project will be extracted
    /// @param progressCallback Optional callback for progress (0-100)
    /// @return true if import succeeded and project opened, false otherwise
    ///
    /// Extracts archive to targetDir/<archive_name>/ folder.
    /// Automatically opens the extracted project on success.
    bool importArchive(const QString& archivePath,
                       const QString& targetDir,
                       std::function<void(int)> progressCallback = nullptr);

    /// @brief Add a new chapter to a section
    /// @param sectionType "frontmatter", "body", or "backmatter"
    /// @param partId Part ID (only used if sectionType is "body")
    /// @param title Chapter title
    /// @param sourceFilePath Source file to copy/move
    /// @param copyFile true to copy, false to move
    /// @return Element ID if successful, empty string if failed
    ///
    /// Copies/moves the source file to the appropriate project folder,
    /// creates a BookElement, adds it to the book structure, and saves manifest.
    QString addChapterToSection(const QString& sectionType,
                               const QString& partId,
                               const QString& title,
                               const QString& sourceFilePath,
                               bool copyFile = true);

signals:
    /// @brief Emitted when a project is successfully opened
    /// @param projectPath Absolute path to the project folder
    void projectOpened(const QString& projectPath);

    /// @brief Emitted when a project is closed
    void projectClosed();

    /// @brief Emitted when work mode changes
    /// @param mode New work mode
    void workModeChanged(WorkMode mode);

    /// @brief Emitted when dirty state changes
    /// @param dirty true if project has unsaved changes
    void dirtyStateChanged(bool dirty);

private:
    /// @brief Private constructor (singleton)
    ProjectManager();

    /// @brief Private destructor
    ~ProjectManager();

    /// @brief Create folder structure for new project
    /// @param projectPath Root path of the project
    /// @return true if all folders created successfully
    bool createFolderStructure(const std::filesystem::path& projectPath);

    /// @brief Validate existing project folder structure
    /// @param projectPath Root path of the project
    /// @return true if structure is valid
    bool validateFolderStructure(const std::filesystem::path& projectPath);

    /// @brief Set work mode and emit signal
    /// @param mode New work mode
    void setWorkMode(WorkMode mode);

    /// @brief Collect files recursively for archive export
    /// @param dir Directory to scan
    /// @param files Output vector of file paths
    /// @param excludeFolder Folder name to exclude (e.g., ".kalahari")
    void collectFilesForArchive(const std::filesystem::path& dir,
                               std::vector<std::filesystem::path>& files,
                               const std::string& excludeFolder);

    // =========================================================================
    // Member Variables
    // =========================================================================

    WorkMode m_workMode;                      ///< Current work mode
    std::unique_ptr<Document> m_document;     ///< Current document
    std::filesystem::path m_projectPath;      ///< Project root folder path
    std::filesystem::path m_manifestPath;     ///< Path to .klh manifest file
    bool m_isDirty;                           ///< Has unsaved changes
};

} // namespace core
} // namespace kalahari
