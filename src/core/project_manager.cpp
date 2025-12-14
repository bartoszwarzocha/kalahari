/// @file project_manager.cpp
/// @brief Implementation of ProjectManager for Solution-like architecture
///
/// OpenSpec #00033: Project File System

#include <kalahari/core/project_manager.h>
#include <kalahari/core/document.h>
#include <kalahari/core/book.h>
#include <kalahari/core/part.h>
#include <kalahari/core/book_element.h>
#include <kalahari/core/book_constants.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/logger.h>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimeZone>
#include <QUuid>

#include <zip.h>

namespace kalahari {
namespace core {

// =============================================================================
// Singleton Instance
// =============================================================================

ProjectManager& ProjectManager::getInstance() {
    static ProjectManager instance;
    return instance;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

ProjectManager::ProjectManager()
    : QObject(nullptr)
    , m_workMode(WorkMode::NoDocument)
    , m_document(nullptr)
    , m_isDirty(false)
{
    Logger::getInstance().info("ProjectManager initialized");
}

ProjectManager::~ProjectManager() {
    // Auto-save on destruction if dirty
    if (m_isDirty && m_document) {
        saveManifest();
    }
    Logger::getInstance().info("ProjectManager destroyed");
}

// =============================================================================
// Project Lifecycle
// =============================================================================

bool ProjectManager::createProject(const QString& parentDir,
                                   const QString& title,
                                   const QString& author,
                                   const QString& language,
                                   bool createSubfolder) {
    // Close any existing project first
    if (isProjectOpen()) {
        if (!closeProject(true)) {
            Logger::getInstance().warn("createProject: Failed to close existing project");
            return false;
        }
    }

    // Determine project path
    std::filesystem::path projectPath;
    if (createSubfolder) {
        // Sanitize title for folder name (replace invalid chars)
        QString safeFolderName = title;
        safeFolderName.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");
        projectPath = std::filesystem::path(parentDir.toStdWString()) / safeFolderName.toStdWString();
    } else {
        projectPath = std::filesystem::path(parentDir.toStdWString());
    }

    Logger::getInstance().info("Creating project at: {}", projectPath.string());

    // Check if path already exists
    if (std::filesystem::exists(projectPath)) {
        Logger::getInstance().error("Project path already exists: {}", projectPath.string());
        return false;
    }

    // Create folder structure
    if (!createFolderStructure(projectPath)) {
        Logger::getInstance().error("Failed to create folder structure");
        return false;
    }

    // Create Document object
    m_document = std::make_unique<Document>(
        title.toStdString(),
        author.toStdString(),
        language.toStdString()
    );

    // Set paths
    m_projectPath = projectPath;
    QString manifestFileName = title;
    manifestFileName.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");
    m_manifestPath = projectPath / (manifestFileName.toStdWString() + L".klh");

    // Save manifest
    if (!saveManifest()) {
        Logger::getInstance().error("Failed to save initial manifest");
        // Cleanup: remove created folders
        std::error_code ec;
        std::filesystem::remove_all(projectPath, ec);
        m_document.reset();
        m_projectPath.clear();
        m_manifestPath.clear();
        return false;
    }

    // Set state
    m_isDirty = false;
    setWorkMode(WorkMode::ProjectMode);

    Logger::getInstance().info("Project created successfully: {}", m_manifestPath.string());
    emit projectOpened(QString::fromStdWString(m_projectPath.wstring()));

    return true;
}

bool ProjectManager::openProject(const QString& manifestPath) {
    std::filesystem::path path(manifestPath.toStdWString());

    // Validate manifest path
    if (!std::filesystem::exists(path)) {
        Logger::getInstance().error("Manifest file not found: {}", path.string());
        return false;
    }

    if (path.extension() != ".klh") {
        Logger::getInstance().error("Invalid manifest file (not .klh): {}", path.string());
        return false;
    }

    // Close any existing project first
    if (isProjectOpen()) {
        if (!closeProject(true)) {
            Logger::getInstance().warn("openProject: Failed to close existing project");
            return false;
        }
    }

    Logger::getInstance().info("Opening project: {}", path.string());

    // Read manifest JSON
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::getInstance().error("Failed to open manifest file: {}", path.string());
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    // Parse JSON
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        Logger::getInstance().error("Failed to parse manifest JSON: {}",
                                    parseError.errorString().toStdString());
        return false;
    }

    if (!jsonDoc.isObject()) {
        Logger::getInstance().error("Manifest is not a JSON object");
        return false;
    }

    QJsonObject root = jsonDoc.object();

    // Validate required sections
    if (!root.contains("kalahari") || !root.contains("document")) {
        Logger::getInstance().error("Manifest missing required sections (kalahari, document)");
        return false;
    }

    // Extract document metadata
    QJsonObject kalahariSection = root["kalahari"].toObject();
    QJsonObject documentSection = root["document"].toObject();

    QString version = kalahariSection["version"].toString("1.0");
    QString minVersion = kalahariSection["minVersion"].toString("0.4.0");

    QString docId = documentSection["id"].toString();
    QString title = documentSection["title"].toString();
    QString author = documentSection["author"].toString();
    QString language = documentSection["language"].toString("en");
    QString genre = documentSection["genre"].toString();

    Logger::getInstance().debug("Loading project: '{}' by {} (v{})",
                                title.toStdString(), author.toStdString(), version.toStdString());

    // Create Document from loaded data
    m_document = std::make_unique<Document>(
        title.toStdString(),
        author.toStdString(),
        language.toStdString()
    );
    m_document->setGenre(genre.toStdString());

    // Set paths
    m_manifestPath = path;
    m_projectPath = path.parent_path();

    // Load book structure from manifest
    if (root.contains("structure")) {
        QJsonObject structureSection = root["structure"].toObject();
        if (!loadStructureFromManifest(structureSection)) {
            Logger::getInstance().warn("Failed to load book structure from manifest");
        }
    }

    // Validate folder structure (warn if missing, but continue)
    if (!validateFolderStructure(m_projectPath)) {
        Logger::getInstance().warn("Project folder structure is incomplete");
        // Create missing folders
        createFolderStructure(m_projectPath);
    }

    // Set state
    m_isDirty = false;
    setWorkMode(WorkMode::ProjectMode);

    Logger::getInstance().info("Project opened successfully: {}", title.toStdString());
    emit projectOpened(QString::fromStdWString(m_projectPath.wstring()));

    return true;
}

bool ProjectManager::closeProject(bool promptSave) {
    if (!isProjectOpen()) {
        return true; // Nothing to close
    }

    if (promptSave && m_isDirty) {
        Logger::getInstance().info("Auto-saving project before close");
        saveManifest();
    }

    Logger::getInstance().info("Closing project: {}", m_manifestPath.string());

    // Clear state
    m_document.reset();
    m_projectPath.clear();
    m_manifestPath.clear();
    m_isDirty = false;

    setWorkMode(WorkMode::NoDocument);
    emit projectClosed();

    return true;
}

bool ProjectManager::saveManifest() {
    if (!m_document || m_manifestPath.empty()) {
        Logger::getInstance().error("saveManifest: No document or manifest path");
        return false;
    }

    Logger::getInstance().debug("Saving manifest to: {}", m_manifestPath.string());

    // Build manifest JSON
    QJsonObject root;

    // Kalahari section
    QJsonObject kalahariSection;
    kalahariSection["version"] = "1.0";
    kalahariSection["minVersion"] = "0.4.0";
    root["kalahari"] = kalahariSection;

    // Document section
    QJsonObject documentSection;
    documentSection["id"] = QString::fromStdString(m_document->getId());
    documentSection["title"] = QString::fromStdString(m_document->getTitle());
    documentSection["author"] = QString::fromStdString(m_document->getAuthor());
    documentSection["language"] = QString::fromStdString(m_document->getLanguage());
    documentSection["genre"] = QString::fromStdString(m_document->getGenre());

    // Preserve 'created' timestamp from Document, only update 'modified'
    auto createdTime = m_document->getCreated();
    auto createdMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        createdTime.time_since_epoch()).count();
    QDateTime createdDt = QDateTime::fromMSecsSinceEpoch(createdMs, QTimeZone::UTC);
    documentSection["created"] = createdDt.toString(Qt::ISODate);
    documentSection["modified"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root["document"] = documentSection;

    // Structure section - serialize book structure
    root["structure"] = saveStructureToManifest();

    // Statistics section
    QJsonObject statisticsSection;
    statisticsSection["totalWords"] = 0;
    statisticsSection["totalChapters"] = 0;
    statisticsSection["lastEdited"] = "";
    root["statistics"] = statisticsSection;

    // Settings section
    QJsonObject settingsSection;
    settingsSection["defaultPerspective"] = "writer";
    settingsSection["autoSaveInterval"] = 300;
    root["settings"] = settingsSection;

    // Write to file
    QJsonDocument jsonDoc(root);
    QFile file(QString::fromStdWString(m_manifestPath.wstring()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::getInstance().error("Failed to open manifest for writing: {}", m_manifestPath.string());
        return false;
    }

    file.write(jsonDoc.toJson(QJsonDocument::Indented));
    file.close();

    m_isDirty = false;
    emit dirtyStateChanged(false);

    Logger::getInstance().info("Manifest saved successfully");
    return true;
}

bool ProjectManager::isProjectOpen() const {
    return m_workMode == WorkMode::ProjectMode && m_document != nullptr;
}

// =============================================================================
// Path Helpers
// =============================================================================

QString ProjectManager::getProjectPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString(m_projectPath.wstring());
}

QString ProjectManager::getManifestPath() const {
    if (m_manifestPath.empty()) return QString();
    return QString::fromStdWString(m_manifestPath.wstring());
}

QString ProjectManager::getContentPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / "content").wstring());
}

QString ProjectManager::getFrontmatterPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / "content" / "frontmatter").wstring());
}

QString ProjectManager::getBodyPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / "content" / "body").wstring());
}

QString ProjectManager::getBackmatterPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / "content" / "backmatter").wstring());
}

QString ProjectManager::getMetadataPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / "metadata").wstring());
}

QString ProjectManager::getMindmapsPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / "mindmaps").wstring());
}

QString ProjectManager::getTimelinesPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / "timelines").wstring());
}

QString ProjectManager::getResourcesPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / "resources").wstring());
}

QString ProjectManager::getKalahariPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / ".kalahari").wstring());
}

QString ProjectManager::getCachePath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / ".kalahari" / "cache").wstring());
}

QString ProjectManager::getBackupPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / ".kalahari" / "backup").wstring());
}

QString ProjectManager::getRecoveryPath() const {
    if (m_projectPath.empty()) return QString();
    return QString::fromStdWString((m_projectPath / ".kalahari" / "recovery").wstring());
}

// =============================================================================
// State Accessors
// =============================================================================

WorkMode ProjectManager::getWorkMode() const {
    return m_workMode;
}

Document* ProjectManager::getDocument() {
    return m_document.get();
}

const Document* ProjectManager::getDocument() const {
    return m_document.get();
}

bool ProjectManager::isDirty() const {
    return m_isDirty;
}

void ProjectManager::setDirty(bool dirty) {
    if (m_isDirty != dirty) {
        m_isDirty = dirty;
        emit dirtyStateChanged(dirty);
    }
}

// =============================================================================
// Book Structure Management
// =============================================================================

bool ProjectManager::loadStructureFromManifest(const QJsonObject& structureObj) {
    if (!m_document) {
        Logger::getInstance().error("loadStructureFromManifest: No document loaded");
        return false;
    }

    Book& book = m_document->getBook();

    // Clear existing structure
    book.clearAll();

    // Parse frontmatter array
    if (structureObj.contains("frontmatter")) {
        QJsonArray frontmatterArray = structureObj["frontmatter"].toArray();
        for (const QJsonValue& val : frontmatterArray) {
            QJsonObject elemObj = val.toObject();
            QString id = elemObj["id"].toString();
            QString title = elemObj["title"].toString();
            QString file = elemObj["file"].toString();
            QString type = elemObj["type"].toString(TYPE_PREFACE);
            int wordCount = elemObj["wordCount"].toInt(0);

            auto element = std::make_shared<BookElement>(
                type.toStdString(),
                id.toStdString(),
                title.toStdString(),
                std::filesystem::path(file.toStdWString())
            );
            element->setWordCount(wordCount);

            // Load status from manifest (OpenSpec #00034)
            if (elemObj.contains("status")) {
                QString status = elemObj["status"].toString();
                element->setMetadata("status", status.toStdString());
            }

            book.addFrontMatter(element);

            Logger::getInstance().debug("Loaded frontmatter: {} ({})",
                                        title.toStdString(), id.toStdString());
        }
    }

    // Parse body array (parts with chapters)
    if (structureObj.contains("body")) {
        QJsonArray bodyArray = structureObj["body"].toArray();
        for (const QJsonValue& partVal : bodyArray) {
            QJsonObject partObj = partVal.toObject();
            QString partId = partObj["id"].toString();
            QString partTitle = partObj["title"].toString();

            auto part = std::make_shared<Part>(
                partId.toStdString(),
                partTitle.toStdString()
            );

            // Parse chapters within this part
            if (partObj.contains("chapters")) {
                QJsonArray chaptersArray = partObj["chapters"].toArray();
                for (const QJsonValue& chapVal : chaptersArray) {
                    QJsonObject chapObj = chapVal.toObject();
                    QString chapId = chapObj["id"].toString();
                    QString chapTitle = chapObj["title"].toString();
                    QString chapFile = chapObj["file"].toString();
                    int wordCount = chapObj["wordCount"].toInt(0);

                    auto chapter = std::make_shared<BookElement>(
                        TYPE_CHAPTER,
                        chapId.toStdString(),
                        chapTitle.toStdString(),
                        std::filesystem::path(chapFile.toStdWString())
                    );
                    chapter->setWordCount(wordCount);

                    // Load status from manifest (OpenSpec #00034)
                    if (chapObj.contains("status")) {
                        QString status = chapObj["status"].toString();
                        chapter->setMetadata("status", status.toStdString());
                    }

                    part->addChapter(chapter);

                    Logger::getInstance().debug("Loaded chapter: {} ({})",
                                                chapTitle.toStdString(), chapId.toStdString());
                }
            }

            book.addPart(part);
            Logger::getInstance().debug("Loaded part: {} with {} chapters",
                                        partTitle.toStdString(), part->getChapterCount());
        }
    }

    // Parse backmatter array
    if (structureObj.contains("backmatter")) {
        QJsonArray backmatterArray = structureObj["backmatter"].toArray();
        for (const QJsonValue& val : backmatterArray) {
            QJsonObject elemObj = val.toObject();
            QString id = elemObj["id"].toString();
            QString title = elemObj["title"].toString();
            QString file = elemObj["file"].toString();
            QString type = elemObj["type"].toString(TYPE_EPILOGUE);
            int wordCount = elemObj["wordCount"].toInt(0);

            auto element = std::make_shared<BookElement>(
                type.toStdString(),
                id.toStdString(),
                title.toStdString(),
                std::filesystem::path(file.toStdWString())
            );
            element->setWordCount(wordCount);

            // Load status from manifest (OpenSpec #00034)
            if (elemObj.contains("status")) {
                QString status = elemObj["status"].toString();
                element->setMetadata("status", status.toStdString());
            }

            book.addBackMatter(element);

            Logger::getInstance().debug("Loaded backmatter: {} ({})",
                                        title.toStdString(), id.toStdString());
        }
    }

    Logger::getInstance().info("Book structure loaded: {} frontmatter, {} parts, {} backmatter",
                               book.getFrontMatter().size(),
                               book.getPartCount(),
                               book.getBackMatter().size());
    return true;
}

QJsonObject ProjectManager::saveStructureToManifest() const {
    QJsonObject structureObj;

    if (!m_document) {
        // Return empty structure if no document
        structureObj["frontmatter"] = QJsonArray();
        structureObj["body"] = QJsonArray();
        structureObj["backmatter"] = QJsonArray();
        return structureObj;
    }

    const Book& book = m_document->getBook();

    // Serialize frontmatter
    QJsonArray frontmatterArray;
    for (const auto& element : book.getFrontMatter()) {
        QJsonObject elemObj;
        elemObj["id"] = QString::fromStdString(element->getId());
        elemObj["title"] = QString::fromStdString(element->getTitle());
        elemObj["file"] = QString::fromStdWString(element->getFile().wstring());
        elemObj["type"] = QString::fromStdString(element->getType());
        elemObj["wordCount"] = element->getWordCount();

        // Save status to manifest (OpenSpec #00034)
        auto status = element->getMetadata("status");
        if (status.has_value()) {
            elemObj["status"] = QString::fromStdString(status.value());
        }

        frontmatterArray.append(elemObj);
    }
    structureObj["frontmatter"] = frontmatterArray;

    // Serialize body (parts with chapters)
    QJsonArray bodyArray;
    for (const auto& part : book.getBody()) {
        QJsonObject partObj;
        partObj["id"] = QString::fromStdString(part->getId());
        partObj["title"] = QString::fromStdString(part->getTitle());

        QJsonArray chaptersArray;
        for (const auto& chapter : part->getChapters()) {
            QJsonObject chapObj;
            chapObj["id"] = QString::fromStdString(chapter->getId());
            chapObj["title"] = QString::fromStdString(chapter->getTitle());
            chapObj["file"] = QString::fromStdWString(chapter->getFile().wstring());
            chapObj["wordCount"] = chapter->getWordCount();

            // Save status to manifest (OpenSpec #00034)
            auto status = chapter->getMetadata("status");
            if (status.has_value()) {
                chapObj["status"] = QString::fromStdString(status.value());
            }

            chaptersArray.append(chapObj);
        }
        partObj["chapters"] = chaptersArray;
        bodyArray.append(partObj);
    }
    structureObj["body"] = bodyArray;

    // Serialize backmatter
    QJsonArray backmatterArray;
    for (const auto& element : book.getBackMatter()) {
        QJsonObject elemObj;
        elemObj["id"] = QString::fromStdString(element->getId());
        elemObj["title"] = QString::fromStdString(element->getTitle());
        elemObj["file"] = QString::fromStdWString(element->getFile().wstring());
        elemObj["type"] = QString::fromStdString(element->getType());
        elemObj["wordCount"] = element->getWordCount();

        // Save status to manifest (OpenSpec #00034)
        auto status = element->getMetadata("status");
        if (status.has_value()) {
            elemObj["status"] = QString::fromStdString(status.value());
        }

        backmatterArray.append(elemObj);
    }
    structureObj["backmatter"] = backmatterArray;

    return structureObj;
}

QString ProjectManager::loadChapterContent(const QString& elementId) {
    BookElement* element = findElement(elementId);
    if (!element) {
        Logger::getInstance().warn("loadChapterContent: Element not found: {}",
                                   elementId.toStdString());
        return QString();
    }

    // Resolve relative path against project path
    std::filesystem::path rtfPath = m_projectPath / element->getFile();
    
    // Check for .kchapter file (new format)
    std::filesystem::path kchapterPath = rtfPath;
    kchapterPath.replace_extension(".kchapter");

    QString htmlContent;

    if (std::filesystem::exists(kchapterPath)) {
        // Load from .kchapter format (preferred)
        QString kchapterPathStr = QString::fromStdWString(kchapterPath.wstring());
        auto doc = ChapterDocument::load(kchapterPathStr);
        if (doc.has_value()) {
            htmlContent = doc->html();
            Logger::getInstance().debug("Loaded .kchapter for element: {} ({} chars)",
                                        elementId.toStdString(), htmlContent.length());
        } else {
            Logger::getInstance().error("loadChapterContent: Failed to load .kchapter: {}",
                                       kchapterPath.string());
            return QString();
        }
    } else if (std::filesystem::exists(rtfPath)) {
        // Legacy RTF file - migrate to .kchapter
        Logger::getInstance().info("Migrating RTF to .kchapter: {}", rtfPath.string());

        QFile rtfFile(QString::fromStdWString(rtfPath.wstring()));
        if (!rtfFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            Logger::getInstance().warn("loadChapterContent: Failed to open RTF: {}",
                                       rtfPath.string());
            return QString();
        }

        QTextStream in(&rtfFile);
        in.setEncoding(QStringConverter::Utf8);
        htmlContent = in.readAll();
        rtfFile.close();

        // Create .kchapter from RTF content
        ChapterDocument doc = ChapterDocument::fromHtmlContent(
            htmlContent, QString::fromStdString(element->getTitle()));
        
        QString kchapterPathStr = QString::fromStdWString(kchapterPath.wstring());
        if (doc.save(kchapterPathStr)) {
            // Backup RTF file
            std::filesystem::path backupPath = rtfPath;
            backupPath += ".bak";
            std::error_code ec;
            std::filesystem::rename(rtfPath, backupPath, ec);
            if (ec) {
                Logger::getInstance().warn("Failed to backup RTF: {} ({})",
                                          rtfPath.string(), ec.message());
            }
            
            // Update element file path to .kchapter
            std::filesystem::path relPath = element->getFile();
            relPath.replace_extension(".kchapter");
            element->setFile(relPath);
            
            Logger::getInstance().info("Migration complete: {} -> {}",
                                      rtfPath.string(), kchapterPath.string());
        } else {
            Logger::getInstance().error("Failed to save migrated .kchapter: {}",
                                       kchapterPath.string());
        }
    } else {
        Logger::getInstance().warn("loadChapterContent: No file found: {}",
                                   rtfPath.string());
        return QString();
    }

    // Cache content in element
    element->setContent(htmlContent);
    element->setDirty(false);  // Just loaded, not dirty

    return htmlContent;
}

bool ProjectManager::saveChapterContent(const QString& elementId) {
    BookElement* element = findElement(elementId);
    if (!element) {
        Logger::getInstance().warn("saveChapterContent: Element not found: {}",
                                   elementId.toStdString());
        return false;
    }

    if (!element->isContentLoaded()) {
        Logger::getInstance().debug("saveChapterContent: No content loaded for: {}",
                                    elementId.toStdString());
        return true;  // Nothing to save
    }

    // Resolve path - always use .kchapter extension
    std::filesystem::path filePath = m_projectPath / element->getFile();
    
    // Ensure .kchapter extension
    if (filePath.extension() != ".kchapter") {
        filePath.replace_extension(".kchapter");
        
        // Update element file path
        std::filesystem::path relPath = element->getFile();
        relPath.replace_extension(".kchapter");
        element->setFile(relPath);
    }

    // Create parent directories if needed
    std::filesystem::path parentDir = filePath.parent_path();
    if (!std::filesystem::exists(parentDir)) {
        std::error_code ec;
        if (!std::filesystem::create_directories(parentDir, ec)) {
            Logger::getInstance().error("saveChapterContent: Failed to create directory: {} ({})",
                                        parentDir.string(), ec.message());
            return false;
        }
    }

    // Create ChapterDocument from HTML content
    ChapterDocument doc = ChapterDocument::fromHtmlContent(
        element->getContent(),
        QString::fromStdString(element->getTitle()));
    
    // Copy metadata from element if available
    auto status = element->getMetadata("status");
    if (status.has_value()) {
        doc.setStatus(QString::fromStdString(status.value()));
    }
    
    auto notes = element->getMetadata("notes");
    if (notes.has_value()) {
        doc.setNotes(QString::fromStdString(notes.value()));
    }
    
    // Update word count in element
    element->setWordCount(doc.wordCount());

    // Save to .kchapter file
    QString filePathStr = QString::fromStdWString(filePath.wstring());
    if (!doc.save(filePathStr)) {
        Logger::getInstance().error("saveChapterContent: Failed to save .kchapter: {}",
                                    filePath.string());
        return false;
    }

    element->setDirty(false);
    element->touch();  // Update modified timestamp

    Logger::getInstance().debug("Saved .kchapter for element: {} ({} words)",
                                elementId.toStdString(), doc.wordCount());
    return true;
}

BookElement* ProjectManager::findElement(const QString& elementId) {
    if (!m_document) {
        return nullptr;
    }

    std::string id = elementId.toStdString();
    Book& book = m_document->getBook();

    // Search frontmatter
    for (auto& element : book.getFrontMatter()) {
        if (element->getId() == id) {
            return element.get();
        }
    }

    // Search body (parts and chapters)
    for (auto& part : book.getBody()) {
        for (const auto& chapter : part->getChapters()) {
            if (chapter->getId() == id) {
                return chapter.get();
            }
        }
    }

    // Search backmatter
    for (auto& element : book.getBackMatter()) {
        if (element->getId() == id) {
            return element.get();
        }
    }

    return nullptr;
}

Part* ProjectManager::findPart(const QString& partId) {
    if (!m_document) {
        return nullptr;
    }

    std::string id = partId.toStdString();
    Book& book = m_document->getBook();

    for (auto& part : book.getBody()) {
        if (part->getId() == id) {
            return part.get();
        }
    }

    return nullptr;
}

std::vector<QString> ProjectManager::getDirtyElements() const {
    std::vector<QString> dirtyIds;

    if (!m_document) {
        return dirtyIds;
    }

    const Book& book = m_document->getBook();

    // Check frontmatter
    for (const auto& element : book.getFrontMatter()) {
        if (element->isDirty()) {
            dirtyIds.push_back(QString::fromStdString(element->getId()));
        }
    }

    // Check body (chapters)
    for (const auto& part : book.getBody()) {
        for (const auto& chapter : part->getChapters()) {
            if (chapter->isDirty()) {
                dirtyIds.push_back(QString::fromStdString(chapter->getId()));
            }
        }
    }

    // Check backmatter
    for (const auto& element : book.getBackMatter()) {
        if (element->isDirty()) {
            dirtyIds.push_back(QString::fromStdString(element->getId()));
        }
    }

    return dirtyIds;
}

bool ProjectManager::saveAllDirty() {
    std::vector<QString> dirtyIds = getDirtyElements();

    if (dirtyIds.empty()) {
        Logger::getInstance().debug("saveAllDirty: No dirty elements to save");
        return true;
    }

    Logger::getInstance().info("Saving {} dirty elements", dirtyIds.size());

    bool allSaved = true;
    for (const QString& id : dirtyIds) {
        if (!saveChapterContent(id)) {
            Logger::getInstance().error("Failed to save element: {}", id.toStdString());
            allSaved = false;
        }
    }

    return allSaved;
}

std::vector<std::pair<QString, QString>> ProjectManager::getIncompleteElements() const {
    std::vector<std::pair<QString, QString>> result;
    
    if (!m_document) {
        return result;
    }
    
    const Book& book = m_document->getBook();
    
    auto checkElement = [&result](const BookElement* element) {
        auto status = element->getMetadata("status");
        QString statusStr = status.has_value() 
            ? QString::fromStdString(status.value()).toLower()
            : "draft";  // Default to draft if no status
        
        if (statusStr != "final") {
            result.emplace_back(
                QString::fromStdString(element->getId()),
                statusStr
            );
        }
    };
    
    // Check frontmatter
    for (const auto& element : book.getFrontMatter()) {
        checkElement(element.get());
    }
    
    // Check body chapters
    for (const auto& part : book.getBody()) {
        for (const auto& chapter : part->getChapters()) {
            checkElement(chapter.get());
        }
    }
    
    // Check backmatter
    for (const auto& element : book.getBackMatter()) {
        checkElement(element.get());
    }
    
    return result;
}

std::map<QString, int> ProjectManager::getStatusStatistics() const {
    std::map<QString, int> stats;
    
    if (!m_document) {
        return stats;
    }
    
    const Book& book = m_document->getBook();
    
    auto countElement = [&stats](const BookElement* element) {
        auto status = element->getMetadata("status");
        QString statusStr = status.has_value() 
            ? QString::fromStdString(status.value()).toLower()
            : "draft";  // Default to draft if no status
        
        stats[statusStr]++;
    };
    
    // Count frontmatter
    for (const auto& element : book.getFrontMatter()) {
        countElement(element.get());
    }
    
    // Count body chapters
    for (const auto& part : book.getBody()) {
        for (const auto& chapter : part->getChapters()) {
            countElement(chapter.get());
        }
    }
    
    // Count backmatter
    for (const auto& element : book.getBackMatter()) {
        countElement(element.get());
    }
    
    return stats;
}

QString ProjectManager::addChapterToSection(const QString& sectionType,
                                           const QString& partId,
                                           const QString& title,
                                           const QString& sourceFilePath,
                                           bool copyFile) {
    auto& logger = Logger::getInstance();

    if (!isProjectOpen() || !m_document) {
        logger.error("addChapterToSection: No project open");
        return QString();
    }

    Book& book = m_document->getBook();

    // Generate unique element ID
    QString elementId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);

    // Determine target directory and file path
    QString targetDir;
    if (sectionType == "frontmatter") {
        targetDir = getFrontmatterPath();
    } else if (sectionType == "body") {
        targetDir = getBodyPath() + "/" + partId;
    } else if (sectionType == "backmatter") {
        targetDir = getBackmatterPath();
    } else {
        logger.error("addChapterToSection: Invalid section type: {}", sectionType.toStdString());
        return QString();
    }

    // Create target directory if needed
    QDir().mkpath(targetDir);

    // Determine target filename (use element ID + original extension or .rtf)
    QFileInfo sourceInfo(sourceFilePath);
    QString extension = sourceInfo.suffix().isEmpty() ? "rtf" : sourceInfo.suffix();
    QString targetFileName = elementId + "." + extension;
    QString targetFilePath = targetDir + "/" + targetFileName;

    // Copy or move file
    QFile sourceFile(sourceFilePath);
    bool fileOk = false;
    if (copyFile) {
        fileOk = sourceFile.copy(targetFilePath);
    } else {
        fileOk = sourceFile.rename(targetFilePath);
    }

    if (!fileOk) {
        logger.error("addChapterToSection: Failed to {} file from {} to {}",
                    copyFile ? "copy" : "move",
                    sourceFilePath.toStdString(),
                    targetFilePath.toStdString());
        return QString();
    }

    // Determine element type based on section
    std::string elementType;
    if (sectionType == "frontmatter") {
        elementType = TYPE_PREFACE;  // Default for front matter
    } else if (sectionType == "body") {
        elementType = TYPE_CHAPTER;
    } else {
        elementType = TYPE_EPILOGUE;  // Default for back matter
    }

    // Create relative path from project root
    QString relativePath = QDir(QString::fromStdString(m_projectPath.string())).relativeFilePath(targetFilePath);

    // Create new BookElement
    auto element = std::make_shared<BookElement>(
        elementType,
        elementId.toStdString(),
        title.toStdString(),
        std::filesystem::path(relativePath.toStdWString())
    );

    // Add to book structure
    if (sectionType == "frontmatter") {
        book.addFrontMatter(element);
    } else if (sectionType == "body") {
        Part* part = findPart(partId);
        if (part) {
            part->addChapter(element);
        } else {
            logger.error("addChapterToSection: Part not found: {}", partId.toStdString());
            // Cleanup copied file
            QFile::remove(targetFilePath);
            return QString();
        }
    } else if (sectionType == "backmatter") {
        book.addBackMatter(element);
    }

    // Save manifest
    saveManifest();
    setDirty(false);

    logger.info("addChapterToSection: Added '{}' to {} (part: {}, id: {})",
               title.toStdString(), sectionType.toStdString(),
               partId.toStdString(), elementId.toStdString());

    // Emit signal to refresh UI
    emit projectOpened(QString::fromStdWString(m_projectPath.wstring()));

    return elementId;
}

// =============================================================================
// Reordering Operations (OpenSpec #00034 Phase D)
// =============================================================================

bool ProjectManager::reorderChapter(const QString& partId, int fromIndex, int toIndex) {
    auto& logger = Logger::getInstance();

    if (!isProjectOpen() || !m_document) {
        logger.error("reorderChapter: No project open");
        return false;
    }

    Part* part = findPart(partId);
    if (!part) {
        logger.error("reorderChapter: Part not found: {}", partId.toStdString());
        return false;
    }

    if (fromIndex < 0 || toIndex < 0) {
        logger.error("reorderChapter: Invalid indices: from={}, to={}", fromIndex, toIndex);
        return false;
    }

    bool success = part->moveChapter(static_cast<size_t>(fromIndex), static_cast<size_t>(toIndex));

    if (success) {
        logger.info("reorderChapter: Moved chapter in part '{}' from index {} to {}",
                   partId.toStdString(), fromIndex, toIndex);
        saveManifest();
    }

    return success;
}

bool ProjectManager::reorderPart(int fromIndex, int toIndex) {
    auto& logger = Logger::getInstance();

    if (!isProjectOpen() || !m_document) {
        logger.error("reorderPart: No project open");
        return false;
    }

    if (fromIndex < 0 || toIndex < 0) {
        logger.error("reorderPart: Invalid indices: from={}, to={}", fromIndex, toIndex);
        return false;
    }

    Book& book = m_document->getBook();
    bool success = book.movePart(static_cast<size_t>(fromIndex), static_cast<size_t>(toIndex));

    if (success) {
        logger.info("reorderPart: Moved part from index {} to {}", fromIndex, toIndex);
        saveManifest();
    }

    return success;
}

// =============================================================================
// Private Helpers
// =============================================================================

bool ProjectManager::createFolderStructure(const std::filesystem::path& projectPath) {
    std::error_code ec;

    // List of folders to create
    std::vector<std::filesystem::path> folders = {
        projectPath,
        projectPath / "content",
        projectPath / "content" / "frontmatter",
        projectPath / "content" / "body",
        projectPath / "content" / "backmatter",
        projectPath / "metadata",
        projectPath / "mindmaps",
        projectPath / "timelines",
        projectPath / "resources",
        projectPath / "resources" / "images",
        projectPath / "resources" / "research",
        projectPath / ".kalahari",
        projectPath / ".kalahari" / "cache",
        projectPath / ".kalahari" / "backup",
        projectPath / ".kalahari" / "recovery"
    };

    for (const auto& folder : folders) {
        if (!std::filesystem::exists(folder)) {
            if (!std::filesystem::create_directories(folder, ec)) {
                Logger::getInstance().error("Failed to create folder: {} ({})",
                                            folder.string(), ec.message());
                return false;
            }
            Logger::getInstance().debug("Created folder: {}", folder.string());
        }
    }

    // Create .gitignore in .kalahari folder
    std::filesystem::path gitignorePath = projectPath / ".kalahari" / ".gitignore";
    if (!std::filesystem::exists(gitignorePath)) {
        QFile gitignore(QString::fromStdWString(gitignorePath.wstring()));
        if (gitignore.open(QIODevice::WriteOnly | QIODevice::Text)) {
            gitignore.write("# Kalahari IDE files - not for version control\n");
            gitignore.write("*\n");
            gitignore.close();
        }
    }

    // Create empty placeholder files in metadata folder
    std::vector<std::pair<std::string, std::string>> placeholders = {
        {"characters.json", "[]"},
        {"locations.json", "[]"},
        {"notes.json", "[]"}
    };

    for (const auto& [filename, content] : placeholders) {
        std::filesystem::path filePath = projectPath / "metadata" / filename;
        if (!std::filesystem::exists(filePath)) {
            QFile file(QString::fromStdWString(filePath.wstring()));
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(content.c_str());
                file.close();
            }
        }
    }

    Logger::getInstance().info("Folder structure created successfully");
    return true;
}

bool ProjectManager::validateFolderStructure(const std::filesystem::path& projectPath) {
    // Check for required folders
    std::vector<std::filesystem::path> requiredFolders = {
        projectPath / "content",
        projectPath / "content" / "frontmatter",
        projectPath / "content" / "body",
        projectPath / "content" / "backmatter",
        projectPath / "metadata"
    };

    for (const auto& folder : requiredFolders) {
        if (!std::filesystem::exists(folder)) {
            Logger::getInstance().debug("Missing required folder: {}", folder.string());
            return false;
        }
    }

    return true;
}


void ProjectManager::setWorkMode(WorkMode mode) {
    if (m_workMode != mode) {
        m_workMode = mode;
        Logger::getInstance().debug("Work mode changed to: {}",
                                    mode == WorkMode::NoDocument ? "NoDocument" :
                                    mode == WorkMode::ProjectMode ? "ProjectMode" : "StandaloneMode");
        emit workModeChanged(mode);
    }
}

void ProjectManager::collectFilesForArchive(const std::filesystem::path& dir,
                                           std::vector<std::filesystem::path>& files,
                                           const std::string& excludeFolder) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        // Skip excluded folder
        std::string pathStr = entry.path().string();
        if (pathStr.find("/" + excludeFolder + "/") != std::string::npos ||
            pathStr.find("\\" + excludeFolder + "\\") != std::string::npos ||
            entry.path().filename() == excludeFolder) {
            continue;
        }

        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
}

bool ProjectManager::exportArchive(const QString& outputPath,
                                   std::function<void(int)> progressCallback) {
    auto& logger = Logger::getInstance();

    if (!isProjectOpen()) {
        logger.error("exportArchive: No project open");
        return false;
    }

    logger.info("Exporting project to: {}", outputPath.toStdString());

    // Collect files to archive (excluding .kalahari folder)
    std::vector<std::filesystem::path> files;
    collectFilesForArchive(m_projectPath, files, ".kalahari");

    if (files.empty()) {
        logger.error("exportArchive: No files to archive");
        return false;
    }

    // Create ZIP archive
    int zipError = 0;
    zip_t* archive = zip_open(outputPath.toStdString().c_str(),
                              ZIP_CREATE | ZIP_TRUNCATE, &zipError);
    if (!archive) {
        logger.error("exportArchive: Failed to create ZIP file: error {}", zipError);
        return false;
    }

    // Add each file to archive
    for (size_t i = 0; i < files.size(); ++i) {
        std::filesystem::path relativePath = files[i].lexically_relative(m_projectPath);

        // Create zip source from file
        zip_source_t* source = zip_source_file(archive, files[i].string().c_str(), 0, -1);
        if (!source) {
            logger.warn("exportArchive: Failed to create source for: {}", files[i].string());
            continue;
        }

        // Add file to archive with UTF-8 encoding for path
        if (zip_file_add(archive, relativePath.string().c_str(), source, ZIP_FL_ENC_UTF_8) < 0) {
            logger.warn("exportArchive: Failed to add file: {}", relativePath.string());
            zip_source_free(source);
            continue;
        }

        // Report progress
        if (progressCallback) {
            progressCallback(static_cast<int>((i + 1) * 100 / files.size()));
        }
    }

    // Close archive
    if (zip_close(archive) < 0) {
        logger.error("exportArchive: Failed to close ZIP file");
        return false;
    }

    logger.info("exportArchive: Successfully exported {} files", files.size());
    return true;
}

bool ProjectManager::importArchive(const QString& archivePath,
                                   const QString& targetDir,
                                   std::function<void(int)> progressCallback) {
    auto& logger = Logger::getInstance();

    logger.info("Importing archive: {} to {}", archivePath.toStdString(), targetDir.toStdString());

    // Open ZIP archive
    int zipError = 0;
    zip_t* archive = zip_open(archivePath.toStdString().c_str(), ZIP_RDONLY, &zipError);
    if (!archive) {
        logger.error("importArchive: Failed to open ZIP file: error {}", zipError);
        return false;
    }

    // Determine project name from archive name
    QFileInfo archiveInfo(archivePath);
    QString projectName = archiveInfo.completeBaseName();
    if (projectName.endsWith(".klh", Qt::CaseInsensitive)) {
        projectName.chop(4);
    }

    QString extractDir = targetDir + "/" + projectName;

    // Check for name conflicts
    if (QDir(extractDir).exists()) {
        logger.error("importArchive: Target directory already exists: {}", extractDir.toStdString());
        zip_close(archive);
        return false;
    }

    // Create extract directory
    QDir().mkpath(extractDir);

    // Extract all entries
    zip_int64_t numEntries = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; i < numEntries; ++i) {
        const char* name = zip_get_name(archive, i, ZIP_FL_ENC_UTF_8);
        if (!name) continue;

        QString entryPath = extractDir + "/" + QString::fromUtf8(name);

        // Check if it's a directory (ends with /)
        if (QString::fromUtf8(name).endsWith('/')) {
            QDir().mkpath(entryPath);
            continue;
        }

        // Ensure parent directory exists
        QFileInfo fileInfo(entryPath);
        QDir().mkpath(fileInfo.absolutePath());

        // Open file in archive
        zip_file_t* zf = zip_fopen_index(archive, i, 0);
        if (!zf) {
            logger.warn("importArchive: Failed to open entry: {}", name);
            continue;
        }

        // Read and write file
        QFile outFile(entryPath);
        if (outFile.open(QIODevice::WriteOnly)) {
            char buffer[8192];
            zip_int64_t bytesRead;
            while ((bytesRead = zip_fread(zf, buffer, sizeof(buffer))) > 0) {
                outFile.write(buffer, bytesRead);
            }
            outFile.close();
        } else {
            logger.warn("importArchive: Failed to create file: {}", entryPath.toStdString());
        }

        zip_fclose(zf);

        // Report progress
        if (progressCallback) {
            progressCallback(static_cast<int>((i + 1) * 100 / numEntries));
        }
    }

    zip_close(archive);

    // Find and open the .klh manifest
    QDir projectDir(extractDir);
    QStringList klhFiles = projectDir.entryList({"*.klh"}, QDir::Files);
    if (klhFiles.isEmpty()) {
        logger.error("importArchive: No .klh manifest found in archive");
        return false;
    }

    logger.info("importArchive: Opening extracted project: {}", klhFiles.first().toStdString());
    return openProject(extractDir + "/" + klhFiles.first());
}

} // namespace core
} // namespace kalahari
