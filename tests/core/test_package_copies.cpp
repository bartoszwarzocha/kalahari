/// @file test_package_copies.cpp
/// @brief Copies of the book type packages in the folder of a book: a new book gets them, the
/// book opens and works with them where its type is not installed, a newer installed package
/// refreshes them, and the other books do not see them

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/database_types.h>
#include <kalahari/core/package_copies.h>
#include <kalahari/core/project_database.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/editor/kml_document_model.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <zip.h>

#include <algorithm>
#include <string>

using namespace kalahari::core;

namespace {

const QString BUILT_IN = QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes");

/// Files of folder @p path and of the folders in it, relative to it, sorted
QStringList filesOf(const QString& path) {
    QStringList files;
    const QDir dir(path);
    QDirIterator it(path, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        files << dir.relativeFilePath(it.next());
    }
    files.sort();
    return files;
}

/// Folders of the copies of the book in @p projectDir, sorted
QStringList copiesOf(const QString& projectDir) {
    return QDir(PackageCopies::folderOf(projectDir))
        .entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
}

QByteArray contentsOf(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

/// Copy folder @p from, with the folders in it, to @p to
void copyFolder(const QString& from, const QString& to) {
    const QDir source(from);
    for (const QString& file : filesOf(from)) {
        const QString target = QDir(to).filePath(file);
        REQUIRE(QDir().mkpath(QFileInfo(target).absolutePath()));
        REQUIRE(QFile::copy(source.filePath(file), target));
    }
}

/// Set the version of the package in folder @p package to @p version
void setVersion(const QString& package, const QString& version) {
    const QString manifest = QDir(package).filePath(QStringLiteral("booktype.json"));
    QJsonObject json = QJsonDocument::fromJson(contentsOf(manifest)).object();
    json.insert(QStringLiteral("version"), version);
    QFile file(manifest);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(json).toJson());
}

/// The packages installed with the program, in a folder of the test that it can change
struct Installed {
    QTemporaryDir dir;

    Installed() {
        REQUIRE(dir.isValid());
        copyFolder(BUILT_IN, folder());
        load();
    }
    ~Installed() { ProjectManager::getInstance().loadBookTypes({BUILT_IN}); }
    Installed(const Installed&) = delete;
    Installed& operator=(const Installed&) = delete;

    QString folder() const { return dir.filePath(QStringLiteral("booktypes")); }
    QString package(const QString& id) const { return QDir(folder()).filePath(id); }

    /// The program loads the packages of the folder, as when it starts
    void load() const {
        auto& pm = ProjectManager::getInstance();
        pm.loadBookTypes({folder()});
        REQUIRE(pm.installedBookTypes().problems().isEmpty());
    }

    /// Packages @p ids are not installed any more
    void remove(const QStringList& ids) const {
        for (const QString& id : ids) {
            REQUIRE(QDir(package(id)).removeRecursively());
        }
        load();
    }
};

/// Kinds with their packages
std::string references(const QList<KindRef>& kinds) {
    QStringList list;
    for (const KindRef& ref : kinds) {
        list << ref.reference();
    }
    return list.join(QStringLiteral(", ")).toStdString();
}

/// Paragraphs of the text of element @p id
QStringList paragraphsOf(const QString& id) {
    auto& pm = ProjectManager::getInstance();
    const ProjectElement* element = pm.findElement(id);
    REQUIRE(element != nullptr);
    const std::optional<ChapterDocument> chapter = ChapterDocument::load(pm.filePathOf(*element));
    REQUIRE(chapter.has_value());
    kalahari::editor::KmlDocumentModel model;
    REQUIRE(model.loadKml(chapter->kml()));
    QStringList paragraphs;
    for (size_t i = 0; i < model.paragraphCount(); ++i) {
        paragraphs << model.paragraphText(i);
    }
    return paragraphs;
}

/// Type and version of the type in the .klh file @p manifest
QString savedType(const QString& manifest) {
    const QJsonObject type =
        QJsonDocument::fromJson(contentsOf(manifest)).object().value("type").toObject();
    return type.value("id").toString() + QLatin1Char(' ') + type.value("version").toString();
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

}  // namespace

TEST_CASE("Package copies: a version is newer by its numbers", "[core][package_copies]") {
    CHECK(PackageCopies::isNewer("1.1", "1.0"));
    CHECK(PackageCopies::isNewer("1.10", "1.9"));
    CHECK(PackageCopies::isNewer("2.0", "1.9.9"));
    CHECK(PackageCopies::isNewer("1.0.1", "1.0"));
    CHECK_FALSE(PackageCopies::isNewer("1.0", "1.0"));
    CHECK_FALSE(PackageCopies::isNewer("1.0", "1.0.0"));
    CHECK_FALSE(PackageCopies::isNewer("1.0.0", "1.0"));
    CHECK_FALSE(PackageCopies::isNewer("1.9", "1.10"));
}

TEST_CASE("Package copies: a new book has a copy of each package it uses",
          "[core][package_copies]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();

    SECTION("A novel: its type and the base package, with their data") {
        REQUIRE(pm.createProject(dir.path(), "Moja powieść", "Anna", "pl", true,
                                 "kalahari.novel"));
        const QString projectDir = pm.getProjectPath();
        CHECK(copiesOf(projectDir) == QStringList{"kalahari.base", "kalahari.novel"});

        // The copy is the data of the package as the program has it
        const QString base = PackageCopies::folderOf(projectDir, "kalahari.base");
        CHECK(filesOf(base) == QStringList{"booktype.json", "styles.json",
                                           "templates/title_page.kchapter"});
        for (const QString& file : filesOf(base)) {
            CHECK(contentsOf(QDir(base).filePath(file)) ==
                  contentsOf(QDir(installed.package("kalahari.base")).filePath(file)));
        }
        CHECK(filesOf(PackageCopies::folderOf(projectDir, "kalahari.novel")) ==
              QStringList{"booktype.json", "styles.json"});

        // The book takes its kinds from its copies
        const BookTypePackage* novel = pm.bookTypes().package("kalahari.novel");
        REQUIRE(novel != nullptr);
        CHECK(QFileInfo(novel->directory) ==
              QFileInfo(PackageCopies::folderOf(projectDir, "kalahari.novel")));
        REQUIRE(pm.closeProject(false));
    }

    SECTION("A collection of stories: also the novel, whose kinds it uses") {
        REQUIRE(pm.createProject(dir.path(), "Stories", "Anna", "en", true,
                                 "kalahari.short_stories"));
        CHECK(copiesOf(pm.getProjectPath()) ==
              QStringList{"kalahari.base", "kalahari.novel", "kalahari.short_stories"});
        REQUIRE(pm.closeProject(false));
    }

    SECTION("A user project: the packages of its kinds") {
        REQUIRE(pm.createProject(dir.path(), "Notes", "Anna", "en", true));
        CHECK(copiesOf(pm.getProjectPath()) == QStringList{"kalahari.base"});
        REQUIRE(pm.closeProject(false));

        NewProject request;
        request.parentDir = dir.path();
        request.title = QStringLiteral("Essays");
        request.author = QStringLiteral("Anna");
        request.language = QStringLiteral("en");
        const BookTypeRegistry& types = pm.installedBookTypes();
        PaletteChoice palette;
        palette.kinds = {types.findKind("kalahari.base", "chapter"),
                         types.findKind("kalahari.nonfiction", "bibliography")};
        request.palette = palette;
        REQUIRE(pm.createProject(request));
        CHECK(copiesOf(pm.getProjectPath()) ==
              QStringList{"kalahari.base", "kalahari.nonfiction"});
        REQUIRE(pm.closeProject(false));
    }

    SECTION("A book made without opening it") {
        NewProject request;
        request.parentDir = dir.path();
        request.title = QStringLiteral("Later");
        request.author = QStringLiteral("Anna");
        request.language = QStringLiteral("en");
        request.typeId = QStringLiteral("kalahari.poetry");
        const QString manifest = pm.createProjectWithoutOpening(request);
        REQUIRE_FALSE(manifest.isEmpty());
        CHECK(copiesOf(QFileInfo(manifest).absolutePath()) ==
              QStringList{"kalahari.base", "kalahari.poetry"});
    }
}

TEST_CASE("Package copies: a book opens and works where its type is not installed",
          "[core][package_copies]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();

    REQUIRE(pm.createProject(dir.path(), "Moja powieść", "Anna Nowak", "pl", true,
                             "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const std::string mainKinds = references(pm.kindsFor(BookPlace::Main));
    REQUIRE(pm.closeProject(false));

    // Another computer: the program has no novel, nor the collection of stories that uses it
    installed.remove({"kalahari.novel", "kalahari.short_stories"});
    REQUIRE(pm.installedBookTypes().package("kalahari.novel") == nullptr);

    QStringList problems;
    REQUIRE(pm.openProject(manifest, &problems));
    CHECK(problems.isEmpty());
    REQUIRE(pm.project() != nullptr);
    CHECK(pm.bookTypes().package("kalahari.novel") != nullptr);
    CHECK(pm.bookTypes().problems().isEmpty());

    // The book offers what it offered where the novel was installed
    CHECK(references(pm.kindsFor(BookPlace::Main)) == mainKinds);
    CHECK(pm.mainTextKind().reference() == "kalahari.base:chapter");
    const KindRef prologue = pm.bookTypes().findKind("kalahari.novel", "prologue");
    REQUIRE(prologue);
    const QString added = pm.addElement(prologue, "Prolog", BookPlace::Main);
    REQUIRE_FALSE(added.isEmpty());
    CHECK(pm.book()->mainElements.first().id == added);

    // A title page added again has the title and the author, from the template of the copy
    const QString titlePage = pm.book()->frontElements.first().id;
    REQUIRE(pm.removeElement(titlePage).has_value());
    const KindRef titleKind = pm.bookTypes().findKind("kalahari.base", "title_page");
    REQUIRE(titleKind);
    const QString again = pm.addElement(titleKind, "Strona tytułowa", BookPlace::Front);
    REQUIRE_FALSE(again.isEmpty());
    CHECK(paragraphsOf(again) == QStringList{"Moja powieść", "", "Anna Nowak"});

    // The copy serves this book only: no new novel, and other books do not see it
    NewProject request;
    request.parentDir = dir.path();
    request.title = QStringLiteral("Druga");
    request.author = QStringLiteral("Anna");
    request.language = QStringLiteral("pl");
    request.typeId = QStringLiteral("kalahari.novel");
    problems.clear();
    CHECK_FALSE(pm.createProject(request, &problems));
    CHECK(problems.contains(QStringLiteral("the book type kalahari.novel is not installed")));
    CHECK(QFileInfo(pm.getManifestPath()) == QFileInfo(manifest));

    REQUIRE(pm.createProject(dir.path(), "Zeszyt", "Anna", "pl", true));
    CHECK(pm.bookTypes().package("kalahari.novel") == nullptr);
    for (const BookPlace place : {BookPlace::Front, BookPlace::Main, BookPlace::Back}) {
        for (const KindRef& kind : pm.kindsFor(place)) {
            CHECK(kind.package->id == "kalahari.base");
        }
    }
    CHECK(copiesOf(pm.getProjectPath()) == QStringList{"kalahari.base"});
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Package copies: a newer installed package refreshes the copy, an older one does not",
          "[core][package_copies]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();

    REQUIRE(pm.createProject(dir.path(), "Moja powieść", "Anna", "pl", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QString projectDir = pm.getProjectPath();
    const std::string mainKinds = references(pm.kindsFor(BookPlace::Main));

    // A style the book uses
    ParagraphStyle style;
    style.id = QStringLiteral("letter");
    style.name = QStringLiteral("Letter");
    style.baseStyle = QStringLiteral("normal");
    REQUIRE(pm.getDatabase() != nullptr);
    pm.getDatabase()->saveParagraphStyle(style);
    REQUIRE(pm.closeProject(false));
    CHECK(savedType(manifest) == "kalahari.novel 1.0");

    // A newer novel installed: the book opens with it, and keeps its styles
    setVersion(installed.package("kalahari.novel"), "1.1");
    installed.load();
    REQUIRE(pm.openProject(manifest));
    REQUIRE(PackageCopies::read(projectDir, "kalahari.novel").has_value());
    CHECK(PackageCopies::read(projectDir, "kalahari.novel")->version == "1.1");
    CHECK(PackageCopies::read(projectDir, "kalahari.base")->version == "1.0");
    CHECK(pm.bookTypes().package("kalahari.novel")->version == "1.1");
    CHECK(pm.project()->type->version == "1.1");
    CHECK(references(pm.kindsFor(BookPlace::Main)) == mainKinds);
    const QList<ParagraphStyle> styles = pm.getDatabase()->getParagraphStyles();
    const auto letter = std::find_if(styles.cbegin(), styles.cend(),
                                     [](const ParagraphStyle& s) { return s.id == "letter"; });
    REQUIRE(letter != styles.cend());
    CHECK(letter->name == "Letter");
    CHECK(letter->baseStyle == "normal");
    REQUIRE(pm.closeProject(false));
    CHECK(savedType(manifest) == "kalahari.novel 1.1");
    CHECK(copiesOf(projectDir) == QStringList{"kalahari.base", "kalahari.novel"});

    // The older novel again: the newer copy stays and serves the book
    setVersion(installed.package("kalahari.novel"), "1.0");
    installed.load();
    REQUIRE(pm.openProject(manifest));
    CHECK(PackageCopies::read(projectDir, "kalahari.novel")->version == "1.1");
    CHECK(pm.bookTypes().package("kalahari.novel")->version == "1.1");
    CHECK(pm.project()->type->version == "1.1");
    CHECK(pm.installedBookTypes().package("kalahari.novel")->version == "1.0");
    REQUIRE(pm.closeProject(false));
    CHECK(savedType(manifest) == "kalahari.novel 1.1");
}

TEST_CASE("Package copies: a book gets its copies when it opens without them",
          "[core][package_copies]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();
    REQUIRE(pm.createProject(dir.path(), "Old", "Anna", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QString projectDir = pm.getProjectPath();
    REQUIRE(pm.closeProject(false));

    SECTION("A book made before the copies") {
        REQUIRE(QDir(PackageCopies::folderOf(projectDir)).removeRecursively());
        REQUIRE(pm.openProject(manifest));
        CHECK(copiesOf(projectDir) == QStringList{"kalahari.base", "kalahari.novel"});
        REQUIRE(pm.closeProject(false));
    }

    SECTION("A copy that cannot be read") {
        const QString copy = PackageCopies::folderOf(projectDir, "kalahari.novel");
        QFile file(QDir(copy).filePath("booktype.json"));
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("{ not a package");
        file.close();
        REQUIRE_FALSE(PackageCopies::read(projectDir, "kalahari.novel").has_value());

        REQUIRE(pm.openProject(manifest));
        REQUIRE(PackageCopies::read(projectDir, "kalahari.novel").has_value());
        CHECK(contentsOf(QDir(copy).filePath("booktype.json")) ==
              contentsOf(QDir(installed.package("kalahari.novel")).filePath("booktype.json")));
        CHECK(copiesOf(projectDir) == QStringList{"kalahari.base", "kalahari.novel"});
        REQUIRE(pm.closeProject(false));
    }

    SECTION("Copies that cannot be written: the installed packages serve the book") {
        REQUIRE(QDir(PackageCopies::folderOf(projectDir)).removeRecursively());
        QFile blocker(PackageCopies::folderOf(projectDir));
        REQUIRE(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        REQUIRE(pm.openProject(manifest));
        const BookTypePackage* novel = pm.bookTypes().package("kalahari.novel");
        REQUIRE(novel != nullptr);
        CHECK(QFileInfo(novel->directory) == QFileInfo(installed.package("kalahari.novel")));
        CHECK_FALSE(pm.kindsFor(BookPlace::Main).isEmpty());
        REQUIRE(pm.closeProject(false));
    }
}

TEST_CASE("Package copies: the archive of a book takes them to another computer",
          "[core][package_copies][archive]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();
    REQUIRE(pm.createProject(dir.path(), "Travels", "Anna", "en", true, "kalahari.novel"));
    const std::string mainKinds = references(pm.kindsFor(BookPlace::Main));
    const QString archive = QDir(dir.path()).filePath("Travels.klh.zip");
    REQUIRE(pm.exportArchive(archive));
    REQUIRE(pm.closeProject(false));
    const QStringList entries = entriesOf(archive);
    CHECK(entries.contains("types/kalahari.novel/booktype.json"));
    CHECK(entries.contains("types/kalahari.base/templates/title_page.kchapter"));

    installed.remove({"kalahari.novel", "kalahari.short_stories"});
    const QString target = QDir(dir.path()).filePath("elsewhere");
    REQUIRE(pm.importArchive(archive, target));
    CHECK(references(pm.kindsFor(BookPlace::Main)) == mainKinds);
    REQUIRE(pm.closeProject(false));
}
