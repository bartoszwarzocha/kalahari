/// @file project_manager.cpp
/// @brief Implementation of ProjectManager: the book project open in the program

#include <kalahari/core/project_manager.h>
#include <kalahari/core/backup_manager.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/resource_paths.h>
#include <kalahari/core/settings_manager.h>
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
#include <QSet>
#include <QTimeZone>
#include <QUuid>

#include <algorithm>
#include <cerrno>
#include <utility>

#include <zip.h>

namespace kalahari {
namespace core {

namespace {

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

/// @p path as the file system tells files apart: without the case of the letters on Windows
/// and macOS
QString fileKey(const QString& path) {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return QDir::cleanPath(path).toCaseFolded();
#else
    return QDir::cleanPath(path);
#endif
}

/// Files of the elements of @p project in @p projectDir, as fileKey() of their absolute
/// paths; the elements whose ids @p skipped has are left out
QSet<QString> fileKeysOf(const BookProject& project, const QString& projectDir,
                         const QSet<QString>& skipped = {}) {
    QSet<QString> keys;
    const QDir dir(projectDir);
    for (const ProjectElement* element : allElements(project)) {
        if (!element->file.isEmpty() && !skipped.contains(element->id)) {
            keys.insert(fileKey(dir.filePath(element->file)));
        }
    }
    return keys;
}

/// Chapter files (.kchapter) in folder @p folder (absolute) whose fileKey() @p used does not
/// have, as absolute paths by name
QStringList unassignedPathsIn(const QString& folder, const QSet<QString>& used) {
    const QDir dir(folder);
    QStringList paths;
    const QFileInfoList entries = dir.entryInfoList({QStringLiteral("*.kchapter")}, QDir::Files,
                                                    QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& entry : entries) {
        const QString path = QDir::cleanPath(entry.absoluteFilePath());
        if (!used.contains(fileKey(path))) {
            paths.append(path);
        }
    }
    return paths;
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

/// Largest .klh file that readArchive() reads from an archive; a book's has a few kilobytes
constexpr qsizetype MAX_ARCHIVED_MANIFEST = 16 * 1024 * 1024;

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

/// Folder of the backups of the database, in a project's folder and in its archive
constexpr QLatin1String BACKUPS_FOLDER(".backups");

/// Whether archive entry @p path is a backup of the database: a file project_*.db in the
/// .backups folder
bool isArchivedBackup(const QString& path) {
    const QString name = path.section(QLatin1Char('/'), 1);
    return path.section(QLatin1Char('/'), 0, 0) == BACKUPS_FOLDER &&
           !name.contains(QLatin1Char('/')) && name.startsWith(QLatin1String("project_")) &&
           name.endsWith(QLatin1String(".db"), Qt::CaseInsensitive);
}

/// Give file @p path the time of its last change @p time
void setModificationTime(const QString& path, const QDateTime& time) {
    QFile file(path);
    if (file.open(QIODevice::ReadWrite | QIODevice::ExistingOnly)) {
        file.setFileTime(time, QFileDevice::FileModificationTime);
    }
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

/// What libzip says went wrong
struct ZipFailure {
    QString text;              ///< Its description, in English
    int zipError = ZIP_ER_OK;  ///< Its error, ZIP_ER_*
    int systemError = 0;       ///< The error of the system (errno); 0 when it has none
};

/// The failure that libzip's @p error describes
ZipFailure failureOf(zip_error_t* error) {
    return {QString::fromUtf8(zip_error_strerror(error)), zip_error_code_zip(error),
            zip_error_system_type(error) == ZIP_ET_SYS ? zip_error_code_system(error) : 0};
}

/// Whether the error of the system @p systemError says that the disk has no room left
bool isDiskFull(int systemError) {
#ifdef EDQUOT
    if (systemError == EDQUOT) {
        return true;
    }
#endif
    return systemError == ENOSPC;
}

/// What went wrong when libzip read an archive, as @p failure says it
ArchiveProblem::Kind readingKind(const ZipFailure& failure) {
    using Kind = ArchiveProblem::Kind;
    switch (failure.zipError) {
        case ZIP_ER_NOENT:
            return Kind::NotFound;
        case ZIP_ER_OPEN:
        case ZIP_ER_READ:
        case ZIP_ER_SEEK:
        case ZIP_ER_TELL:
            return failure.systemError == ENOENT ? Kind::NotFound : Kind::CannotRead;
        case ZIP_ER_NOZIP:  // also an archive without its end
            return Kind::NotArchive;
        case ZIP_ER_INCONS:
        case ZIP_ER_CRC:
        case ZIP_ER_ZLIB:
        case ZIP_ER_EOF:
#ifdef ZIP_ER_COMPRESSED_DATA
        case ZIP_ER_COMPRESSED_DATA:  // libzip 1.6
#endif
#ifdef ZIP_ER_TRUNCATED_ZIP
        case ZIP_ER_TRUNCATED_ZIP:  // libzip 1.11
#endif
            return Kind::Damaged;
        case ZIP_ER_MULTIDISK:
        case ZIP_ER_COMPNOTSUPP:
        case ZIP_ER_ENCRNOTSUPP:
        case ZIP_ER_NOPASSWD:
        case ZIP_ER_WRONGPASSWD:
            return Kind::Unsupported;
        default:
            return Kind::Other;
    }
}

/// What went wrong when libzip wrote an archive, as @p failure says it; @p readsFiles: whether
/// libzip was reading the files that go to the archive, as it does when the archive is closed
ArchiveProblem::Kind writingKind(const ZipFailure& failure, bool readsFiles) {
    using Kind = ArchiveProblem::Kind;
    if (readsFiles && (failure.zipError == ZIP_ER_OPEN || failure.zipError == ZIP_ER_READ)) {
        return Kind::UnreadableFile;
    }
    switch (failure.zipError) {
        case ZIP_ER_OPEN:  // the archive that the new one replaces
        case ZIP_ER_READ:
        case ZIP_ER_TMPOPEN:
        case ZIP_ER_WRITE:
        case ZIP_ER_RENAME:
        case ZIP_ER_CLOSE:
        case ZIP_ER_REMOVE:
        case ZIP_ER_SEEK:
        case ZIP_ER_TELL:
        case ZIP_ER_RDONLY:
            return isDiskFull(failure.systemError) ? Kind::DiskFull : Kind::CannotWrite;
        default:
            return Kind::Other;
    }
}

/// What went wrong when @p file was written
ArchiveProblem::Kind writingKind(const QFileDevice& file) {
    // Qt says that the disk has no room left with ResourceError
    return file.error() == QFileDevice::ResourceError ? ArchiveProblem::Kind::DiskFull
                                                       : ArchiveProblem::Kind::CannotWrite;
}

/// Archive @p path opened with libzip's @p flags; nullptr, with what went wrong in @p failure,
/// when it cannot be opened
zip_t* openZip(const QString& path, int flags, ZipFailure* failure) {
    zip_error_t zipError;
    zip_error_init(&zipError);
    zip_source_t* source = fileSource(path, &zipError);
    zip_t* archive = source ? zip_open_from_source(source, flags, &zipError) : nullptr;
    if (!archive) {
        if (source) {
            zip_source_free(source);
        }
        *failure = failureOf(&zipError);
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
                                   QStringList* problems,
                                   const std::optional<BookSections>& sections) {
    NewProject project;
    project.parentDir = parentDir;
    project.title = title;
    project.author = author;
    project.language = language;
    project.createSubfolder = createSubfolder;
    project.typeId = typeId;
    project.sections = sections;
    return createProject(project, problems);
}

bool ProjectManager::createProject(const NewProject& project, QStringList* problems) {
    auto& logger = Logger::getInstance();
    const auto fail = [&logger, problems](const QStringList& found) {
        for (const QString& problem : found) {
            logger.error("createProject: {}", problem.toStdString());
        }
        if (problems) {
            problems->append(found);
        }
        return false;
    };

    // What is wrong comes before anything changes: the open project stays open
    const QStringList wrong = newProjectProblems(project);
    if (!wrong.isEmpty()) {
        return fail(wrong);
    }
    if (isProjectOpen() && !closeProject(true)) {
        return fail({QStringLiteral("the open project cannot be closed")});
    }

    const QString projectDir =
        newProjectFolder(project.parentDir, project.title, project.createSubfolder);
    const bool folderExisted = QFileInfo(projectDir).exists();
    const QString manifestPath = createProjectWithoutOpening(project, problems);
    if (manifestPath.isEmpty()) {
        return false;
    }

    QStringList found;
    if (!openProject(manifestPath, &found)) {
        removeNewProject(projectDir, folderExisted);
        found.append(QStringLiteral("the new project cannot be opened"));
        return fail(found);
    }

    logger.info("Project created successfully: {}", manifestPath.toStdString());
    return true;
}

QString ProjectManager::createProjectWithoutOpening(const NewProject& request,
                                                    QStringList* problems) const {
    auto& logger = Logger::getInstance();
    const auto fail = [&logger, problems](const QStringList& found) {
        for (const QString& problem : found) {
            logger.error("createProject: {}", problem.toStdString());
        }
        if (problems) {
            problems->append(found);
        }
        return QString();
    };

    const QStringList wrong = newProjectProblems(request);
    if (!wrong.isEmpty()) {
        return fail(wrong);
    }
    const BookTypePackage* type =
        request.typeId.isEmpty() ? nullptr : bookTypes().package(request.typeId);

    const QString projectDir =
        newProjectFolder(request.parentDir, request.title, request.createSubfolder);
    const bool folderExisted = QFileInfo(projectDir).exists();
    if (!QDir().mkpath(projectDir)) {
        return fail({QStringLiteral("%1: the folder cannot be made")
                         .arg(QDir::toNativeSeparators(projectDir))});
    }

    BookProject project;
    project.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    project.created = now();
    project.modified = project.created;
    if (type) {
        project.type = ProjectType{type->id, type->version, {}};
    } else if (request.palette) {
        // A user project has the kinds the writer chose in the palette
        for (const KindRef& kind : request.palette->kinds) {
            project.kinds.append(KindReference{kind.package->id, kind.kind->id});
        }
    } else {
        // ... or, without the palette, the kinds of the base package
        for (const KindRef& kind : bookTypes().allKinds()) {
            if (kind.package->id == QLatin1String(BASE_PACKAGE)) {
                project.kinds.append(KindReference{kind.package->id, kind.kind->id});
            }
        }
    }

    ProjectBook book;
    book.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    book.title = request.title.trimmed();
    book.author = request.author;
    book.language = request.language;
    book.partsLayer = type ? type->partsLayer : false;
    if (request.sections) {
        book.setSections(*request.sections);
    }
    book.folder = QString::fromLatin1(BookProject::BOOK_FOLDER);
    project.books.append(book);

    QString manifestPath = QDir(projectDir).filePath(
        safeFileName(request.title) + QLatin1String(BookProject::FILE_EXTENSION));
    logger.info("Creating project at: {}", manifestPath.toStdString());

    // Numbers of the default titles, for each kind: "Chapter 1", "Part I"
    QHash<const ElementKind*, int> numbers;
    const auto newElement = [&project, &numbers, &request](const KindRef& kind) {
        ProjectElement element;
        element.id = project.newElementId();
        element.kind = KindReference{kind.package->id, kind.kind->id};
        element.title = kind.kind->defaultTitle(request.language, ++numbers[kind.kind]);
        return element;
    };

    // A text element with the text of its kind's template, put in @p list at @p index
    const auto addText = [&](const KindRef& kind, BookPlace place, QList<ProjectElement>& list,
                             qsizetype index) {
        const ProjectBook& first = project.books.first();
        ProjectElement element = newElement(kind);
        element.file = freeChapterFile(project, projectDir, folderOf(place, &first),
                                       kind.kind->id);
        element.status = QStringLiteral("draft");

        ChapterDocument chapter;
        chapter.setKml(startingText(kind, first));
        chapter.setTitle(element.title);
        chapter.setStatus(element.status);
        const QString path = QDir(projectDir).filePath(element.file);
        if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !chapter.save(path)) {
            return QStringLiteral("%1: the file cannot be written")
                .arg(QDir::toNativeSeparators(path));
        }
        list.insert(index, element);
        return QString();
    };

    // Kinds that open in a window of their own wait until the program has the window
    const auto notYet = [&logger](const KindRef& kind) {
        logger.debug("createProject: The book does not start with {}, which the program cannot "
                     "open yet",
                     kind.reference().toStdString());
    };

    if (type) {
        // The elements a book of the type starts with, in the order of the type
        for (const StartElement& start : bookTypes().startElements(type->id)) {
            if (!start.kind || start.kind.kind->form != ElementForm::Text) {
                notYet(start.kind);
                continue;
            }
            QList<ProjectElement>& list = project.elementsIn(start.place);
            const QString problem = addText(start.kind, start.place, list, list.size());
            if (!problem.isEmpty()) {
                removeNewProject(projectDir, folderExisted);
                return fail({problem});
            }
        }
    } else if (request.palette) {
        // The elements the writer chose: the groups first, so that the text elements that can
        // be inside them go into them
        struct MadeGroup {
            BookPlace place;
            QString id;
            QString kindId;
        };
        QList<MadeGroup> groups;
        for (const StartElement& start : request.palette->elements) {
            if (start.kind.kind->form != ElementForm::Group) {
                continue;
            }
            QList<ProjectElement>& list = project.elementsIn(start.place);
            const ProjectElement group = newElement(start.kind);
            list.insert(BookProject::newIndexIn(bookTypes(), list, start.kind), group);
            groups.append(MadeGroup{start.place, group.id, start.kind.kind->id});
        }
        for (const StartElement& start : request.palette->elements) {
            if (start.kind.kind->form == ElementForm::Window) {
                notYet(start.kind);
                continue;
            }
            if (start.kind.kind->form != ElementForm::Text) {
                continue;  // a group, made above
            }
            // The first group of its place that can have it, unless it stands at the start or
            // at the end of the place
            QList<ProjectElement>* list = &project.elementsIn(start.place);
            if (start.kind.kind->position == KindPosition::Any) {
                const auto group = std::find_if(
                    groups.cbegin(), groups.cend(), [&start](const MadeGroup& made) {
                        return made.place == start.place &&
                               start.kind.kind->allowsInside(made.kindId);
                    });
                if (group != groups.cend()) {
                    list = &project.findElement(group->id)->elements;
                }
            }
            const QString problem = addText(start.kind, start.place, *list,
                                            BookProject::newIndexIn(bookTypes(), *list,
                                                                    start.kind));
            if (!problem.isEmpty()) {
                removeNewProject(projectDir, folderExisted);
                return fail({problem});
            }
        }
    }

    if (!project.save(manifestPath)) {
        removeNewProject(projectDir, folderExisted);
        return fail({QStringLiteral("%1: the file cannot be written")
                         .arg(QDir::toNativeSeparators(manifestPath))});
    }
    return manifestPath;
}

QStringList ProjectManager::newProjectProblems(const NewProject& project) const {
    QStringList problems;
    if (safeFileName(project.title).isEmpty()) {
        problems.append(QStringLiteral("the book has no title"));
    }

    if (!project.typeId.isEmpty()) {
        const BookTypePackage* type = bookTypes().package(project.typeId);
        if (!type) {
            problems.append(
                QStringLiteral("the book type %1 is not installed").arg(project.typeId));
        } else if (type->role != PackageRole::Type) {
            problems.append(QStringLiteral("%1 is not a book type").arg(project.typeId));
        }
        if (project.palette) {
            problems.append(QStringLiteral("a book of a type takes no palette"));
        }
    } else if (project.palette) {
        // The kinds of the palette, and its elements of these kinds, each in a place the kind
        // allows
        if (project.palette->kinds.isEmpty()) {
            problems.append(QStringLiteral("the project has no kinds"));
        }
        for (const KindRef& kind : project.palette->kinds) {
            if (!kind || !kind.package) {
                problems.append(QStringLiteral("a kind of the palette is not installed"));
            }
        }
        for (const StartElement& start : project.palette->elements) {
            if (!start.kind || !start.kind.package) {
                problems.append(QStringLiteral("an element of the palette has no kind"));
                continue;
            }
            const auto& kinds = project.palette->kinds;
            const bool ofProject =
                std::any_of(kinds.cbegin(), kinds.cend(), [&start](const KindRef& kind) {
                    return kind.kind == start.kind.kind;
                });
            if (!ofProject) {
                problems.append(QStringLiteral("%1 is not a kind of the project")
                                    .arg(start.kind.reference()));
            } else if (!start.kind.kind->allows(start.place)) {
                problems.append(QStringLiteral("%1 cannot be in %2")
                                    .arg(start.kind.reference(), bookPlaceName(start.place)));
            }
        }
    }

    // The project's folder must be new or empty
    const QString projectDir =
        newProjectFolder(project.parentDir, project.title, project.createSubfolder);
    if (!projectDir.isEmpty() && !canHoldNewProject(projectDir)) {
        problems.append(QStringLiteral("%1: the folder exists and is not empty")
                            .arg(QDir::toNativeSeparators(projectDir)));
    }
    return problems;
}

void ProjectManager::removeNewProject(const QString& projectDir, bool folderExisted) {
    QDir(projectDir).removeRecursively();
    if (folderExisted) {
        QDir().mkpath(projectDir);
    }
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
        // The copy of project.db alone has everything: what the database's log still holds
        // goes into the file first
        if (!m_database->checkpoint()) {
            Logger::getInstance().warn("closeProject: The database log cannot be written to "
                                       "project.db, so its backup can miss the last changes");
        }
        // Where the Folders page of the Settings says, and as many copies as it says
        const auto& settings = SettingsManager::getInstance();
        m_backupManager->setCommonFolder(
            QString::fromStdString(settings.get<std::string>(BackupManager::FOLDER_SETTING)),
            m_project ? m_project->id : QString());
        m_backupManager->createBackup();
        m_backupManager->rotateBackups(
            std::max(1, settings.get<int>(BackupManager::COUNT_SETTING)));
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

bool ProjectManager::isTextStatus(const QString& status) {
    return textStatuses().contains(status);
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

KindRef ProjectManager::mainTextKind() const {
    if (!m_project) {
        return {};
    }
    const BookTypeRegistry& registry = bookTypes();
    if (const std::optional<ProjectType>& projectType = m_project->type) {
        if (const BookTypePackage* type = registry.package(projectType->id)) {
            if (const KindRef primary = registry.findKind(type->id, type->primaryKind)) {
                return primary;
            }
        }
    }
    for (const KindRef& kind : m_project->kindsIn(registry, BookPlace::Main)) {
        if (kind.kind->form == ElementForm::Text) {
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

const ProjectElement* ProjectManager::elementWithFile(const QString& path) const {
    if (!m_project || path.isEmpty()) {
        return nullptr;
    }
    // QFileInfo compares the files themselves when they are on the disk
    const QFileInfo file(path);
    for (const ProjectElement* element : allElements(*m_project)) {
        if (!element->file.isEmpty() && QFileInfo(filePathOf(*element)) == file) {
            return element;
        }
    }
    return nullptr;
}

QString ProjectManager::addElement(const KindRef& kind, const QString& title, BookPlace place,
                                   const QString& groupId, qsizetype index,
                                   const QStringList& takeInside) {
    auto& logger = Logger::getInstance();
    if (!m_project) {
        logger.error("addElement: No project is open");
        return QString();
    }

    // The project as it was comes back when the change cannot be saved. It shares its lists
    // with the project until they change, so it is taken before any pointer into them.
    const BookProject previous = *m_project;

    // The place the writer chose, or the place of its kind
    const ElementPlace at =
        index < 0 ? newPlaceOf(kind, place, groupId) : ElementPlace{place, groupId, index};
    QList<ProjectElement>* list = listForNew(kind, place, at.groupId);
    if (!list) {
        return QString();
    }
    if (kind.kind->form == ElementForm::Window) {
        logger.error("addElement: Elements of {} open in a window that the program does not "
                     "have yet",
                     kind.reference().toStdString());
        return QString();
    }
    if (at.index > list->size()) {
        logger.error("addElement: The list has no place {}", at.index);
        return QString();
    }

    // Only a group takes elements inside it: elements of the content of its part of the book,
    // which has no groups, each once, and only those that can be inside it
    const QList<const ProjectElement*> content =
        takeInside.isEmpty() ? QList<const ProjectElement*>()
                             : BookProject::contentOf(bookTypes(), m_project->elementsIn(place));
    for (const QString& insideId : takeInside) {
        const auto inside =
            std::find_if(content.cbegin(), content.cend(), [&insideId](const ProjectElement* e) {
                return e->id == insideId;
            });
        const KindRef insideKind = inside != content.cend() ? kindOf(**inside) : KindRef{};
        if (kind.kind->form != ElementForm::Group || !insideKind ||
            !insideKind.kind->allowsInside(kind.kind->id) || takeInside.count(insideId) > 1) {
            logger.error("addElement: {} cannot go inside a new element of {}",
                         insideId.toStdString(), kind.reference().toStdString());
            return QString();
        }
    }

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

    const auto restore = [this, &previous, &element]() {
        *m_project = previous;
        if (!element.file.isEmpty()) {
            QFile::remove(filePathOf(element));
        }
        m_elementStates.remove(element.id);
    };

    // The elements it takes go inside it in order; it still goes before the element that was
    // at its place, whose list can lose the elements it takes. The groups stay where they are,
    // but their lists can move in memory.
    qsizetype insertAt = at.index;
    if (!takeInside.isEmpty()) {
        QString beforeId;
        for (qsizetype i = at.index; i < list->size() && beforeId.isEmpty(); ++i) {
            if (!takeInside.contains(list->at(i).id)) {
                beforeId = list->at(i).id;
            }
        }
        for (const QString& insideId : takeInside) {
            element.elements.append(*m_project->takeElement(insideId));
        }
        list = at.groupId.isEmpty() ? &m_project->elementsIn(place)
                                    : &m_project->findElement(at.groupId)->elements;
        insertAt = list->size();
        for (qsizetype i = 0; i < list->size(); ++i) {
            if (list->at(i).id == beforeId) {
                insertAt = i;
            }
        }
    }

    list->insert(insertAt, element);
    if (!saveManifest()) {
        restore();
        return QString();
    }

    logger.info("addElement: Added '{}' ({}, id: {}) with {} elements inside it",
                title.toStdString(), kind.reference().toStdString(), element.id.toStdString(),
                takeInside.size());
    return element.id;
}

qsizetype ProjectManager::newIndexIn(const QList<ProjectElement>& elements,
                                     const KindRef& kind) const {
    return BookProject::newIndexIn(bookTypes(), elements, kind);
}

ElementPlace ProjectManager::newPlaceOf(const KindRef& kind, BookPlace place,
                                        const QString& groupId) const {
    ElementPlace at{place, groupId, 0};
    if (!m_project || !kind) {
        return at;
    }
    const ProjectElement* group = groupId.isEmpty() ? nullptr : findElement(groupId);
    if (!groupId.isEmpty() && !group) {
        return at;
    }
    const QList<ProjectElement>& list = group ? group->elements : m_project->elementsIn(place);
    at.index = newIndexIn(list, kind);

    // In the body: before the elements that close it, also when they end a part
    if (place != BookPlace::Main || group || kind.kind->position != KindPosition::Any ||
        kind.kind->form == ElementForm::Group) {
        return at;
    }
    const QList<const ProjectElement*> closing = BookProject::closingElementsOf(bookTypes(), list);
    const std::optional<ElementPlace> first =
        closing.isEmpty() ? std::nullopt : m_project->placeOf(closing.first()->id);
    if (!first) {
        return at;
    }
    if (first->groupId.isEmpty()) {
        at.index = first->index;
        return at;
    }
    const QList<KindRef> kinds = kindsFor(place, first->groupId);
    const bool fits = std::any_of(kinds.cbegin(), kinds.cend(), [&kind](const KindRef& offered) {
        return offered.kind == kind.kind;
    });
    return fits ? *first : at;
}

QString ProjectManager::addFile(const QString& sourcePath, bool copy, const KindRef& kind,
                                const QString& title, BookPlace place, const QString& groupId,
                                qsizetype index) {
    auto& logger = Logger::getInstance();

    // The place the writer chose, or the place of its kind
    const ElementPlace at =
        index < 0 ? newPlaceOf(kind, place, groupId) : ElementPlace{place, groupId, index};
    QList<ProjectElement>* list = listForNew(kind, place, at.groupId);
    if (!list) {
        return QString();
    }
    if (kind.kind->form != ElementForm::Text) {
        logger.error("addFile: {} is not a kind of text", kind.reference().toStdString());
        return QString();
    }
    if (at.index > list->size()) {
        logger.error("addFile: The list has no place {}", at.index);
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
        list->insert(at.index, element);
        if (!saveManifest()) {
            list->removeAt(at.index);
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

bool ProjectManager::setSections(const BookSections& sections) {
    ProjectBook* current = book();
    if (!current) {
        Logger::getInstance().warn("setSections: No project is open");
        return false;
    }
    const BookSections previous = current->sections();
    const QJsonObject previousExtra = current->sectionNamesExtra;
    current->setSections(sections);
    if (!saveManifest()) {
        current->setSections(previous);
        current->sectionNamesExtra = previousExtra;
        return false;
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

QStringList ProjectManager::filesOf(const ProjectElement& element) const {
    QList<const ProjectElement*> inside{&element};
    appendAll(element.elements, inside);
    QSet<QString> ids;
    for (const ProjectElement* each : inside) {
        ids.insert(each->id);
    }
    // Files that other elements have too stay
    const QSet<QString> elsewhere =
        m_project ? fileKeysOf(*m_project, getProjectPath(), ids) : QSet<QString>();

    QStringList files;
    QSet<QString> listed;
    for (const ProjectElement* each : inside) {
        const QString path = filePathOf(*each);
        if (path.isEmpty() || !QFileInfo(path).isFile()) {
            continue;
        }
        const QString key = fileKey(path);
        if (!elsewhere.contains(key) && !listed.contains(key)) {
            files.append(path);
            listed.insert(key);
        }
    }
    return files;
}

QStringList ProjectManager::unassignedFolders() const {
    QStringList folders;
    if (!m_project) {
        return folders;
    }
    const QDir projectDir(getProjectPath());
    for (const BookPlace place : {BookPlace::Main, BookPlace::Workshop}) {
        const QString folder = QDir::cleanPath(projectDir.filePath(folderOf(place, book())));
        if (std::none_of(folders.cbegin(), folders.cend(), [&folder](const QString& listed) {
                return fileKey(listed) == fileKey(folder);
            })) {
            folders.append(folder);
        }
    }
    return folders;
}

QStringList ProjectManager::unassignedPaths() const {
    QStringList paths;
    if (!m_project) {
        return paths;
    }
    const QSet<QString> used = fileKeysOf(*m_project, getProjectPath());
    for (const QString& folder : unassignedFolders()) {
        paths.append(unassignedPathsIn(folder, used));
    }
    return paths;
}

QList<UnassignedFile> ProjectManager::unassignedFiles() const {
    QList<UnassignedFile> unassigned;
    const QDir projectDir(getProjectPath());
    for (const QString& path : unassignedPaths()) {
        UnassignedFile file;
        file.file = projectDir.relativeFilePath(path);
        if (const std::optional<ChapterDocument> chapter = ChapterDocument::load(path)) {
            file.title = chapter->title();
        }
        unassigned.append(file);
    }
    return unassigned;
}

bool ProjectManager::listsAsUnassigned(const QString& path) const {
    const QFileInfo info(path);
    // As unassignedPathsIn() finds them: hidden files are left out
    if (info.suffix().compare(QStringLiteral("kchapter"), Qt::CaseInsensitive) != 0 ||
        info.isHidden()) {
        return false;
    }
    const QString folder = fileKey(info.absolutePath());
    const QStringList folders = unassignedFolders();
    return std::any_of(folders.cbegin(), folders.cend(),
                       [&folder](const QString& each) { return fileKey(each) == folder; });
}

QString ProjectManager::addUnassignedFile(const QString& file, const KindRef& kind,
                                          const QString& title, BookPlace place,
                                          const QString& groupId, qsizetype index) {
    auto& logger = Logger::getInstance();
    if (!m_project) {
        logger.error("addUnassignedFile: No project is open");
        return QString();
    }

    // One of unassignedFiles()
    const QString relative = QDir::cleanPath(file);
    const QString path = QDir::cleanPath(QDir(getProjectPath()).filePath(relative));
    const QStringList unassigned = unassignedPaths();
    if (relative.isEmpty() || QDir::isAbsolutePath(relative) ||
        std::none_of(unassigned.cbegin(), unassigned.cend(), [&path](const QString& free) {
            return fileKey(free) == fileKey(path);
        })) {
        logger.error("addUnassignedFile: {} is not a chapter file of the book or the Workshop "
                     "that no element has",
                     file.toStdString());
        return QString();
    }
    const std::optional<ChapterDocument> chapter = ChapterDocument::load(path);
    if (!chapter) {
        logger.error("addUnassignedFile: {} cannot be read", path.toStdString());
        return QString();
    }

    // The place the writer chose, or the place of its kind
    const ElementPlace at =
        index < 0 ? newPlaceOf(kind, place, groupId) : ElementPlace{place, groupId, index};
    QList<ProjectElement>* list = listForNew(kind, place, at.groupId);
    if (!list) {
        return QString();
    }
    if (kind.kind->form != ElementForm::Text) {
        logger.error("addUnassignedFile: {} is not a kind of text",
                     kind.reference().toStdString());
        return QString();
    }
    if (at.index > list->size()) {
        logger.error("addUnassignedFile: The list has no place {}", at.index);
        return QString();
    }

    // The element keeps the status of the file
    ProjectElement element;
    element.id = m_project->newElementId();
    element.kind = KindReference{kind.package->id, kind.kind->id};
    element.title = title;
    element.file = relative;
    element.status = textStatuses().contains(chapter->status()) ? chapter->status()
                                                                 : QStringLiteral("draft");
    list->insert(at.index, element);
    if (!saveManifest()) {
        list->removeAt(at.index);
        return QString();
    }

    // ... and its notes; the file takes the title
    ElementState& state = m_elementStates[element.id];
    state.notes = chapter->notes();
    state.wordCount = chapter->wordCount();
    if (!writeChapterFile(element, std::nullopt)) {
        logger.warn("addUnassignedFile: The title of {} is not in its chapter file",
                    element.id.toStdString());
    }

    logger.info("addUnassignedFile: Added {} as '{}' ({}, id: {})", file.toStdString(),
                title.toStdString(), kind.reference().toStdString(), element.id.toStdString());
    return element.id;
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

QList<ElementPlace> ProjectManager::movePlacesOf(const QString& elementId) const {
    return m_project ? m_project->movePlacesOf(bookTypes(), elementId) : QList<ElementPlace>{};
}

bool ProjectManager::canMoveTo(const QString& elementId, const ElementPlace& to) const {
    return movePlacesOf(elementId).contains(to);
}

bool ProjectManager::moveElementTo(const QString& elementId, const ElementPlace& to) {
    auto& logger = Logger::getInstance();
    const std::optional<ElementPlace> from =
        m_project ? m_project->placeOf(elementId) : std::nullopt;
    if (!from) {
        logger.warn("moveElementTo: The project has no element {}", elementId.toStdString());
        return false;
    }
    if (*from == to) {
        return true;  // It is there
    }
    const std::string where = to.groupId.isEmpty() ? bookPlaceName(to.place).toStdString()
                                                   : to.groupId.toStdString();

    // The project as it was comes back when the change cannot be saved
    const BookProject previous = *m_project;
    if (!m_project->moveElementTo(bookTypes(), elementId, to)) {
        logger.warn("moveElementTo: {} cannot go to place {} of {}", elementId.toStdString(),
                    to.index, where);
        return false;
    }
    if (!saveManifest()) {
        *m_project = previous;
        return false;
    }
    logger.info("moveElementTo: Moved {} to place {} of {}", elementId.toStdString(), to.index,
                where);
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
    const auto state = m_elementStates.constFind(elementId);
    if (state == m_elementStates.constEnd()) {
        return 0;
    }
    return state->editorWordCount.value_or(state->wordCount);
}

void ProjectManager::setEditorWordCount(const QString& elementId, int words) {
    if (ElementState* state = stateOf(elementId)) {
        state->editorWordCount = words;
    }
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
            const KindRef kind = kindOf(*element);
            ++statistics.kinds[kind ? kind.reference() : element->kind.toString()];
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
    state.editorWordCount.reset();
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
        state->editorWordCount.reset();  // the file counts the words again
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

bool ProjectManager::saveChapterDetails(const QString& elementId) {
    const ProjectElement* element = findElement(elementId);
    if (!element || !stateOf(elementId)) {
        return true;  // Not a text element with a file
    }
    const QString path = filePathOf(*element);
    if (!QFileInfo::exists(path)) {
        return true;  // A missing file is not made here
    }
    const std::optional<ChapterDocument> chapter = ChapterDocument::load(path);
    if (chapter && chapter->title() == element->title &&
        chapter->status() == statusOf(*element) &&
        chapter->notes() == m_elementStates.value(elementId).notes) {
        return true;
    }
    if (!writeChapterFile(*element, std::nullopt)) {
        return false;
    }
    Logger::getInstance().info("saveChapterDetails: The file of '{}' has its title, status and "
                               "notes",
                               element->title.toStdString());
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
    ElementState& state = m_elementStates[element.id];
    state.wordCount = chapter.wordCount();
    if (kml) {
        state.editorWordCount.reset();  // the file has the text of the editor now
    }
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

QStringList ProjectManager::databaseBackups() const {
    if (!isProjectOpen()) {
        return {};
    }
    const QString projectDir = getProjectPath();
    BackupManager kept(projectDir);
    kept.setCommonFolder(QString::fromStdString(SettingsManager::getInstance().get<std::string>(
                             BackupManager::FOLDER_SETTING)),
                         m_project->id);
    QStringList backups = kept.availableBackups();

    // The backups made before a common folder was chosen stay in the project's own folder
    QStringList names;
    for (const QString& backup : std::as_const(backups)) {
        names.append(QFileInfo(backup).fileName());
    }
    for (const QString& backup : BackupManager(projectDir).availableBackups()) {
        if (!names.contains(QFileInfo(backup).fileName())) {
            backups.append(backup);
        }
    }
    std::stable_sort(backups.begin(), backups.end(), [](const QString& a, const QString& b) {
        return QFileInfo(a).lastModified() > QFileInfo(b).lastModified();
    });
    return backups;
}

bool ProjectManager::exportArchive(const QString& outputPath,
                                   const std::function<void(int)>& progressCallback,
                                   ArchiveProblem* problem, bool withBackups) {
    auto& logger = Logger::getInstance();
    const auto fail = [&logger, problem](ArchiveProblem::Kind kind, const QString& line) {
        logger.error("exportArchive: {}", line.toStdString());
        if (problem) {
            problem->kind = kind;
            problem->lines.append(line);
        }
        return false;
    };

    if (!isProjectOpen()) {
        return fail(ArchiveProblem::Kind::Other, QStringLiteral("no project is open"));
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
        return fail(ArchiveProblem::Kind::Other, QStringLiteral("the project folder has no files"));
    }

    ZipFailure failure;
    zip_t* archive = openZip(outputPath, ZIP_CREATE | ZIP_TRUNCATE, &failure);
    if (!archive) {
        return fail(writingKind(failure, false),
                    QStringLiteral("%1: %2").arg(QFileInfo(outputPath).fileName(), failure.text));
    }

    // The files of the folder, and the backups of the database from wherever they are kept
    const QDir dir(projectDir);
    QList<std::pair<QString, QString>> entries;  // the name in the archive and the file
    for (const QString& name : std::as_const(files)) {
        entries.append({name, dir.filePath(name)});
    }
    if (withBackups) {
        for (const QString& backup : databaseBackups()) {
            entries.append(
                {QString(BACKUPS_FOLDER) + QLatin1Char('/') + QFileInfo(backup).fileName(),
                 backup});
        }
    }

    for (const auto& [name, path] : std::as_const(entries)) {
        // Entries are named with "/" in UTF-8, so the archive opens on every system
        zip_error_t sourceError;
        zip_error_init(&sourceError);
        zip_source_t* source = fileSource(path, &sourceError);
        if (!source || zip_file_add(archive, name.toUtf8().constData(), source,
                                    ZIP_FL_ENC_UTF_8 | ZIP_FL_OVERWRITE) < 0) {
            // A file of the book that cannot be read, or an entry libzip does not take
            const ArchiveProblem::Kind kind =
                source ? ArchiveProblem::Kind::Other : ArchiveProblem::Kind::UnreadableFile;
            const QString reason =
                (source ? failureOf(zip_get_error(archive)) : failureOf(&sourceError)).text;
            zip_error_fini(&sourceError);
            if (source) {
                zip_source_free(source);
            }
            zip_discard(archive);
            QFile::remove(outputPath);
            return fail(kind, QStringLiteral("%1: %2").arg(name, reason));
        }
        zip_error_fini(&sourceError);
    }

    // The files are read and packed here, so the progress is that of closing the archive
    std::function<void(int)> progress = progressCallback;
    if (progress) {
        zip_register_progress_callback_with_state(
            archive, 0.01,
            [](zip_t*, double done, void* callback) {
                (*static_cast<std::function<void(int)>*>(callback))(static_cast<int>(done * 100));
            },
            nullptr, &progress);
    }
    if (zip_close(archive) < 0) {
        failure = failureOf(zip_get_error(archive));
        zip_discard(archive);
        QFile::remove(outputPath);
        return fail(writingKind(failure, true),
                    QStringLiteral("%1: %2").arg(QFileInfo(outputPath).fileName(), failure.text));
    }
    if (progress) {
        progress(100);
    }

    logger.info("exportArchive: Exported {} files", entries.size());
    return true;
}

std::optional<BookProject> ProjectManager::readArchive(const QString& archivePath,
                                                       ArchiveProblem* problem, int* backups) {
    if (backups) {
        *backups = 0;
    }
    QStringList found;
    const auto fail = [&found, problem](ArchiveProblem::Kind kind,
                                        const QString& line) -> std::optional<BookProject> {
        if (!line.isEmpty()) {
            found.append(line);
        }
        for (const QString& each : std::as_const(found)) {
            Logger::getInstance().warn("readArchive: {}", each.toStdString());
        }
        if (problem) {
            problem->kind = kind;
            problem->lines.append(found);
        }
        return std::nullopt;
    };

    const QString archiveName = QFileInfo(archivePath).fileName();
    ZipFailure failure;
    zip_t* archive = openZip(archivePath, ZIP_RDONLY, &failure);
    if (!archive) {
        return fail(readingKind(failure), QStringLiteral("%1: %2").arg(archiveName, failure.text));
    }

    // The .klh file at the top of the project that unpackArchive() finds: the first by name
    QString manifest;
    zip_uint64_t manifestIndex = 0;
    int backupCount = 0;
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; i < count; ++i) {
        const char* rawName = zip_get_name(archive, static_cast<zip_uint64_t>(i), 0);
        const QString name = rawName ? QString::fromUtf8(rawName) : QString();
        const QString path = entryPath(name);
        if (!name.endsWith(QLatin1Char('/')) && isArchivedBackup(path)) {
            ++backupCount;
            continue;
        }
        if (path.isEmpty() || path.contains(QLatin1Char('/')) ||
            name.endsWith(QLatin1Char('/')) || name.endsWith(QLatin1Char('\\')) ||
            !path.endsWith(QLatin1String(BookProject::FILE_EXTENSION), Qt::CaseInsensitive)) {
            continue;
        }
        if (manifest.isEmpty() || path.compare(manifest, Qt::CaseInsensitive) < 0) {
            manifest = path;
            manifestIndex = static_cast<zip_uint64_t>(i);
        }
    }
    if (manifest.isEmpty()) {
        zip_close(archive);
        return fail(ArchiveProblem::Kind::NoBook,
                    QStringLiteral("%1: the archive has no .klh file").arg(archiveName));
    }

    QByteArray data;
    QString reason;
    ArchiveProblem::Kind kind = ArchiveProblem::Kind::None;
    zip_file_t* entry = zip_fopen_index(archive, manifestIndex, 0);
    if (!entry) {
        failure = failureOf(zip_get_error(archive));
        reason = failure.text;
        kind = readingKind(failure);
    } else {
        char buffer[8192];
        zip_int64_t read = 0;
        while ((read = zip_fread(entry, buffer, sizeof(buffer))) > 0) {
            data.append(buffer, static_cast<qsizetype>(read));
            if (data.size() > MAX_ARCHIVED_MANIFEST) {
                reason = QStringLiteral("too large for the file of a book");
                kind = ArchiveProblem::Kind::DamagedBook;
                break;
            }
        }
        if (read < 0) {
            failure = failureOf(zip_file_get_error(entry));
            reason = failure.text;
            kind = readingKind(failure);
        }
        zip_fclose(entry);
    }
    zip_close(archive);
    if (kind != ArchiveProblem::Kind::None) {
        return fail(kind, QStringLiteral("%1: %2").arg(manifest, reason));
    }

    std::optional<BookProject> project = BookProject::read(data, manifest, found);
    if (!project) {
        // A book of a newer version is not damaged: the writer needs a newer program
        return fail(BookProject::isNewerFormat(data) ? ArchiveProblem::Kind::NewerBook
                                                     : ArchiveProblem::Kind::DamagedBook,
                    QString());
    }
    if (backups) {
        *backups = backupCount;
    }
    return project;
}

QString ProjectManager::unpackArchive(const QString& archivePath,
                                      const QString& targetDir,
                                      const std::function<void(int)>& progressCallback,
                                      ArchiveProblem* problem) {
    auto& logger = Logger::getInstance();
    const auto fail = [&logger, problem](ArchiveProblem::Kind kind, const QStringList& lines) {
        for (const QString& line : lines) {
            logger.error("unpackArchive: {}", line.toStdString());
        }
        if (problem) {
            problem->kind = kind;
            problem->lines.append(lines);
        }
        return QString();
    };

    logger.info("Unpacking archive: {} to {}", archivePath.toStdString(), targetDir.toStdString());

    // The project goes to a new folder named after the archive
    const QString extractDir = QDir(targetDir).filePath(archiveProjectName(archivePath));
    if (QFileInfo::exists(extractDir)) {
        return fail(ArchiveProblem::Kind::FolderExists,
                    {QStringLiteral("%1: the folder already exists")
                         .arg(QDir::toNativeSeparators(extractDir))});
    }

    ZipFailure failure;
    zip_t* archive = openZip(archivePath, ZIP_RDONLY, &failure);
    if (!archive) {
        return fail(readingKind(failure),
                    {QStringLiteral("%1: %2").arg(QFileInfo(archivePath).fileName(),
                                                  failure.text)});
    }
    if (!QDir().mkpath(extractDir)) {
        zip_close(archive);
        return fail(ArchiveProblem::Kind::CannotWrite,
                    {QStringLiteral("%1: the folder cannot be made")
                         .arg(QDir::toNativeSeparators(extractDir))});
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
                logger.warn("unpackArchive: Skipped {}, which is outside the project",
                            name.toStdString());
            }
            continue;
        }
        if (name.endsWith(QLatin1Char('/')) || name.endsWith(QLatin1Char('\\'))) {
            dir.mkpath(path);
            continue;
        }
        if (isArchivedLocalFile(path)) {
            logger.debug("unpackArchive: Skipped {}, which served the project where it was "
                         "archived",
                         path.toStdString());
            continue;
        }

        const QString filePath = dir.filePath(path);
        QString reason;
        ArchiveProblem::Kind kind = ArchiveProblem::Kind::None;
        zip_file_t* entry = zip_fopen_index(archive, static_cast<zip_uint64_t>(i), 0);
        if (!entry) {
            failure = failureOf(zip_get_error(archive));
            reason = failure.text;
            kind = readingKind(failure);
        } else {
            QSaveFile file(filePath);
            if (!QDir().mkpath(QFileInfo(filePath).absolutePath()) ||
                !file.open(QIODevice::WriteOnly)) {
                reason = file.errorString();
                kind = writingKind(file);
            } else {
                char buffer[8192];
                zip_int64_t read = 0;
                while ((read = zip_fread(entry, buffer, sizeof(buffer))) > 0) {
                    if (file.write(buffer, read) != read) {
                        break;
                    }
                }
                if (read < 0) {
                    failure = failureOf(zip_file_get_error(entry));
                    reason = failure.text;
                    kind = readingKind(failure);
                } else if (read > 0 || !file.commit()) {
                    reason = file.errorString();
                    kind = writingKind(file);
                }
            }
            zip_fclose(entry);
        }
        if (kind != ArchiveProblem::Kind::None) {
            zip_close(archive);
            undo();
            return fail(kind, {QStringLiteral("%1: %2").arg(path, reason)});
        }

        // A backup keeps its time, by which the backups of a book are sorted
        if (isArchivedBackup(path)) {
            zip_stat_t entryInfo;
            zip_stat_init(&entryInfo);
            if (zip_stat_index(archive, static_cast<zip_uint64_t>(i), 0, &entryInfo) == 0 &&
                (entryInfo.valid & ZIP_STAT_MTIME) != 0) {
                setModificationTime(filePath, QDateTime::fromSecsSinceEpoch(
                                                  static_cast<qint64>(entryInfo.mtime)));
            }
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
        return fail(ArchiveProblem::Kind::NoBook,
                    {QStringLiteral("%1: the archive has no .klh file")
                         .arg(QFileInfo(archivePath).fileName())});
    }

    // A book whose .klh file cannot be read does not stay behind
    QString manifest = dir.filePath(manifests.first());
    QStringList found;
    const std::optional<BookProject> project = BookProject::load(manifest, found);
    if (!project) {
        if (found.isEmpty()) {
            found.append(QStringLiteral("%1: cannot be read").arg(manifests.first()));
        }
        // A book of a newer version is not damaged: the writer needs a newer program
        const ArchiveProblem::Kind kind = BookProject::isNewerFormatFile(manifest)
                                              ? ArchiveProblem::Kind::NewerBook
                                              : ArchiveProblem::Kind::DamagedBook;
        undo();
        return fail(kind, found);
    }

    // The backups of the database go where the Folders page keeps the backups of books
    const QString common = QString::fromStdString(
        SettingsManager::getInstance().get<std::string>(BackupManager::FOLDER_SETTING));
    if (!common.trimmed().isEmpty()) {
        BackupManager backups(extractDir);
        backups.setCommonFolder(common, project->id);
        if (!backups.takeBackupsFrom(dir.filePath(BACKUPS_FOLDER))) {
            logger.warn("unpackArchive: Some backups of the database stay in {}",
                        dir.filePath(BACKUPS_FOLDER).toStdString());
        }
    }
    logger.info("unpackArchive: Unpacked {}", manifest.toStdString());
    return manifest;
}

bool ProjectManager::importArchive(const QString& archivePath,
                                   const QString& targetDir,
                                   const std::function<void(int)>& progressCallback,
                                   ArchiveProblem* problem) {
    const QString manifest = unpackArchive(archivePath, targetDir, progressCallback, problem);
    if (manifest.isEmpty()) {
        return false;
    }

    // Nothing of a book that cannot be opened stays behind
    Logger::getInstance().info("importArchive: Opening extracted project: {}",
                               manifest.toStdString());
    QStringList found;
    if (!openProject(manifest, &found)) {
        QDir(QFileInfo(manifest).absolutePath()).removeRecursively();
        if (problem) {
            problem->kind = ArchiveProblem::Kind::NotOpened;
            problem->lines.append(found);
        }
        return false;
    }
    return true;
}

QString ProjectManager::archiveFileName(const QString& title) {
    const QString name = safeFileName(title);
    return (name.isEmpty() ? QStringLiteral("project") : name) + QLatin1String(ARCHIVE_EXTENSION);
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
