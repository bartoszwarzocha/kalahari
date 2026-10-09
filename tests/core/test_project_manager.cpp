/// @file test_project_manager.cpp
/// @brief ProjectManager: creating, opening and saving book projects, their elements, the text
/// and the data of their chapter files

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/project_database.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/editor/kml_document_model.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <string>
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

    // The project starts as its .klh file; the book's folder comes with its first file
    CHECK_FALSE(QFileInfo::exists(QDir(pm.getProjectPath()).filePath("book")));
    CHECK_FALSE(pm.isDirty());
    REQUIRE(pm.closeProject(false));

    const BookProject saved = savedProject(manifest);
    REQUIRE(saved.type.has_value());
    CHECK(saved.type->id == "kalahari.novel");
    CHECK(saved.books.first().title == "My Novel");

    SECTION("A type without the parts layer") {
        REQUIRE(pm.createProject(dir.path(), "Pilot", "Anna", "en", true, "kalahari.screenplay"));
        REQUIRE(pm.book() != nullptr);
        CHECK_FALSE(pm.book()->partsLayer);
        REQUIRE(pm.closeProject(false));
    }
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
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager does not create a project it cannot make", "[project_manager]") {
    QTemporaryDir dir;
    auto& pm = projects();

    SECTION("A type that is not installed") {
        CHECK_FALSE(pm.createProject(dir.path(), "Thriller", "Anna", "en", true, "acme.thriller"));
        CHECK_FALSE(QFileInfo::exists(QDir(dir.path()).filePath("Thriller")));
    }

    SECTION("A package that is not a type") {
        CHECK_FALSE(pm.createProject(dir.path(), "Base", "Anna", "en", true, "kalahari.base"));
        CHECK_FALSE(QFileInfo::exists(QDir(dir.path()).filePath("Base")));
    }

    SECTION("A folder with files") {
        REQUIRE(QDir(dir.path()).mkpath("Taken"));
        const QString file = QDir(dir.path()).filePath("Taken/notes.txt");
        writeFile(file, "mine");
        CHECK_FALSE(pm.createProject(dir.path(), "Taken", "Anna", "en", true, "kalahari.novel"));
        CHECK(QFileInfo::exists(file));
        CHECK_FALSE(QFileInfo::exists(QDir(dir.path()).filePath("Taken/Taken.klh")));
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
    QJsonObject group = book["main"].toArray().first().toObject();
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
    CHECK(pm.getStatusStatistics() == std::map<QString, int>{{"draft", 1}});
    REQUIRE(pm.setStatus(chapter, "final"));
    CHECK(chapterFileOf(chapter).status() == "final");

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

    // A file that no element names takes chapter_002
    REQUIRE(project.mkpath("book"));
    writeFile(project.filePath("book/chapter_002.kchapter"), "");

    const KindRef chapter = pm.chapterKindFor(BookPlace::Main);
    const QString first = pm.addElement(chapter, "One", BookPlace::Main);
    const QString second = pm.addElement(chapter, "Two", BookPlace::Main);
    const QString titlePage =
        pm.addElement(kind("kalahari.novel", "title_page"), "Title Page", BookPlace::Front);
    REQUIRE_FALSE(first.isEmpty());
    REQUIRE_FALSE(second.isEmpty());
    REQUIRE_FALSE(titlePage.isEmpty());

    const ProjectElement* one = pm.findElement(first);
    REQUIRE(one != nullptr);
    CHECK(one->file == "book/chapter_001.kchapter");
    CHECK(one->kind == KindReference{"kalahari.base", "chapter"});
    CHECK(one->status == "draft");
    CHECK(pm.findElement(second)->file == "book/chapter_003.kchapter");
    CHECK(pm.findElement(titlePage)->file == "book/title_page_001.kchapter");

    const ChapterDocument file = chapterFileOf(first);
    CHECK(file.title() == "One");
    CHECK(file.status() == "draft");
    CHECK(file.kml().isEmpty());

    // The .klh file has them at once
    CHECK_FALSE(pm.isDirty());
    const BookProject saved = savedProject(pm.getManifestPath());
    CHECK(titles(saved.books.first().mainElements) == "One, Two");
    CHECK(titles(saved.books.first().frontElements) == "Title Page");
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

    // One title page
    const KindRef titlePage = kind("kalahari.novel", "title_page");
    CHECK(references(pm.textKindsFor(BookPlace::Front)).find("title_page") != std::string::npos);
    REQUIRE_FALSE(pm.addElement(titlePage, "Title Page", BookPlace::Front).isEmpty());
    CHECK(references(pm.textKindsFor(BookPlace::Front)).find("title_page") == std::string::npos);
    CHECK(pm.addElement(titlePage, "Second Title Page", BookPlace::Front).isEmpty());

    // A part has no file; chapters and mottos go into it, a prologue does not
    const QString part = pm.addElement(pm.partKind(), "Part One", BookPlace::Main);
    REQUIRE_FALSE(part.isEmpty());
    CHECK(pm.findElement(part)->file.isEmpty());
    CHECK(pm.findElement(part)->status.isEmpty());
    CHECK(references(pm.textKindsFor(BookPlace::Main, part)) ==
          "kalahari.base:chapter, kalahari.base:motto");
    const QString chapter =
        pm.addElement(pm.chapterKindFor(BookPlace::Main, part), "Chapter 1", BookPlace::Main, part);
    REQUIRE_FALSE(chapter.isEmpty());
    REQUIRE(pm.findElement(part)->elements.size() == 1);
    CHECK(pm.findElement(part)->elements.first().id == chapter);
    CHECK(pm.addElement(kind("kalahari.novel", "prologue"), "Prologue", BookPlace::Main, part)
              .isEmpty());

    // Kinds the type does not offer there, window elements and groups that do not exist
    CHECK(pm.addElement(kind("kalahari.base", "introduction"), "Intro", BookPlace::Front)
              .isEmpty());
    CHECK(pm.addElement(kind("kalahari.novel", "toc"), "Contents", BookPlace::Front).isEmpty());
    CHECK(pm.addElement(pm.chapterKindFor(BookPlace::Main), "Lost", BookPlace::Main, "no-group")
              .isEmpty());
    CHECK(pm.addElement(pm.chapterKindFor(BookPlace::Main), "Lost", BookPlace::Main, chapter)
              .isEmpty());
    CHECK(titles(pm.book()->mainElements) == "Part One");
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
    CHECK(titles(pm.book()->mainElements) == "Part, First, B");
    CHECK_FALSE(pm.moveElement(b, 3));
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
    CHECK(titles(saved.books.first().mainElements) == "First, B");
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
        CHECK(pm.findElement(id)->file == "book/chapter_001.kchapter");
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
        CHECK(pm.book()->mainElements.isEmpty());
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

    const TextStatistics statistics = pm.statisticsOf(pm.book()->mainElements);
    CHECK(statistics.elements == 2);
    CHECK(statistics.words == 3);
    CHECK(statistics.statuses == std::map<QString, int>{{"draft", 1}, {"final", 1}});

    CHECK(pm.getStatusStatistics() == std::map<QString, int>{{"draft", 1}, {"final", 1}});
    const auto incomplete = pm.getIncompleteElements();
    REQUIRE(incomplete.size() == 1);
    CHECK(incomplete.front() == std::pair<QString, QString>{a, "draft"});
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
