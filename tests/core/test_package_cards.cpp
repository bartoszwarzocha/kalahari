/// @file test_package_cards.cpp
/// @brief Cards of the book type packages in the folder of a book: a new book gets them, a
/// newer installed package refreshes them, an archive takes them along, and a book whose
/// packages the program does not have opens read-only and is named after them

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/database_types.h>
#include <kalahari/core/icon_registry.h>
#include <kalahari/core/package_card.h>
#include <kalahari/core/project_database.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/gui/icon_registrar.h>

#include <QCryptographicHash>
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
#include <vector>

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

/// Folders of the cards of the book in @p projectDir, sorted
QStringList cardsOf(const QString& projectDir) {
    return QDir(PackageCards::folderOf(projectDir))
        .entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
}

QByteArray contentsOf(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

void writeFile(const QString& path, const QByteArray& contents) {
    REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(contents);
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

/// Every file of the folder @p projectDir, sorted, with the hash of its contents, so that a
/// test that fails names the files
std::vector<std::string> allFilesOf(const QString& projectDir) {
    std::vector<std::string> files;
    for (const QString& file : filesOf(projectDir)) {
        const QByteArray hash = QCryptographicHash::hash(
            contentsOf(QDir(projectDir).filePath(file)), QCryptographicHash::Sha256);
        files.push_back(QStringLiteral("%1 %2").arg(file, hash.toHex()).toStdString());
    }
    return files;
}

QJsonObject jsonOf(const QString& path) {
    return QJsonDocument::fromJson(contentsOf(path)).object();
}

void writeJson(const QString& path, const QJsonObject& json) {
    writeFile(path, QJsonDocument(json).toJson());
}

/// Set the version of the package in folder @p package to @p version
void setVersion(const QString& package, const QString& version) {
    const QString manifest = QDir(package).filePath(QStringLiteral("booktype.json"));
    QJsonObject json = jsonOf(manifest);
    json.insert(QStringLiteral("version"), version);
    writeJson(manifest, json);
}

/// Take kind @p kindId out of the package in folder @p package, as a version without it
void removeKind(const QString& package, const QString& kindId) {
    const QString manifest = QDir(package).filePath(QStringLiteral("booktype.json"));
    QJsonObject json = jsonOf(manifest);
    QJsonObject kinds = json.value(QStringLiteral("kinds")).toObject();
    kinds.remove(kindId);
    json.insert(QStringLiteral("kinds"), kinds);
    for (const char* list : {"front", "main", "back", "workshop", "start"}) {
        QJsonArray ids = json.value(QLatin1String(list)).toArray();
        for (qsizetype i = ids.size() - 1; i >= 0; --i) {
            if (ids.at(i).toString() == kindId) {
                ids.removeAt(i);
            }
        }
        json.insert(QLatin1String(list), ids);
    }
    writeJson(manifest, json);
}

/// The packages installed with the program, in a folder of the test that it can change
struct Installed {
    QTemporaryDir dir;

    Installed() {
        REQUIRE(dir.isValid());
        copyFolder(BUILT_IN, folder());
        load();
        // The icons of the program, which the cards take along
        kalahari::gui::registerAllIcons();
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

    /// Packages @p ids are installed again, as the program comes with them
    void restore(const QStringList& ids) const {
        for (const QString& id : ids) {
            REQUIRE(QDir(package(id)).removeRecursively());
            copyFolder(QDir(BUILT_IN).filePath(id), package(id));
        }
        load();
    }
};

/// Type and version of the type in the .klh file @p manifest
QString savedType(const QString& manifest) {
    const QJsonObject type = jsonOf(manifest).value("type").toObject();
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

/// The files that the card of a package with icons @p icons has: card.json and the SVG files
/// of the icons in each icon theme, sorted
QStringList cardFiles(const QStringList& icons) {
    QStringList files{QStringLiteral("card.json")};
    for (const QString& theme : IconRegistry::iconThemes()) {
        for (const QString& icon : icons) {
            files << QStringLiteral("icons/%1/%2.svg").arg(theme, icon);
        }
    }
    files.sort();
    return files;
}

/// A novel "Moja powieść" with a prologue and a chapter with text, closed
struct Novel {
    QString manifest;
    QString projectDir;
    QString prologue;
    QString chapter;

    explicit Novel(const QString& parentDir) {
        auto& pm = ProjectManager::getInstance();
        REQUIRE(pm.createProject(parentDir, "Moja powieść", "Anna Nowak", "pl", true,
                                 "kalahari.novel"));
        manifest = pm.getManifestPath();
        projectDir = pm.getProjectPath();
        prologue = pm.addElement(pm.installedBookTypes().findKind("kalahari.novel", "prologue"),
                                 "Prolog", BookPlace::Main);
        REQUIRE_FALSE(prologue.isEmpty());
        chapter = pm.book()->mainElements.last().id;
        pm.setChapterContent(chapter, QStringLiteral("<kml><p>Ciemna noc</p></kml>"));
        REQUIRE(pm.saveChapterContent(chapter));
        REQUIRE(pm.closeProject(false));
    }
};

}  // namespace

TEST_CASE("Package cards: a version is newer by its numbers", "[core][package_cards]") {
    CHECK(PackageCards::isNewer("1.1", "1.0"));
    CHECK(PackageCards::isNewer("1.10", "1.9"));
    CHECK(PackageCards::isNewer("2.0", "1.9.9"));
    CHECK(PackageCards::isNewer("1.0.1", "1.0"));
    CHECK_FALSE(PackageCards::isNewer("1.0", "1.0"));
    CHECK_FALSE(PackageCards::isNewer("1.0", "1.0.0"));
    CHECK_FALSE(PackageCards::isNewer("1.0.0", "1.0"));
    CHECK_FALSE(PackageCards::isNewer("1.9", "1.10"));
}

TEST_CASE("Package cards: a new book has a card of each package it uses",
          "[core][package_cards]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();

    SECTION("A novel: the name, the version and the icons of its type and of the base") {
        REQUIRE(pm.createProject(dir.path(), "Moja powieść", "Anna", "pl", true,
                                 "kalahari.novel"));
        const QString projectDir = pm.getProjectPath();
        CHECK(cardsOf(projectDir) == QStringList{"kalahari.base", "kalahari.novel"});

        // The card and the icons of the type and of its kinds, in each icon theme; not the
        // rules, the styles or the templates of the package
        const QString novel = PackageCards::folderOf(projectDir, "kalahari.novel");
        CHECK(filesOf(novel) == cardFiles({"template.novel", "template.chapter"}));
        for (const QString& theme : IconRegistry::iconThemes()) {
            const QString icon = QStringLiteral("icons/%1/template.novel.svg").arg(theme);
            CHECK(contentsOf(QDir(novel).filePath(icon)) ==
                  contentsOf(IconRegistry::getInstance().svgFileOf("template.novel", theme)));
        }
        const QStringList base = filesOf(PackageCards::folderOf(projectDir, "kalahari.base"));
        CHECK(base.contains("card.json"));
        CHECK(base.contains("icons/twotone/template.chapter.svg"));
        CHECK(base.contains("icons/rounded/project.book.svg"));
        CHECK(std::all_of(base.cbegin(), base.cend(), [](const QString& file) {
            return file == "card.json" || file.startsWith("icons/");
        }));

        // What the card says of the type
        const std::optional<PackageCard> card = PackageCards::read(projectDir, "kalahari.novel");
        REQUIRE(card.has_value());
        CHECK(card->id == "kalahari.novel");
        CHECK(card->version == "1.0");
        CHECK(card->role == PackageRole::Type);
        CHECK(card->name.text("pl") == "Powieść");
        CHECK(card->name.text("en") == "Novel");
        CHECK(card->icon == "template.novel");
        REQUIRE(card->kinds.size() == 2);
        const ElementKind* prologue = card->kind("prologue");
        REQUIRE(prologue != nullptr);
        CHECK(prologue->name.text("pl") == "Prolog");
        CHECK(prologue->plural.text("en") == "Prologues");
        CHECK(prologue->icon == "template.chapter");
        CHECK(prologue->wordsIn("pl").forms.value("genitive") == "prologu");
        CHECK(card->kind("epilogue") != nullptr);
        CHECK(card->kind("chapter") == nullptr);
        CHECK(PackageCards::read(projectDir, "kalahari.base")->role == PackageRole::Shared);

        // The open book has its cards; it is not read-only
        REQUIRE(pm.packageCard("kalahari.novel") != nullptr);
        CHECK(pm.packageCard("kalahari.novel")->version == "1.0");
        CHECK_FALSE(pm.isReadOnly());
        CHECK(pm.missingPackages().isEmpty());
        REQUIRE(pm.closeProject(false));
        CHECK(pm.packageCard("kalahari.novel") == nullptr);
    }

    SECTION("A collection of stories: also the novel, whose kinds it uses") {
        REQUIRE(pm.createProject(dir.path(), "Stories", "Anna", "en", true,
                                 "kalahari.short_stories"));
        CHECK(cardsOf(pm.getProjectPath()) ==
              QStringList{"kalahari.base", "kalahari.novel", "kalahari.short_stories"});
        REQUIRE(pm.closeProject(false));
    }

    SECTION("A user project: the packages of its kinds") {
        REQUIRE(pm.createProject(dir.path(), "Notes", "Anna", "en", true));
        CHECK(cardsOf(pm.getProjectPath()) == QStringList{"kalahari.base"});
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
        CHECK(cardsOf(pm.getProjectPath()) ==
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
        CHECK(cardsOf(QFileInfo(manifest).absolutePath()) ==
              QStringList{"kalahari.base", "kalahari.poetry"});
    }
}

TEST_CASE("Package cards: a card is read as it is written", "[core][package_cards]") {
    const Installed installed;
    const BookTypePackage* novel = ProjectManager::getInstance().installedBookTypes().package(
        "kalahari.novel");
    REQUIRE(novel != nullptr);
    const PackageCard card = PackageCard::of(*novel);

    const std::optional<PackageCard> again = PackageCard::fromJson(card.toJson());
    REQUIRE(again.has_value());
    CHECK(again->toJson() == card.toJson());
    CHECK(again->kind("epilogue")->wordsIn("en").forms.value("indefinite") == "an epilogue");

    // Not a card: another format, or without a package id or a version
    const auto without = [&card](const char* key, const QJsonValue& value) {
        QJsonObject json = card.toJson();
        json.insert(QLatin1String(key), value);
        return PackageCard::fromJson(json);
    };
    CHECK_FALSE(without("format", 2).has_value());
    CHECK_FALSE(without("id", "Novel").has_value());
    CHECK_FALSE(without("version", "").has_value());

    // An icon file only of a name an icon can have, in the card's folder
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QString problem;
    REQUIRE(PackageCards::write(*novel, dir.filePath("card"), &problem));
    CHECK(problem.isEmpty());
    const std::optional<PackageCard> written = PackageCard::read(dir.filePath("card"));
    REQUIRE(written.has_value());
    CHECK_FALSE(written->iconFile("template.novel", "twotone").isEmpty());
    CHECK(written->iconFile("template.missing", "twotone").isEmpty());
    CHECK(written->iconFile("../card", "twotone").isEmpty());
    CHECK(written->iconFile("template.novel", "..").isEmpty());
    CHECK(PackageCard().iconFile("template.novel", "twotone").isEmpty());
}

TEST_CASE("Package cards: a book whose type the program does not have opens read-only",
          "[core][package_cards]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();
    const Novel novel(dir.path());
    const std::vector<std::string> files = allFilesOf(novel.projectDir);
    REQUIRE(std::any_of(files.begin(), files.end(), [](const std::string& file) {
        return file.rfind("project.db ", 0) == 0;
    }));

    // Another computer: the program has no novel, nor the collection of stories that uses it
    installed.remove({"kalahari.novel", "kalahari.short_stories"});

    QStringList problems;
    REQUIRE(pm.openProject(novel.manifest, &problems));
    CHECK(problems.isEmpty());
    CHECK(pm.isReadOnly());
    CHECK(pm.missingPackages() == QStringList{"kalahari.novel"});

    // Its database is read from a copy, where the program writes what is its own (the
    // statistics of writing, the places of the cursor); the book's folder has no lock
    ProjectDatabase* database = pm.getDatabase();
    REQUIRE(database != nullptr);
    CHECK(database->isOpen());
    const QString copyDir = database->projectPath();
    CHECK(QDir(copyDir) != QDir(novel.projectDir));
    database->setMetadata("test", "written");
    CHECK(database->getMetadata("test") == "written");
    CHECK_FALSE(QFileInfo::exists(QDir(novel.projectDir).filePath(".kalahari.lock")));

    // The book is named after its card of the type, and its text can be read
    const PackageCard* card = pm.packageCard("kalahari.novel");
    REQUIRE(card != nullptr);
    CHECK(card->name.text("pl") == "Powieść");
    CHECK_FALSE(card->iconFile("template.novel", "twotone").isEmpty());
    CHECK(pm.packageCard("kalahari.base") != nullptr);
    CHECK(pm.bookTypes().package("kalahari.novel") == nullptr);
    CHECK(pm.loadChapterContent(novel.chapter) == "<kml><p>Ciemna noc</p></kml>");
    const ProjectElement* prologue = pm.findElement(novel.prologue);
    REQUIRE(prologue != nullptr);
    CHECK(pm.formOf(*prologue) == ElementForm::Text);

    // Nothing changes it
    const KindRef chapterKind = pm.installedBookTypes().findKind("kalahari.base", "chapter");
    REQUIRE(chapterKind);
    CHECK(pm.addElement(chapterKind, "New", BookPlace::Main).isEmpty());
    CHECK(pm.addBookFile("Notes").isEmpty());
    CHECK_FALSE(pm.renameElement(novel.chapter, "Renamed"));
    CHECK_FALSE(pm.removeElement(novel.prologue).has_value());
    CHECK_FALSE(pm.moveElement(novel.prologue, 1));
    CHECK_FALSE(pm.setStatus(novel.chapter, "final"));
    CHECK_FALSE(pm.setNotes(novel.chapter, "A note"));
    CHECK_FALSE(pm.setSections(BookSections{}));
    pm.setChapterContent(novel.chapter, QStringLiteral("<kml><p>Changed</p></kml>"));
    CHECK(pm.loadChapterContent(novel.chapter) == "<kml><p>Ciemna noc</p></kml>");
    CHECK(pm.getDirtyElements().empty());
    CHECK_FALSE(pm.saveManifest());
    CHECK(pm.book()->mainElements.first().id == novel.prologue);
    REQUIRE(pm.closeProject(false));
    CHECK_FALSE(pm.isReadOnly());
    CHECK(pm.missingPackages().isEmpty());
    CHECK_FALSE(QDir(copyDir).exists());

    // Nothing was written into its folder: its .klh file, its chapters, its cards, its
    // database and its backups are as they were, and there is no new file (test of
    // 19 October: project.db and .backups appeared in such a book)
    CHECK(allFilesOf(novel.projectDir) == files);

    // A book without a database, as from another program, gets none
    REQUIRE(QFile::remove(QDir(novel.projectDir).filePath("project.db")));
    REQUIRE(QDir(QDir(novel.projectDir).filePath(".backups")).removeRecursively());
    const std::vector<std::string> withoutDatabase = allFilesOf(novel.projectDir);
    REQUIRE(pm.openProject(novel.manifest));
    CHECK(pm.isReadOnly());
    REQUIRE(pm.getDatabase() != nullptr);
    CHECK(pm.getDatabase()->isOpen());
    REQUIRE(pm.closeProject(true));
    CHECK(allFilesOf(novel.projectDir) == withoutDatabase);

    // The program has the novel again: the book can be changed
    installed.restore({"kalahari.novel", "kalahari.short_stories"});
    REQUIRE(pm.openProject(novel.manifest));
    CHECK_FALSE(pm.isReadOnly());
    CHECK(pm.renameElement(novel.chapter, "Renamed"));
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Package cards: a book opens read-only where the program has its type without a kind "
          "it uses",
          "[core][package_cards]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();
    const Novel novel(dir.path());

    // A version of the novel without the prologue, which the book has
    removeKind(installed.package("kalahari.novel"), "prologue");
    installed.load();
    REQUIRE(pm.installedBookTypes().package("kalahari.novel") != nullptr);

    REQUIRE(pm.openProject(novel.manifest));
    CHECK(pm.isReadOnly());
    CHECK(pm.missingPackages() == QStringList{"kalahari.novel"});
    CHECK_FALSE(pm.renameElement(novel.chapter, "Renamed"));
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Package cards: a newer installed package refreshes the card, an older one does not",
          "[core][package_cards]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();

    REQUIRE(pm.createProject(dir.path(), "Moja powieść", "Anna", "pl", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QString projectDir = pm.getProjectPath();

    // A style the book uses
    ParagraphStyle style;
    style.id = QStringLiteral("letter");
    style.name = QStringLiteral("Letter");
    style.baseStyle = QStringLiteral("normal");
    REQUIRE(pm.getDatabase() != nullptr);
    pm.getDatabase()->saveParagraphStyle(style);
    REQUIRE(pm.closeProject(false));
    CHECK(savedType(manifest) == "kalahari.novel 1.0");

    // A newer novel installed: the card and the .klh file name it, and the styles stay
    setVersion(installed.package("kalahari.novel"), "1.1");
    installed.load();
    REQUIRE(pm.openProject(manifest));
    CHECK_FALSE(pm.isReadOnly());
    CHECK(PackageCards::read(projectDir, "kalahari.novel")->version == "1.1");
    CHECK(PackageCards::read(projectDir, "kalahari.base")->version == "1.0");
    CHECK(pm.packageCard("kalahari.novel")->version == "1.1");
    CHECK(pm.project()->type->version == "1.1");
    const QList<ParagraphStyle> styles = pm.getDatabase()->getParagraphStyles();
    const auto letter = std::find_if(styles.cbegin(), styles.cend(),
                                     [](const ParagraphStyle& s) { return s.id == "letter"; });
    REQUIRE(letter != styles.cend());
    CHECK(letter->name == "Letter");
    CHECK(letter->baseStyle == "normal");
    REQUIRE(pm.closeProject(false));
    CHECK(savedType(manifest) == "kalahari.novel 1.1");
    CHECK(cardsOf(projectDir) == QStringList{"kalahari.base", "kalahari.novel"});

    // The older novel again: the newer card and version stay; the book works with the novel
    // the program has
    setVersion(installed.package("kalahari.novel"), "1.0");
    installed.load();
    const QByteArray card = contentsOf(
        QDir(PackageCards::folderOf(projectDir, "kalahari.novel")).filePath("card.json"));
    REQUIRE(pm.openProject(manifest));
    CHECK_FALSE(pm.isReadOnly());
    CHECK(pm.project()->type->version == "1.1");
    CHECK(pm.bookTypes().package("kalahari.novel")->version == "1.0");
    CHECK(contentsOf(QDir(PackageCards::folderOf(projectDir, "kalahari.novel"))
                         .filePath("card.json")) == card);
    REQUIRE(pm.closeProject(false));
    CHECK(savedType(manifest) == "kalahari.novel 1.1");
}

TEST_CASE("Package cards: a book gets its cards when it opens without them",
          "[core][package_cards]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();
    REQUIRE(pm.createProject(dir.path(), "Old", "Anna", "en", true, "kalahari.novel"));
    const QString manifest = pm.getManifestPath();
    const QString projectDir = pm.getProjectPath();
    REQUIRE(pm.closeProject(false));
    const QString novel = PackageCards::folderOf(projectDir, "kalahari.novel");
    const QStringList novelFiles = filesOf(novel);

    SECTION("A book made before the cards") {
        REQUIRE(QDir(PackageCards::folderOf(projectDir)).removeRecursively());
        REQUIRE(pm.openProject(manifest));
        CHECK(cardsOf(projectDir) == QStringList{"kalahari.base", "kalahari.novel"});
        CHECK(filesOf(novel) == novelFiles);
        CHECK(pm.packageCard("kalahari.novel") != nullptr);
        REQUIRE(pm.closeProject(false));
    }

    SECTION("A card that cannot be read") {
        writeFile(QDir(novel).filePath("card.json"), "{ not a card");
        REQUIRE_FALSE(PackageCards::read(projectDir, "kalahari.novel").has_value());

        REQUIRE(pm.openProject(manifest));
        REQUIRE(PackageCards::read(projectDir, "kalahari.novel").has_value());
        CHECK(filesOf(novel) == novelFiles);
        REQUIRE(pm.closeProject(false));
    }

    SECTION("A whole copy of the package, as an earlier version kept it: a card in its place") {
        REQUIRE(QDir(novel).removeRecursively());
        copyFolder(installed.package("kalahari.novel"), novel);
        REQUIRE(QFileInfo::exists(QDir(novel).filePath("booktype.json")));

        REQUIRE(pm.openProject(manifest));
        CHECK(filesOf(novel) == novelFiles);
        REQUIRE(pm.closeProject(false));
    }

    SECTION("Cards that cannot be written: the book opens as it is") {
        REQUIRE(QDir(PackageCards::folderOf(projectDir)).removeRecursively());
        writeFile(PackageCards::folderOf(projectDir), "not a folder");

        REQUIRE(pm.openProject(manifest));
        CHECK_FALSE(pm.isReadOnly());
        CHECK(pm.packageCard("kalahari.novel") == nullptr);
        CHECK_FALSE(pm.kindsFor(BookPlace::Main).isEmpty());
        REQUIRE(pm.closeProject(false));
    }
}

TEST_CASE("Package cards: the archive of a book takes them to another computer",
          "[core][package_cards][archive]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();
    REQUIRE(pm.createProject(dir.path(), "Travels", "Anna", "en", true, "kalahari.novel"));
    const QString archive = QDir(dir.path()).filePath("Travels.klh.zip");
    REQUIRE(pm.exportArchive(archive));
    REQUIRE(pm.closeProject(false));
    const QStringList entries = entriesOf(archive);
    CHECK(entries.contains("types/kalahari.novel/card.json"));
    CHECK(entries.contains("types/kalahari.novel/icons/twotone/template.novel.svg"));
    CHECK(entries.contains("types/kalahari.base/card.json"));
    CHECK(std::none_of(entries.cbegin(), entries.cend(), [](const QString& entry) {
        return entry.endsWith(QLatin1String("booktype.json"));
    }));

    // Imported where the program has no novel: read-only, named after its card
    installed.remove({"kalahari.novel", "kalahari.short_stories"});
    const QString target = QDir(dir.path()).filePath("elsewhere");
    REQUIRE(pm.importArchive(archive, target));
    CHECK(pm.isReadOnly());
    REQUIRE(pm.packageCard("kalahari.novel") != nullptr);
    CHECK(pm.packageCard("kalahari.novel")->name.text("en") == "Novel");

    // A read-only book can be exported
    const QString again = QDir(dir.path()).filePath("Again.klh.zip");
    CHECK(pm.exportArchive(again));
    CHECK(entriesOf(again).contains("types/kalahari.novel/card.json"));
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("Package cards: the cards of the book of an archive are unpacked before the import",
          "[core][package_cards][archive]") {
    const Installed installed;
    QTemporaryDir dir;
    auto& pm = ProjectManager::getInstance();
    REQUIRE(pm.createProject(dir.path(), "Travels", "Anna", "en", true, "kalahari.novel"));
    const QString projectDir = pm.getProjectPath();
    const QString archive = QDir(dir.path()).filePath("Travels.klh.zip");
    REQUIRE(pm.exportArchive(archive));
    REQUIRE(pm.closeProject(false));

    // Only the cards, as the folder of the book has them
    QTemporaryDir unpacked;
    REQUIRE(unpacked.isValid());
    REQUIRE(ProjectManager::unpackArchiveCards(archive, unpacked.path()));
    CHECK(filesOf(unpacked.path()) == filesOf(projectDir).filter(QStringLiteral("types/")));
    CHECK(contentsOf(QDir(unpacked.path()).filePath("types/kalahari.novel/card.json")) ==
          contentsOf(QDir(projectDir).filePath("types/kalahari.novel/card.json")));
    const std::optional<BookProject> project = ProjectManager::readArchive(archive);
    REQUIRE(project.has_value());
    const QHash<QString, PackageCard> cards = PackageCards::readAll(unpacked.path(), *project);
    CHECK(cards.size() == 2);
    REQUIRE(cards.contains("kalahari.novel"));
    CHECK(cards.value("kalahari.novel").kind("prologue") != nullptr);

    // An archive without cards, of an earlier version, unpacks nothing
    QTemporaryDir empty;
    REQUIRE(empty.isValid());
    const QString earlier = QDir(dir.path()).filePath("Earlier.klh.zip");
    int error = 0;
    zip_t* zip = zip_open(QFile::encodeName(earlier).constData(), ZIP_CREATE | ZIP_TRUNCATE,
                          &error);
    REQUIRE(zip != nullptr);
    const QByteArray klh = contentsOf(QDir(projectDir).filePath("Travels.klh"));
    zip_source_t* source =
        zip_source_buffer(zip, klh.constData(), static_cast<zip_uint64_t>(klh.size()), 0);
    REQUIRE(source != nullptr);
    REQUIRE(zip_file_add(zip, "Travels.klh", source, ZIP_FL_ENC_UTF_8) >= 0);
    REQUIRE(zip_close(zip) == 0);
    CHECK(ProjectManager::unpackArchiveCards(earlier, empty.path()));
    CHECK(filesOf(empty.path()).isEmpty());

    // A file that is not an archive
    CHECK_FALSE(ProjectManager::unpackArchiveCards(QDir(projectDir).filePath("Travels.klh"),
                                                   empty.path()));
}
