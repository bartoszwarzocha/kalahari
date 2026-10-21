/// @file test_project_manager.cpp
/// @brief ProjectManager: creating, opening and saving book projects, their elements, the text
/// and the data of their chapter files

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/database_types.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/project_database.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/editor/kml_document_model.h>

#include "other_program.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>

#include <zip.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

using namespace kalahari::core;

namespace {

/// ProjectManager with the built-in book types of the source tree
ProjectManager& projects() {
    auto& pm = ProjectManager::getInstance();
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.bookTypes().problems().isEmpty());
    return pm;
}

KindRef kind(const QString& packageId, const QString& kindId) {
    const KindRef found = ProjectManager::getInstance().bookTypes().findKind(packageId, kindId);
    REQUIRE(found);
    return found;
}

/// The open book takes kind @p kindId of package @p packageId too: its .klh file names the kind,
/// and the book opens again with a copy of the package. Kinds taken before are no longer valid.
void takeKind(const QString& packageId, const QString& kindId) {
    auto& pm = ProjectManager::getInstance();
    const QString manifest = pm.getManifestPath();
    pm.project()->kinds.append(KindReference{packageId, kindId});
    REQUIRE(pm.saveManifest());
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(manifest));
}

/// Kinds with their packages, "?" for no kind
std::string references(const QList<KindRef>& kinds) {
    QStringList list;
    for (const KindRef& ref : kinds) {
        list << (ref ? ref.reference() : QStringLiteral("?"));
    }
    return list.join(QStringLiteral(", ")).toStdString();
}

QJsonObject readJson(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QJsonDocument::fromJson(file.readAll()).object();
}

void writeJson(const QString& path, const QJsonObject& json) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(json).toJson());
}

void writeFile(const QString& path, const QByteArray& data) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(data);
}

/// The project as its .klh file has it
BookProject savedProject(const QString& manifest) {
    QStringList problems;
    std::optional<BookProject> project = BookProject::load(manifest, problems);
    INFO(problems.join(QStringLiteral("; ")).toStdString());
    REQUIRE(project.has_value());
    return *project;
}

/// The chapter file of element @p id
ChapterDocument chapterFileOf(const QString& id) {
    auto& pm = ProjectManager::getInstance();
    const ProjectElement* element = pm.findElement(id);
    REQUIRE(element != nullptr);
    std::optional<ChapterDocument> chapter = ChapterDocument::load(pm.filePathOf(*element));
    REQUIRE(chapter.has_value());
    return *chapter;
}

/// Titles of @p elements, in order
std::string titles(const QList<ProjectElement>& elements) {
    QStringList list;
    for (const ProjectElement& element : elements) {
        list << element.title;
    }
    return list.join(QStringLiteral(", ")).toStdString();
}

/// A chapter's KML with one paragraph
QString kmlWith(const QString& text) {
    return QStringLiteral("<kml><p>%1</p></kml>").arg(text);
}

/// Paragraphs of the text of element @p id
QStringList paragraphsOf(const QString& id) {
    kalahari::editor::KmlDocumentModel model;
    REQUIRE(model.loadKml(chapterFileOf(id).kml()));
    QStringList paragraphs;
    for (size_t i = 0; i < model.paragraphCount(); ++i) {
        paragraphs << model.paragraphText(i);
    }
    return paragraphs;
}

/// Whether the database of the open project has paragraph style @p id
bool hasParagraphStyle(const QString& id) {
    ProjectDatabase* database = ProjectManager::getInstance().getDatabase();
    REQUIRE(database != nullptr);
    const QList<ParagraphStyle> styles = database->getParagraphStyles();
    return std::any_of(styles.cbegin(), styles.cend(),
                       [&id](const ParagraphStyle& style) { return style.id == id; });
}

/// An archive entry: its name and its data
using Entry = std::pair<QByteArray, QByteArray>;

/// Write archive @p path with @p entries, named as they are
void writeArchive(const QString& path, const QList<Entry>& entries) {
    int error = 0;
    zip_t* archive = zip_open(QFile::encodeName(path).constData(), ZIP_CREATE | ZIP_TRUNCATE,
                              &error);
    REQUIRE(archive != nullptr);
    for (const Entry& entry : entries) {
        zip_source_t* source = zip_source_buffer(archive, entry.second.constData(),
                                                 static_cast<zip_uint64_t>(entry.second.size()), 0);
        REQUIRE(source != nullptr);
        REQUIRE(zip_file_add(archive, entry.first.constData(), source, ZIP_FL_ENC_RAW) >= 0);
    }
    REQUIRE(zip_close(archive) == 0);
}

/// The file of a book, which libzip packs to a part of its size
QByteArray compressibleBook() {
    QByteArray book("{ \"notes\": [");
    for (int i = 0; i < 50; ++i) {
        book += "\"a note of the book\", ";
    }
    return book + "\"\" ] }";
}

/// Change the bytes of archive @p path with @p change, which gets them with the start of the
/// packed data of the first entry and the start of its record in the list of entries
void changeFirstEntry(const QString& path,
                      const std::function<void(QByteArray&, qsizetype, qsizetype)>& change) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadWrite));
    QByteArray data = file.readAll();
    REQUIRE(data.startsWith("PK\x03\x04"));
    // The local header: 30 bytes, the name and the extra field
    const auto word = [&data](qsizetype at) {
        return static_cast<qsizetype>(static_cast<unsigned char>(data.at(at)) |
                                      (static_cast<unsigned char>(data.at(at + 1)) << 8));
    };
    const qsizetype start = 30 + word(26) + word(28);
    const qsizetype central = data.indexOf("PK\x01\x02");
    REQUIRE(central > start + 16);
    change(data, start, central);
    REQUIRE(file.seek(0));
    REQUIRE(file.write(data) == data.size());
}

/// Names of the entries of archive @p path, sorted
QStringList entriesOf(const QString& path) {
    int error = 0;
    zip_t* archive = zip_open(QFile::encodeName(path).constData(), ZIP_RDONLY, &error);
    REQUIRE(archive != nullptr);
    QStringList names;
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; i < count; ++i) {
        names << QString::fromUtf8(zip_get_name(archive, static_cast<zip_uint64_t>(i), 0));
    }
    zip_discard(archive);
    names.sort();
    return names;
}

/// Keeps the backups of the books in a common folder while it lives
class BackupFolder {
public:
    explicit BackupFolder(const QString& folder) { set(folder); }
    ~BackupFolder() {
        try {
            set(QString());
        } catch (...) {
            // A destructor may not throw: the later tests then find the folder in the settings
            std::fputs("BackupFolder: the common folder of the backups is still set\n", stderr);
        }
    }
    BackupFolder(const BackupFolder&) = delete;
    BackupFolder& operator=(const BackupFolder&) = delete;

    /// Keep the backups in @p folder; empty: in the folder of each book
    static void set(const QString& folder) {
        SettingsManager::getInstance().set<std::string>(BackupManager::FOLDER_SETTING,
                                                        folder.toStdString());
    }
};

/// The day of January 2000 when a backup is made
QDateTime dayOfBackup(int day) {
    return QDateTime(QDate(2000, 1, day), QTime(0, 0));
}

/// Write backup @p path, made on @p day of January 2000
void writeBackup(const QString& path, int day) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write(QByteArray("backup of day ") + QByteArray::number(day)) > 0);
    REQUIRE(file.flush());
    REQUIRE(file.setFileTime(dayOfBackup(day), QFileDevice::FileModificationTime));
}

/// Whether file @p path was last changed on @p day of January 2000, as far as an archive keeps
/// the time: to two seconds
bool madeOnDay(const QString& path, int day) {
    return qAbs(QFileInfo(path).lastModified().secsTo(dayOfBackup(day))) <= 2;
}

/// Files of folder @p path, with the files of the folders in it, as archive entries named
/// with @p separator
QList<Entry> entriesOfFolder(const QString& path, QChar separator) {
    QList<Entry> entries;
    const QDir dir(path);
    QDirIterator it(path, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString name = dir.relativeFilePath(it.next());
        QFile file(dir.filePath(name));
        REQUIRE(file.open(QIODevice::ReadOnly));
        entries.append({QString(name).replace(QLatin1Char('/'), separator).toUtf8(),
                        file.readAll()});
    }
    return entries;
}

}  // namespace

// =============================================================================
// Creating, opening and closing
// =============================================================================

TEST_CASE("ProjectManager closes the database after projectAboutToClose", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();

    REQUIRE(pm.createProject(dir.path(), "Close Test", "Author", "en", true));
    const QString manifest = QDir(pm.getProjectPath()).filePath("Close Test.klh");
    REQUIRE(pm.closeProject(false));

    REQUIRE(pm.openProject(manifest));
    REQUIRE(pm.getDatabase() != nullptr);

    bool databaseOpenOnSignal = false;
    int signalCount = 0;
    auto connection = QObject::connect(&pm, &ProjectManager::projectAboutToClose, [&]() {
        ++signalCount;
        databaseOpenOnSignal = pm.getDatabase() != nullptr && pm.getDatabase()->isOpen();
    });

    REQUIRE(pm.closeProject(false));
    QObject::disconnect(connection);

    CHECK(signalCount == 1);
    CHECK(databaseOpenOnSignal);
    CHECK(pm.getDatabase() == nullptr);
    CHECK(pm.project() == nullptr);
    CHECK_FALSE(pm.isProjectOpen());
}

TEST_CASE("Closing a book backs up its database where the settings say", "[project_manager]") {
    QTemporaryDir dir;
    const QDir root(dir.path());
    auto& pm = projects();
    auto& settings = SettingsManager::getInstance();
    settings.set<std::string>(BackupManager::FOLDER_SETTING,
                              root.filePath("Backups").toStdString());
    settings.set<int>(BackupManager::COUNT_SETTING, 2);

    REQUIRE(pm.createProject(root.filePath("Books"), "Travels", "Anna", "en", true));
    const QString projectFolder = pm.getProjectPath();
    const QString manifest = QDir(projectFolder).filePath("Travels.klh");
    // A change of the database just before the book is closed
    ParagraphStyle style;
    style.id = "travel_note";
    style.name = "Travel note";
    REQUIRE(pm.getDatabase() != nullptr);
    pm.getDatabase()->saveParagraphStyle(style);
    REQUIRE(pm.closeProject(false));

    // The book has a folder in the common folder, and its own .backups folder stays empty
    const QDir backups(root.filePath("Backups/Travels"));
    const QStringList filter{QStringLiteral("project_*.db")};
    const QStringList copies = backups.entryList(filter, QDir::Files);
    REQUIRE(copies.size() == 1);
    CHECK_FALSE(QFileInfo::exists(QDir(projectFolder).filePath(".backups")));

    // The copy alone has the change, also when it was still in the log of the database
    {
        QTemporaryDir restored;
        REQUIRE(QFile::copy(backups.filePath(copies.first()),
                            QDir(restored.path()).filePath("project.db")));
        ProjectDatabase database;
        REQUIRE(database.open(restored.path()));
        const QList<ParagraphStyle> styles = database.getParagraphStyles();
        CHECK(std::any_of(styles.cbegin(), styles.cend(), [](const ParagraphStyle& saved) {
            return saved.id == "travel_note";
        }));
        database.close();
    }

    // As many copies as the settings say: the oldest goes (the earlier copies are made older,
    // as if they were made on other days)
    int day = 1;
    const auto ageCopies = [&backups, &filter, &day]() {
        for (const QString& copy : backups.entryList(filter, QDir::Files)) {
            if (copy.startsWith("project_2000")) {
                continue;
            }
            const QString aged =
                backups.filePath(QString("project_200001%1_000000.db").arg(day, 2, 10, QChar('0')));
            REQUIRE(QFile::rename(backups.filePath(copy), aged));
            QFile file(aged);
            REQUIRE(file.open(QIODevice::ReadWrite));
            REQUIRE(file.setFileTime(QDateTime(QDate(2000, 1, day), QTime(0, 0)),
                                     QFileDevice::FileModificationTime));
            ++day;
        }
    };
    for (int round = 0; round < 2; ++round) {
        ageCopies();
        REQUIRE(pm.openProject(manifest));
        REQUIRE(pm.closeProject(false));
    }
    const QStringList left = backups.entryList(filter, QDir::Files, QDir::Name);
    REQUIRE(left.size() == 2);
    CHECK(left.first() == "project_20000102_000000.db");
    CHECK_FALSE(left.last().startsWith("project_2000"));

    // Without a common folder the copy goes to the book's own .backups folder
    settings.set<std::string>(BackupManager::FOLDER_SETTING, std::string());
    REQUIRE(pm.openProject(manifest));
    REQUIRE(pm.closeProject(false));
    const QDir ownBackups(QDir(projectFolder).filePath(".backups"));
    CHECK(ownBackups.entryList(filter, QDir::Files).size() == 1);
}

TEST_CASE("ProjectManager creates a book of a type", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();

    REQUIRE(pm.createProject(dir.path(), "My Novel", "Anna Nowak", "pl", true, "kalahari.novel"));
    CHECK(pm.isProjectOpen());
    CHECK(pm.getDatabase() != nullptr);
    const QString manifest = QDir(dir.path()).filePath("My Novel/My Novel.klh");
    CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(manifest));
    CHECK(QFileInfo(pm.getProjectPath()) == QFileInfo(QDir(dir.path()).filePath("My Novel")));

    const BookProject* project = pm.project();
    REQUIRE(project != nullptr);
    REQUIRE(project->type.has_value());
    CHECK(project->type->id == "kalahari.novel");
    CHECK(project->type->version == pm.bookTypes().package("kalahari.novel")->version);
    CHECK(project->kinds.isEmpty());
    CHECK_FALSE(project->id.isEmpty());
    CHECK(project->created.isValid());

    const ProjectBook* book = pm.book();
    REQUIRE(book != nullptr);
    CHECK(book->title == "My Novel");
    CHECK(book->author == "Anna Nowak");
    CHECK(book->language == "pl");
    CHECK(book->folder == "book");
    CHECK(book->partsLayer);

    // The book starts with the elements of its type, in the language of its text
    REQUIRE(book->frontElements.size() == 1);
    const ProjectElement titlePage = book->frontElements.first();
    CHECK(titlePage.kind == KindReference{"kalahari.base", "title_page"});
    CHECK(titlePage.title == "Strona tytułowa");
    CHECK(titlePage.file == "book/title_page_001.kchapter");
    CHECK(titlePage.status == "draft");
    REQUIRE(book->mainElements.size() == 1);
    const ProjectElement chapter = book->mainElements.first();
    CHECK(chapter.kind == KindReference{"kalahari.base", "chapter"});
    CHECK(chapter.title == "Rozdział 1");
    CHECK(chapter.file == "book/chapter_001.kchapter");
    CHECK(chapter.status == "draft");
    CHECK(book->backElements.isEmpty());
    CHECK(project->workshop.elements.isEmpty());

    // The title page has the title and the author of the book, the chapter no text
    const ChapterDocument titleFile = chapterFileOf(titlePage.id);
    CHECK(titleFile.title() == "Strona tytułowa");
    CHECK(titleFile.status() == "draft");
    CHECK(paragraphsOf(titlePage.id) == QStringList{"My Novel", "", "Anna Nowak"});
    CHECK(pm.wordCount(titlePage.id) == 4);
    const ChapterDocument chapterFile = chapterFileOf(chapter.id);
    CHECK(chapterFile.title() == "Rozdział 1");
    CHECK(chapterFile.kml().isEmpty());
    CHECK_FALSE(pm.isDirty());
    REQUIRE(pm.closeProject(false));

    const BookProject saved = savedProject(manifest);
    REQUIRE(saved.type.has_value());
    CHECK(saved.type->id == "kalahari.novel");
    CHECK(saved.books.first().title == "My Novel");
    CHECK(titles(saved.books.first().frontElements) == "Strona tytułowa");
    CHECK(titles(saved.books.first().mainElements) == "Rozdział 1");

    SECTION("The title and the author as they are written") {
        REQUIRE(pm.createProject(dir.path(), "Tom & Jerry", "Anna \"Ania\" <Nowak>", "en", true,
                                 "kalahari.novel"));
        REQUIRE(pm.book() != nullptr);
        REQUIRE_FALSE(pm.book()->frontElements.isEmpty());
        CHECK(pm.book()->frontElements.first().title == "Title page");
        CHECK(pm.book()->mainElements.first().title == "Chapter 1");
        CHECK(paragraphsOf(pm.book()->frontElements.first().id) ==
              QStringList{"Tom & Jerry", "", "Anna \"Ania\" <Nowak>"});
        REQUIRE(pm.closeProject(false));
    }

    SECTION("A type without the parts layer") {
        REQUIRE(pm.createProject(dir.path(), "Pilot", "Anna", "en", true, "kalahari.screenplay"));
        REQUIRE(pm.book() != nullptr);
        CHECK_FALSE(pm.book()->partsLayer);
        REQUIRE(pm.closeProject(false));
    }
}

TEST_CASE("A new book shows the sections the writer chose", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    const QString custom = QString::fromLatin1(ProjectBook::CUSTOM_SECTIONS);

    SECTION("Without a choice, as the type says, with the first set of names") {
        REQUIRE(pm.createProject(dir.path(), "Novel", "Anna", "pl", true, "kalahari.novel"));
        CHECK(pm.book()->sections() == BookSections{true, QString(), {}});
        REQUIRE(pm.closeProject(false));
        REQUIRE(pm.createProject(dir.path(), "Pilot", "Anna", "pl", true,
                                 "kalahari.screenplay"));
        CHECK(pm.book()->sections() == BookSections{false, QString(), {}});
        REQUIRE(pm.closeProject(false));
        REQUIRE(pm.createProject(dir.path(), "Notes", "Anna", "pl", true));
        CHECK(pm.book()->sections() == BookSections{false, QString(), {}});
        REQUIRE(pm.closeProject(false));
    }

    SECTION("A set of names, and no sections for a type that shows them") {
        REQUIRE(pm.createProject(dir.path(), "Novel", "Anna", "pl", true, "kalahari.novel",
                                 nullptr, BookSections{true, QStringLiteral("arc"), {}}));
        CHECK(pm.book()->sectionName(BookPlace::Main) == "Rozwinięcie");
        const QString manifest = pm.getManifestPath();
        REQUIRE(pm.closeProject(false));
        CHECK(savedProject(manifest).books.first().sections() ==
              BookSections{true, QStringLiteral("arc"), {}});

        REQUIRE(pm.createProject(dir.path(), "Plain", "Anna", "pl", true, "kalahari.novel",
                                 nullptr, BookSections{false, QString(), {}}));
        CHECK_FALSE(pm.book()->partsLayer);
        REQUIRE(pm.closeProject(false));
    }

    SECTION("The writer's own names, and sections for a user project") {
        REQUIRE(pm.createProject(dir.path(), "Notes", "Anna", "pl", true, QString(), nullptr,
                                 BookSections{true, custom,
                                              {QStringLiteral(" Wstęp "),
                                               QStringLiteral("Opowieść"), QString()}}));
        CHECK(pm.book()->partsLayer);
        CHECK(pm.book()->sectionNames ==
              QStringList{"Wstęp", "Opowieść", "Sekcja końcowa"});
        const QString manifest = pm.getManifestPath();
        REQUIRE(pm.closeProject(false));
        CHECK(savedProject(manifest).books.first().sectionNames ==
              QStringList{"Wstęp", "Opowieść", "Sekcja końcowa"});
    }
}

TEST_CASE("ProjectManager shows, hides and names the sections of the open book",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    CHECK_FALSE(pm.setSections(BookSections{}));  // no book is open

    REQUIRE(pm.createProject(dir.path(), "Sections", "Anna", "pl", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QString custom = QString::fromLatin1(ProjectBook::CUSTOM_SECTIONS);
    const std::string elements =
        titles(pm.book()->frontElements) + " | " + titles(pm.book()->mainElements);

    // Saved at once; the elements stay in their sections
    REQUIRE(pm.setSections(BookSections{false, QString(), {}}));
    CHECK_FALSE(pm.book()->partsLayer);
    CHECK_FALSE(pm.isDirty());
    CHECK_FALSE(savedProject(manifest).books.first().partsLayer);
    CHECK(titles(pm.book()->frontElements) + " | " + titles(pm.book()->mainElements) ==
          elements);

    const BookSections own{true, custom,
                           {QStringLiteral("Wstęp"), QStringLiteral("Opowieść"),
                            QStringLiteral("Dodatki")}};
    REQUIRE(pm.setSections(own));
    CHECK(pm.book()->sectionName(BookPlace::Main) == "Opowieść");
    CHECK(savedProject(manifest).books.first().sections() == own);

    // A file that cannot be written: the book keeps its sections
    REQUIRE(QFile::remove(manifest));
    REQUIRE(QDir().mkpath(manifest));
    CHECK_FALSE(pm.setSections(BookSections{false, QStringLiteral("arc"), {}}));
    CHECK(pm.book()->sections() == own);
    REQUIRE(QDir().rmdir(manifest));
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager names the book and the Workshop and groups the Workshop",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    // No book is open
    CHECK_FALSE(pm.setBookName("Book"));
    CHECK_FALSE(pm.setWorkshopName("Workshop"));
    CHECK_FALSE(pm.setWorkshopGrouped(true));

    REQUIRE(pm.createProject(dir.path(), "Names", "Anna", "pl", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    CHECK(pm.book()->name.isEmpty());
    CHECK(pm.project()->workshop.name.isEmpty());
    CHECK_FALSE(pm.project()->workshop.grouped);

    // Saved at once, without the spaces at the ends of the names
    REQUIRE(pm.setBookName("  Tom pierwszy "));
    REQUIRE(pm.setWorkshopName("Pracownia"));
    REQUIRE(pm.setWorkshopGrouped(true));
    CHECK_FALSE(pm.isDirty());
    BookProject saved = savedProject(manifest);
    CHECK(saved.books.first().name == "Tom pierwszy");
    CHECK(saved.workshop.name == "Pracownia");
    CHECK(saved.workshop.grouped);

    // An empty name gives the default one back
    REQUIRE(pm.setBookName("   "));
    REQUIRE(pm.setWorkshopName(QString()));
    saved = savedProject(manifest);
    CHECK(saved.books.first().name.isEmpty());
    CHECK(saved.workshop.name.isEmpty());

    // A file that cannot be written: the names and the grouping stay
    REQUIRE(QFile::remove(manifest));
    REQUIRE(QDir().mkpath(manifest));
    CHECK_FALSE(pm.setBookName("Lost"));
    CHECK_FALSE(pm.setWorkshopName("Lost"));
    CHECK_FALSE(pm.setWorkshopGrouped(false));
    CHECK(pm.book()->name.isEmpty());
    CHECK(pm.project()->workshop.name.isEmpty());
    CHECK(pm.project()->workshop.grouped);
    REQUIRE(QDir().rmdir(manifest));
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("A new book starts with the elements of its type", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    // Titles of the front part | titles of the main part
    const auto start = [&pm, &dir](const char* type) {
        REQUIRE(pm.createProject(dir.path(), QString::fromLatin1(type), "Anna", "en", true,
                                 QString::fromLatin1(type)));
        const ProjectBook* book = pm.book();
        REQUIRE(book != nullptr);
        CHECK(book->backElements.isEmpty());
        const std::string parts = titles(book->frontElements) + " | " + titles(book->mainElements);
        REQUIRE(pm.closeProject(false));
        return parts;
    };
    CHECK(start("kalahari.novel") == "Title page | Chapter 1");
    CHECK(start("kalahari.short_stories") == "Title page | Story 1");
    CHECK(start("kalahari.nonfiction") == "Title page, Introduction | Chapter 1");
    CHECK(start("kalahari.screenplay") == "Title page | Act I");
    CHECK(start("kalahari.poetry") == "Title page | Poem 1");
}

TEST_CASE("A user project has the kinds of the base package", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();

    REQUIRE(pm.createProject(dir.path(), "Notes", "Anna", "en", true));
    REQUIRE(pm.project() != nullptr);
    CHECK(pm.project()->isUserProject());

    qsizetype baseKinds = 0;
    for (const KindRef& ref : pm.bookTypes().allKinds()) {
        baseKinds += ref.package->id == "kalahari.base" ? 1 : 0;
    }
    CHECK(pm.project()->kinds.size() == baseKinds);
    CHECK(pm.project()->kinds.contains(KindReference{"kalahari.base", "chapter"}));
    CHECK_FALSE(pm.book()->partsLayer);

    CHECK(pm.chapterKindFor(BookPlace::Main).reference() == "kalahari.base:chapter");
    CHECK(pm.partKind().reference() == "kalahari.base:part");

    // It starts empty
    CHECK(pm.book()->frontElements.isEmpty());
    CHECK(pm.book()->mainElements.isEmpty());
    CHECK(pm.book()->backElements.isEmpty());
    CHECK_FALSE(QFileInfo::exists(QDir(pm.getProjectPath()).filePath("book")));
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("A user project has the kinds and the elements chosen in the palette",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();

    NewProject request;
    request.parentDir = dir.path();
    request.title = QStringLiteral("Saga");
    request.author = QStringLiteral("Anna Nowak");
    request.language = QStringLiteral("pl");
    PaletteChoice palette;
    palette.kinds = {kind("kalahari.base", "title_page"), kind("kalahari.base", "chapter"),
                     kind("kalahari.base", "part"),       kind("kalahari.base", "work_note"),
                     kind("kalahari.base", "character"),  kind("kalahari.base", "afterword"),
                     kind("kalahari.novel", "prologue"),  kind("kalahari.novel", "epilogue"),
                     kind("kalahari.poetry", "poem"),     kind("kalahari.poetry", "cycle")};
    // In the order of the palette: the parts of the book, then the Workshop
    palette.elements = {{kind("kalahari.base", "title_page"), BookPlace::Front},
                        {kind("kalahari.base", "chapter"), BookPlace::Main},
                        {kind("kalahari.base", "part"), BookPlace::Main},
                        {kind("kalahari.novel", "prologue"), BookPlace::Main},
                        {kind("kalahari.novel", "epilogue"), BookPlace::Main},
                        {kind("kalahari.poetry", "poem"), BookPlace::Main},
                        {kind("kalahari.poetry", "cycle"), BookPlace::Main},
                        {kind("kalahari.base", "work_note"), BookPlace::Workshop},
                        {kind("kalahari.base", "character"), BookPlace::Workshop}};
    request.palette = palette;
    REQUIRE(pm.createProject(request));

    const BookProject* project = pm.project();
    REQUIRE(project != nullptr);
    CHECK(project->isUserProject());
    const QList<KindReference> kinds{
        {"kalahari.base", "title_page"}, {"kalahari.base", "chapter"},
        {"kalahari.base", "part"},       {"kalahari.base", "work_note"},
        {"kalahari.base", "character"},  {"kalahari.base", "afterword"},
        {"kalahari.novel", "prologue"},  {"kalahari.novel", "epilogue"},
        {"kalahari.poetry", "poem"},     {"kalahari.poetry", "cycle"}};
    CHECK(project->kinds == kinds);
    CHECK(pm.kindsFor(BookPlace::Back).size() == 1);  // the afterword, without an element

    // The title page has the title and the author of the book
    const ProjectBook* book = pm.book();
    REQUIRE(book != nullptr);
    REQUIRE(book->frontElements.size() == 1);
    CHECK(book->frontElements.first().title == "Strona tytułowa");
    CHECK(paragraphsOf(book->frontElements.first().id) ==
          QStringList{"Saga", "", "Anna Nowak"});

    // Each text element goes into the first group made with it that can have it; the prologue
    // and the epilogue stay at the start and at the end of the body
    CHECK(titles(book->mainElements) == "Prolog, Część I, Cykl I, Epilog");
    REQUIRE(book->mainElements.size() == 4);
    const ProjectElement& part = book->mainElements.at(1);
    CHECK(part.file.isEmpty());
    CHECK(part.status.isEmpty());
    CHECK(titles(part.elements) == "Rozdział 1");
    CHECK(titles(book->mainElements.at(2).elements) == "Wiersz 1");
    const ProjectElement& chapter = part.elements.first();
    CHECK(chapter.file == "book/chapter_001.kchapter");
    CHECK(chapter.status == "draft");
    CHECK(chapterFileOf(chapter.id).title() == "Rozdział 1");
    CHECK(book->backElements.isEmpty());

    // The working note is in the Workshop's folder; a character waits for its window
    CHECK(titles(project->workshop.elements) == "Notatka 1");
    CHECK(project->workshop.elements.first().file == "workshop/work_note_001.kchapter");
    CHECK(QFileInfo::exists(QDir(pm.getProjectPath()).filePath("workshop/work_note_001.kchapter")));

    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    const BookProject saved = savedProject(manifest);
    CHECK(saved.kinds == kinds);
    CHECK(titles(saved.books.first().mainElements) == "Prolog, Część I, Cykl I, Epilog");
}

TEST_CASE("ProjectManager creates a project without opening it", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Open Book", "Anna", "en", true, "kalahari.novel"));
    const QString open = pm.getManifestPath();

    NewProject request;
    request.parentDir = dir.path();
    request.title = QStringLiteral("Second");
    request.author = QStringLiteral("Anna");
    request.language = QStringLiteral("en");
    request.typeId = QStringLiteral("kalahari.novel");
    QStringList problems;

    SECTION("The open book stays open, and the new one is whole") {
        const QString manifest = pm.createProjectWithoutOpening(request, &problems);
        CHECK(problems.isEmpty());
        CHECK(QFileInfo(manifest) == QFileInfo(QDir(dir.path()).filePath("Second/Second.klh")));
        CHECK(pm.isProjectOpen());
        CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(open));

        const BookProject saved = savedProject(manifest);
        CHECK(saved.books.first().title == "Second");
        CHECK(titles(saved.books.first().mainElements) == "Chapter 1");
        CHECK(QFileInfo::exists(QDir(dir.path()).filePath("Second/book/chapter_001.kchapter")));
        REQUIRE(pm.openProject(manifest));
        REQUIRE(pm.book() != nullptr);
        CHECK(pm.book()->title == "Second");
    }

    SECTION("A project that cannot be made leaves nothing behind") {
        REQUIRE(QDir(dir.path()).mkpath("Second"));
        writeFile(QDir(dir.path()).filePath("Second/notes.txt"), "mine");
        CHECK(pm.createProjectWithoutOpening(request, &problems).isEmpty());
        REQUIRE(problems.size() == 1);
        CHECK(problems.first().endsWith(": the folder exists and is not empty"));
        CHECK(QDir(QDir(dir.path()).filePath("Second"))
                  .entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden) ==
              QStringList{"notes.txt"});
        CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(open));
    }
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager does not create a user project from a wrong palette",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    NewProject request;
    request.parentDir = dir.path();
    request.title = QStringLiteral("Palette");
    request.author = QStringLiteral("Anna");
    request.language = QStringLiteral("en");
    PaletteChoice palette;
    palette.kinds = {kind("kalahari.base", "chapter")};
    QStringList expected;

    SECTION("A book of a type") {
        request.typeId = QStringLiteral("kalahari.novel");
        expected = {"a book of a type takes no palette"};
    }
    SECTION("No kinds") {
        palette.kinds.clear();
        expected = {"the project has no kinds"};
    }
    SECTION("An element of a kind the project does not have") {
        palette.elements = {{kind("kalahari.base", "part"), BookPlace::Main}};
        expected = {"kalahari.base:part is not a kind of the project"};
    }
    SECTION("An element in a place its kind does not allow") {
        palette.kinds = {kind("kalahari.base", "part")};
        palette.elements = {{kind("kalahari.base", "part"), BookPlace::Front}};
        expected = {"kalahari.base:part cannot be in front"};
    }
    request.palette = palette;

    CHECK(pm.newProjectProblems(request) == expected);
    QStringList problems;
    CHECK_FALSE(pm.createProject(request, &problems));
    CHECK(problems == expected);
    CHECK_FALSE(pm.isProjectOpen());
    CHECK_FALSE(QFileInfo::exists(QDir(dir.path()).filePath("Palette")));
}

TEST_CASE("ProjectManager does not create a project it cannot make", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    QStringList problems;

    SECTION("A type that is not installed") {
        CHECK_FALSE(pm.createProject(dir.path(), "Thriller", "Anna", "en", true, "acme.thriller",
                                     &problems));
        CHECK(problems == QStringList{"the book type acme.thriller is not installed"});
        CHECK_FALSE(QFileInfo::exists(QDir(dir.path()).filePath("Thriller")));
    }

    SECTION("A package that is not a type") {
        CHECK_FALSE(pm.createProject(dir.path(), "Base", "Anna", "en", true, "kalahari.base",
                                     &problems));
        CHECK(problems == QStringList{"kalahari.base is not a book type"});
        CHECK_FALSE(QFileInfo::exists(QDir(dir.path()).filePath("Base")));
    }

    SECTION("A folder with files") {
        REQUIRE(QDir(dir.path()).mkpath("Taken"));
        const QString file = QDir(dir.path()).filePath("Taken/notes.txt");
        writeFile(file, "mine");
        CHECK_FALSE(pm.createProject(dir.path(), "Taken", "Anna", "en", true, "kalahari.novel",
                                     &problems));
        REQUIRE(problems.size() == 1);
        CHECK(problems.first().endsWith(": the folder exists and is not empty"));
        CHECK(QFileInfo::exists(file));
        CHECK_FALSE(QFileInfo::exists(QDir(dir.path()).filePath("Taken/Taken.klh")));
        CHECK(QDir(dir.path()).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot |
                                         QDir::Hidden) == QStringList{"Taken"});
    }

    CHECK_FALSE(pm.isProjectOpen());

    SECTION("An empty folder takes the project") {
        REQUIRE(QDir(dir.path()).mkpath("Empty"));
        const QString folder = QDir(dir.path()).filePath("Empty");
        REQUIRE(pm.createProject(folder, "Book", "Anna", "en", false, "kalahari.novel"));
        CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(QDir(folder).filePath("Book.klh")));
        REQUIRE(pm.closeProject(false));
    }
}

TEST_CASE("A book that cannot be created leaves the open book open", "[project_manager]") {
    // Regression: the open book was closed before the folder of the new one was checked
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Open Book", "Anna", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    REQUIRE(QDir(dir.path()).mkpath("Taken"));
    writeFile(QDir(dir.path()).filePath("Taken/notes.txt"), "mine");

    QStringList problems;
    CHECK_FALSE(pm.createProject(dir.path(), "Taken", "Anna", "en", true, "kalahari.novel",
                                 &problems));
    REQUIRE(problems.size() == 1);
    CHECK(problems.first().endsWith(": the folder exists and is not empty"));
    CHECK(pm.isProjectOpen());
    CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(manifest));
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("A book that cannot be read leaves the open book open", "[project_manager]") {
    // Regression: the open book was closed before the chosen file was read
    QTemporaryDir dir;
    const QDir parent(dir.path());
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Open Book", "Anna", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    CHECK(ProjectManager::projectFileProblems(manifest).isEmpty());

    // A book of an older version
    REQUIRE(parent.mkpath("Old"));
    const QString old = parent.filePath("Old/Old.klh");
    writeFile(old, R"({"format": 1, "title": "Old"})");
    const QStringList found = ProjectManager::projectFileProblems(old);
    REQUIRE_FALSE(found.isEmpty());
    CHECK(found.first().startsWith("format: must be 2"));

    QStringList problems;
    CHECK_FALSE(pm.openProject(old, &problems));
    CHECK(problems == found);
    CHECK(pm.isProjectOpen());
    CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(manifest));

    // Neither a missing file nor a file of another kind closes it
    writeFile(parent.filePath("notes.txt"), "mine");
    CHECK(ProjectManager::projectFileProblems(parent.filePath("None.klh")) ==
          QStringList{"None.klh: the file does not exist"});
    CHECK(ProjectManager::projectFileProblems(parent.filePath("notes.txt")) ==
          QStringList{"notes.txt: not a .klh file"});
    CHECK_FALSE(pm.openProject(parent.filePath("None.klh")));
    CHECK_FALSE(pm.openProject(parent.filePath("notes.txt")));
    CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(manifest));

    // The open book opens again from what closing it saved
    REQUIRE(pm.openProject(manifest));
    CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(manifest));
    REQUIRE(pm.book() != nullptr);
    CHECK(pm.book()->title == "Open Book");
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("The folder of a new project, and whether it can hold the project", "[project_manager]") {
    // The New Book dialog checks the folder with them before the open book is closed
    QTemporaryDir dir;
    const QDir parent(dir.path());

    SECTION("A subfolder named after the title, with the characters of a file name") {
        CHECK(ProjectManager::newProjectFolder(dir.path(), "  Who? Me: <yes>  ", true) ==
              QDir::cleanPath(parent.filePath("Who_ Me_ _yes_")));
        CHECK(ProjectManager::newProjectFolder(dir.path() + "/", "Book", true) ==
              QDir::cleanPath(parent.filePath("Book")));
    }

    SECTION("The folder itself without a subfolder") {
        CHECK(ProjectManager::newProjectFolder(dir.path() + "/", "Book", false) ==
              QDir::cleanPath(dir.path()));
    }

    SECTION("No folder without a title") {
        CHECK(ProjectManager::newProjectFolder(dir.path(), "   ", true).isEmpty());
        CHECK(ProjectManager::newProjectFolder(dir.path(), QString(), false).isEmpty());
    }

    SECTION("A new or empty folder can hold it, a folder with files or a file cannot") {
        CHECK(ProjectManager::canHoldNewProject(parent.filePath("New")));
        REQUIRE(parent.mkpath("Empty"));
        CHECK(ProjectManager::canHoldNewProject(parent.filePath("Empty")));
        REQUIRE(parent.mkpath("Taken"));
        writeFile(parent.filePath("Taken/notes.txt"), "mine");
        CHECK_FALSE(ProjectManager::canHoldNewProject(parent.filePath("Taken")));
        CHECK_FALSE(ProjectManager::canHoldNewProject(parent.filePath("Taken/notes.txt")));
        CHECK_FALSE(ProjectManager::canHoldNewProject(dir.path()));
    }
}

TEST_CASE("ProjectManager keeps the identity and unknown fields of a project", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();

    REQUIRE(pm.createProject(dir.path(), "Identity Test", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));

    QJsonObject root = readJson(manifest);
    root["id"] = "1771221457515-61aa";
    root["created"] = "2026-02-16T05:57:37Z";
    root["custom"] = QJsonObject{{"key", "value"}};
    QJsonArray books = root["books"].toArray();
    QJsonObject book = books.first().toObject();
    book["series"] = "kept";
    books[0] = book;
    root["books"] = books;
    writeJson(manifest, root);

    REQUIRE(pm.openProject(manifest));
    CHECK(pm.project()->id == "1771221457515-61aa");
    REQUIRE(pm.saveManifest());
    REQUIRE(pm.closeProject(false));

    const QJsonObject saved = readJson(manifest);
    CHECK(saved["id"].toString() == "1771221457515-61aa");
    CHECK(saved["created"].toString() == "2026-02-16T05:57:37Z");
    const QDateTime modified = QDateTime::fromString(saved["modified"].toString(), Qt::ISODate);
    CHECK(modified > QDateTime::fromString("2026-02-16T05:57:37Z", Qt::ISODate));
    CHECK(saved["custom"].toObject()["key"].toString() == "value");
    const QJsonObject savedBook = saved["books"].toArray().first().toObject();
    CHECK(savedBook["series"].toString() == "kept");
    CHECK(savedBook["title"].toString() == "Identity Test");
}

TEST_CASE("ProjectManager does not open a project it cannot read", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    QStringList problems;

    SECTION("A project of the old format") {
        const QString manifest = QDir(dir.path()).filePath("Old.klh");
        writeJson(manifest,
                  QJsonObject{{"kalahari", QJsonObject{{"version", "1.0"}}},
                              {"document", QJsonObject{{"title", "Old"}}},
                              {"structure", QJsonObject{{"body", QJsonArray{}}}}});
        CHECK_FALSE(pm.openProject(manifest, &problems));
        REQUIRE(problems.size() == 1);
        CHECK(problems.first().startsWith("format:"));
    }

    SECTION("A file that does not exist") {
        CHECK_FALSE(pm.openProject(QDir(dir.path()).filePath("Missing.klh"), &problems));
        REQUIRE(problems.size() == 1);
        CHECK(problems.first().startsWith("Missing.klh:"));
    }

    CHECK_FALSE(pm.isProjectOpen());
    CHECK(pm.getDatabase() == nullptr);
}

TEST_CASE("ProjectManager says why it does not open a book", "[project_manager]") {
    QTemporaryDir dir;
    const QDir parent(dir.path());
    auto& pm = projects();
    writeFile(parent.filePath("notes.txt"), "mine");
    writeFile(parent.filePath("Old.klh"), R"({"title": "Old"})");
    writeFile(parent.filePath("Future.klh"), R"({"format": 99, "title": "Future"})");
    writeFile(parent.filePath("Broken.klh"), R"({"format": 2, )");

    CHECK(ProjectManager::openFailure(parent.filePath("None.klh")) == OpenFailure::Missing);
    CHECK(ProjectManager::openFailure(parent.filePath("notes.txt")) == OpenFailure::NotBook);
    CHECK(ProjectManager::openFailure(parent.filePath("Old.klh")) == OpenFailure::Older);
    CHECK(ProjectManager::openFailure(parent.filePath("Future.klh")) == OpenFailure::Newer);
    QStringList problems;
    CHECK(ProjectManager::openFailure(parent.filePath("Broken.klh"), &problems) ==
          OpenFailure::Unreadable);
    CHECK(problems == ProjectManager::projectFileProblems(parent.filePath("Broken.klh")));

    // The last try to open a book says why it failed
    for (const auto& [file, failure] :
         {std::pair{"None.klh", OpenFailure::Missing}, std::pair{"Old.klh", OpenFailure::Older},
          std::pair{"Broken.klh", OpenFailure::Unreadable}}) {
        INFO(file);
        CHECK_FALSE(pm.openProject(parent.filePath(file)));
        CHECK(pm.lastOpenFailure() == failure);
    }
    REQUIRE(pm.createProject(dir.path(), "Opened", "Anna", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(manifest));
    CHECK(pm.lastOpenFailure() == OpenFailure::None);
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager converts a book of an older version and keeps its old file",
          "[project_manager]") {
    QTemporaryDir dir;
    const QDir folder(dir.path());
    auto& pm = projects();
    const auto bytesOf = [](const QString& path) {
        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadOnly));
        return file.readAll();
    };

    // A book of the first format, with the text, the status and the notes of a chapter in
    // its file
    const QString manifest = folder.filePath("Old Novel.klh");
    const QByteArray old = R"({
        "document": {"id": "old-1", "title": "Old Novel", "author": "Anna", "language": "pl"},
        "structure": {
            "frontmatter": [{"id": "fm", "title": "Dedykacja",
                             "file": "content/dedication.kchapter", "type": "dedication"}],
            "body": [{"id": "p1", "title": "Część pierwsza", "chapters": [
                {"id": "c1", "title": "Rozdział 1", "file": "content/chapter_001.kchapter"}]}],
            "backmatter": [{"id": "bib", "title": "Bibliografia",
                            "file": "content/bibliography.kchapter", "type": "bibliography"}]
        }
    })";
    writeFile(manifest, old);
    REQUIRE(folder.mkpath("content"));
    ChapterDocument chapter;
    chapter.setKml(kmlWith("Pierwsze zdanie."));
    chapter.setStatus("revision");
    chapter.setNotes("Notatka do rozdziału");
    REQUIRE(chapter.save(folder.filePath("content/chapter_001.kchapter")));
    const QByteArray chapterFile = bytesOf(folder.filePath("content/chapter_001.kchapter"));
    REQUIRE(ProjectManager::openFailure(manifest) == OpenFailure::Older);

    SECTION("it is converted, and its old file stays beside it as it was") {
        QString copy;
        QStringList others;
        QStringList problems;
        REQUIRE(pm.convertOlderBook(manifest, &copy, &others, &problems));
        CHECK(problems.isEmpty());
        CHECK(QFileInfo(copy) == QFileInfo(folder.filePath("Old Novel.format1.klh.bak")));
        CHECK(bytesOf(copy) == old);
        CHECK(others == QStringList{"Bibliografia"});
        CHECK(bytesOf(folder.filePath("content/chapter_001.kchapter")) == chapterFile);

        // It opens with its parts, chapters, texts, statuses and notes
        CHECK(ProjectManager::openFailure(manifest) == OpenFailure::None);
        REQUIRE(pm.openProject(manifest));
        const ProjectBook* book = pm.book();
        REQUIRE(book);
        CHECK(book->title == "Old Novel");
        CHECK(book->author == "Anna");
        CHECK(titles(book->frontElements) == "Dedykacja");
        CHECK(titles(book->mainElements) == "Część pierwsza");
        const ProjectElement& first = book->mainElements.first().elements.first();
        CHECK(first.title == "Rozdział 1");
        CHECK(first.status == "revision");
        CHECK(pm.notes("c1") == "Notatka do rozdziału");
        CHECK(pm.loadChapterContent("c1").contains("Pierwsze zdanie."));
        CHECK(titles(book->files) == "Bibliografia");
        REQUIRE(pm.closeProject(false));

        // Converted once
        problems.clear();
        CHECK_FALSE(pm.convertOlderBook(manifest, nullptr, nullptr, &problems));
        CHECK(problems ==
              QStringList{"Old Novel.klh: not a book of an older version of Kalahari"});
        CHECK(QFileInfo::exists(folder.filePath("Old Novel.format1.klh.bak")));
        CHECK_FALSE(QFileInfo::exists(folder.filePath("Old Novel.format1-2.klh.bak")));
    }

    SECTION("a book that cannot be converted stays as it was, without a copy") {
        const QByteArray outside = R"({"document": {"title": "Out"}, "structure": {"body": [
            {"chapters": [{"title": "Out", "file": "../outside.kchapter"}]}]}})";
        writeFile(manifest, outside);
        QString copy;
        QStringList others{"before"};
        QStringList problems;
        CHECK_FALSE(pm.convertOlderBook(manifest, &copy, &others, &problems));
        CHECK(problems == QStringList{"structure.body[0].chapters[0].file: the file "
                                      "../outside.kchapter is outside the folder of the book"});
        CHECK(copy.isEmpty());
        CHECK(others == QStringList{"before"});
        CHECK(bytesOf(manifest) == outside);
        CHECK(folder.entryList(QDir::Files) == QStringList{"Old Novel.klh"});
    }

    SECTION("a book that another Kalahari program has open stays as it was") {
        kalahari::test::OtherProgram other;
        REQUIRE(other.pid() != 0);
        REQUIRE(kalahari::test::lockBook(folder.path(), other.pid()));
        QStringList problems;
        CHECK_FALSE(pm.convertOlderBook(manifest, nullptr, nullptr, &problems));
        REQUIRE(problems.size() == 1);
        CHECK(problems.first().startsWith(
            "Old Novel.klh: the book is open in another program that is running ("));
        CHECK(bytesOf(manifest) == old);
        CHECK_FALSE(QFileInfo::exists(folder.filePath("Old Novel.format1.klh.bak")));
    }
}

TEST_CASE("ProjectManager does not open a book that another Kalahari program has open",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Shared", "Anna", "en", true, "kalahari.novel"));
    const QString shared = pm.getManifestPath();
    const QString folder = QFileInfo(shared).absolutePath();

    // The lock of this program does not count
    CHECK(ProjectManager::openFailure(shared) == OpenFailure::None);
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.createProject(dir.path(), "Mine", "Anna", "en", true, "kalahari.novel"));
    const QString mine = pm.getManifestPath();

    SECTION("a program that runs: the open book stays open") {
        kalahari::test::OtherProgram other;
        REQUIRE(other.pid() != 0);
        REQUIRE(kalahari::test::lockBook(folder, other.pid()));
        QStringList problems;
        CHECK(ProjectManager::openFailure(shared, &problems) == OpenFailure::OpenElsewhere);
        REQUIRE(problems.size() == 1);
        CHECK(problems.first().startsWith(
            "Shared.klh: the book is open in another program that is running ("));
        CHECK_FALSE(pm.openProject(shared));
        CHECK(pm.lastOpenFailure() == OpenFailure::OpenElsewhere);
        CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(mine));

        // A program that has ended, e.g. after a crash, leaves a lock that does not count
        other.stop();
        CHECK(ProjectManager::openFailure(shared) == OpenFailure::None);
        REQUIRE(pm.openProject(shared));
        CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(shared));
    }

#ifndef Q_OS_WIN
    SECTION("a program that is no Kalahari program, with the PID of one that has ended") {
        QProcess sleeper;
        sleeper.start(QStringLiteral("sleep"), {QStringLiteral("30")});
        REQUIRE(sleeper.waitForStarted());
        REQUIRE(kalahari::test::lockBook(folder, sleeper.processId()));
        CHECK(ProjectManager::openFailure(shared) == OpenFailure::None);
        REQUIRE(pm.openProject(shared));
        CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(shared));
        sleeper.kill();
        sleeper.waitForFinished();
    }
#endif
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager opens elements whose package is not installed, read-only",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();

    REQUIRE(pm.createProject(dir.path(), "Thriller", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QString part = pm.addElement(pm.partKind(), "Act One", BookPlace::Main);
    const QString chapter =
        pm.addElement(pm.chapterKindFor(BookPlace::Main, part), "Scene", BookPlace::Main, part);
    REQUIRE_FALSE(chapter.isEmpty());
    pm.setChapterContent(chapter, kmlWith("Dark night"));
    REQUIRE(pm.saveChapterContent(chapter));
    REQUIRE(pm.closeProject(false));

    // Kinds of a package of thrillers, which is not installed
    QJsonObject root = readJson(manifest);
    QJsonArray books = root["books"].toArray();
    QJsonObject book = books.first().toObject();
    QJsonObject group;
    for (const auto& element : book["main"].toArray()) {
        if (element.toObject()["id"].toString() == part) {
            group = element.toObject();
        }
    }
    QJsonObject scene = group["elements"].toArray().first().toObject();
    scene["kind"] = "acme.thriller:scene";
    group["kind"] = "acme.thriller:arc";
    group["elements"] = QJsonArray{scene};
    book["main"] = QJsonArray{group};
    books[0] = book;
    root["books"] = books;
    root["workshop"] = QJsonObject{
        {"elements", QJsonArray{QJsonObject{{"id", "board"},
                                            {"kind", "acme.thriller:board"},
                                            {"title", "Clues"},
                                            {"file", "workshop/board.json"}}}}};
    writeJson(manifest, root);

    QStringList problems;
    REQUIRE(pm.openProject(manifest, &problems));
    CHECK(problems.isEmpty());
    CHECK(pm.isReadOnly());
    CHECK(pm.missingPackages() == QStringList{"acme.thriller"});

    const ProjectElement* arc = pm.findElement(part);
    const ProjectElement* sceneElement = pm.findElement(chapter);
    const ProjectElement* board = pm.findElement("board");
    REQUIRE(arc != nullptr);
    REQUIRE(sceneElement != nullptr);
    REQUIRE(board != nullptr);
    CHECK_FALSE(pm.kindOf(*sceneElement));
    CHECK(pm.formOf(*arc) == ElementForm::Group);
    CHECK(pm.formOf(*sceneElement) == ElementForm::Text);
    CHECK(pm.formOf(*board) == ElementForm::Window);

    // Its text opens, is counted and keeps its status, which does not change: the book is
    // read-only without the package
    CHECK(pm.loadChapterContent(chapter) == kmlWith("Dark night"));
    CHECK(pm.wordCount(chapter) == 2);
    CHECK(pm.getStatusStatistics() == std::map<QString, int>{{"draft", 2}});  // and the title page
    CHECK_FALSE(pm.setStatus(chapter, "final"));
    CHECK(chapterFileOf(chapter).status() == "draft");

    // No new elements of its kinds: the project does not offer them
    CHECK(pm.textKindsFor(BookPlace::Main, part).isEmpty());
    REQUIRE(pm.closeProject(false));
}

// =============================================================================
// Elements
// =============================================================================

TEST_CASE("ProjectManager gives a new element a chapter file named after its kind",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "New Chapters", "Author", "en", true, "kalahari.novel"));
    const QDir project(pm.getProjectPath());

    // The book starts with chapter_001; a file that no element names takes chapter_002
    writeFile(project.filePath("book/chapter_002.kchapter"), "");

    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const QString first = pm.addElement(chapter, "One", BookPlace::Main);
    const QString second = pm.addElement(chapter, "Two", BookPlace::Main);
    const QString dedication =
        pm.addElement(kind("kalahari.novel", "dedication"), "For Anna", BookPlace::Front);
    REQUIRE_FALSE(first.isEmpty());
    REQUIRE_FALSE(second.isEmpty());
    REQUIRE_FALSE(dedication.isEmpty());

    const ProjectElement* one = pm.findElement(first);
    REQUIRE(one != nullptr);
    CHECK(one->file == "book/chapter_003.kchapter");
    CHECK(one->kind == KindReference{"kalahari.base", "chapter"});
    CHECK(one->status == "draft");
    CHECK(pm.findElement(second)->file == "book/chapter_004.kchapter");
    CHECK(pm.findElement(dedication)->file == "book/dedication_001.kchapter");

    const ChapterDocument file = chapterFileOf(first);
    CHECK(file.title() == "One");
    CHECK(file.status() == "draft");
    CHECK(file.kml().isEmpty());

    // The .klh file has them at once
    CHECK_FALSE(pm.isDirty());
    const BookProject saved = savedProject(pm.getManifestPath());
    CHECK(titles(saved.books.first().mainElements) == "Chapter 1, One, Two");
    CHECK(titles(saved.books.first().frontElements) == "Title page, For Anna");
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager offers the kinds of the project's type, within their limits",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Kinds", "Author", "en", true, "kalahari.novel"));

    CHECK(references(pm.textKindsFor(BookPlace::Main)) ==
          "kalahari.novel:prologue, kalahari.base:chapter, kalahari.novel:epilogue");
    CHECK(pm.chapterKindFor(BookPlace::Main).reference() == "kalahari.base:chapter");
    CHECK(pm.partKind().reference() == "kalahari.base:part");

    // One title page: the one the book starts with
    const KindRef titlePage = kind("kalahari.novel", "title_page");
    CHECK(references(pm.textKindsFor(BookPlace::Front)).find("title_page") == std::string::npos);
    CHECK(pm.addElement(titlePage, "Second Title Page", BookPlace::Front).isEmpty());

    // Without it, a new one can be added, with the text of its template
    REQUIRE(pm.removeElement(pm.book()->frontElements.first().id).has_value());
    CHECK(references(pm.textKindsFor(BookPlace::Front)).find("title_page") != std::string::npos);
    const QString newTitlePage = pm.addElement(titlePage, "Title Page", BookPlace::Front);
    REQUIRE_FALSE(newTitlePage.isEmpty());
    CHECK(pm.findElement(newTitlePage)->file == "book/title_page_002.kchapter");
    CHECK(paragraphsOf(newTitlePage) == QStringList{"Kinds", "", "Author"});
    CHECK(references(pm.textKindsFor(BookPlace::Front)).find("title_page") == std::string::npos);

    // A part has no file; chapters, mottos, the prologue and the epilogue go into it
    const QString part = pm.addElement(pm.partKind(), "Part One", BookPlace::Main);
    REQUIRE_FALSE(part.isEmpty());
    CHECK(pm.findElement(part)->file.isEmpty());
    CHECK(pm.findElement(part)->status.isEmpty());
    CHECK(references(pm.textKindsFor(BookPlace::Main, part)) ==
          "kalahari.novel:prologue, kalahari.base:chapter, kalahari.novel:epilogue, "
          "kalahari.base:motto");
    const QString chapter =
        pm.addElement(pm.chapterKindFor(BookPlace::Main, part), "Chapter 1", BookPlace::Main, part);
    REQUIRE_FALSE(chapter.isEmpty());
    REQUIRE(pm.findElement(part)->elements.size() == 1);
    CHECK(pm.findElement(part)->elements.first().id == chapter);

    // The book has one prologue: in the part, none in the body
    REQUIRE_FALSE(
        pm.addElement(kind("kalahari.novel", "prologue"), "Prologue", BookPlace::Main, part)
            .isEmpty());
    CHECK(references(pm.textKindsFor(BookPlace::Main)) ==
          "kalahari.base:chapter, kalahari.novel:epilogue");
    CHECK(pm.addElement(kind("kalahari.novel", "prologue"), "Prologue", BookPlace::Main).isEmpty());

    // Kinds the type does not offer there, window elements and groups that do not exist
    CHECK(pm.addElement(kind("kalahari.base", "introduction"), "Intro", BookPlace::Front)
              .isEmpty());
    CHECK(pm.addElement(kind("kalahari.novel", "toc"), "Contents", BookPlace::Front).isEmpty());
    CHECK(pm.addElement(pm.chapterKindFor(BookPlace::Main), "Lost", BookPlace::Main, "no-group")
              .isEmpty());
    CHECK(pm.addElement(pm.chapterKindFor(BookPlace::Main), "Lost", BookPlace::Main, chapter)
              .isEmpty());
    CHECK(titles(pm.book()->mainElements) == "Chapter 1, Part One");
    REQUIRE(pm.closeProject(false));

    SECTION("Short stories") {
        REQUIRE(pm.createProject(dir.path(), "Stories", "Author", "en", true,
                                 "kalahari.short_stories"));
        CHECK(pm.chapterKindFor(BookPlace::Main).reference() == "kalahari.short_stories:story");
        CHECK(pm.partKind().reference() == "kalahari.short_stories:section");
        REQUIRE(pm.closeProject(false));
    }
}

TEST_CASE("ProjectManager renames, moves and removes elements", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Changes", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();

    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const QString a = pm.addElement(chapter, "A", BookPlace::Main);
    const QString b = pm.addElement(chapter, "B", BookPlace::Main);
    const QString part = pm.addElement(pm.partKind(), "Part", BookPlace::Main);
    const QString c = pm.addElement(chapter, "C", BookPlace::Main, part);
    REQUIRE_FALSE(c.isEmpty());

    REQUIRE(pm.renameElement(a, "First"));
    CHECK(chapterFileOf(a).title() == "First");

    REQUIRE(pm.moveElement(part, 0));
    CHECK(titles(pm.book()->mainElements) == "Part, Chapter 1, First, B");
    CHECK_FALSE(pm.moveElement(b, 4));
    CHECK_FALSE(pm.moveElement("no-element", 0));

    const QString file = pm.filePathOf(*pm.findElement(c));
    const std::optional<ProjectElement> removed = pm.removeElement(part);
    REQUIRE(removed.has_value());
    CHECK(removed->elements.size() == 1);
    CHECK(pm.findElement(c) == nullptr);
    CHECK(QFileInfo::exists(file));  // the files stay
    CHECK_FALSE(pm.removeElement(part).has_value());
    CHECK_FALSE(pm.isDirty());
    REQUIRE(pm.closeProject(false));

    const BookProject saved = savedProject(manifest);
    CHECK(titles(saved.books.first().mainElements) == "Chapter 1, First, B");
}

TEST_CASE("ProjectManager puts a new element in the place of its kind", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Places", "Author", "en", true, "kalahari.novel"));
    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const KindRef prologueKind = kind("kalahari.novel", "prologue");
    const KindRef epilogueKind = kind("kalahari.novel", "epilogue");

    // The prologue goes first, the epilogue last, and a new chapter or part before the epilogue
    const QString epilogue = pm.addElement(epilogueKind, "Epilogue", BookPlace::Main);
    const QString prologue = pm.addElement(prologueKind, "Prologue", BookPlace::Main);
    REQUIRE_FALSE(epilogue.isEmpty());
    REQUIRE_FALSE(prologue.isEmpty());
    REQUIRE_FALSE(pm.addElement(chapter, "Chapter 2", BookPlace::Main).isEmpty());
    const QString part = pm.addElement(pm.partKind(), "Part One", BookPlace::Main);
    REQUIRE_FALSE(part.isEmpty());
    CHECK(titles(pm.book()->mainElements) == "Prologue, Chapter 1, Chapter 2, Part One, Epilogue");
    CHECK(pm.newIndexIn(pm.book()->mainElements, prologueKind) == 0);
    CHECK(pm.newIndexIn(pm.book()->mainElements, chapter) == 4);
    CHECK(pm.newIndexIn(pm.book()->mainElements, epilogueKind) == 5);

    // In a part as well
    REQUIRE(pm.removeElement(prologue).has_value());
    REQUIRE(pm.removeElement(epilogue).has_value());
    REQUIRE_FALSE(pm.addElement(chapter, "Inside", BookPlace::Main, part).isEmpty());
    REQUIRE_FALSE(pm.addElement(epilogueKind, "Last Words", BookPlace::Main, part).isEmpty());
    REQUIRE_FALSE(pm.addElement(prologueKind, "First Words", BookPlace::Main, part).isEmpty());
    REQUIRE_FALSE(pm.addElement(chapter, "Also Inside", BookPlace::Main, part).isEmpty());
    CHECK(titles(pm.findElement(part)->elements) ==
          "First Words, Inside, Also Inside, Last Words");

    // A text file added to the book goes there too
    const QString textPath = QDir(dir.path()).filePath("notes.txt");
    writeFile(textPath, "Notes");
    REQUIRE_FALSE(pm.addFile(textPath, true, chapter, "Notes", BookPlace::Main, part).isEmpty());
    CHECK(titles(pm.findElement(part)->elements) ==
          "First Words, Inside, Also Inside, Notes, Last Words");

    // The place the writer chose: any place in the list, its end too, but not beyond it
    REQUIRE_FALSE(pm.addElement(chapter, "Between", BookPlace::Main, QString(), 1).isEmpty());
    CHECK(titles(pm.book()->mainElements) == "Chapter 1, Between, Chapter 2, Part One");
    CHECK(pm.addElement(chapter, "Beyond", BookPlace::Main, QString(), 5).isEmpty());
    REQUIRE_FALSE(pm.addElement(chapter, "Last", BookPlace::Main, QString(), 4).isEmpty());
    REQUIRE_FALSE(pm.addElement(chapter, "First", BookPlace::Main, part, 0).isEmpty());
    CHECK(titles(pm.findElement(part)->elements) ==
          "First, First Words, Inside, Also Inside, Notes, Last Words");

    // The .klh file has them in their places
    const BookProject saved = savedProject(pm.getManifestPath());
    CHECK(titles(saved.books.first().mainElements) == "Chapter 1, Between, Chapter 2, Part One, Last");
    CHECK(titles(saved.books.first().mainElements.at(3).elements) ==
          "First, First Words, Inside, Also Inside, Notes, Last Words");
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("A new chapter of the body goes before the epilogue that ends the last part",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Epilogue", "Author", "en", true, "kalahari.novel"));
    takeKind("kalahari.short_stories", "story");
    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const KindRef prologueKind = kind("kalahari.novel", "prologue");
    const KindRef epilogueKind = kind("kalahari.novel", "epilogue");

    // The prologue, and Part One with a chapter and the epilogue
    REQUIRE(pm.removeElement(pm.book()->mainElements.first().id).has_value());
    const QString prologue = pm.addElement(prologueKind, "Prologue", BookPlace::Main);
    const QString partOne = pm.addElement(pm.partKind(), "Part One", BookPlace::Main);
    REQUIRE_FALSE(pm.addElement(chapter, "Chapter 1", BookPlace::Main, partOne).isEmpty());
    const QString epilogue = pm.addElement(epilogueKind, "Epilogue", BookPlace::Main, partOne);
    REQUIRE_FALSE(prologue.isEmpty());
    REQUIRE_FALSE(epilogue.isEmpty());
    REQUIRE(titles(pm.book()->mainElements) == "Prologue, Part One");

    // A new chapter of the body goes into Part One, before the epilogue
    CHECK(pm.newPlaceOf(chapter, BookPlace::Main) == ElementPlace{BookPlace::Main, partOne, 1});
    REQUIRE_FALSE(pm.addElement(chapter, "Chapter 2", BookPlace::Main).isEmpty());
    CHECK(titles(pm.book()->mainElements) == "Prologue, Part One");
    CHECK(titles(pm.findElement(partOne)->elements) == "Chapter 1, Chapter 2, Epilogue");

    // So does a text file added to the body
    const QString textPath = QDir(dir.path()).filePath("notes.txt");
    writeFile(textPath, "Notes");
    REQUIRE_FALSE(pm.addFile(textPath, true, chapter, "Notes", BookPlace::Main).isEmpty());
    CHECK(titles(pm.findElement(partOne)->elements) == "Chapter 1, Chapter 2, Notes, Epilogue");

    // The prologue, a part, the epilogue, an element of a part and of the front keep the place
    // of their kind
    CHECK(pm.newPlaceOf(prologueKind, BookPlace::Main) ==
          ElementPlace{BookPlace::Main, QString(), 0});
    CHECK(pm.newPlaceOf(pm.partKind(), BookPlace::Main) ==
          ElementPlace{BookPlace::Main, QString(), 2});
    CHECK(pm.newPlaceOf(epilogueKind, BookPlace::Main) ==
          ElementPlace{BookPlace::Main, QString(), 2});
    CHECK(pm.newPlaceOf(chapter, BookPlace::Main, partOne) ==
          ElementPlace{BookPlace::Main, partOne, 3});
    CHECK(pm.newPlaceOf(kind("kalahari.base", "dedication"), BookPlace::Front) ==
          ElementPlace{BookPlace::Front, QString(), 1});
    CHECK(pm.newPlaceOf(chapter, BookPlace::Main, "no-group") ==
          ElementPlace{BookPlace::Main, "no-group", 0});

    // An empty part after Part One: the epilogue still ends the body
    const QString partTwo = pm.addElement(pm.partKind(), "Part Two", BookPlace::Main);
    REQUIRE(titles(pm.book()->mainElements) == "Prologue, Part One, Part Two");
    CHECK(pm.newPlaceOf(chapter, BookPlace::Main) == ElementPlace{BookPlace::Main, partOne, 3});

    // A chapter in Part Two: nothing ends the body, a new chapter goes to its end
    REQUIRE_FALSE(pm.addElement(chapter, "Chapter 3", BookPlace::Main, partTwo).isEmpty());
    CHECK(pm.newPlaceOf(chapter, BookPlace::Main) == ElementPlace{BookPlace::Main, QString(), 3});

    // A kind that cannot be in a part goes to the end of the body
    const KindRef story = kind("kalahari.short_stories", "story");
    REQUIRE(pm.removeElement(pm.findElement(partTwo)->elements.first().id).has_value());
    CHECK(pm.newPlaceOf(story, BookPlace::Main) == ElementPlace{BookPlace::Main, QString(), 3});

    // The epilogue at the end of the body: a new chapter goes before it, in the body
    REQUIRE(pm.removeElement(epilogue).has_value());
    REQUIRE_FALSE(pm.addElement(epilogueKind, "Epilogue", BookPlace::Main).isEmpty());
    REQUIRE(titles(pm.book()->mainElements) == "Prologue, Part One, Part Two, Epilogue");
    CHECK(pm.newPlaceOf(chapter, BookPlace::Main) == ElementPlace{BookPlace::Main, QString(), 3});
    REQUIRE_FALSE(pm.addElement(chapter, "Chapter 4", BookPlace::Main).isEmpty());
    CHECK(titles(pm.book()->mainElements) ==
          "Prologue, Part One, Part Two, Chapter 4, Epilogue");

    // The .klh file has them in their places
    const BookProject saved = savedProject(pm.getManifestPath());
    CHECK(saved.books.first().mainElements == pm.book()->mainElements);
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("A new part takes the epilogue from the end of the last part", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Parts", "Author", "en", true, "kalahari.novel"));
    takeKind("kalahari.short_stories", "story");
    const QString manifest = pm.getManifestPath();
    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const KindRef part = pm.partKind();

    // Part One with a chapter and the epilogue, and a story, which cannot be in a part
    REQUIRE(pm.removeElement(pm.book()->mainElements.first().id).has_value());
    const QString partOne = pm.addElement(part, "Part One", BookPlace::Main);
    REQUIRE_FALSE(pm.addElement(chapter, "Chapter 1", BookPlace::Main, partOne).isEmpty());
    const QString epilogue =
        pm.addElement(kind("kalahari.novel", "epilogue"), "Epilogue", BookPlace::Main, partOne);
    REQUIRE_FALSE(epilogue.isEmpty());
    const QString story = pm.addElement(kind("kalahari.short_stories", "story"), "Story",
                                        BookPlace::Main, QString(), 0);
    REQUIRE_FALSE(story.isEmpty());
    const QString titlePage = pm.book()->frontElements.first().id;
    const QString epilogueFile = pm.findElement(epilogue)->file;
    const QDir bookFolder(QDir(pm.getProjectPath()).filePath("book"));
    const BookProject before = *pm.project();
    const QStringList files = bookFolder.entryList(QDir::Files);

    // Only a group takes elements inside it, each once, and only elements of its part of the
    // book that can be inside it
    const auto takes = [&pm](const KindRef& kind, const QStringList& inside) {
        return pm.addElement(kind, "New", BookPlace::Main, QString(), -1, inside);
    };
    CHECK(takes(chapter, {epilogue}).isEmpty());
    CHECK(takes(part, {story}).isEmpty());
    CHECK(takes(part, {partOne}).isEmpty());
    CHECK(takes(part, {epilogue, epilogue}).isEmpty());
    CHECK(takes(part, {titlePage}).isEmpty());
    CHECK(takes(part, {"no-element"}).isEmpty());
    CHECK(*pm.project() == before);
    CHECK(bookFolder.entryList(QDir::Files) == files);

    // Part Two takes the epilogue; the epilogue keeps its file
    const QString partTwo = takes(part, {epilogue});
    REQUIRE_FALSE(partTwo.isEmpty());
    CHECK(titles(pm.book()->mainElements) == "Story, Part One, New");
    CHECK(titles(pm.findElement(partOne)->elements) == "Chapter 1");
    CHECK(titles(pm.findElement(partTwo)->elements) == "Epilogue");
    CHECK(pm.findElement(epilogue)->file == epilogueFile);

    // A new chapter of the body goes into Part Two, before the epilogue
    CHECK(pm.newPlaceOf(chapter, BookPlace::Main) == ElementPlace{BookPlace::Main, partTwo, 0});
    REQUIRE_FALSE(pm.addElement(chapter, "Chapter 2", BookPlace::Main).isEmpty());
    CHECK(titles(pm.findElement(partTwo)->elements) == "Chapter 2, Epilogue");
    CHECK(savedProject(manifest).books.first().mainElements == pm.book()->mainElements);

    // A new part at the place of the epilogue that it takes goes where the epilogue was
    REQUIRE(pm.removeElement(epilogue).has_value());
    const QString atEnd =
        pm.addElement(kind("kalahari.novel", "epilogue"), "Last Words", BookPlace::Main);
    REQUIRE(titles(pm.book()->mainElements) == "Story, Part One, New, Last Words");
    const QString partThree =
        pm.addElement(part, "Part Three", BookPlace::Main, QString(), 3, {atEnd});
    REQUIRE_FALSE(partThree.isEmpty());
    CHECK(titles(pm.book()->mainElements) == "Story, Part One, New, Part Three");
    CHECK(titles(pm.findElement(partThree)->elements) == "Last Words");

    // A project that cannot be saved stays as it was, without the new chapter file
    const BookProject unchanged = *pm.project();
    const QStringList chapterFiles = bookFolder.entryList(QDir::Files);
    REQUIRE(QFile::remove(manifest));
    REQUIRE(QDir().mkdir(manifest));
    CHECK(pm.addElement(part, "Part Four", BookPlace::Main, QString(), -1, {atEnd}).isEmpty());
    CHECK(pm.addElement(chapter, "Chapter 3", BookPlace::Main).isEmpty());
    CHECK(*pm.project() == unchanged);
    CHECK(bookFolder.entryList(QDir::Files) == chapterFiles);
    REQUIRE(QDir().rmdir(manifest));
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("A new element added after another goes right after it", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "After", "Author", "en", true, "kalahari.novel"));
    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const KindRef prologueKind = kind("kalahari.novel", "prologue");
    const KindRef epilogueKind = kind("kalahari.novel", "epilogue");
    const KindRef note = kind("kalahari.base", "work_note");

    // Prologue, Chapter 1, Chapter 2, Part One (Inside), Epilogue
    const QString prologue = pm.addElement(prologueKind, "Prologue", BookPlace::Main);
    const QString first = pm.book()->mainElements.at(1).id;
    const QString second = pm.addElement(chapter, "Chapter 2", BookPlace::Main);
    const QString part = pm.addElement(pm.partKind(), "Part One", BookPlace::Main);
    const QString inside = pm.addElement(chapter, "Inside", BookPlace::Main, part);
    const QString epilogue = pm.addElement(epilogueKind, "Epilogue", BookPlace::Main);
    REQUIRE(titles(pm.book()->mainElements) ==
            "Prologue, Chapter 1, Chapter 2, Part One, Epilogue");

    // Right after the element, in its list
    CHECK(pm.newPlaceAfter(first, chapter) == ElementPlace{BookPlace::Main, QString(), 2});
    CHECK(pm.newPlaceAfter(prologue, chapter) == ElementPlace{BookPlace::Main, QString(), 1});
    CHECK(pm.newPlaceAfter(inside, chapter) == ElementPlace{BookPlace::Main, part, 1});
    CHECK(pm.newPlaceAfter(second, pm.partKind()) == ElementPlace{BookPlace::Main, QString(), 3});

    // Not after the epilogue, but before it
    CHECK(pm.newPlaceAfter(epilogue, chapter) == ElementPlace{BookPlace::Main, QString(), 4});

    // A prologue goes where its kind goes
    REQUIRE(pm.removeElement(prologue).has_value());
    CHECK(pm.newPlaceAfter(second, prologueKind) == ElementPlace{BookPlace::Main, QString(), 0});

    // No place: a kind that cannot be in the list, an element that the project does not have,
    // a file of the book outside its text, no kind
    CHECK_FALSE(pm.newPlaceAfter(inside, pm.partKind()).has_value());
    CHECK_FALSE(pm.newPlaceAfter(first, note).has_value());
    CHECK_FALSE(pm.newPlaceAfter("no-element", chapter).has_value());
    const QString bookFile = pm.addBookFile("Notes");
    REQUIRE_FALSE(bookFile.isEmpty());
    CHECK_FALSE(pm.newPlaceAfter(bookFile, chapter).has_value());
    CHECK_FALSE(pm.newPlaceAfter(first, KindRef{}).has_value());

    // The element added there, with the status the writer chose
    const std::optional<ElementPlace> at = pm.newPlaceAfter(first, chapter);
    REQUIRE(at.has_value());
    const QString added =
        pm.addElement(chapter, "Chapter 1a", at->place, at->groupId, at->index, {}, std::nullopt,
                      "revision");
    REQUIRE_FALSE(added.isEmpty());
    CHECK(titles(pm.book()->mainElements) == "Chapter 1, Chapter 1a, Chapter 2, Part One, Epilogue");
    CHECK(pm.findElement(added)->status == "revision");
    CHECK(chapterFileOf(added).status() == "revision");
    CHECK(pm.addElement(chapter, "Wrong", BookPlace::Main, QString(), -1, {}, std::nullopt,
                        "finished")
              .isEmpty());
    CHECK(titles(pm.book()->mainElements) == "Chapter 1, Chapter 1a, Chapter 2, Part One, Epilogue");

    // In the Workshop too
    const QString idea = pm.addElement(note, "Idea", BookPlace::Workshop);
    REQUIRE_FALSE(idea.isEmpty());
    CHECK(pm.newPlaceAfter(idea, note) == ElementPlace{BookPlace::Workshop, QString(), 1});
    CHECK_FALSE(pm.newPlaceAfter(idea, chapter).has_value());
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager adds working notes and copies of files to the Workshop",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Workshop", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QDir project(pm.getProjectPath());
    const KindRef note = kind("kalahari.base", "work_note");
    const KindRef material = kind("kalahari.base", "material");

    // The kinds of the Workshop; the program adds working notes and materials, and the other
    // kinds that open in a window of their own wait for their windows
    CHECK(references(pm.kindsFor(BookPlace::Workshop)) ==
          "kalahari.base:work_note, kalahari.base:mindmap, kalahari.base:timeline, "
          "kalahari.base:character, kalahari.base:location, kalahari.base:item, "
          "kalahari.base:material");
    CHECK(ProjectManager::canAdd(note));
    CHECK(ProjectManager::canAdd(material));
    CHECK(ProjectManager::canAdd(pm.chapterKindFor(BookPlace::Main)));
    CHECK(ProjectManager::canAdd(pm.partKind()));
    CHECK_FALSE(ProjectManager::canAdd(kind("kalahari.base", "character")));
    CHECK_FALSE(ProjectManager::canAdd(kind("kalahari.base", "toc")));
    CHECK_FALSE(ProjectManager::canAdd(KindRef{}));

    // A working note is a text in the Workshop's folder
    const QString idea = pm.addElement(note, "Idea", BookPlace::Workshop);
    REQUIRE_FALSE(idea.isEmpty());
    CHECK(pm.findElement(idea)->file == "workshop/work_note_001.kchapter");
    CHECK(pm.formOf(*pm.findElement(idea)) == ElementForm::Text);

    // A material is a copy of the file, named as it, without a status; the file stays
    const QString picture = QDir(dir.path()).filePath("island map.jpg");
    writeFile(picture, "a picture");
    CHECK(pm.newMaterialFile(picture) == "workshop/island map.jpg");
    const QString map = pm.addMaterial(picture, material, "Island map");
    REQUIRE_FALSE(map.isEmpty());
    const ProjectElement* mapElement = pm.findElement(map);
    REQUIRE(mapElement != nullptr);
    CHECK(mapElement->file == "workshop/island map.jpg");
    CHECK(mapElement->kind == KindReference{"kalahari.base", "material"});
    CHECK(mapElement->status.isEmpty());
    CHECK(pm.formOf(*mapElement) == ElementForm::Window);
    QFile copy(pm.filePathOf(*mapElement));
    REQUIRE(copy.open(QIODevice::ReadOnly));
    CHECK(copy.readAll() == "a picture");
    CHECK(QFileInfo::exists(picture));

    // A name the Workshop has gets a number, also when only a file has it; a name without a
    // suffix too
    writeFile(project.filePath("workshop/island map_2.jpg"), "the writer's file");
    CHECK(pm.newMaterialFile(picture) == "workshop/island map_3.jpg");
    const QString again = pm.addMaterial(picture, material, "Island map");
    REQUIRE_FALSE(again.isEmpty());
    CHECK(pm.findElement(again)->file == "workshop/island map_3.jpg");
    const QString readme = QDir(dir.path()).filePath("README");
    writeFile(readme, "notes");
    REQUIRE_FALSE(pm.addMaterial(readme, material, "Readme").isEmpty());
    CHECK(pm.newMaterialFile(readme) == "workshop/README_2");
    CHECK(titles(pm.project()->workshop.elements) == "Idea, Island map, Island map, Readme");

    // Not a kind of materials, no file that can be read, or a project that cannot be saved:
    // nothing is added, and no copy stays
    const QStringList files = QDir(project.filePath("workshop")).entryList(QDir::Files);
    CHECK(pm.addMaterial(picture, note, "Wrong").isEmpty());
    CHECK(pm.addMaterial(picture, kind("kalahari.base", "character"), "Wrong").isEmpty());
    CHECK(pm.addMaterial(picture, pm.chapterKindFor(BookPlace::Main), "Wrong").isEmpty());
    CHECK(pm.addMaterial(QDir(dir.path()).filePath("no file.jpg"), material, "Wrong").isEmpty());
    CHECK(pm.addMaterial(dir.path(), material, "Wrong").isEmpty());
    const BookProject unchanged = *pm.project();
    REQUIRE(QFile::remove(manifest));
    REQUIRE(QDir().mkdir(manifest));
    CHECK(pm.addMaterial(picture, material, "Wrong").isEmpty());
    REQUIRE(QDir().rmdir(manifest));
    CHECK(*pm.project() == unchanged);
    CHECK(QDir(project.filePath("workshop")).entryList(QDir::Files) == files);

    // The .klh file has them, and the materials are no files without an element
    REQUIRE(pm.saveManifest());
    CHECK(pm.unassignedFiles().isEmpty());
    REQUIRE(pm.closeProject(false));
    const BookProject saved = savedProject(manifest);
    CHECK(titles(saved.workshop.elements) == "Idea, Island map, Island map, Readme");
    CHECK(saved.workshop.elements.at(1).file == "workshop/island map.jpg");

    // Without a book there is no file
    CHECK(pm.newMaterialFile(picture).isEmpty());
    CHECK(pm.addMaterial(picture, material, "Nothing").isEmpty());
}

TEST_CASE("ProjectManager keeps the description, own fields, links and turn of a material",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Materials", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QString first =
        pm.addElement(pm.chapterKindFor(BookPlace::Main), "Chapter 1", BookPlace::Main);
    const QString second =
        pm.addElement(pm.chapterKindFor(BookPlace::Main), "Chapter 2", BookPlace::Main);
    REQUIRE_FALSE(first.isEmpty());
    REQUIRE_FALSE(second.isEmpty());
    const QString picture = QDir(dir.path()).filePath("map.png");
    writeFile(picture, "a picture");
    const KindRef material = kind("kalahari.base", "material");
    const QString map = pm.addMaterial(picture, material, "Map");
    const QString photo = pm.addMaterial(picture, material, "Photo");
    REQUIRE_FALSE(map.isEmpty());
    REQUIRE_FALSE(photo.isEmpty());
    CHECK(pm.isMaterial(*pm.findElement(map)));
    CHECK_FALSE(pm.isMaterial(*pm.findElement(first)));
    const QString body = pm.project()->sectionLink(BookPlace::Main);
    REQUIRE_FALSE(body.isEmpty());

    // The description is the notes of an element without a chapter file
    REQUIRE(pm.setDescription(map, "The oasis and the road."));
    CHECK(pm.findElement(map)->description == "The oasis and the road.");
    CHECK(pm.notes(map) == "The oasis and the road.");
    REQUIRE(pm.setNotes(photo, "Taken in 1931."));
    CHECK(pm.findElement(photo)->description == "Taken in 1931.");

    // The writer's own fields, in the writer's order; their names are suggested for others
    REQUIRE(pm.setFields(map, {{"Scale", "1 : 50 000", {}}, {"source", "the author", {}}}));
    REQUIRE(pm.setFields(photo, {{"Scale", "", {}}, {"Place", "Siwa", {}}, {" ", "", {}}}));
    CHECK(pm.findElement(map)->fields.size() == 2);
    CHECK(pm.findElement(photo)->fields.at(1).value == "Siwa");
    CHECK(pm.fieldNames() == QStringList{"Place", "Scale", "source"});

    // Links: only what the project has, not the element itself, each once
    REQUIRE(pm.setLinks(map, {first, body, "nothing", map, first, second}));
    CHECK(pm.findElement(map)->links == QStringList{first, body, second});
    REQUIRE(pm.setLinks(photo, {second}));
    CHECK(pm.materialsLinkedTo(first) == QList<const ProjectElement*>{pm.findElement(map)});
    CHECK(pm.materialsLinkedTo(second) ==
          (QList<const ProjectElement*>{pm.findElement(map), pm.findElement(photo)}));
    CHECK(pm.materialsLinkedTo(body).size() == 1);
    CHECK(pm.materialsLinkedTo(QString()).isEmpty());

    // The turn of the picture: a right angle only
    REQUIRE(pm.setRotation(map, 90));
    CHECK(pm.findElement(map)->rotation == 90);
    CHECK_FALSE(pm.setRotation(map, 45));
    CHECK_FALSE(pm.setRotation("nothing", 180));
    CHECK(pm.findElement(map)->rotation == 90);

    // A .klh file that cannot be written leaves everything as it was
    const BookProject unchanged = *pm.project();
    REQUIRE(QFile::remove(manifest));
    REQUIRE(QDir().mkdir(manifest));
    CHECK_FALSE(pm.setDescription(map, "Changed"));
    CHECK_FALSE(pm.setFields(map, {}));
    CHECK_FALSE(pm.setLinks(map, {}));
    CHECK_FALSE(pm.setRotation(map, 180));
    CHECK_FALSE(pm.removeElement(second).has_value());
    CHECK(*pm.project() == unchanged);
    REQUIRE(QDir().rmdir(manifest));
    REQUIRE(pm.saveManifest());

    // An element deleted from the book leaves the links
    REQUIRE(pm.removeElement(second).has_value());
    CHECK(pm.findElement(map)->links == QStringList{first, body});
    CHECK(pm.findElement(photo)->links.isEmpty());

    // The .klh file has them all
    REQUIRE(pm.closeProject(false));
    const BookProject saved = savedProject(manifest);
    const ProjectElement* savedMap = saved.findElement(map);
    REQUIRE(savedMap != nullptr);
    CHECK(savedMap->description == "The oasis and the road.");
    CHECK(savedMap->fields.at(0).name == "Scale");
    CHECK(savedMap->links == QStringList{first, body});
    CHECK(savedMap->rotation == 90);
    REQUIRE(pm.openProject(manifest));
    CHECK(pm.notes(map) == "The oasis and the road.");
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager adds a material with a web address, with a picture's data, or "
          "without a file",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Sources", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QDir project(QFileInfo(manifest).absolutePath());
    const KindRef material = kind("kalahari.base", "material");
    const QString chapter =
        pm.addElement(pm.chapterKindFor(BookPlace::Main), "Chapter 1", BookPlace::Main);
    REQUIRE_FALSE(chapter.isEmpty());
    const QString body = pm.project()->sectionLink(BookPlace::Main);

    // A web address only: no file
    NewMaterial page;
    page.title = "Kalahari";
    page.url = "https://pl.wikipedia.org/wiki/Kalahari";
    page.links = {chapter, "nothing", chapter, body};
    const QString pageId = pm.addMaterial(page, material);
    REQUIRE_FALSE(pageId.isEmpty());
    const ProjectElement* pageElement = pm.findElement(pageId);
    CHECK(pageElement->url == "https://pl.wikipedia.org/wiki/Kalahari");
    CHECK(pageElement->file.isEmpty());
    CHECK(pageElement->links == QStringList{chapter, body});

    // Neither a file nor an address: a paper book, a talk
    NewMaterial book;
    book.title = "Caravan Trails";
    const QString bookId = pm.addMaterial(book, material);
    REQUIRE_FALSE(bookId.isEmpty());
    CHECK(pm.findElement(bookId)->file.isEmpty());
    CHECK(pm.findElement(bookId)->url.isEmpty());

    // A picture from the clipboard: its data in a file of the Workshop
    NewMaterial shot;
    shot.title = "Screenshot";
    shot.data = "png data";
    shot.fileName = "screenshot.png";
    const QString shotId = pm.addMaterial(shot, material);
    REQUIRE_FALSE(shotId.isEmpty());
    CHECK(pm.findElement(shotId)->file == "workshop/screenshot.png");
    QFile shotFile(project.filePath("workshop/screenshot.png"));
    REQUIRE(shotFile.open(QIODevice::ReadOnly));
    CHECK(shotFile.readAll() == "png data");
    shotFile.close();
    CHECK(pm.findElement(pm.addMaterial(shot, material))->file == "workshop/screenshot_2.png");

    // The copy of a picture on the web, saved later, named as the picture
    NewMaterial camel;
    camel.title = "Camel";
    camel.url = "https://example.org/camel.jpg";
    const QString camelId = pm.addMaterial(camel, material);
    REQUIRE_FALSE(camelId.isEmpty());
    REQUIRE(pm.setMaterialCopy(camelId, "jpeg data", "camel.jpg"));
    CHECK(pm.findElement(camelId)->file == "workshop/camel.jpg");
    CHECK(pm.findElement(camelId)->url == "https://example.org/camel.jpg");
    // Once: a material with a file keeps it
    CHECK_FALSE(pm.setMaterialCopy(camelId, "other", "camel.jpg"));
    CHECK_FALSE(pm.setMaterialCopy("nothing", "other", "camel.jpg"));
    CHECK_FALSE(QFileInfo::exists(project.filePath("workshop/camel_2.jpg")));

    // A .klh file that cannot be written leaves no copy and no element
    const qsizetype count = pm.project()->workshop.elements.size();
    REQUIRE(QFile::remove(manifest));
    REQUIRE(QDir().mkdir(manifest));
    CHECK(pm.addMaterial(shot, material).isEmpty());
    CHECK_FALSE(pm.setMaterialCopy(pageId, "jpeg data", "page.jpg"));
    REQUIRE(QDir().rmdir(manifest));
    CHECK(pm.project()->workshop.elements.size() == count);
    CHECK(pm.findElement(pageId)->file.isEmpty());
    CHECK_FALSE(QFileInfo::exists(project.filePath("workshop/screenshot_3.png")));
    CHECK_FALSE(QFileInfo::exists(project.filePath("workshop/page.jpg")));
    REQUIRE(pm.saveManifest());

    // The .klh file has the addresses
    REQUIRE(pm.closeProject(false));
    const BookProject saved = savedProject(manifest);
    REQUIRE(saved.findElement(pageId) != nullptr);
    CHECK(saved.findElement(pageId)->url == "https://pl.wikipedia.org/wiki/Kalahari");
    CHECK(saved.findElement(camelId)->file == "workshop/camel.jpg");
    CHECK(saved.findElement(bookId)->file.isEmpty());
}

TEST_CASE("Move to Start and Move to End keep the prologue first and the epilogue last",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Moves", "Author", "en", true, "kalahari.novel"));
    const QString prologue =
        pm.addElement(kind("kalahari.novel", "prologue"), "Prologue", BookPlace::Main);
    const QString epilogue =
        pm.addElement(kind("kalahari.novel", "epilogue"), "Epilogue", BookPlace::Main);
    const QString second = pm.addElement(pm.chapterKindFor(BookPlace::Main), "Chapter 2",
                                         BookPlace::Main);
    REQUIRE(titles(pm.book()->mainElements) == "Prologue, Chapter 1, Chapter 2, Epilogue");
    const QString first = pm.book()->mainElements.at(1).id;

    // A chapter goes after the prologue and before the epilogue
    CHECK(pm.startIndexOf(second) == 1);
    CHECK(pm.startIndexOf(first) == 1);
    CHECK(pm.endIndexOf(first) == 2);
    CHECK(pm.endIndexOf(second) == 2);

    // The prologue goes first and the epilogue last; neither passes the other
    CHECK(pm.startIndexOf(prologue) == 0);
    CHECK(pm.endIndexOf(prologue) == 2);
    CHECK(pm.startIndexOf(epilogue) == 1);
    CHECK(pm.endIndexOf(epilogue) == 3);

    // Without them, the start and the end of the list
    REQUIRE(pm.removeElement(prologue).has_value());
    REQUIRE(pm.removeElement(epilogue).has_value());
    CHECK(pm.startIndexOf(second) == 0);
    CHECK(pm.endIndexOf(first) == 1);
    CHECK(pm.startIndexOf("no-element") == -1);
    CHECK(pm.endIndexOf("no-element") == -1);
    REQUIRE(pm.closeProject(false));
    CHECK(pm.startIndexOf(second) == -1);
}

TEST_CASE("ProjectManager moves an element to another part of the book or group, and saves it",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Places", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QString part = pm.addElement(pm.partKind(), "Part One", BookPlace::Main);
    const QString epilogue =
        pm.addElement(kind("kalahari.novel", "epilogue"), "Epilogue", BookPlace::Main);
    REQUIRE(titles(pm.book()->mainElements) == "Chapter 1, Part One, Epilogue");
    const QString chapter = pm.book()->mainElements.first().id;
    const auto inBody = [](const QString& groupId, qsizetype index) {
        return ElementPlace{BookPlace::Main, groupId, index};
    };

    // The epilogue goes into the part, also in the .klh file; its file stays where it is
    const QString file = pm.filePathOf(*pm.findElement(epilogue));
    CHECK(pm.movePlacesOf(epilogue).contains(inBody(part, 0)));
    CHECK(pm.canMoveTo(epilogue, inBody(part, 0)));
    CHECK_FALSE(pm.canMoveTo(epilogue, {BookPlace::Back, QString(), 0}));
    REQUIRE(pm.moveElementTo(epilogue, inBody(part, 0)));
    CHECK(titles(pm.book()->mainElements) == "Chapter 1, Part One");
    CHECK(titles(pm.findElement(part)->elements) == "Epilogue");
    CHECK(pm.filePathOf(*pm.findElement(epilogue)) == file);
    CHECK(QFileInfo::exists(file));
    CHECK(titles(savedProject(manifest).books.first().mainElements.last().elements) ==
          "Epilogue");
    CHECK_FALSE(pm.isDirty());

    // A chapter into the part, before the epilogue; to the place it has: nothing changes
    REQUIRE(pm.moveElementTo(chapter, inBody(part, 0)));
    CHECK(titles(pm.findElement(part)->elements) == "Chapter 1, Epilogue");
    REQUIRE(pm.moveElementTo(chapter, inBody(part, 0)));
    CHECK(titles(pm.findElement(part)->elements) == "Chapter 1, Epilogue");

    // A chapter can also be in the front and the back section (test report of 19 October,
    // part 3: the only chapter of a book could not be moved), but not before the title page
    CHECK(pm.canMoveTo(chapter, {BookPlace::Front, QString(), 1}));
    CHECK_FALSE(pm.canMoveTo(chapter, {BookPlace::Front, QString(), 0}));
    CHECK(pm.canMoveTo(chapter, {BookPlace::Back, QString(), 0}));

    // Where its kind cannot be, or a place that is not in the book: the book stays as it was
    const BookProject unchanged = *pm.project();
    CHECK_FALSE(pm.moveElementTo(part, {BookPlace::Back, QString(), 0}));
    CHECK_FALSE(pm.moveElementTo(part, inBody(part, 0)));
    CHECK_FALSE(pm.moveElementTo(chapter, inBody(part, 5)));
    CHECK_FALSE(pm.moveElementTo("no-element", inBody(QString(), 0)));
    CHECK(pm.movePlacesOf("no-element").isEmpty());
    CHECK(*pm.project() == unchanged);

    // A book that cannot be saved stays as it was
    REQUIRE(QFile::remove(manifest));
    REQUIRE(QDir().mkdir(manifest));
    CHECK_FALSE(pm.moveElementTo(epilogue, inBody(QString(), 1)));
    CHECK(*pm.project() == unchanged);
    REQUIRE(QDir().rmdir(manifest));
    REQUIRE(pm.closeProject(false));
    CHECK(pm.movePlacesOf(epilogue).isEmpty());
}

// =============================================================================
// Files added to the project
// =============================================================================

TEST_CASE("ProjectManager makes a chapter of a text file added to the project",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Text Import", "Author", "en", true, "kalahari.novel"));

    const QString textPath = QDir(dir.path()).filePath("notes.txt");
    writeFile(textPath, "First line\r\nSecond & last\r\n");
    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);

    SECTION("Copied") {
        const QString id = pm.addFile(textPath, true, chapter, "Notes", BookPlace::Main);
        REQUIRE_FALSE(id.isEmpty());
        CHECK(pm.findElement(id)->file == "book/chapter_002.kchapter");
        CHECK(pm.findElement(id)->status == "draft");
        CHECK(QFile::exists(textPath));
        CHECK(pm.wordCount(id) == 4);  // "&" is no word

        kalahari::editor::KmlDocumentModel model;
        REQUIRE(model.loadKml(pm.loadChapterContent(id)));
        REQUIRE(model.paragraphCount() == 2);  // the last line end makes no empty paragraph
        CHECK(model.paragraphText(0) == "First line");
        CHECK(model.paragraphText(1) == "Second & last");
    }

    SECTION("An empty line before the last line end stays an empty paragraph") {
        writeFile(textPath, "First line\n\n");
        const QString id = pm.addFile(textPath, true, chapter, "Notes", BookPlace::Main);
        REQUIRE_FALSE(id.isEmpty());

        kalahari::editor::KmlDocumentModel model;
        REQUIRE(model.loadKml(pm.loadChapterContent(id)));
        REQUIRE(model.paragraphCount() == 2);
        CHECK(model.paragraphText(0) == "First line");
        CHECK(model.paragraphText(1).isEmpty());
    }

    SECTION("Moved") {
        const QString id = pm.addFile(textPath, false, chapter, "Notes", BookPlace::Main);
        REQUIRE_FALSE(id.isEmpty());
        CHECK_FALSE(QFile::exists(textPath));
        CHECK(pm.loadChapterContent(id).contains("Second &amp; last"));
    }

    SECTION("Not a chapter or text file") {
        const QString imagePath = QDir(dir.path()).filePath("cover.png");
        writeFile(imagePath, "png");
        CHECK(pm.addFile(imagePath, false, chapter, "Cover", BookPlace::Main).isEmpty());
        CHECK(QFile::exists(imagePath));
        CHECK(titles(pm.book()->mainElements) == "Chapter 1");
    }

    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager adds a chapter file with its notes and status", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Chapter Import", "Author", "en", true,
                             "kalahari.novel"));

    const QString source = QDir(dir.path()).filePath("old.kchapter");
    ChapterDocument chapter = ChapterDocument::fromKmlContent(kmlWith("Old text"), "Old title");
    chapter.setNotes("Remember the dog");
    chapter.setStatus("revision");
    chapter.setComments(QJsonArray{QJsonObject{{"id", "c1"}}});
    REQUIRE(chapter.save(source));

    const QString id = pm.addFile(source, false, kind("kalahari.novel", "preface"), "Preface",
                                  BookPlace::Front);
    REQUIRE_FALSE(id.isEmpty());
    CHECK_FALSE(QFile::exists(source));
    CHECK(pm.findElement(id)->file == "book/preface_001.kchapter");
    CHECK(pm.findElement(id)->status == "revision");
    CHECK(pm.notes(id) == "Remember the dog");
    CHECK(pm.wordCount(id) == 2);

    const ChapterDocument added = chapterFileOf(id);
    CHECK(added.title() == "Preface");
    CHECK(added.kml() == kmlWith("Old text"));
    CHECK(added.comments().size() == 1);
    REQUIRE(pm.closeProject(false));
}

/// Files and titles of @p files, "file: title" each
std::string fileTitles(const QList<UnassignedFile>& files) {
    QStringList list;
    for (const UnassignedFile& file : files) {
        list << file.file + QStringLiteral(": ") + file.title;
    }
    return list.join(QStringLiteral(", ")).toStdString();
}

TEST_CASE("ProjectManager gives the files of an element and of the elements inside it",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Files", "Author", "en", true, "kalahari.novel"));

    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const QString part = pm.addElement(pm.partKind(), "Part", BookPlace::Main);
    const QString a = pm.addElement(chapter, "A", BookPlace::Main, part);
    const QString b = pm.addElement(chapter, "B", BookPlace::Main, part);
    REQUIRE_FALSE(b.isEmpty());
    const QString fileA = pm.filePathOf(*pm.findElement(a));
    const QString fileB = pm.filePathOf(*pm.findElement(b));

    // In reading order; a part has no file
    CHECK(pm.filesOf(*pm.findElement(part)) == QStringList{fileA, fileB});
    CHECK(pm.filesOf(*pm.findElement(b)) == QStringList{fileB});

    SECTION("A file that is not on the disk is left out") {
        REQUIRE(QFile::remove(fileA));
        CHECK(pm.filesOf(*pm.findElement(part)) == QStringList{fileB});
    }

    SECTION("A file that an element outside them has is left out") {
        const QString chapterOne = pm.book()->mainElements.first().id;
        pm.findElement(chapterOne)->file = pm.findElement(b)->file;
        CHECK(pm.filesOf(*pm.findElement(part)) == QStringList{fileA});

        // Two of them with one file: it is there once
        pm.findElement(b)->file = pm.findElement(a)->file;
        CHECK(pm.filesOf(*pm.findElement(part)) == QStringList{fileA});
    }

    SECTION("Also of an element taken out of the project") {
        const std::optional<ProjectElement> removed = pm.removeElement(part);
        REQUIRE(removed.has_value());
        CHECK(pm.filesOf(*removed) == QStringList{fileA, fileB});
    }

    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager lists the chapter files that no element has", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Unassigned", "Author", "en", true, "kalahari.novel"));
    const QDir project(pm.getProjectPath());
    CHECK(pm.unassignedFiles().isEmpty());

    // The files of deleted elements stay, with their titles
    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const QString part = pm.addElement(pm.partKind(), "Part", BookPlace::Main);
    REQUIRE_FALSE(pm.addElement(chapter, "A", BookPlace::Main, part).isEmpty());
    REQUIRE_FALSE(pm.addElement(chapter, "B", BookPlace::Main, part).isEmpty());
    REQUIRE(pm.removeElement(part).has_value());
    CHECK(fileTitles(pm.unassignedFiles()) ==
          "book/chapter_002.kchapter: A, book/chapter_003.kchapter: B");

    // Chapter files of the Workshop too, and one that cannot be read, without a title; not the
    // writer's other files, nor the files in folders of these folders
    REQUIRE(project.mkpath(QStringLiteral("workshop")));
    REQUIRE(ChapterDocument::fromKmlContent(kmlWith("Idea"), "Idea")
                .save(project.filePath(QStringLiteral("workshop/note_001.kchapter"))));
    writeFile(project.filePath(QStringLiteral("book/broken.kchapter")), "not a chapter");
    writeFile(project.filePath(QStringLiteral("book/cover.png")), "png");
    REQUIRE(project.mkpath(QStringLiteral("book/old")));
    REQUIRE(ChapterDocument::fromKmlContent(kmlWith("Old"), "Old")
                .save(project.filePath(QStringLiteral("book/old/chapter_009.kchapter"))));
    CHECK(fileTitles(pm.unassignedFiles()) ==
          "book/broken.kchapter: , book/chapter_002.kchapter: A, "
          "book/chapter_003.kchapter: B, workshop/note_001.kchapter: Idea");

    // So the program can tell whether the file of a deleted element will be on the list
    CHECK(pm.listsAsUnassigned(project.filePath(QStringLiteral("book/chapter_001.kchapter"))));
    CHECK(pm.listsAsUnassigned(project.filePath(QStringLiteral("workshop/note_001.kchapter"))));
    CHECK(pm.listsAsUnassigned(project.filePath(QStringLiteral("book/./new.KCHAPTER"))));
    CHECK_FALSE(pm.listsAsUnassigned(project.filePath(QStringLiteral("book/cover.png"))));
    CHECK_FALSE(
        pm.listsAsUnassigned(project.filePath(QStringLiteral("book/old/chapter_009.kchapter"))));
    CHECK_FALSE(pm.listsAsUnassigned(project.filePath(QStringLiteral("other.kchapter"))));

    // A new chapter does not take the name of a file without an element
    const QString c = pm.addElement(chapter, "C", BookPlace::Main);
    REQUIRE_FALSE(c.isEmpty());
    CHECK(pm.findElement(c)->file == "book/chapter_004.kchapter");

    REQUIRE(pm.closeProject(false));
    CHECK(pm.unassignedFiles().isEmpty());
    CHECK_FALSE(
        pm.listsAsUnassigned(project.filePath(QStringLiteral("book/chapter_001.kchapter"))));
}

TEST_CASE("ProjectManager keeps empty text files with the book, outside its text",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Book Files", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QDir project(pm.getProjectPath());
    CHECK(pm.newBookFile() == "book/file_001.kchapter");
    const std::size_t incomplete = pm.getIncompleteElements().size();

    // A new file gets a chapter file of its own in the book's folder, but no kind and no
    // status; a file that no element names is not taken
    writeFile(project.filePath("book/file_001.kchapter"), "");
    const QString notes = pm.addBookFile("Notes");
    REQUIRE_FALSE(notes.isEmpty());
    const ProjectElement* file = pm.findElement(notes);
    REQUIRE(file != nullptr);
    CHECK(file->file == "book/file_002.kchapter");
    CHECK(file->kind.toString().isEmpty());
    CHECK(file->status.isEmpty());
    CHECK(pm.isBookFile(notes));
    CHECK(pm.formOf(*file) == ElementForm::Text);
    CHECK(chapterFileOf(notes).title() == "Notes");
    CHECK(chapterFileOf(notes).kml().isEmpty());

    // It is not an element of the text of the book: it has no place there and no status
    const QList<const ProjectElement*> text = pm.project()->readingOrder();
    CHECK(std::none_of(text.cbegin(), text.cend(),
                       [&notes](const ProjectElement* element) { return element->id == notes; }));
    CHECK(pm.movePlacesOf(notes).isEmpty());
    CHECK_FALSE(pm.setStatus(notes, "final"));
    CHECK(pm.getIncompleteElements().size() == incomplete);

    // Its text is saved and its words are counted
    pm.setChapterContent(notes, kmlWith("Three short words"));
    CHECK(pm.getDirtyElements() == std::vector<QString>{notes});
    REQUIRE(pm.saveAllDirty());
    CHECK(pm.wordCount(notes) == 3);
    CHECK(fileTitles(pm.unassignedFiles()) == "book/file_001.kchapter: ");

    // The .klh file has it at once, with its new title
    REQUIRE(pm.renameElement(notes, "Ideas"));
    CHECK(chapterFileOf(notes).title() == "Ideas");
    CHECK_FALSE(pm.isDirty());
    const BookProject saved = savedProject(manifest);
    REQUIRE(saved.books.first().files.size() == 1);
    CHECK(saved.books.first().files.first().title == "Ideas");
    REQUIRE(pm.closeProject(false));

    REQUIRE(pm.openProject(manifest));
    CHECK(pm.isBookFile(notes));
    CHECK(pm.wordCount(notes) == 3);
    CHECK(pm.loadChapterContent(notes) == kmlWith("Three short words"));

    // Removed, its file stays as a file without an element
    REQUIRE(pm.removeElement(notes).has_value());
    CHECK_FALSE(pm.isBookFile(notes));
    CHECK(fileTitles(pm.unassignedFiles()) ==
          "book/file_001.kchapter: , book/file_002.kchapter: Ideas");
    REQUIRE(pm.closeProject(false));

    // Without a book there is no file
    CHECK(pm.newBookFile().isEmpty());
    CHECK(pm.addBookFile("Nothing").isEmpty());
}

TEST_CASE("ProjectManager adds a chapter file that no element has to the book",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Back", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QDir project(pm.getProjectPath());

    // A chapter with a status and notes, deleted from the book
    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const QString old = pm.addElement(chapter, "The Storm", BookPlace::Main);
    REQUIRE(pm.setStatus(old, "revision"));
    REQUIRE(pm.setNotes(old, "Remember the dog"));
    pm.setChapterContent(old, kmlWith("Rain and wind"));
    REQUIRE(pm.saveChapterContent(old));
    REQUIRE(pm.removeElement(old).has_value());
    const QString file = QStringLiteral("book/chapter_002.kchapter");
    REQUIRE(QFileInfo::exists(project.filePath(file)));

    // Back in the front matter, as the writer names it: the file stays where it is
    const QString id = pm.addUnassignedFile(file, kind("kalahari.novel", "preface"),
                                            "Preface", BookPlace::Front);
    REQUIRE_FALSE(id.isEmpty());
    const ProjectElement* added = pm.findElement(id);
    REQUIRE(added != nullptr);
    CHECK(added->file == file);
    CHECK(added->kind.toString() == kind("kalahari.novel", "preface").reference());
    CHECK(added->status == "revision");
    CHECK(pm.notes(id) == "Remember the dog");
    CHECK(pm.wordCount(id) == 3);
    CHECK(titles(pm.book()->frontElements) == "Title page, Preface");
    CHECK(chapterFileOf(id).title() == "Preface");
    CHECK(chapterFileOf(id).kml() == kmlWith("Rain and wind"));
    CHECK(pm.unassignedFiles().isEmpty());
    CHECK(titles(savedProject(manifest).books.first().frontElements) == "Title page, Preface");

    SECTION("Not a file that an element has, or that is not in the book's folders") {
        const QString used = pm.findElement(id)->file;
        CHECK(pm.addUnassignedFile(used, chapter, "Again", BookPlace::Main).isEmpty());

        writeFile(project.filePath(QStringLiteral("loose.kchapter")), "{}");
        writeFile(QDir(dir.path()).filePath(QStringLiteral("outside.kchapter")), "{}");
        writeFile(project.filePath(QStringLiteral("book/cover.png")), "png");
        for (const QString& other :
             {QStringLiteral("loose.kchapter"), QStringLiteral("../outside.kchapter"),
              project.filePath(QStringLiteral("loose.kchapter")),
              QStringLiteral("book/cover.png"), QStringLiteral("book/missing.kchapter"),
              QString()}) {
            INFO(other.toStdString());
            CHECK(pm.addUnassignedFile(other, chapter, "Other", BookPlace::Main).isEmpty());
        }
        CHECK(titles(pm.book()->mainElements) == "Chapter 1");
    }

    SECTION("Not a file that cannot be read, nor as a kind that cannot be there; the file stays") {
        const QString broken = QStringLiteral("book/broken.kchapter");
        writeFile(project.filePath(broken), "not a chapter");
        CHECK(pm.addUnassignedFile(broken, chapter, "Broken", BookPlace::Main).isEmpty());
        CHECK(QFileInfo::exists(project.filePath(broken)));

        REQUIRE(pm.removeElement(id).has_value());
        CHECK(pm.addUnassignedFile(file, pm.partKind(), "Part", BookPlace::Main).isEmpty());
        CHECK(pm.addUnassignedFile(file, kind("kalahari.novel", "preface"), "Preface",
                                   BookPlace::Main)
                  .isEmpty());
        CHECK(QFileInfo::exists(project.filePath(file)));
        CHECK(pm.unassignedFiles().size() == 2);
    }

    SECTION("In the place the writer chose, or in the place of its kind") {
        REQUIRE(pm.removeElement(id).has_value());
        const QString epilogue = pm.addElement(kind("kalahari.novel", "epilogue"), "Epilogue",
                                               BookPlace::Main);
        REQUIRE_FALSE(epilogue.isEmpty());
        REQUIRE_FALSE(pm.addUnassignedFile(file, chapter, "The Storm", BookPlace::Main).isEmpty());
        CHECK(titles(pm.book()->mainElements) == "Chapter 1, The Storm, Epilogue");

        const QString part = pm.addElement(pm.partKind(), "Part", BookPlace::Main);
        REQUIRE(pm.removeElement(pm.book()->mainElements.at(1).id).has_value());
        REQUIRE_FALSE(
            pm.addUnassignedFile(file, chapter, "The Storm", BookPlace::Main, part, 0).isEmpty());
        CHECK(titles(pm.findElement(part)->elements) == "The Storm");
    }

    REQUIRE(pm.closeProject(false));
}

// =============================================================================
// Text, status and notes of chapters
// =============================================================================

TEST_CASE("ProjectManager saves and opens again the text of a chapter", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "New Chapter", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();

    const QString id = pm.addElement(pm.chapterKindFor(BookPlace::Main), "Chapter", BookPlace::Main);
    REQUIRE_FALSE(id.isEmpty());
    CHECK(pm.getDirtyElements().empty());

    pm.setChapterContent(id, kmlWith("The new text"));
    CHECK(pm.getDirtyElements() == std::vector<QString>{id});
    REQUIRE(pm.saveAllDirty());
    CHECK(pm.getDirtyElements().empty());
    CHECK(pm.wordCount(id) == 3);

    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(manifest));
    CHECK(pm.wordCount(id) == 3);
    CHECK(pm.loadChapterContent(id) == kmlWith("The new text"));

    // Text that is thrown away is read from the file again
    pm.setChapterContent(id, kmlWith("Thrown away"));
    pm.discardChapterContent(id);
    CHECK(pm.getDirtyElements().empty());
    CHECK(pm.loadChapterContent(id) == kmlWith("The new text"));
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager keeps the status in the .klh file and in the chapter file",
          "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Statuses", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QString id = pm.addElement(pm.chapterKindFor(BookPlace::Main), "Chapter", BookPlace::Main);
    const QString part = pm.addElement(pm.partKind(), "Part", BookPlace::Main);
    REQUIRE_FALSE(id.isEmpty());

    // What else the chapter file has stays in it
    ChapterDocument file = chapterFileOf(id);
    file.setComments(QJsonArray{QJsonObject{{"id", "c1"}}});
    REQUIRE(file.save(pm.filePathOf(*pm.findElement(id))));

    REQUIRE(pm.setStatus(id, "revision"));
    CHECK(pm.findElement(id)->status == "revision");
    CHECK(chapterFileOf(id).status() == "revision");
    CHECK(chapterFileOf(id).comments().size() == 1);

    CHECK_FALSE(pm.setStatus(id, "done"));
    CHECK_FALSE(pm.setStatus(part, "final"));  // groups have no status
    CHECK(ProjectManager::statusOf(*pm.findElement(part)) == "draft");
    CHECK(ProjectManager::isTextStatus("draft"));
    CHECK(ProjectManager::isTextStatus("revision"));
    CHECK(ProjectManager::isTextStatus("final"));
    CHECK_FALSE(ProjectManager::isTextStatus("done"));
    CHECK_FALSE(ProjectManager::isTextStatus(""));
    REQUIRE(pm.closeProject(false));

    // The .klh file decides
    REQUIRE(pm.openProject(manifest));
    CHECK(pm.findElement(id)->status == "revision");
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager keeps the notes of a chapter in its chapter file", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Notes", "Author", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QString id = pm.addElement(pm.chapterKindFor(BookPlace::Main), "Chapter", BookPlace::Main);
    REQUIRE_FALSE(id.isEmpty());

    REQUIRE(pm.setNotes(id, "Check the dates"));
    CHECK(chapterFileOf(id).notes() == "Check the dates");

    // The text saved later keeps the notes
    pm.setChapterContent(id, kmlWith("Text"));
    REQUIRE(pm.saveChapterContent(id));
    CHECK(chapterFileOf(id).notes() == "Check the dates");
    REQUIRE(pm.closeProject(false));

    REQUIRE(pm.openProject(manifest));
    CHECK(pm.notes(id) == "Check the dates");
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager writes the title, the status and the notes of a chapter to its file",
          "[project_manager]") {
    // A book made before Kalahari wrote them to the files has others there: "Chapter 2" for
    // "Rozdział 2"
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Details", "Author", "en", true, "kalahari.novel"));
    const QString id = pm.addElement(pm.chapterKindFor(BookPlace::Main), "The Journey",
                                     BookPlace::Main);
    REQUIRE_FALSE(id.isEmpty());
    REQUIRE(pm.setStatus(id, "revision"));
    REQUIRE(pm.setNotes(id, "Check the dates"));
    const QString path = pm.filePathOf(*pm.findElement(id));

    ChapterDocument earlier = chapterFileOf(id);
    earlier.setKml(kmlWith("Text"));
    earlier.setTitle("Chapter 2");
    earlier.setStatus("draft");
    earlier.setNotes("");
    REQUIRE(earlier.save(path));

    SECTION("A file with others gets them, and keeps its text") {
        REQUIRE(pm.saveChapterDetails(id));
        const ChapterDocument file = chapterFileOf(id);
        CHECK(file.title() == "The Journey");
        CHECK(file.status() == "revision");
        CHECK(file.notes() == "Check the dates");
        CHECK(file.kml() == earlier.kml());
    }

    SECTION("A missing file is not made") {
        REQUIRE(QFile::remove(path));
        CHECK(pm.saveChapterDetails(id));
        CHECK_FALSE(QFileInfo::exists(path));
    }

    SECTION("A part has no file") {
        const QString part = pm.addElement(pm.partKind(), "Part", BookPlace::Main);
        REQUIRE_FALSE(part.isEmpty());
        CHECK(pm.saveChapterDetails(part));
    }
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager counts the words and statuses of the book", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Counts", "Author", "en", true, "kalahari.novel"));

    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const QString a = pm.addElement(chapter, "A", BookPlace::Main);
    const QString part = pm.addElement(pm.partKind(), "Part", BookPlace::Main);
    const QString b = pm.addElement(chapter, "B", BookPlace::Main, part);
    REQUIRE_FALSE(b.isEmpty());
    pm.setChapterContent(a, kmlWith("one two"));
    pm.setChapterContent(b, kmlWith("three"));
    REQUIRE(pm.saveAllDirty());
    REQUIRE(pm.setStatus(b, "final"));

    // The book starts with a title page ("Counts", "Author") and an empty chapter, both drafts
    const QString titlePage = pm.book()->frontElements.first().id;
    const QString chapter1 = pm.book()->mainElements.first().id;
    const TextStatistics statistics = pm.statisticsOf(pm.book()->mainElements);
    CHECK(statistics.elements == 3);
    CHECK(statistics.words == 3);
    CHECK(statistics.statuses == std::map<QString, int>{{"draft", 2}, {"final", 1}});
    CHECK(statistics.kinds == std::map<QString, int>{{"kalahari.base:chapter", 3}});
    CHECK(pm.statisticsOf(pm.book()->frontElements).words == 2);

    CHECK(pm.getStatusStatistics() == std::map<QString, int>{{"draft", 3}, {"final", 1}});
    const auto incomplete = pm.getIncompleteElements();
    REQUIRE(incomplete.size() == 3);
    CHECK(incomplete.at(0) == std::pair<QString, QString>{titlePage, "draft"});
    CHECK(incomplete.at(1) == std::pair<QString, QString>{chapter1, "draft"});
    CHECK(incomplete.at(2) == std::pair<QString, QString>{a, "draft"});
    REQUIRE(pm.closeProject(false));
}

namespace {

/// The dialogue dashes count as words or not (Settings > Editor > General > Word Count) until
/// the test ends; then the choice is as it was
class DashRule {
public:
    explicit DashRule(bool dashesAreWords)
        : m_saved(SettingsManager::getInstance().get<bool>(KEY)) {
        set(dashesAreWords);
    }
    ~DashRule() {
        try {
            set(m_saved);
        } catch (...) {
            std::fputs("The rule of the dialogue dashes could not be given back\n", stderr);
        }
    }
    DashRule(const DashRule&) = delete;
    DashRule& operator=(const DashRule&) = delete;

    /// Count the dialogue dashes as words, or not
    static void set(bool dashesAreWords) {
        SettingsManager::getInstance().set<bool>(KEY, dashesAreWords);
    }

private:
    static constexpr const char* KEY = "editor.wordCount.dashesAsWords";
    bool m_saved;
};

}  // namespace

TEST_CASE("The words of the book follow the rule of the dialogue dashes at once",
          "[project_manager]") {
    // Regression: the counts of the book were those its chapter files were saved with, so
    // the dialogue dashes counted as words only in the chapter in front
    QTemporaryDir dir;
    auto& pm = projects();
    DashRule rule(false);
    REQUIRE(pm.createProject(dir.path(), "Dashes", "Author", "en", true, "kalahari.novel"));

    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const QString saved = pm.addElement(chapter, "Saved", BookPlace::Main);
    const QString edited = pm.addElement(chapter, "Edited", BookPlace::Main);
    REQUIRE_FALSE(edited.isEmpty());
    pm.setChapterContent(saved, kmlWith(QStringLiteral("\u2013 Tak \u2013 powiedzia\u0142.")));
    REQUIRE(pm.saveAllDirty());
    REQUIRE(chapterFileOf(saved).wordCount() == 2);  // saved without the dashes
    // A text in its editor, with changes that are not saved
    pm.setEditorWords(edited, tallyWords(QStringLiteral("\u2014 Nie \u2014 odpar\u0142a.")));

    CHECK(pm.wordCount(saved) == 2);
    CHECK(pm.wordCount(edited) == 2);
    CHECK(pm.statisticsOf(pm.book()->mainElements).words == 4);

    SECTION("The dashes as words: the saved text and the text in the editor") {
        DashRule::set(true);
        CHECK(pm.wordCount(saved) == 4);
        CHECK(pm.wordCount(edited) == 4);
        CHECK(pm.statisticsOf(pm.book()->mainElements).words == 8);

        DashRule::set(false);
        CHECK(pm.statisticsOf(pm.book()->mainElements).words == 4);
    }

    SECTION("The book opened again: its file saved without the dashes counts them") {
        const QString manifest = pm.getManifestPath();
        REQUIRE(pm.closeProject(false));
        DashRule::set(true);
        REQUIRE(pm.openProject(manifest));
        CHECK(pm.wordCount(saved) == 4);
        CHECK(pm.statisticsOf(pm.book()->mainElements).words == 4);  // "Edited" has no text

        // ... and so do the details of the chapter written to its file
        REQUIRE(pm.setStatus(saved, "revision"));
        CHECK(pm.wordCount(saved) == 4);
        DashRule::set(false);
        CHECK(pm.wordCount(saved) == 2);
    }

    REQUIRE(pm.closeProject(false));
}

TEST_CASE("The main text kind of a book names its texts", "[project_manager]") {
    auto& pm = projects();
    CHECK_FALSE(pm.mainTextKind());

    const std::vector<std::pair<QString, QString>> types{
        {"kalahari.novel", "kalahari.base:chapter"},
        {"kalahari.short_stories", "kalahari.short_stories:story"},
        {"kalahari.poetry", "kalahari.poetry:poem"},
        {"kalahari.screenplay", "kalahari.screenplay:act"},
        {"kalahari.nonfiction", "kalahari.base:chapter"},
        // A user project: the first text kind of its main part
        {QString(), "kalahari.base:chapter"},
    };
    for (const auto& [type, kind] : types) {
        INFO(type.toStdString());
        QTemporaryDir dir;
        REQUIRE(pm.createProject(dir.path(), "Book", "Author", "en", true, type));
        CHECK(pm.mainTextKind().reference() == kind);
        REQUIRE(pm.closeProject(false));
    }
    CHECK_FALSE(pm.mainTextKind());
}

TEST_CASE("A new book is added to the recent books", "[project_manager]") {
    // Regression: only opening a book added it to the recent books, so the Dashboard
    // did not show a book just created
    QTemporaryDir dir;
    auto& recent = RecentBooksManager::getInstance();
    auto& pm = projects();
    int changes = 0;
    auto connection = QObject::connect(&recent, &RecentBooksManager::recentFilesChanged,
                                       [&changes]() { ++changes; });

    REQUIRE(pm.createProject(dir.path(), "Recent Test", "Author", "en", true, "kalahari.novel"));
    const QString manifest = QFileInfo(pm.getManifestPath()).absoluteFilePath();
    QObject::disconnect(connection);

    CHECK(changes >= 1);
    REQUIRE_FALSE(recent.getRecentFiles().isEmpty());
    CHECK(recent.getRecentFiles().first() == manifest);

    REQUIRE(pm.closeProject(false));
    recent.removeRecentFile(manifest);
}

TEST_CASE("The book opened at startup is the book opened last, not one added without opening",
          "[project_manager]") {
    // Regression: a book created or imported without opening goes to the top of the recent
    // books, and the program opened it at startup in place of the book the writer worked on
    QTemporaryDir dir;
    auto& recent = RecentBooksManager::getInstance();
    auto& pm = projects();
    const QStringList saved = recent.getRecentFiles();

    REQUIRE(pm.createProject(dir.path(), "Opened", "Author", "en", true, "kalahari.novel"));
    const QString opened = QFileInfo(pm.getManifestPath()).absoluteFilePath();
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.createProject(dir.path(), "Added", "Author", "en", true, "kalahari.novel"));
    const QString added = QFileInfo(pm.getManifestPath()).absoluteFilePath();
    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(opened));
    CHECK(recent.lastOpenedBook() == opened);

    // The other book goes to the top of the list, as a book created without opening does
    recent.addRecentFile(added);
    CHECK(recent.getRecentFiles().first() == added);
    CHECK(recent.lastOpenedBook() == opened);
    REQUIRE(pm.closeProject(false));
    CHECK(recent.lastOpenedBook() == opened);

    // Opened, it is the book of the startup
    REQUIRE(pm.openProject(added));
    CHECK(recent.lastOpenedBook() == added);
    REQUIRE(pm.closeProject(false));

    // Gone from the list, the first of the list is opened
    recent.removeRecentFile(added);
    REQUIRE(recent.getRecentFiles().value(0) == opened);
    CHECK(recent.lastOpenedBook() == opened);

    recent.removeRecentFile(opened);
    for (auto it = saved.crbegin(); it != saved.crend(); ++it) {
        recent.addRecentFile(*it);
    }
}

// =============================================================================
// Archives
// =============================================================================

TEST_CASE("ProjectManager exports the open book to an archive and imports it",
          "[project_manager][archive]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Travels", "Anna", "en", true, "kalahari.novel"));
    const QString chapter = pm.book()->mainElements.first().id;
    pm.setChapterContent(chapter, kmlWith("Far away"));
    REQUIRE(pm.saveChapterContent(chapter));

    // A style in the database, and a backup of the database, which serves this computer only
    ParagraphStyle style;
    style.id = "travel_note";
    style.name = "Travel note";
    REQUIRE(pm.getDatabase() != nullptr);
    pm.getDatabase()->saveParagraphStyle(style);
    const QDir project(pm.getProjectPath());
    REQUIRE(project.mkpath(".backups"));
    writeFile(project.filePath(".backups/project_backup.db"), "old");

    // The archive has the files of the book with the cards of its packages, without its lock,
    // the log files of its database and the backups; the book stays open
    const QString archive = project.filePath("Travels.klh.zip");
    ArchiveProblem problem;
    QList<int> progress;
    REQUIRE(pm.exportArchive(archive, [&progress](int percent) { progress << percent; },
                             &problem));
    CHECK(problem.kind == ArchiveProblem::Kind::None);
    CHECK(problem.lines.isEmpty());
    REQUIRE_FALSE(progress.isEmpty());
    CHECK(std::is_sorted(progress.cbegin(), progress.cend()));
    CHECK(progress.last() == 100);
    CHECK(pm.isProjectOpen());
    QStringList entries{"Travels.klh", "book/chapter_001.kchapter",
                        "book/title_page_001.kchapter", "project.db"};
    QDirIterator cards(project.filePath("types"), QDir::Files, QDirIterator::Subdirectories);
    while (cards.hasNext()) {
        entries << project.relativeFilePath(cards.next());
    }
    entries.sort();
    CHECK(entries.contains("types/kalahari.base/card.json"));
    CHECK(entries.contains("types/kalahari.novel/card.json"));
    CHECK(entriesOf(archive) == entries);

    // An archive written to the book's folder is not in the next one
    REQUIRE(pm.exportArchive(archive));
    CHECK(entriesOf(archive) == entries);

    // The imported book opens in a folder of its own, with its text and its styles
    const QString target = QDir(dir.path()).filePath("imported");
    REQUIRE(pm.importArchive(archive, target, nullptr, &problem));
    CHECK(problem.lines.isEmpty());
    const QDir imported(QDir(target).filePath("Travels"));
    CHECK(QFileInfo(pm.getProjectPath()) == QFileInfo(imported.path()));
    CHECK(pm.book()->title == "Travels");
    CHECK(titles(pm.book()->mainElements) == "Chapter 1");
    CHECK(pm.loadChapterContent(chapter) == kmlWith("Far away"));
    CHECK(hasParagraphStyle("travel_note"));
    CHECK_FALSE(QFileInfo::exists(imported.filePath(".backups/project_backup.db")));
    REQUIRE(pm.closeProject(false));

    // A second import does not touch the folder of the first
    writeFile(imported.filePath("mine.txt"), "mine");
    CHECK_FALSE(pm.importArchive(archive, target, nullptr, &problem));
    CHECK(problem.kind == ArchiveProblem::Kind::FolderExists);
    REQUIRE(problem.lines.size() == 1);
    CHECK(problem.lines.first().endsWith(": the folder already exists"));
    CHECK(QFileInfo::exists(imported.filePath("mine.txt")));
    CHECK_FALSE(pm.isProjectOpen());
}

TEST_CASE("ProjectManager imports an archive of an earlier version", "[project_manager][archive]") {
    // Regression: earlier versions archived the open book as it was, with its lock, which kept
    // the imported book from opening, the log files of its database, and names with "\" on
    // Windows
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Old Archive", "Anna", "en", true, "kalahari.novel"));
    const QString chapter = pm.book()->mainElements.first().id;
    pm.setChapterContent(chapter, kmlWith("Kept text"));
    REQUIRE(pm.saveChapterContent(chapter));
    ParagraphStyle style;
    style.id = "old_note";
    style.name = "Old note";
    REQUIRE(pm.getDatabase() != nullptr);
    pm.getDatabase()->saveParagraphStyle(style);

    const QList<Entry> entries = entriesOfFolder(pm.getProjectPath(), QLatin1Char('\\'));
    REQUIRE(pm.closeProject(false));
    QStringList names;
    for (const Entry& entry : entries) {
        names << QString::fromUtf8(entry.first);
    }
    REQUIRE(names.contains(".kalahari.lock"));
    REQUIRE(names.contains("project.db-wal"));
    REQUIRE(names.contains("book\\chapter_001.kchapter"));
    const QString archive = QDir(dir.path()).filePath("Old Archive.klh.zip");
    writeArchive(archive, entries);

    ArchiveProblem problem;
    REQUIRE(pm.importArchive(archive, QDir(dir.path()).filePath("imported"), nullptr, &problem));
    CHECK(problem.lines.isEmpty());
    const QDir imported(QDir(dir.path()).filePath("imported/Old Archive"));
    CHECK(QFileInfo(pm.getProjectPath()) == QFileInfo(imported.path()));
    CHECK(QFileInfo(imported.filePath("book/chapter_001.kchapter")).isFile());
    CHECK(pm.loadChapterContent(chapter) == kmlWith("Kept text"));
    CHECK(hasParagraphStyle("old_note"));  // which the log of the database had
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager imports only what goes into the book's folder",
          "[project_manager][archive]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Safe", "Anna", "en", true));
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));
    QFile file(manifest);
    REQUIRE(file.open(QIODevice::ReadOnly));

    const QString archive = QDir(dir.path()).filePath("Safe copy.klh.zip");
    writeArchive(archive, {{"Safe.klh", file.readAll()},
                           {"../outside.txt", "x"},
                           {"notes/../../outside2.txt", "x"},
                           {"/absolute.txt", "x"},
                           {"C:/drive.txt", "x"},
                           {"C:drive2.txt", "x"},
                           {"./notes/./kept.txt", "kept"}});
    const QString target = QDir(dir.path()).filePath("imported");
    REQUIRE(pm.importArchive(archive, target));
    const QDir imported(QDir(target).filePath("Safe copy"));
    CHECK(QFileInfo(imported.filePath("notes/kept.txt")).isFile());
    CHECK(QDir(target).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden) ==
          QStringList{"Safe copy"});
    CHECK_FALSE(QFileInfo::exists(QDir(dir.path()).filePath("outside.txt")));
    CHECK_FALSE(QFileInfo::exists(imported.filePath("absolute.txt")));
    CHECK_FALSE(QFileInfo::exists(imported.filePath("drive.txt")));
    // Nothing named after a drive is in the book's folder, looked for in its list: on Windows
    // QDir::filePath("C:") is "C:", the current folder of drive C, and "C:drive2.txt" is in it
    const QStringList names =
        imported.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    CHECK_FALSE(names.contains("C:"));
    CHECK_FALSE(names.contains("C:drive2.txt"));
    CHECK_FALSE(QFileInfo::exists(QStringLiteral("C:drive2.txt")));
    REQUIRE(pm.closeProject(false));

    SECTION("An archive without a book") {
        const QString notes = QDir(dir.path()).filePath("Notes.klh.zip");
        writeArchive(notes, {{"notes.txt", "x"}});
        ArchiveProblem problem;
        CHECK_FALSE(pm.importArchive(notes, target, nullptr, &problem));
        CHECK(problem.kind == ArchiveProblem::Kind::NoBook);
        CHECK(problem.lines == QStringList{"Notes.klh.zip: the archive has no .klh file"});
        CHECK_FALSE(QFileInfo::exists(QDir(target).filePath("Notes")));
    }
}

TEST_CASE("ProjectManager reads the book of an archive without unpacking it",
          "[project_manager][archive]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Travels", "Anna", "en", true, "kalahari.novel"));
    const QString archive = QDir(dir.path()).filePath("Travels.klh.zip");
    REQUIRE(pm.exportArchive(archive));
    REQUIRE(pm.closeProject(false));
    const QStringList before =
        QDir(dir.path()).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    ArchiveProblem problem;
    const std::optional<BookProject> project = ProjectManager::readArchive(archive, &problem);
    REQUIRE(project.has_value());
    CHECK(problem.kind == ArchiveProblem::Kind::None);
    CHECK(problem.lines.isEmpty());
    REQUIRE(project->books.size() == 1);
    CHECK(project->books.first().title == "Travels");
    CHECK(project->books.first().author == "Anna");
    REQUIRE(project->type.has_value());
    CHECK(project->type->id == "kalahari.novel");
    CHECK(titles(project->books.first().mainElements) == "Chapter 1");
    CHECK(QDir(dir.path()).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot) == before);

    SECTION("An archive without a book") {
        const QString notes = QDir(dir.path()).filePath("Notes.klh.zip");
        writeArchive(notes, {{"notes.txt", "x"}, {"book/Inside.klh", "{}"}});
        CHECK_FALSE(ProjectManager::readArchive(notes, &problem).has_value());
        CHECK(problem.kind == ArchiveProblem::Kind::NoBook);
        CHECK(problem.lines == QStringList{"Notes.klh.zip: the archive has no .klh file"});
    }

    SECTION("A book whose file is not JSON") {
        const QString broken = QDir(dir.path()).filePath("Broken.klh.zip");
        writeArchive(broken, {{"Broken.klh", "{ not JSON"}});
        CHECK_FALSE(ProjectManager::readArchive(broken, &problem).has_value());
        CHECK(problem.kind == ArchiveProblem::Kind::DamagedBook);
        REQUIRE(problem.lines.size() == 1);
        CHECK(problem.lines.first().startsWith("Broken.klh: not valid JSON"));
    }

    SECTION("A book of a newer version of Kalahari") {
        const QString future = QDir(dir.path()).filePath("Future.klh.zip");
        writeArchive(future, {{"Future.klh", QByteArray("{ \"format\": ") +
                                                 QByteArray::number(BookProject::FORMAT + 1) +
                                                 QByteArray(", \"id\": \"p\" }")}});
        CHECK_FALSE(ProjectManager::readArchive(future, &problem).has_value());
        CHECK(problem.kind == ArchiveProblem::Kind::NewerBook);
    }

    SECTION("A file that is not an archive") {
        const QString text = QDir(dir.path()).filePath("Text.klh.zip");
        writeFile(text, "plain text");
        CHECK_FALSE(ProjectManager::readArchive(text, &problem).has_value());
        CHECK(problem.kind == ArchiveProblem::Kind::NotArchive);
        REQUIRE(problem.lines.size() == 1);
        CHECK(problem.lines.first().startsWith("Text.klh.zip: "));
    }

    SECTION("An archive that is not there") {
        const QString gone = QDir(dir.path()).filePath("Gone.klh.zip");
        CHECK_FALSE(ProjectManager::readArchive(gone, &problem).has_value());
        CHECK(problem.kind == ArchiveProblem::Kind::NotFound);
        REQUIRE(problem.lines.size() == 1);
        CHECK(problem.lines.first().startsWith("Gone.klh.zip: "));
    }

    SECTION("An archive whose data are damaged") {
        const QString damaged = QDir(dir.path()).filePath("Damaged.klh.zip");
        writeArchive(damaged, {{"Damaged.klh", compressibleBook()}});
        changeFirstEntry(damaged, [](QByteArray& data, qsizetype start, qsizetype) {
            // The middle of the packed data of the book's file
            for (qsizetype i = start + 4; i < start + 16; ++i) {
                data[i] = static_cast<char>(~data[i]);
            }
        });
        CHECK_FALSE(ProjectManager::readArchive(damaged, &problem).has_value());
        CHECK(problem.kind == ArchiveProblem::Kind::Damaged);
        REQUIRE(problem.lines.size() == 1);
        CHECK(problem.lines.first().startsWith("Damaged.klh: "));
    }

    SECTION("An encrypted archive") {
        const QString encrypted = QDir(dir.path()).filePath("Encrypted.klh.zip");
        writeArchive(encrypted, {{"Encrypted.klh", compressibleBook()}});
        changeFirstEntry(encrypted, [](QByteArray& data, qsizetype, qsizetype central) {
            // The flag of an encrypted entry, in the entry and in the list of entries
            data[6] = static_cast<char>(data[6] | 1);
            data[central + 8] = static_cast<char>(data[central + 8] | 1);
        });
        CHECK_FALSE(ProjectManager::readArchive(encrypted, &problem).has_value());
        CHECK(problem.kind == ArchiveProblem::Kind::Unsupported);
    }

    SECTION("The book that unpackArchive() unpacks: the first .klh file at the top") {
        const QString two = QDir(dir.path()).filePath("Two.klh.zip");
        QFile file(QDir(dir.path()).filePath("Travels/Travels.klh"));
        REQUIRE(file.open(QIODevice::ReadOnly));
        writeArchive(two, {{"b.klh", "{ not JSON"}, {"A.KLH", file.readAll()}});
        const std::optional<BookProject> first = ProjectManager::readArchive(two, &problem);
        REQUIRE(first.has_value());
        CHECK(first->books.first().title == "Travels");
    }
}

TEST_CASE("ProjectManager unpacks an archive without opening its book",
          "[project_manager][archive]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Travels", "Anna", "en", true, "kalahari.novel"));
    const QString chapter = pm.book()->mainElements.first().id;
    pm.setChapterContent(chapter, kmlWith("Far away"));
    REQUIRE(pm.saveChapterContent(chapter));
    const QString archive = QDir(dir.path()).filePath("Travels copy.klh.zip");
    REQUIRE(pm.exportArchive(archive));
    const QString open = pm.getManifestPath();

    // The open book stays open; the unpacked one is in a folder named after the archive
    const QString target = QDir(dir.path()).filePath("imported");
    ArchiveProblem problem;
    int progress = 0;
    const QString manifest = ProjectManager::unpackArchive(
        archive, target, [&progress](int percent) { progress = percent; }, &problem);
    CHECK(problem.kind == ArchiveProblem::Kind::None);
    CHECK(problem.lines.isEmpty());
    CHECK(progress == 100);
    CHECK(QFileInfo(manifest) == QFileInfo(QDir(target).filePath("Travels copy/Travels.klh")));
    CHECK(pm.getManifestPath() == open);
    REQUIRE(pm.closeProject(false));

    // It opens later as any book does
    REQUIRE(pm.openProject(manifest));
    CHECK(pm.loadChapterContent(chapter) == kmlWith("Far away"));
    REQUIRE(pm.closeProject(false));

    SECTION("A book whose file cannot be read does not stay behind") {
        const QString broken = QDir(dir.path()).filePath("Broken.klh.zip");
        writeArchive(broken, {{"Broken.klh", "{ not JSON"}, {"book/chapter_001.kchapter", "x"}});
        CHECK(ProjectManager::unpackArchive(broken, target, nullptr, &problem).isEmpty());
        CHECK(problem.kind == ArchiveProblem::Kind::DamagedBook);
        REQUIRE(problem.lines.size() == 1);
        CHECK(problem.lines.first().startsWith("Broken.klh: not valid JSON"));
        CHECK_FALSE(QFileInfo::exists(QDir(target).filePath("Broken")));

        // Nor when it is imported
        problem = ArchiveProblem();
        CHECK_FALSE(pm.importArchive(broken, target, nullptr, &problem));
        CHECK(problem.kind == ArchiveProblem::Kind::DamagedBook);
        CHECK_FALSE(problem.lines.isEmpty());
        CHECK_FALSE(QFileInfo::exists(QDir(target).filePath("Broken")));
        CHECK_FALSE(pm.isProjectOpen());
    }

    SECTION("A folder in which the book cannot be made") {
        // A file in the place of a folder of the path
        const QString file = QDir(dir.path()).filePath("file.txt");
        writeFile(file, "x");
        CHECK(ProjectManager::unpackArchive(archive, QDir(file).filePath("Books"), nullptr,
                                            &problem)
                  .isEmpty());
        CHECK(problem.kind == ArchiveProblem::Kind::CannotWrite);
        REQUIRE(problem.lines.size() == 1);
        CHECK(problem.lines.first().endsWith(": the folder cannot be made"));
    }

    SECTION("A book that cannot be opened does not stay behind") {
        // A folder in the place of the database of the book
        const QString strange = QDir(dir.path()).filePath("Strange.klh.zip");
        QFile file(QDir(dir.path()).filePath("Travels/Travels.klh"));
        REQUIRE(file.open(QIODevice::ReadOnly));
        writeArchive(strange, {{"Strange.klh", file.readAll()}, {"project.db/notes.txt", "x"}});
        CHECK_FALSE(pm.importArchive(strange, target, nullptr, &problem));
        CHECK(problem.kind == ArchiveProblem::Kind::NotOpened);
        CHECK_FALSE(problem.lines.isEmpty());
        CHECK_FALSE(QFileInfo::exists(QDir(target).filePath("Strange")));
        CHECK_FALSE(pm.isProjectOpen());
    }
}

TEST_CASE("ProjectManager says why an archive cannot be written", "[project_manager][archive]") {
    QTemporaryDir dir;
    auto& pm = projects();
    REQUIRE(pm.createProject(dir.path(), "Travels", "Anna", "en", true, "kalahari.novel"));

    // A folder that is not there: libzip cannot make the file of the archive in it
    const QString archive = QDir(dir.path()).filePath("Missing/Travels.klh.zip");
    ArchiveProblem problem;
    CHECK_FALSE(pm.exportArchive(archive, nullptr, &problem));
    CHECK(problem.kind == ArchiveProblem::Kind::CannotWrite);
    REQUIRE(problem.lines.size() == 1);
    CHECK(problem.lines.first().startsWith("Travels.klh.zip: "));
    CHECK_FALSE(QFileInfo::exists(archive));
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("The backups of the database go to an archive when the writer wants them",
          "[project_manager][archive]") {
    QTemporaryDir dir;
    const QDir root(dir.path());
    const BackupFolder backupFolder(root.filePath("Backups"));
    auto& pm = projects();
    REQUIRE(pm.createProject(root.filePath("Books"), "Travels", "Anna", "en", true,
                             "kalahari.novel"));
    const QDir project(pm.getProjectPath());

    // Two backups where the settings keep them, and two made before, in the book's own folder:
    // a name in both folders counts once
    BackupManager kept(project.path());
    kept.setCommonFolder(root.filePath("Backups"), pm.project()->id);
    const QString made = kept.createBackup();  // with the file that names the book
    REQUIRE_FALSE(made.isEmpty());
    REQUIRE(QFile::remove(made));
    const QDir common(kept.backupDir());
    writeBackup(common.filePath("project_20000103_000000.db"), 3);
    writeBackup(common.filePath("project_20000102_000000.db"), 2);
    REQUIRE(project.mkpath(".backups"));
    writeBackup(project.filePath(".backups/project_20000102_000000.db"), 2);
    writeBackup(project.filePath(".backups/project_20000101_000000.db"), 1);
    const QStringList backups = pm.databaseBackups();
    REQUIRE(backups.size() == 3);
    CHECK(QFileInfo(backups.at(0)) == QFileInfo(common.filePath("project_20000103_000000.db")));
    CHECK(QFileInfo(backups.at(1)) == QFileInfo(common.filePath("project_20000102_000000.db")));
    CHECK(QFileInfo(backups.at(2)) ==
          QFileInfo(project.filePath(".backups/project_20000101_000000.db")));

    // Without them the archive is as before; with them, they are in its .backups folder
    const QString archive = root.filePath("Travels copy.klh.zip");
    ArchiveProblem problem;
    REQUIRE(pm.exportArchive(archive, nullptr, &problem));
    CHECK(entriesOf(archive).filter(".backups").isEmpty());
    int count = -1;
    REQUIRE(ProjectManager::readArchive(archive, &problem, &count).has_value());
    CHECK(count == 0);
    REQUIRE(pm.exportArchive(archive, nullptr, &problem, true));
    CHECK(problem.lines.isEmpty());
    const QStringList archived{".backups/project_20000101_000000.db",
                               ".backups/project_20000102_000000.db",
                               ".backups/project_20000103_000000.db"};
    CHECK(entriesOf(archive).filter(".backups") == archived);
    REQUIRE(ProjectManager::readArchive(archive, &problem, &count).has_value());
    CHECK(count == 3);
    REQUIRE(pm.closeProject(false));

    // The import takes them, with their times, where the settings keep the backups, to a
    // folder of the imported book
    const QString manifest =
        ProjectManager::unpackArchive(archive, root.filePath("Imported"), nullptr, &problem);
    REQUIRE_FALSE(manifest.isEmpty());
    CHECK(problem.lines.isEmpty());
    const QDir imported(QFileInfo(manifest).absolutePath());
    CHECK_FALSE(QFileInfo::exists(imported.filePath(".backups")));
    REQUIRE(pm.openProject(manifest));
    const QStringList taken = pm.databaseBackups();
    REQUIRE(pm.closeProject(false));
    REQUIRE(taken.size() == 3);
    CHECK(QFileInfo(taken.first()).absoluteDir() == QDir(root.filePath("Backups/Travels copy")));
    for (int i = 0; i < 3; ++i) {
        CHECK(QFileInfo(taken.at(i)).fileName() == QFileInfo(archived.at(2 - i)).fileName());
        CHECK(madeOnDay(taken.at(i), 3 - i));
    }

    // Without a common folder they stay in the book's own .backups folder
    BackupFolder::set(QString());
    const QString second =
        ProjectManager::unpackArchive(archive, root.filePath("Imported 2"), nullptr, &problem);
    REQUIRE_FALSE(second.isEmpty());
    const QDir own(QDir(QFileInfo(second).absolutePath()).filePath(".backups"));
    CHECK(own.entryList(QDir::Files, QDir::Name) ==
          QStringList{"project_20000101_000000.db", "project_20000102_000000.db",
                      "project_20000103_000000.db"});
    CHECK(madeOnDay(own.filePath("project_20000101_000000.db"), 1));
}

TEST_CASE("An archive is named after its book", "[project_manager][archive]") {
    CHECK(ProjectManager::archiveFileName("Test powieści") == "Test powieści.klh.zip");
    CHECK(ProjectManager::archiveFileName(" Who? Me: \"Anna\" ") == "Who_ Me_ _Anna_.klh.zip");
    CHECK(ProjectManager::archiveFileName("") == "project.klh.zip");
    CHECK(ProjectManager::archiveProjectName(ProjectManager::archiveFileName("Travels")) ==
          "Travels");
}

TEST_CASE("An imported book takes the name of its archive", "[project_manager][archive]") {
    CHECK(ProjectManager::archiveProjectName("/books/My Novel.klh.zip") == "My Novel");
    CHECK(ProjectManager::archiveProjectName("C:/books/My Novel.KLH.ZIP") == "My Novel");
    CHECK(ProjectManager::archiveProjectName("/books/v1.2 draft.zip") == "v1.2 draft");
    CHECK(ProjectManager::archiveProjectName("/books/.klh.zip") == "project");
}
