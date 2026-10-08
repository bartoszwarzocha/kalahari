/// @file test_project_manager.cpp
/// @brief Unit tests for ProjectManager project lifecycle

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book.h>
#include <kalahari/core/book_element.h>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/document.h>
#include <kalahari/core/part.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/project_database.h>
#include <kalahari/editor/kml_document_model.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <filesystem>

using namespace kalahari::core;
namespace fs = std::filesystem;

namespace {

class TempDir {
public:
    TempDir()
        : m_path(fs::temp_directory_path() /
                 ("kalahari_pm_" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString())) {
        fs::create_directories(m_path);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(m_path, ec);
    }

    QString path() const { return QString::fromStdString(m_path.string()); }

private:
    fs::path m_path;
};

/// The chapter files in a folder whose names begin with ".kchapter"
QStringList dotChapterFilesIn(const QDir& folder) {
    return folder.entryList({QStringLiteral(".kchapter*")}, QDir::Files | QDir::Hidden);
}

/// The files the manifest names for the chapters of the first part
QStringList chapterFilesIn(const QString& manifest) {
    QFile file(manifest);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonObject structure = QJsonDocument::fromJson(file.readAll()).object()["structure"].toObject();
    const QJsonObject part = structure["body"].toArray().first().toObject();
    const QJsonArray chapters = part["chapters"].toArray();
    QStringList files;
    for (const auto& chapter : chapters) {
        files << chapter.toObject()["file"].toString();
    }
    return files;
}

/// A chapter's KML with one paragraph
QString kmlWith(const QString& text) {
    return QStringLiteral("<kml><p>%1</p></kml>").arg(text);
}

} // namespace

TEST_CASE("ProjectManager closes the database after projectAboutToClose", "[project_manager]") {
    TempDir dir;
    auto& pm = ProjectManager::getInstance();

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
    CHECK_FALSE(pm.isProjectOpen());
}

TEST_CASE("ProjectManager keeps manifest identity and unknown fields when saving", "[project_manager]") {
    TempDir dir;
    auto& pm = ProjectManager::getInstance();

    REQUIRE(pm.createProject(dir.path(), "Identity Test", "Author", "en", true));
    const QString manifest = QDir(pm.getProjectPath()).filePath("Identity Test.klh");
    REQUIRE(pm.closeProject(false));

    auto readManifest = [&manifest]() {
        QFile file(manifest);
        REQUIRE(file.open(QIODevice::ReadOnly));
        return QJsonDocument::fromJson(file.readAll()).object();
    };
    auto writeManifest = [&manifest](const QJsonObject& root) {
        QFile file(manifest);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(root).toJson());
    };

    QJsonObject root = readManifest();
    QJsonObject document = root["document"].toObject();
    document["id"] = "1771221457515-61aa";
    document["created"] = "2026-02-16T05:57:37+00:00";
    document["description"] = "kept";
    root["document"] = document;
    root["statistics"] = QJsonObject{{"totalWords", 1234}, {"totalChapters", 5}, {"lastEdited", "x"}};
    root["settings"] = QJsonObject{{"defaultPerspective", "editor"}, {"autoSaveInterval", 60}};
    root["custom"] = QJsonObject{{"key", "value"}};
    writeManifest(root);

    REQUIRE(pm.openProject(manifest));
    CHECK(pm.getDocument()->getId() == "1771221457515-61aa");
    REQUIRE(pm.saveManifest());
    REQUIRE(pm.closeProject(false));

    const QJsonObject saved = readManifest();
    const QJsonObject savedDocument = saved["document"].toObject();
    CHECK(savedDocument["id"].toString() == "1771221457515-61aa");
    CHECK(QDateTime::fromString(savedDocument["created"].toString(), Qt::ISODate) ==
          QDateTime::fromString("2026-02-16T05:57:37+00:00", Qt::ISODate));
    CHECK(savedDocument["description"].toString() == "kept");
    CHECK(savedDocument["title"].toString() == "Identity Test");
    CHECK(saved["statistics"].toObject()["totalWords"].toInt() == 1234);
    CHECK(saved["settings"].toObject()["autoSaveInterval"].toInt() == 60);
    CHECK(saved["custom"].toObject()["key"].toString() == "value");
}

TEST_CASE("ProjectManager makes a chapter of a text file added to the project", "[project_manager]") {
    TempDir dir;
    auto& pm = ProjectManager::getInstance();
    REQUIRE(pm.createProject(dir.path(), "Text Import", "Author", "en", true));

    const QString textPath = QDir(dir.path()).filePath("notes.txt");
    {
        QFile file(textPath);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("First line\r\nSecond & last\r\n");
    }

    SECTION("Copied") {
        const QString id = pm.addChapterToSection("frontmatter", QString(), "Notes", textPath, true);
        REQUIRE_FALSE(id.isEmpty());
        BookElement* element = pm.findElement(id);
        REQUIRE(element != nullptr);
        CHECK(element->getFile().extension() == ".kchapter");
        CHECK(QFile::exists(textPath));

        kalahari::editor::KmlDocumentModel chapter;
        REQUIRE(chapter.loadKml(pm.loadChapterContent(id)));
        REQUIRE(chapter.paragraphCount() == 2);  // the last line end makes no empty paragraph
        CHECK(chapter.paragraphText(0) == "First line");
        CHECK(chapter.paragraphText(1) == "Second & last");
    }

    SECTION("Moved") {
        const QString id = pm.addChapterToSection("frontmatter", QString(), "Notes", textPath, false);
        REQUIRE_FALSE(id.isEmpty());
        CHECK_FALSE(QFile::exists(textPath));
        CHECK(pm.loadChapterContent(id).contains("Second &amp; last"));
    }

    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager gives a new chapter a chapter file of its own", "[project_manager]") {
    TempDir dir;
    auto& pm = ProjectManager::getInstance();
    REQUIRE(pm.createProject(dir.path(), "New Chapters", "Author", "en", true));
    const QDir project(pm.getProjectPath());
    Book& book = pm.getDocument()->getBook();

    // The part's chapters are in content/body/part_001, where a file no chapter names
    // takes chapter_002.kchapter
    auto part = std::make_shared<Part>("part-001", "Part One");
    part->addChapter(std::make_shared<BookElement>(
        "chapter", "ch-001", "One", fs::path("content/body/part_001/chapter_001.kchapter")));
    book.addPart(part);
    REQUIRE(project.mkpath("content/body/part_001"));
    {
        QFile taken(project.filePath("content/body/part_001/chapter_002.kchapter"));
        REQUIRE(taken.open(QIODevice::WriteOnly));
    }

    SECTION("A chapter goes next to the chapters of its part") {
        BookElement chapter("chapter", "ch-new", "New Chapter");
        REQUIRE(pm.createChapterFile(chapter, "body", "part-001"));
        // The manifest gets "/" between the folders on every system
        CHECK(QString::fromStdWString(chapter.getFile().wstring()) ==
              "content/body/part_001/chapter_003.kchapter");

        const auto saved =
            ChapterDocument::load(project.filePath("content/body/part_001/chapter_003.kchapter"));
        REQUIRE(saved.has_value());
        CHECK(saved->title() == "New Chapter");
        CHECK(saved->kml().isEmpty());
    }

    SECTION("A chapter of a part without chapters goes to the part's folder") {
        book.addPart(std::make_shared<Part>("part-002", "Part Two"));
        BookElement chapter("chapter", "ch-new", "New Chapter");
        REQUIRE(pm.createChapterFile(chapter, "body", "part-002"));
        CHECK(QString::fromStdWString(chapter.getFile().wstring()) ==
              "content/body/part-002/chapter_001.kchapter");
        CHECK(QFile::exists(project.filePath("content/body/part-002/chapter_001.kchapter")));
    }

    SECTION("Front and back matter go to the folders of their sections") {
        BookElement preface("preface", "fm-new", "Preface");
        REQUIRE(pm.createChapterFile(preface, "frontmatter"));
        CHECK(QString::fromStdWString(preface.getFile().wstring()) ==
              "content/frontmatter/preface_001.kchapter");

        // A type that is no plain name does not name the file
        BookElement notes("closing notes", "bm-new", "Notes");
        REQUIRE(pm.createChapterFile(notes, "backmatter"));
        CHECK(QString::fromStdWString(notes.getFile().wstring()) ==
              "content/backmatter/chapter_001.kchapter");
    }

    CHECK(dotChapterFilesIn(project).isEmpty());
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("ProjectManager saves and opens again the text of a new chapter", "[project_manager]") {
    // Regression: a chapter added in the Navigator had no file. Saved, its text went to
    // ".kchapter" in the project's folder, and opened again it was read as an old RTF file
    // to convert - the chapter opened empty, with a message that it is damaged
    TempDir dir;
    auto& pm = ProjectManager::getInstance();
    REQUIRE(pm.createProject(dir.path(), "New Chapter", "Author", "en", true));
    const QString manifest = QDir(pm.getProjectPath()).filePath("New Chapter.klh");
    const QDir project(pm.getProjectPath());

    // As the Navigator adds it: the chapter's file first, then the chapter in its part
    auto part = std::make_shared<Part>("part-001", "Part One");
    pm.getDocument()->getBook().addPart(part);
    auto chapter = std::make_shared<BookElement>("chapter", "ch-new", "New Chapter");
    REQUIRE(pm.createChapterFile(*chapter, "body", "part-001"));
    part->addChapter(chapter);
    REQUIRE(pm.saveManifest());

    chapter->setContent(kmlWith("The new text"));
    REQUIRE(pm.saveChapterContent("ch-new"));
    CHECK(chapterFilesIn(manifest) == QStringList{"content/body/part-001/chapter_001.kchapter"});

    REQUIRE(pm.closeProject(false));
    REQUIRE(pm.openProject(manifest));
    CHECK(pm.loadChapterContent("ch-new") == kmlWith("The new text"));
    CHECK(dotChapterFilesIn(project).isEmpty());
    REQUIRE(pm.closeProject(false));
}
