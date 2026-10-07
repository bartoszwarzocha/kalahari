/// @file test_project_manager.cpp
/// @brief Unit tests for ProjectManager project lifecycle

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_element.h>
#include <kalahari/core/document.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/project_database.h>
#include <kalahari/editor/kml_document_model.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
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
