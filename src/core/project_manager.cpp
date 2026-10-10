/// @file project_manager.cpp
/// @brief Implementation of ProjectManager: the book project open in the program

#include <kalahari/core/project_manager.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/resource_paths.h>
#include <kalahari/core/standalone_file.h>
#include <kalahari/core/text_file.h>
#include <kalahari/editor/clipboard_handler.h>

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimeZone>
#include <QUuid>

#include <algorithm>
#include <utility>

#include <zip.h>

namespace kalahari {
namespace core {

namespace {

/// Package whose kinds a user project starts with
constexpr const char* BASE_PACKAGE = "kalahari.base";

/// Statuses of text elements, from the first draft to the final text
const QStringList& textStatuses() {
    static const QStringList statuses{QStringLiteral("draft"), QStringLiteral("revision"),
                                      QStringLiteral("final")};
    return statuses;
}

QString toQString(const std::filesystem::path& path) {
    return QString::fromStdWString(path.wstring());
}

/// @p title as a file or folder name: characters that file systems do not allow become "_"
QString safeFileName(const QString& title) {
    static const QRegularExpression invalid(QStringLiteral("[<>:\"/\\\\|?*]"));
    QString name = title.trimmed();
    name.replace(invalid, QStringLiteral("_"));
    return name;
}

void appendAll(const QList<ProjectElement>& elements, QList<const ProjectElement*>& all) {
    for (const ProjectElement& element : elements) {
        all.append(&element);
        appendAll(element.elements, all);
    }
}

/// Every element of @p project: of the books in reading order, then of the Workshop
QList<const ProjectElement*> allElements(const BookProject& project) {
    QList<const ProjectElement*> all;
    for (qsizetype book = 0; book < project.books.size(); ++book) {
        all.append(project.readingOrder(book));
    }
    appendAll(project.workshop.elements, all);
    return all;
}

/// Elements of @p kind in @p project
qsizetype countOf(const BookTypeRegistry& registry, const BookProject& project,
                  const KindRef& kind) {
    const QList<const ProjectElement*> all = allElements(project);
    return std::count_if(all.cbegin(), all.cend(),
                         [&registry, &kind](const ProjectElement* element) {
                             return BookProject::kindOf(registry, *element).kind == kind.kind;
                         });
}

/// Whether the elements of @p kind in @p project reached the kind's limit
bool limitReached(const BookTypeRegistry& registry, const BookProject& project,
                  const KindRef& kind) {
    return kind.kind->limit > 0 && countOf(registry, project, kind) >= kind.kind->limit;
}

/// Seconds since 1970 in UTC, as the .klh file keeps them
QDateTime now() {
    return QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch(), QTimeZone::utc());
}

/// Folder of the files of @p place, relative to the project: the book's or the Workshop's
QString folderOf(BookPlace place, const ProjectBook* book) {
    if (place == BookPlace::Workshop) {
        return QString::fromLatin1(BookProject::WORKSHOP_FOLDER);
    }
    return book && !book->folder.isEmpty() ? book->folder
                                           : QString::fromLatin1(BookProject::BOOK_FOLDER);
}

/// A chapter file that no element of @p project and no file of @p projectDir has, for a new
/// element of kind @p kindId in @p folder: <folder>/<kind>_001.kchapter, _002...
QString freeChapterFile(const BookProject& project, const QString& projectDir,
                        const QString& folder, const QString& kindId) {
    static const QRegularExpression notPlain(QStringLiteral("[^A-Za-z0-9_]"));
    QString name = kindId.isEmpty() ? QStringLiteral("chapter") : kindId;
    name.replace(notPlain, QStringLiteral("_"));

    const QDir dir(projectDir);
    for (int number = 1;; ++number) {
        const QString file = QStringLiteral("%1/%2_%3.kchapter")
                                 .arg(folder, name)
                                 .arg(number, 3, 10, QLatin1Char('0'));
        if (!project.hasFile(file) && !QFileInfo::exists(dir.filePath(file))) {
            return file;
        }
    }
}

/// Log of the database of a project, which has the changes not yet written to project.db
constexpr QLatin1String DATABASE_LOG("project.db-wal");

/// Whether file @p path of a project, relative to its folder, serves only the project open
/// in this place: its lock, the log files of its database and the database's backups
bool isLocalFile(const QString& path) {
    static const QStringList files{QStringLiteral(".kalahari.lock"), QString(DATABASE_LOG),
                                   QStringLiteral("project.db-shm"),
                                   QStringLiteral("project.db-journal")};
    static const QStringList folders{QStringLiteral(".backups"), QStringLiteral(".kalahari")};
    const QString first = path.section(QLatin1Char('/'), 0, 0);
    return files.contains(path) || (path.contains(QLatin1Char('/')) && folders.contains(first));
}

/// Whether archive entry @p path served only the project where it was archived: its lock and
/// the shared memory of its database, which archives of earlier versions have. The log of the
/// database stays, as it may have changes that the database file does not have.
bool isArchivedLocalFile(const QString& path) {
    return path == QLatin1String(".kalahari.lock") || path == QLatin1String("project.db-shm");
}

/// Files of project folder @p projectDir that go to its archive, relative to it, with "/";
/// without file @p skipped (the archive, when it is written to the project's folder)
QStringList archivedFiles(const QString& projectDir, const QString& skipped) {
    QStringList files;
    const QDir dir(projectDir);
    const QString skippedPath = dir.relativeFilePath(QFileInfo(skipped).absoluteFilePath());
    QDirIterator it(projectDir, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = dir.relativeFilePath(it.next());
        if (!isLocalFile(path) && path != skippedPath) {
            files.append(path);
        }
    }
    files.sort();
    return files;
}

/// Path inside the project's folder of archive entry @p name, with "/"; empty for an entry
/// that would land outside the folder (an absolute path, a drive or "..")
QString entryPath(const QString& name) {
    QString path = name;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));  // archives of earlier versions
    if (path.startsWith(QLatin1Char('/')) || path.contains(QLatin1Char(':'))) {
        return QString();
    }
    QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    parts.removeAll(QStringLiteral("."));
    if (parts.contains(QStringLiteral(".."))) {
        return QString();
    }
    return parts.join(QLatin1Char('/'));
}

/// A source of libzip for file @p path; libzip reads the file when the archive is closed
zip_source_t* fileSource(const QString& path, zip_error_t* error) {
#ifdef _WIN32
    // Narrow file names are not Unicode on Windows
    return zip_source_win32w_create(reinterpret_cast<const wchar_t*>(path.utf16()), 0, -1,
                                    error);
#else
    return zip_source_file_create(QFile::encodeName(path).constData(), 0, -1, error);
#endif
}

/// Archive @p path opened with libzip's @p flags; nullptr, with the reason in @p error, when
/// it cannot be opened
zip_t* openZip(const QString& path, int flags, QString* error) {
    zip_error_t zipError;
    zip_error_init(&zipError);
    zip_source_t* source = fileSource(path, &zipError);
    zip_t* archive = source ? zip_open_from_source(source, flags, &zipError) : nullptr;
    if (!archive) {
        if (source) {
            zip_source_free(source);
        }
        *error = QString::fromUtf8(zip_error_strerror(&zipError));
    }
    zip_error_fini(&zipError);
    return archive;
}

}  // namespace

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
    , m_isDirty(false)
{
    Logger::getInstance().info("ProjectManager initialized");
}

ProjectManager::~ProjectManager() {
    // Auto-save on destruction if dirty
    if (m_isDirty && m_project) {
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
                                   bool createSubfolder,
                                   const QString& typeId,
                                   QStringList* problems) {
    auto& logger = Logger::getInstance();
    const auto fail = [&logger, problems](const QString& problem) {
        logger.error("createProject: {}", problem.toStdString());
        if (problems) {
            problems->append(problem);
        }
        return false;
    };

    const QString fileName = safeFileName(title);
    if (fileName.isEmpty()) {
        return fail(QStringLiteral("the book has no title"));
    }

    // The type before anything is written
    const BookTypePackage* type = nullptr;
    if (!typeId.isEmpty()) {
        type = bookTypes().package(typeId);
        if (!type) {
            return fail(QStringLiteral("the book type %1 is not installed").arg(typeId));
        }
        if (type->role != PackageRole::Type) {
            return fail(QStringLiteral("%1 is not a book type").arg(typeId));
        }
    }

    // The project's folder must be new or empty; the open project stays open when it is not
    const QString projectDir = newProjectFolder(parentDir, title, createSubfolder);
    const bool folderExisted = QFileInfo(projectDir).exists();
    if (!canHoldNewProject(projectDir)) {
        return fail(QStringLiteral("%1: the folder exists and is not empty")
                        .arg(QDir::toNativeSeparators(projectDir)));
    }

    // Close any existing project first
    if (isProjectOpen() && !closeProject(true)) {
        return fail(QStringLiteral("the open project cannot be closed"));
    }

    if (!QDir().mkpath(projectDir)) {
        return fail(QStringLiteral("%1: the folder cannot be made")
                        .arg(QDir::toNativeSeparators(projectDir)));
    }

    BookProject project;
    project.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    project.created = now();
    project.modified = project.created;
    if (type) {
        project.type = ProjectType{type->id, type->version, {}};
    } else {
        // A user project starts with the kinds of the base package
        for (const KindRef& kind : bookTypes().allKinds()) {
            if (kind.package->id == QLatin1String(BASE_PACKAGE)) {
                project.kinds.append(KindReference{kind.package->id, kind.kind->id});
            }
        }
    }

    ProjectBook book;
    book.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    book.title = title.trimmed();
    book.author = author;
    book.language = language;
    book.partsLayer = type ? type->partsLayer : false;
    book.folder = QString::fromLatin1(BookProject::BOOK_FOLDER);
    project.books.append(book);

    const QString manifestPath =
        QDir(projectDir).filePath(fileName + QLatin1String(BookProject::FILE_EXTENSION));
    logger.info("Creating project at: {}", manifestPath.toStdString());

    // The folder goes back to how it was when the project cannot be made
    const auto undo = [&projectDir, folderExisted]() {
        QDir(projectDir).removeRecursively();
        if (folderExisted) {
            QDir().mkpath(projectDir);
        }
    };

    // The elements a book of the type starts with, each with the text of its kind's template.
    // Kinds that open in a window of their own wait until the program has the window.
    if (type) {
        QHash<const ElementKind*, int> numbers;
        for (const StartElement& start : bookTypes().startElements(type->id)) {
            if (!start.kind || start.kind.kind->form != ElementForm::Text) {
                logger.debug("createProject: The book does not start with {}, which the "
                             "program cannot open yet",
                             start.kind.reference().toStdString());
                continue;
            }
            const ProjectBook& first = project.books.first();
            ProjectElement element;
            element.id = project.newElementId();
            element.kind = KindReference{start.kind.package->id, start.kind.kind->id};
            element.title = start.kind.kind->defaultTitle(language, ++numbers[start.kind.kind]);
            element.file = freeChapterFile(project, projectDir, folderOf(start.place, &first),
                                           start.kind.kind->id);
            element.status = QStringLiteral("draft");

            ChapterDocument chapter;
            chapter.setKml(startingText(start.kind, first));
            chapter.setTitle(element.title);
            chapter.setStatus(element.status);
            const QString path = QDir(projectDir).filePath(element.file);
            if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !chapter.save(path)) {
                undo();
                return fail(QStringLiteral("%1: the file cannot be written")
                                .arg(QDir::toNativeSeparators(path)));
            }
            project.elementsIn(start.place).append(element);
        }
    }

    if (!project.save(manifestPath)) {
        undo();
        return fail(QStringLiteral("%1: the file cannot be written")
                        .arg(QDir::toNativeSeparators(manifestPath)));
    }

    QStringList found;
    if (!openProject(manifestPath, &found)) {
        undo();
        if (problems) {
            problems->append(found);
        }
        return fail(QStringLiteral("the new project cannot be opened"));
    }

    logger.info("Project created successfully: {}", manifestPath.toStdString());
    return true;
}

QString ProjectManager::newProjectFolder(const QString& parentDir, const QString& title,
                                         bool createSubfolder) {
    const QString fileName = safeFileName(title);
    if (fileName.isEmpty()) {
        return QString();
    }
    return QDir::cleanPath(createSubfolder ? QDir(parentDir).filePath(fileName) : parentDir);
}

bool ProjectManager::canHoldNewProject(const QString& folder) {
    const QFileInfo info(folder);
    return !info.exists() || (info.isDir() && QDir(folder).isEmpty());
}

QStringList ProjectManager::projectFileProblems(const QString& manifestPath) {
    const QFileInfo manifestInfo(manifestPath);
    if (!manifestInfo.isFile()) {
        return {QStringLiteral("%1: the file does not exist").arg(manifestInfo.fileName())};
    }
    if (manifestInfo.suffix().compare(QStringLiteral("klh"), Qt::CaseInsensitive) != 0) {
        return {QStringLiteral("%1: not a .klh file").arg(manifestInfo.fileName())};
    }
    QStringList problems;
    if (!BookProject::load(manifestPath, problems) && problems.isEmpty()) {
        problems.append(QStringLiteral("%1: cannot be read").arg(manifestInfo.fileName()));
    }
    return problems;
}

bool ProjectManager::openProject(const QString& manifestPath, QStringList* problems) {
    auto& logger = Logger::getInstance();
    const QFileInfo manifestInfo(manifestPath);

    QStringList found;
    const auto fail = [&logger, &found, problems](const QString& problem) {
        if (!problem.isEmpty()) {
            found.append(problem);
        }
        for (const QString& line : found) {
            logger.error("openProject: {}", line.toStdString());
        }
        if (problems) {
            problems->append(found);
        }
        return false;
    };

    // The file is read before the open project is closed, so the open project stays open
    // when the file cannot be read
    found = projectFileProblems(manifestPath);
    if (!found.isEmpty()) {
        return fail(QString());
    }

    // Close any existing project first
    if (isProjectOpen() && !closeProject(true)) {
        return fail(QStringLiteral("the open project cannot be closed"));
    }

    // Read again: closing saved the open project, which can be this one
    logger.info("Opening project: {}", manifestPath.toStdString());
    std::optional<BookProject> project = BookProject::load(manifestPath, found);
    if (!project) {
        return fail(QString());
    }

    // OpenSpec #00041: Acquire project lock
    const QString projectDir = manifestInfo.absolutePath();
    m_projectLock = std::make_unique<ProjectLock>(projectDir);
    if (!m_projectLock->tryAcquire()) {
        m_projectLock.reset();
        return fail(QStringLiteral("%1: the book is open in another window of Kalahari")
                        .arg(manifestInfo.fileName()));
    }

    // OpenSpec #00041: Open project database
    m_database = std::make_unique<ProjectDatabase>();
    if (!m_database->open(projectDir)) {
        m_database.reset();
        m_projectLock->release();
        m_projectLock.reset();
        return fail(QStringLiteral("project.db: the database of the book cannot be opened"));
    }
    m_backupManager = std::make_unique<BackupManager>(projectDir);

    m_project = std::make_unique<BookProject>(std::move(*project));
    m_manifestPath = std::filesystem::path(manifestInfo.absoluteFilePath().toStdWString());
    m_projectPath = m_manifestPath.parent_path();

    // The elements of packages that are not installed stay in the book
    const QStringList missing = m_project->missingPackages(bookTypes());
    if (!missing.isEmpty()) {
        logger.warn("The book uses packages that are not installed: {}",
                    missing.join(QStringLiteral(", ")).toStdString());
    }

    loadElementStates();

    // Set state
    m_isDirty = false;
    setWorkMode(WorkMode::ProjectMode);

    logger.info("Project opened successfully: {}",
                book() ? book()->title.toStdString() : std::string());
    emit projectOpened(toQString(m_projectPath));

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
    emit projectAboutToClose();

    // OpenSpec #00041: Backup and close database
    if (m_backupManager && m_database && m_database->isOpen()) {
        m_backupManager->createBackup();
        m_backupManager->rotateBackups(5);
    }
    if (m_database) {
        m_database->close();
        m_database.reset();
    }
    m_backupManager.reset();

    // OpenSpec #00041: Release project lock
    if (m_projectLock) {
        m_projectLock->release();
        m_projectLock.reset();
    }

    // Clear state
    m_project.reset();
    m_elementStates.clear();
    m_projectPath.clear();
    m_manifestPath.clear();
    m_isDirty = false;

    setWorkMode(WorkMode::NoDocument);
    emit projectClosed();

    return true;
}

bool ProjectManager::saveManifest() {
    if (!m_project || m_manifestPath.empty()) {
        Logger::getInstance().error("saveManifest: No project open");
        return false;
    }

    Logger::getInstance().debug("Saving manifest to: {}", m_manifestPath.string());

    const QDateTime previous = m_project->modified;
    m_project->modified = now();
    if (!m_project->save(getManifestPath())) {
        m_project->modified = previous;
        return false;
    }

    setDirty(false);
    Logger::getInstance().info("Manifest saved successfully");
    return true;
}

bool ProjectManager::isProjectOpen() const {
    return m_workMode == WorkMode::ProjectMode && m_project != nullptr;
}

// =============================================================================
// Paths
// =============================================================================

QString ProjectManager::getProjectPath() const {
    if (m_projectPath.empty()) return QString();
    return toQString(m_projectPath);
}

QString ProjectManager::getManifestPath() const {
    if (m_manifestPath.empty()) return QString();
    return toQString(m_manifestPath);
}

QString ProjectManager::filePathOf(const ProjectElement& element) const {
    if (element.file.isEmpty() || m_projectPath.empty()) {
        return QString();
    }
    return QDir::cleanPath(QDir(getProjectPath()).filePath(element.file));
}

// =============================================================================
// State Accessors
// =============================================================================

WorkMode ProjectManager::getWorkMode() const {
    return m_workMode;
}

BookProject* ProjectManager::project() {
    return m_project.get();
}

const BookProject* ProjectManager::project() const {
    return m_project.get();
}

ProjectBook* ProjectManager::book() {
    return m_project && !m_project->books.isEmpty() ? &m_project->books.first() : nullptr;
}

const ProjectBook* ProjectManager::book() const {
    return m_project && !m_project->books.isEmpty() ? &m_project->books.first() : nullptr;
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
// Book Types
// =============================================================================

const BookTypeRegistry& ProjectManager::bookTypes() const {
    if (!m_bookTypesLoaded) {
        m_bookTypesLoaded = true;
        const QString folder = ResourcePaths::getInstance().getBookTypesDir();
        if (folder.isEmpty()) {
            Logger::getInstance().warn("Book types: the resources folder was not found");
        }
        m_bookTypes.load(folder.isEmpty() ? QStringList{} : QStringList{folder});
    }
    return m_bookTypes;
}

void ProjectManager::loadBookTypes(const QStringList& folders) {
    m_bookTypes.load(folders);
    m_bookTypesLoaded = true;
}

KindRef ProjectManager::kindOf(const ProjectElement& element) const {
    return BookProject::kindOf(bookTypes(), element);
}

ElementForm ProjectManager::formOf(const ProjectElement& element) const {
    return BookProject::formOf(bookTypes(), element);
}

QString ProjectManager::statusOf(const ProjectElement& element) {
    return element.status.isEmpty() ? QStringLiteral("draft") : element.status;
}

QList<KindRef> ProjectManager::kindsFor(BookPlace place, const QString& groupId) const {
    QList<KindRef> kinds;
    if (!m_project) {
        return kinds;
    }
    const BookTypeRegistry& registry = bookTypes();
    QList<KindRef> offered;
    if (groupId.isEmpty()) {
        offered = m_project->kindsIn(registry, place);
    } else if (const ProjectElement* group = m_project->findElement(groupId)) {
        offered = m_project->kindsInside(registry, group->kind.kindId);
    }
    for (const KindRef& kind : offered) {
        if (kind && !limitReached(registry, *m_project, kind)) {
            kinds.append(kind);
        }
    }
    return kinds;
}

QList<KindRef> ProjectManager::textKindsFor(BookPlace place, const QString& groupId) const {
    QList<KindRef> kinds = kindsFor(place, groupId);
    kinds.removeIf([](const KindRef& kind) { return kind.kind->form != ElementForm::Text; });
    return kinds;
}

KindRef ProjectManager::chapterKindFor(BookPlace place, const QString& groupId) const {
    const QList<KindRef> kinds = textKindsFor(place, groupId);
    if (kinds.isEmpty()) {
        return {};
    }
    const std::optional<ProjectType> projectType = m_project ? m_project->type : std::nullopt;
    if (projectType) {
        const BookTypeRegistry& registry = bookTypes();
        if (const BookTypePackage* type = registry.package(projectType->id)) {
            const KindRef primary = registry.findKind(type->id, type->primaryKind);
            for (const KindRef& kind : kinds) {
                if (kind.kind == primary.kind) {
                    return kind;
                }
            }
        }
    }
    return kinds.first();
}

KindRef ProjectManager::partKind() const {
    for (const KindRef& kind : kindsFor(BookPlace::Main)) {
        if (kind.kind->form == ElementForm::Group) {
            return kind;
        }
    }
    return {};
}

QString ProjectManager::defaultTitle(const KindRef& kind) const {
    if (!m_project || !kind) {
        return QString();
    }
    const ProjectBook* first = book();
    const int number = static_cast<int>(countOf(bookTypes(), *m_project, kind)) + 1;
    return kind.kind->defaultTitle(first ? first->language : QString(), number);
}

// =============================================================================
// Elements
// =============================================================================

ProjectElement* ProjectManager::findElement(const QString& elementId) {
    return m_project ? m_project->findElement(elementId) : nullptr;
}

const ProjectElement* ProjectManager::findElement(const QString& elementId) const {
    return m_project ? m_project->findElement(elementId) : nullptr;
}

QString ProjectManager::addElement(const KindRef& kind, const QString& title, BookPlace place,
                                   const QString& groupId, qsizetype index) {
    auto& logger = Logger::getInstance();
    QList<ProjectElement>* list = listForNew(kind, place, groupId);
    if (!list) {
        return QString();
    }
    if (kind.kind->form == ElementForm::Window) {
        logger.error("addElement: Elements of {} open in a window that the program does not "
                     "have yet",
                     kind.reference().toStdString());
        return QString();
    }
    if (index > list->size()) {
        logger.error("addElement: The list has no place {}", index);
        return QString();
    }
    const qsizetype at = index < 0 ? newIndexIn(*list, kind) : index;

    ProjectElement element;
    element.id = m_project->newElementId();
    element.kind = KindReference{kind.package->id, kind.kind->id};
    element.title = title;
    if (kind.kind->form == ElementForm::Text) {
        element.file = newChapterFile(kind, place);
        element.status = QStringLiteral("draft");
        if (!writeChapterFile(element, book() ? startingText(kind, *book()) : QString())) {
            m_elementStates.remove(element.id);
            return QString();
        }
    }

    list->insert(at, element);
    if (!saveManifest()) {
        list->removeAt(at);
        if (!element.file.isEmpty()) {
            QFile::remove(filePathOf(element));
        }
        m_elementStates.remove(element.id);
        return QString();
    }

    logger.info("addElement: Added '{}' ({}, id: {})", title.toStdString(),
                kind.reference().toStdString(), element.id.toStdString());
    return element.id;
}

qsizetype ProjectManager::newIndexIn(const QList<ProjectElement>& elements,
                                     const KindRef& kind) const {
    const KindPosition position = kind ? kind.kind->position : KindPosition::Any;
    if (position == KindPosition::Start) {
        return 0;
    }
    if (position == KindPosition::End) {
        return elements.size();
    }
    qsizetype index = elements.size();
    while (index > 0) {
        const KindRef previous = kindOf(elements.at(index - 1));
        if (!previous || previous.kind->position != KindPosition::End) {
            break;
        }
        --index;
    }
    return index;
}

QString ProjectManager::addFile(const QString& sourcePath, bool copy, const KindRef& kind,
                                const QString& title, BookPlace place, const QString& groupId) {
    auto& logger = Logger::getInstance();
    QList<ProjectElement>* list = listForNew(kind, place, groupId);
    if (!list) {
        return QString();
    }
    if (kind.kind->form != ElementForm::Text) {
        logger.error("addFile: {} is not a kind of text", kind.reference().toStdString());
        return QString();
    }
    const StandaloneFile::Type fileType = StandaloneFile::typeOf(sourcePath);
    if (fileType == StandaloneFile::Type::Unsupported) {
        logger.error("addFile: {} is neither a chapter nor a text file",
                     sourcePath.toStdString());
        return QString();
    }

    ProjectElement element;
    element.id = m_project->newElementId();
    element.kind = KindReference{kind.package->id, kind.kind->id};
    element.title = title;
    element.file = newChapterFile(kind, place);
    element.status = QStringLiteral("draft");
    const QString target = filePathOf(element);

    bool written = false;
    if (fileType == StandaloneFile::Type::PlainText) {
        // Its lines become the chapter's paragraphs
        if (std::optional<TextFileContent> content = readTextFile(sourcePath)) {
            // The line end of the last line is no empty paragraph
            if (content->text.endsWith(QLatin1Char('\n'))) {
                content->text.chop(1);
            }
            written = writeChapterFile(element, editor::ClipboardHandler::textToKml(content->text));
        }
    } else if (QDir().mkpath(QFileInfo(target).absolutePath()) &&
               QFile::copy(sourcePath, target)) {
        // The chapter keeps its notes and its status
        if (std::optional<ChapterDocument> chapter = ChapterDocument::load(target)) {
            m_elementStates[element.id].notes = chapter->notes();
            if (textStatuses().contains(chapter->status())) {
                element.status = chapter->status();
            }
            written = writeChapterFile(element, std::nullopt);
        }
    }

    if (written) {
        const qsizetype at = newIndexIn(*list, kind);
        list->insert(at, element);
        if (!saveManifest()) {
            list->removeAt(at);
            written = false;
        }
    }
    if (!written) {
        logger.error("addFile: Cannot add {} as {}", sourcePath.toStdString(),
                     target.toStdString());
        QFile::remove(target);
        m_elementStates.remove(element.id);
        return QString();
    }

    // Moving the file leaves only the chapter
    if (!copy && !QFile::remove(sourcePath)) {
        logger.warn("addFile: The chapter was added, but {} could not be removed",
                    sourcePath.toStdString());
    }

    logger.info("addFile: Added {} as '{}' ({}, id: {})", sourcePath.toStdString(),
                title.toStdString(), kind.reference().toStdString(), element.id.toStdString());
    return element.id;
}

bool ProjectManager::renameElement(const QString& elementId, const QString& title) {
    ProjectElement* element = findElement(elementId);
    if (!element) {
        Logger::getInstance().warn("renameElement: Element not found: {}",
                                   elementId.toStdString());
        return false;
    }
    const QString previous = element->title;
    element->title = title;
    if (!saveManifest()) {
        element->title = previous;
        return false;
    }
    // The chapter file has the title too
    if (stateOf(elementId) && !writeChapterFile(*element, std::nullopt)) {
        Logger::getInstance().warn("renameElement: The title of {} is not in its chapter file",
                                   elementId.toStdString());
    }
    return true;
}

std::optional<ProjectElement> ProjectManager::removeElement(const QString& elementId) {
    if (!m_project) {
        return std::nullopt;
    }
    qsizetype index = -1;
    QList<ProjectElement>* list = m_project->listOf(elementId, &index);
    if (!list) {
        Logger::getInstance().warn("removeElement: Element not found: {}",
                                   elementId.toStdString());
        return std::nullopt;
    }
    ProjectElement element = list->takeAt(index);
    if (!saveManifest()) {
        list->insert(index, element);
        return std::nullopt;
    }

    // The program forgets the element and the elements inside it; their files stay
    QList<const ProjectElement*> removed{&element};
    appendAll(element.elements, removed);
    for (const ProjectElement* gone : removed) {
        m_elementStates.remove(gone->id);
    }
    Logger::getInstance().info("removeElement: Removed '{}' (id: {})",
                               element.title.toStdString(), elementId.toStdString());
    return element;
}

bool ProjectManager::moveElement(const QString& elementId, qsizetype index) {
    if (!m_project) {
        return false;
    }
    qsizetype from = -1;
    QList<ProjectElement>* list = m_project->listOf(elementId, &from);
    if (!list || index < 0 || index >= list->size()) {
        Logger::getInstance().warn("moveElement: Cannot move {} to place {}",
                                   elementId.toStdString(), index);
        return false;
    }
    if (from == index) {
        return true;
    }
    list->move(from, index);
    if (!saveManifest()) {
        list->move(index, from);
        return false;
    }
    return true;
}

qsizetype ProjectManager::startIndexOf(const QString& elementId) const {
    qsizetype from = -1;
    const QList<ProjectElement>* list =
        m_project ? std::as_const(*m_project).listOf(elementId, &from) : nullptr;
    if (!list) {
        return -1;
    }
    const auto isOfStart = [this](const ProjectElement& element) {
        const KindRef kind = kindOf(element);
        return kind && kind.kind->position == KindPosition::Start;
    };
    if (isOfStart(list->at(from))) {
        return 0;
    }
    // After the other elements of the kinds of the start that open the list
    qsizetype index = 0;
    for (qsizetype i = 0; i < list->size(); ++i) {
        if (i == from) {
            continue;
        }
        if (!isOfStart(list->at(i))) {
            break;
        }
        ++index;
    }
    return index;
}

qsizetype ProjectManager::endIndexOf(const QString& elementId) const {
    qsizetype from = -1;
    const QList<ProjectElement>* list =
        m_project ? std::as_const(*m_project).listOf(elementId, &from) : nullptr;
    if (!list) {
        return -1;
    }
    const auto isOfEnd = [this](const ProjectElement& element) {
        const KindRef kind = kindOf(element);
        return kind && kind.kind->position == KindPosition::End;
    };
    const qsizetype last = list->size() - 1;
    if (isOfEnd(list->at(from))) {
        return last;
    }
    // Before the other elements of the kinds of the end that close the list
    qsizetype index = last;
    for (qsizetype i = last; i >= 0; --i) {
        if (i == from) {
            continue;
        }
        if (!isOfEnd(list->at(i))) {
            break;
        }
        --index;
    }
    return index;
}

bool ProjectManager::setStatus(const QString& elementId, const QString& status) {
    ProjectElement* element = findElement(elementId);
    if (!element || !stateOf(elementId) || !textStatuses().contains(status)) {
        Logger::getInstance().warn("setStatus: Cannot set the status {} of {}",
                                   status.toStdString(), elementId.toStdString());
        return false;
    }
    const QString previous = element->status;
    element->status = status;
    if (!saveManifest()) {
        element->status = previous;
        return false;
    }
    // The .klh file has the status; the chapter file keeps a copy
    if (!writeChapterFile(*element, std::nullopt)) {
        Logger::getInstance().warn("setStatus: The status of {} is not in its chapter file",
                                   elementId.toStdString());
    }
    return true;
}

QString ProjectManager::notes(const QString& elementId) const {
    return m_elementStates.value(elementId).notes;
}

bool ProjectManager::setNotes(const QString& elementId, const QString& notes) {
    const ProjectElement* element = findElement(elementId);
    if (!element || !stateOf(elementId)) {
        Logger::getInstance().warn("setNotes: Not a text element: {}", elementId.toStdString());
        return false;
    }
    const QString previous = m_elementStates.value(elementId).notes;
    if (previous == notes) {
        return true;
    }
    m_elementStates[elementId].notes = notes;
    if (!writeChapterFile(*element, std::nullopt)) {
        m_elementStates[elementId].notes = previous;
        return false;
    }
    return true;
}

int ProjectManager::wordCount(const QString& elementId) const {
    return m_elementStates.value(elementId).wordCount;
}

TextStatistics ProjectManager::statisticsOf(const QList<ProjectElement>& elements) const {
    QList<const ProjectElement*> all;
    appendAll(elements, all);

    TextStatistics statistics;
    for (const ProjectElement* element : all) {
        if (formOf(*element) == ElementForm::Text) {
            ++statistics.elements;
            statistics.words += wordCount(element->id);
            ++statistics.statuses[statusOf(*element)];
        }
    }
    return statistics;
}

// =============================================================================
// Text of Chapters
// =============================================================================

QString ProjectManager::loadChapterContent(const QString& elementId) {
    auto& logger = Logger::getInstance();
    const ProjectElement* element = findElement(elementId);
    if (!element || !stateOf(elementId)) {
        logger.warn("loadChapterContent: Not a text element with a file: {}",
                    elementId.toStdString());
        return QString();
    }

    const QString path = filePathOf(*element);
    std::optional<ChapterDocument> chapter = ChapterDocument::load(path);
    if (!chapter) {
        logger.error("loadChapterContent: Cannot read {}", path.toStdString());
        return QString();
    }

    ElementState& state = m_elementStates[elementId];
    state.content = chapter->kml();
    state.loaded = true;
    state.dirty = false;
    state.wordCount = chapter->wordCount();
    state.notes = chapter->notes();
    logger.debug("Loaded .kchapter for element: {} ({} chars)", elementId.toStdString(),
                 state.content.length());
    return state.content;
}

void ProjectManager::setChapterContent(const QString& elementId, const QString& kml) {
    if (ElementState* state = stateOf(elementId)) {
        state->content = kml;
        state->loaded = true;
        state->dirty = true;
    }
}

void ProjectManager::discardChapterContent(const QString& elementId) {
    if (ElementState* state = stateOf(elementId)) {
        state->content.clear();
        state->loaded = false;
        state->dirty = false;
    }
}

bool ProjectManager::saveChapterContent(const QString& elementId) {
    const ProjectElement* element = findElement(elementId);
    if (!element || !stateOf(elementId)) {
        Logger::getInstance().warn("saveChapterContent: Not a text element with a file: {}",
                                   elementId.toStdString());
        return false;
    }

    const ElementState state = m_elementStates.value(elementId);
    if (!state.loaded || !state.dirty) {
        return true;  // Nothing to save
    }
    if (!writeChapterFile(*element, state.content)) {
        return false;
    }
    m_elementStates[elementId].dirty = false;

    Logger::getInstance().debug("Saved .kchapter for element: {} ({} words)",
                                elementId.toStdString(), wordCount(elementId));
    return true;
}

std::vector<QString> ProjectManager::getDirtyElements() const {
    std::vector<QString> dirtyIds;
    if (!m_project) {
        return dirtyIds;
    }
    for (const ProjectElement* element : allElements(*m_project)) {
        if (m_elementStates.value(element->id).dirty) {
            dirtyIds.push_back(element->id);
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
    if (!m_project) {
        return result;
    }
    for (const ProjectElement* element : m_project->readingOrder()) {
        const QString status = statusOf(*element);
        if (formOf(*element) == ElementForm::Text && status != QLatin1String("final")) {
            result.emplace_back(element->id, status);
        }
    }
    return result;
}

std::map<QString, int> ProjectManager::getStatusStatistics() const {
    std::map<QString, int> statistics;
    if (!m_project) {
        return statistics;
    }
    for (const ProjectElement* element : m_project->readingOrder()) {
        if (formOf(*element) == ElementForm::Text) {
            ++statistics[statusOf(*element)];
        }
    }
    return statistics;
}

// =============================================================================
// Private Helpers
// =============================================================================

void ProjectManager::setWorkMode(WorkMode mode) {
    if (m_workMode != mode) {
        m_workMode = mode;
        Logger::getInstance().debug("Work mode changed to: {}",
                                    mode == WorkMode::NoDocument ? "NoDocument" :
                                    mode == WorkMode::ProjectMode ? "ProjectMode" : "StandaloneMode");
        emit workModeChanged(mode);
    }
}

void ProjectManager::loadElementStates() {
    auto& logger = Logger::getInstance();
    m_elementStates.clear();
    if (!m_project) {
        return;
    }
    for (const ProjectElement* element : allElements(*m_project)) {
        if (element->file.isEmpty() || formOf(*element) != ElementForm::Text) {
            continue;
        }
        ElementState state;
        const QString path = filePathOf(*element);
        if (!QFileInfo::exists(path)) {
            logger.warn("The chapter file of '{}' is missing: {}", element->title.toStdString(),
                        path.toStdString());
        } else if (std::optional<ChapterDocument> chapter = ChapterDocument::load(path)) {
            state.wordCount = chapter->wordCount();
            state.notes = chapter->notes();
        }
        m_elementStates.insert(element->id, state);
    }
    logger.debug("Read the word counts and notes of {} chapter files", m_elementStates.size());
}

ProjectManager::ElementState* ProjectManager::stateOf(const QString& elementId) {
    const ProjectElement* element = findElement(elementId);
    if (!element || element->file.isEmpty() || formOf(*element) != ElementForm::Text) {
        return nullptr;
    }
    return &m_elementStates[elementId];
}

bool ProjectManager::writeChapterFile(const ProjectElement& element,
                                      const std::optional<QString>& kml) {
    auto& logger = Logger::getInstance();
    const QString path = filePathOf(element);
    if (path.isEmpty()) {
        logger.error("writeChapterFile: '{}' has no file", element.title.toStdString());
        return false;
    }

    // The file keeps what the program does not change here: comments, highlights, color...
    ChapterDocument chapter;
    if (QFileInfo::exists(path)) {
        if (std::optional<ChapterDocument> existing = ChapterDocument::load(path)) {
            chapter = std::move(*existing);
        } else if (!kml) {
            logger.error("writeChapterFile: {} cannot be read, so it is not changed",
                         path.toStdString());
            return false;
        }
    } else if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        logger.error("writeChapterFile: Cannot make the folder of {}", path.toStdString());
        return false;
    }

    if (kml) {
        chapter.setKml(*kml);
    }
    chapter.setTitle(element.title);
    chapter.setStatus(statusOf(element));
    chapter.setNotes(m_elementStates.value(element.id).notes);
    if (!chapter.save(path)) {
        logger.error("writeChapterFile: Cannot write {}", path.toStdString());
        return false;
    }
    m_elementStates[element.id].wordCount = chapter.wordCount();
    return true;
}

QString ProjectManager::newChapterFile(const KindRef& kind, BookPlace place) const {
    return freeChapterFile(*m_project, getProjectPath(), folderOf(place, book()),
                           kind ? kind.kind->id : QString());
}

QString ProjectManager::startingText(const KindRef& kind, const ProjectBook& book) {
    if (!kind || kind.kind->templateFile.isEmpty()) {
        return QString();
    }
    const QString path = QDir(kind.package->directory).filePath(kind.kind->templateFile);
    const std::optional<ChapterDocument> chapter = ChapterDocument::load(path);
    if (!chapter) {
        Logger::getInstance().warn("The template {} cannot be read, so the element starts "
                                   "empty",
                                   path.toStdString());
        return QString();
    }
    QString kml = chapter->kml();
    kml.replace(QStringLiteral("{title}"), book.title.toHtmlEscaped());
    kml.replace(QStringLiteral("{author}"), book.author.toHtmlEscaped());
    return kml;
}

QList<ProjectElement>* ProjectManager::listForNew(const KindRef& kind, BookPlace place,
                                                  const QString& groupId) {
    auto& logger = Logger::getInstance();
    if (!m_project) {
        logger.error("No project is open");
        return nullptr;
    }
    if (!kind) {
        logger.error("A new element needs a kind");
        return nullptr;
    }

    QList<ProjectElement>* list = nullptr;
    if (groupId.isEmpty()) {
        list = &m_project->elementsIn(place);
    } else {
        ProjectElement* group = m_project->findElement(groupId);
        if (!group || formOf(*group) != ElementForm::Group) {
            logger.error("The project has no group {}", groupId.toStdString());
            return nullptr;
        }
        list = &group->elements;
    }

    // Kinds the project offers there, below their limits
    const QList<KindRef> kinds = kindsFor(place, groupId);
    if (std::none_of(kinds.cbegin(), kinds.cend(),
                     [&kind](const KindRef& offered) { return offered.kind == kind.kind; })) {
        logger.error("An element of {} cannot be added there", kind.reference().toStdString());
        return nullptr;
    }
    return list;
}

bool ProjectManager::exportArchive(const QString& outputPath,
                                   const std::function<void(int)>& progressCallback,
                                   QStringList* problems) {
    auto& logger = Logger::getInstance();
    const auto fail = [&logger, problems](const QString& problem) {
        logger.error("exportArchive: {}", problem.toStdString());
        if (problems) {
            problems->append(problem);
        }
        return false;
    };

    if (!isProjectOpen()) {
        return fail(QStringLiteral("no project is open"));
    }
    logger.info("Exporting project to: {}", outputPath.toStdString());

    // The database file alone has everything, so its log files stay out of the archive; a log
    // that cannot be written to it goes with it
    const QString projectDir = getProjectPath();
    QStringList files = archivedFiles(projectDir, outputPath);
    if (m_database && m_database->isOpen() && !m_database->checkpoint()) {
        logger.warn("exportArchive: The database log cannot be written to project.db, so the "
                    "archive has the log");
        if (QFileInfo::exists(QDir(projectDir).filePath(DATABASE_LOG))) {
            files.append(DATABASE_LOG);
        }
    }
    if (files.isEmpty()) {
        return fail(QStringLiteral("the project folder has no files"));
    }

    QString error;
    zip_t* archive = openZip(outputPath, ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (!archive) {
        return fail(QStringLiteral("%1: %2").arg(QFileInfo(outputPath).fileName(), error));
    }

    const QDir dir(projectDir);
    for (qsizetype i = 0; i < files.size(); ++i) {
        // Entries are named with "/" in UTF-8, so the archive opens on every system
        const QString& name = files.at(i);
        zip_error_t sourceError;
        zip_error_init(&sourceError);
        zip_source_t* source = fileSource(dir.filePath(name), &sourceError);
        if (!source || zip_file_add(archive, name.toUtf8().constData(), source,
                                    ZIP_FL_ENC_UTF_8 | ZIP_FL_OVERWRITE) < 0) {
            const QString reason = source ? QString::fromUtf8(zip_strerror(archive))
                                          : QString::fromUtf8(zip_error_strerror(&sourceError));
            zip_error_fini(&sourceError);
            if (source) {
                zip_source_free(source);
            }
            zip_discard(archive);
            QFile::remove(outputPath);
            return fail(QStringLiteral("%1: %2").arg(name, reason));
        }
        zip_error_fini(&sourceError);
        if (progressCallback) {
            progressCallback(static_cast<int>((i + 1) * 100 / files.size()));
        }
    }

    // The files are read and packed here
    if (zip_close(archive) < 0) {
        const QString reason = QString::fromUtf8(zip_strerror(archive));
        zip_discard(archive);
        QFile::remove(outputPath);
        return fail(QStringLiteral("%1: %2").arg(QFileInfo(outputPath).fileName(), reason));
    }

    logger.info("exportArchive: Exported {} files", files.size());
    return true;
}

bool ProjectManager::importArchive(const QString& archivePath,
                                   const QString& targetDir,
                                   const std::function<void(int)>& progressCallback,
                                   QStringList* problems) {
    auto& logger = Logger::getInstance();
    const auto fail = [&logger, problems](const QString& problem) {
        logger.error("importArchive: {}", problem.toStdString());
        if (problems) {
            problems->append(problem);
        }
        return false;
    };

    logger.info("Importing archive: {} to {}", archivePath.toStdString(), targetDir.toStdString());

    // The project goes to a new folder named after the archive
    const QString extractDir = QDir(targetDir).filePath(archiveProjectName(archivePath));
    if (QFileInfo::exists(extractDir)) {
        return fail(QStringLiteral("%1: the folder already exists")
                        .arg(QDir::toNativeSeparators(extractDir)));
    }

    QString error;
    zip_t* archive = openZip(archivePath, ZIP_RDONLY, &error);
    if (!archive) {
        return fail(QStringLiteral("%1: %2").arg(QFileInfo(archivePath).fileName(), error));
    }
    if (!QDir().mkpath(extractDir)) {
        zip_close(archive);
        return fail(QStringLiteral("%1: the folder cannot be made")
                        .arg(QDir::toNativeSeparators(extractDir)));
    }

    // Nothing of a failed import stays behind
    const auto undo = [&extractDir]() { QDir(extractDir).removeRecursively(); };

    const QDir dir(extractDir);
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; i < count; ++i) {
        const char* rawName = zip_get_name(archive, static_cast<zip_uint64_t>(i), 0);
        const QString name = rawName ? QString::fromUtf8(rawName) : QString();
        const QString path = entryPath(name);
        if (path.isEmpty()) {
            if (!name.isEmpty() && !name.endsWith(QLatin1Char('/'))) {
                logger.warn("importArchive: Skipped {}, which is outside the project",
                            name.toStdString());
            }
            continue;
        }
        if (name.endsWith(QLatin1Char('/')) || name.endsWith(QLatin1Char('\\'))) {
            dir.mkpath(path);
            continue;
        }
        if (isArchivedLocalFile(path)) {
            logger.debug("importArchive: Skipped {}, which served the project where it was "
                         "archived",
                         path.toStdString());
            continue;
        }

        const QString filePath = dir.filePath(path);
        QString reason;
        zip_file_t* entry = zip_fopen_index(archive, static_cast<zip_uint64_t>(i), 0);
        if (!entry) {
            reason = QString::fromUtf8(zip_strerror(archive));
        } else {
            QSaveFile file(filePath);
            if (!QDir().mkpath(QFileInfo(filePath).absolutePath()) ||
                !file.open(QIODevice::WriteOnly)) {
                reason = file.errorString();
            } else {
                char buffer[8192];
                zip_int64_t read = 0;
                while ((read = zip_fread(entry, buffer, sizeof(buffer))) > 0) {
                    if (file.write(buffer, read) != read) {
                        break;
                    }
                }
                if (read < 0) {
                    reason = QString::fromUtf8(zip_file_strerror(entry));
                } else if (read > 0 || !file.commit()) {
                    reason = file.errorString();
                }
            }
            zip_fclose(entry);
        }
        if (!reason.isNull()) {
            zip_close(archive);
            undo();
            return fail(QStringLiteral("%1: %2").arg(path, reason));
        }

        if (progressCallback) {
            progressCallback(static_cast<int>((i + 1) * 100 / count));
        }
    }
    zip_close(archive);

    // The manifest is at the top of the project
    const QStringList manifests = dir.entryList({QStringLiteral("*.klh")}, QDir::Files);
    if (manifests.isEmpty()) {
        undo();
        return fail(QStringLiteral("%1: the archive has no .klh file")
                        .arg(QFileInfo(archivePath).fileName()));
    }

    logger.info("importArchive: Opening extracted project: {}", manifests.first().toStdString());
    QStringList found;
    if (!openProject(dir.filePath(manifests.first()), &found)) {
        undo();
        if (problems) {
            problems->append(found);
        }
        return false;
    }
    return true;
}

QString ProjectManager::archiveProjectName(const QString& archivePath) {
    QString name = QFileInfo(archivePath).fileName();
    for (const QLatin1String suffix : {QLatin1String(".zip"), QLatin1String(".klh")}) {
        if (name.endsWith(suffix, Qt::CaseInsensitive)) {
            name.chop(suffix.size());
        }
    }
    return name.isEmpty() ? QStringLiteral("project") : name;
}

} // namespace core
} // namespace kalahari
