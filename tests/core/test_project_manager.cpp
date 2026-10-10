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

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <zip.h>

#include <algorithm>
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

TEST_CASE("ProjectManager opens elements whose package is not installed", "[project_manager]") {
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

    // Its text opens, is counted and keeps its status
    CHECK(pm.loadChapterContent(chapter) == kmlWith("Dark night"));
    CHECK(pm.wordCount(chapter) == 2);
    CHECK(pm.getStatusStatistics() == std::map<QString, int>{{"draft", 2}});  // and the title page
    REQUIRE(pm.setStatus(chapter, "final"));
    CHECK(chapterFileOf(chapter).status() == "final");
    CHECK(pm.getStatusStatistics() == std::map<QString, int>{{"draft", 1}, {"final", 1}});

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
    pm.project()->kinds.append(KindReference{"kalahari.short_stories", "story"});
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
    pm.project()->kinds.append(KindReference{"kalahari.short_stories", "story"});
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
    CHECK(pm.statisticsOf(pm.book()->frontElements).words == 2);

    CHECK(pm.getStatusStatistics() == std::map<QString, int>{{"draft", 3}, {"final", 1}});
    const auto incomplete = pm.getIncompleteElements();
    REQUIRE(incomplete.size() == 3);
    CHECK(incomplete.at(0) == std::pair<QString, QString>{titlePage, "draft"});
    CHECK(incomplete.at(1) == std::pair<QString, QString>{chapter1, "draft"});
    CHECK(incomplete.at(2) == std::pair<QString, QString>{a, "draft"});
    REQUIRE(pm.closeProject(false));
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

    // The archive has the files of the book, without its lock, the log files of its database
    // and the backups; the book stays open
    const QString archive = project.filePath("Travels.klh.zip");
    QStringList problems;
    int progress = 0;
    REQUIRE(pm.exportArchive(archive, [&progress](int percent) { progress = percent; },
                             &problems));
    CHECK(problems.isEmpty());
    CHECK(progress == 100);
    CHECK(pm.isProjectOpen());
    const QStringList entries{"Travels.klh", "book/chapter_001.kchapter",
                              "book/title_page_001.kchapter", "project.db"};
    CHECK(entriesOf(archive) == entries);

    // An archive written to the book's folder is not in the next one
    REQUIRE(pm.exportArchive(archive));
    CHECK(entriesOf(archive) == entries);

    // The imported book opens in a folder of its own, with its text and its styles
    const QString target = QDir(dir.path()).filePath("imported");
    REQUIRE(pm.importArchive(archive, target, nullptr, &problems));
    CHECK(problems.isEmpty());
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
    CHECK_FALSE(pm.importArchive(archive, target, nullptr, &problems));
    REQUIRE(problems.size() == 1);
    CHECK(problems.first().endsWith(": the folder already exists"));
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

    QStringList problems;
    REQUIRE(pm.importArchive(archive, QDir(dir.path()).filePath("imported"), nullptr, &problems));
    CHECK(problems.isEmpty());
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
        QStringList problems;
        CHECK_FALSE(pm.importArchive(notes, target, nullptr, &problems));
        CHECK(problems == QStringList{"Notes.klh.zip: the archive has no .klh file"});
        CHECK_FALSE(QFileInfo::exists(QDir(target).filePath("Notes")));
    }
}

TEST_CASE("An imported book takes the name of its archive", "[project_manager][archive]") {
    CHECK(ProjectManager::archiveProjectName("/books/My Novel.klh.zip") == "My Novel");
    CHECK(ProjectManager::archiveProjectName("C:/books/My Novel.KLH.ZIP") == "My Novel");
    CHECK(ProjectManager::archiveProjectName("/books/v1.2 draft.zip") == "v1.2 draft");
    CHECK(ProjectManager::archiveProjectName("/books/.klh.zip") == "project");
}
