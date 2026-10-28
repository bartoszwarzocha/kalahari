/// @file test_custom_book_types.cpp
/// @brief The writer's own book types and the kinds a book takes from other types: a type is
/// saved and read as it is, a new book of it gets its kinds, and Project Properties gives an
/// open book new kinds

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/custom_book_type.h>
#include <kalahari/core/package_card.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/gui/icon_registrar.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUuid>

using namespace kalahari::core;

namespace {

const QString BUILT_IN = QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes");

const BookTypeRegistry& installed() {
    return ProjectManager::getInstance().installedBookTypes();
}

KindRef kindOf(const char* package, const char* kind) {
    const KindRef found = installed().findKind(QString::fromLatin1(package),
                                               QString::fromLatin1(kind));
    REQUIRE(found);
    return found;
}

KindReference referenceOf(const char* package, const char* kind) {
    return KindReference{QString::fromLatin1(package), QString::fromLatin1(kind)};
}

/// The references of @p kinds, in their order
QStringList referencesOf(const QList<KindRef>& kinds) {
    QStringList references;
    for (const KindRef& kind : kinds) {
        references << kind.reference();
    }
    return references;
}

QStringList referencesOf(const QList<KindReference>& kinds) {
    QStringList references;
    for (const KindReference& kind : kinds) {
        references << kind.toString();
    }
    return references;
}

void writeFile(const QString& path, const QByteArray& contents) {
    REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(contents);
}

QJsonObject jsonOf(const QString& path) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QJsonDocument::fromJson(file.readAll()).object();
}

/// Folders of the cards of the book in @p projectDir, sorted
QStringList cardsOf(const QString& projectDir) {
    return QDir(PackageCards::folderOf(projectDir))
        .entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
}

/// A new book in @p parentDir, open: of the type @p typeId, or a user project with the kinds
/// of the base package
NewProject newBook(const QString& parentDir, const QString& title, const QString& typeId) {
    NewProject request;
    request.parentDir = parentDir;
    request.title = title;
    request.author = QStringLiteral("Anna");
    request.language = QStringLiteral("en");
    request.typeId = typeId;
    return request;
}

/// Counts the signals ProjectManager::projectKindsChanged() while it is there
struct KindChanges {
    int count = 0;
    QMetaObject::Connection connection;

    KindChanges() {
        connection = QObject::connect(&ProjectManager::getInstance(),
                                      &ProjectManager::projectKindsChanged,
                                      [this]() { ++count; });
    }
    ~KindChanges() { QObject::disconnect(connection); }
    KindChanges(const KindChanges&) = delete;
    KindChanges& operator=(const KindChanges&) = delete;
};

/// The packages of the program load again when it goes, whatever a test loaded
struct BuiltInAgain {
    BuiltInAgain() = default;
    ~BuiltInAgain() { ProjectManager::getInstance().loadBookTypes({BUILT_IN}); }
    BuiltInAgain(const BuiltInAgain&) = delete;
    BuiltInAgain& operator=(const BuiltInAgain&) = delete;
};

}  // namespace

TEST_CASE("Own book types: a type is saved and read as it is", "[core][custom_types]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString folder = dir.filePath(QStringLiteral("booktypes"));

    CustomBookType type;
    type.name = QStringLiteral("  Historical Novel ");
    type.description = QStringLiteral("A novel with a bibliography.");
    type.typeId = QStringLiteral("kalahari.novel");
    type.kinds = {referenceOf("kalahari.nonfiction", "bibliography"),
                  referenceOf("kalahari.nonfiction", "annex")};
    QString problem;
    REQUIRE(CustomBookTypes::save(type, folder, &problem));
    CHECK(problem.isEmpty());

    // A new id names its file in the folder, made for it
    CHECK_FALSE(QUuid::fromString(type.id).isNull());
    CHECK(type.name == QStringLiteral("Historical Novel"));
    CHECK(QFileInfo(type.file) == QFileInfo(QDir(folder).filePath(type.id + ".json")));
    const QJsonObject json = jsonOf(type.file);
    CHECK(json.value("format").toInt() == 1);
    CHECK(json.value("type").toString() == "kalahari.novel");
    CHECK(json.value("kinds").toArray() ==
          QJsonArray{"kalahari.nonfiction:bibliography", "kalahari.nonfiction:annex"});

    // It is read as it was saved
    const QList<CustomBookType> types = CustomBookTypes::load(folder);
    REQUIRE(types.size() == 1);
    CHECK(types.first() == type);

    // Saved again under its id, it replaces its file
    type.description.clear();
    REQUIRE(CustomBookTypes::save(type, folder));
    REQUIRE(CustomBookTypes::load(folder).size() == 1);
    CHECK(CustomBookTypes::load(folder).first().description.isEmpty());
    CHECK_FALSE(jsonOf(type.file).contains("description"));

    // Found by its name, whatever its case and the spaces around it
    CHECK(CustomBookTypes::named(types, QStringLiteral(" historical NOVEL ")) != nullptr);
    CHECK(CustomBookTypes::named(types, QStringLiteral("Novel")) == nullptr);

    // Deleted, it is gone
    REQUIRE(CustomBookTypes::remove(type));
    CHECK_FALSE(QFileInfo::exists(type.file));
    CHECK(CustomBookTypes::load(folder).isEmpty());
    CHECK_FALSE(CustomBookTypes::remove(CustomBookType{}));

    // A type without a name is not saved
    CustomBookType nameless;
    nameless.kinds = {referenceOf("kalahari.base", "chapter")};
    CHECK_FALSE(CustomBookTypes::save(nameless, folder, &problem));
    CHECK_FALSE(problem.isEmpty());
}

TEST_CASE("Own book types: the folder lists them by name, and leaves out files it cannot read",
          "[core][custom_types]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString folder = dir.path();

    for (const char* name : {"c", "A", "b"}) {
        CustomBookType type;
        type.name = QString::fromLatin1(name);
        type.kinds = {referenceOf("kalahari.base", "chapter")};
        REQUIRE(CustomBookTypes::save(type, folder));
    }
    // Not JSON, a format of a later version, no name, no kinds without a type, and fields
    // this version does not know, which are kept
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    writeFile(QDir(folder).filePath("broken.json"), "{ not json");
    writeFile(QDir(folder).filePath("later.json"),
              QJsonDocument(QJsonObject{{"format", 2}, {"id", id}, {"name", "Later"}}).toJson());
    writeFile(QDir(folder).filePath("nameless.json"),
              QJsonDocument(QJsonObject{{"format", 1},
                                        {"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                        {"type", "kalahari.novel"}})
                  .toJson());
    writeFile(QDir(folder).filePath("empty.json"),
              QJsonDocument(QJsonObject{{"format", 1},
                                        {"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                        {"name", "Empty"}})
                  .toJson());
    const QString future = QDir(folder).filePath(id + ".json");
    writeFile(future, QJsonDocument(QJsonObject{{"format", 1},
                                                {"id", id},
                                                {"name", "Future"},
                                                {"type", "kalahari.poetry"},
                                                {"tools", QJsonArray{"rhymes"}}})
                          .toJson());

    QList<CustomBookType> types = CustomBookTypes::load(folder);
    QStringList names;
    for (const CustomBookType& type : types) {
        names << type.name;
    }
    // The case does not count
    CHECK(names == QStringList{"A", "b", "c", "Future"});

    // The type with fields this version does not know saves them back
    CustomBookType& kept = types.last();
    REQUIRE(kept.name == "Future");
    kept.description = QStringLiteral("Poems");
    REQUIRE(CustomBookTypes::save(kept, folder));
    CHECK(jsonOf(future).value("tools").toArray() == QJsonArray{"rhymes"});
    CHECK(jsonOf(future).value("description").toString() == "Poems");
}

TEST_CASE("Own book types: what is wrong with a file of a type", "[core][custom_types]") {
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto problemsOf = [](const QJsonObject& json) {
        QStringList problems;
        CHECK_FALSE(CustomBookType::fromJson(json, problems).has_value());
        return problems.join(QLatin1Char('\n')).toStdString();
    };
    CHECK(problemsOf({{"format", 1}, {"id", "seven"}, {"name", "A"}, {"type", "kalahari.novel"}})
              .find("id:") != std::string::npos);
    CHECK(problemsOf({{"format", 1}, {"id", id}, {"name", "A"}, {"type", "not a package"}})
              .find("type:") != std::string::npos);
    CHECK(problemsOf({{"format", 1}, {"id", id}, {"name", "A"}, {"kinds", QJsonArray{"chapter"}}})
              .find("kinds[0]:") != std::string::npos);
    CHECK(problemsOf({{"format", 1}, {"id", id}, {"name", "A"}})
              .find("kinds:") != std::string::npos);
    CHECK(problemsOf({{"format", 1},
                      {"id", id},
                      {"name", "A"},
                      {"kinds", QJsonArray{"kalahari.base:chapter"}},
                      {"start", QJsonArray{QJsonObject{{"kind", "kalahari.base:chapter"}}}}})
              .find("start[0]:") != std::string::npos);

    // The same kind twice is one kind
    QStringList problems;
    const std::optional<CustomBookType> type = CustomBookType::fromJson(
        {{"format", 1},
         {"id", id},
         {"name", "A"},
         {"kinds", QJsonArray{"kalahari.base:chapter", "kalahari.base:chapter"}},
         {"start", QJsonArray{QJsonObject{{"kind", "kalahari.base:chapter"}, {"place", "main"}}}}},
        problems);
    REQUIRE(type.has_value());
    CHECK(problems.isEmpty());
    CHECK(referencesOf(type->kinds) == QStringList{"kalahari.base:chapter"});
    REQUIRE(type->start.size() == 1);
    CHECK(type->start.first().place == BookPlace::Main);
}

TEST_CASE("Own book types: what a type gives a new book", "[core][custom_types]") {
    SECTION("A type based on a book type takes the kinds it names, of the packages there are") {
        CustomBookType type;
        type.typeId = QStringLiteral("kalahari.novel");
        type.kinds = {referenceOf("kalahari.nonfiction", "bibliography"),
                      referenceOf("kalahari.nowhere", "thing")};
        CHECK(referencesOf(type.kindsIn(installed())) ==
              QStringList{"kalahari.nonfiction:bibliography"});
        CHECK(type.missingKinds(installed()) == QStringList{"kalahari.nowhere:thing"});

        NewProject request;
        request.palette = PaletteChoice{};
        type.applyTo(request, installed());
        CHECK(request.typeId == "kalahari.novel");
        CHECK(referencesOf(request.takenKinds) == QStringList{"kalahari.nonfiction:bibliography"});
        CHECK_FALSE(request.palette.has_value());

        // A novel starts with its title page and its first chapter
        QStringList start;
        for (const StartElement& element : type.startElementsIn(installed())) {
            start << element.kind.reference();
        }
        CHECK(start == QStringList{"kalahari.base:title_page", "kalahari.base:chapter"});
    }

    SECTION("A type without a book type is a user project with its kinds and elements") {
        PaletteChoice choice;
        choice.kinds = {kindOf("kalahari.base", "chapter"), kindOf("kalahari.base", "part"),
                        kindOf("kalahari.poetry", "poem")};
        choice.elements = {StartElement{kindOf("kalahari.base", "part"), BookPlace::Main},
                           StartElement{kindOf("kalahari.base", "chapter"), BookPlace::Main}};
        CustomBookType type = CustomBookType::of(choice);
        CHECK(type.isUserProject());
        CHECK(referencesOf(type.kinds) ==
              QStringList{"kalahari.base:chapter", "kalahari.base:part", "kalahari.poetry:poem"});
        // An element of a kind it does not name, and one in a place its kind cannot be, are
        // left out of the book
        type.start.append(
            CustomStartElement{referenceOf("kalahari.base", "title_page"), BookPlace::Front});
        type.start.append(
            CustomStartElement{referenceOf("kalahari.base", "chapter"), BookPlace::Workshop});

        NewProject request;
        request.typeId = QStringLiteral("kalahari.novel");
        request.takenKinds = {kindOf("kalahari.nonfiction", "annex")};
        type.applyTo(request, installed());
        CHECK(request.typeId.isEmpty());
        CHECK(request.takenKinds.isEmpty());
        REQUIRE(request.palette.has_value());
        CHECK(referencesOf(request.palette->kinds) == referencesOf(choice.kinds));
        REQUIRE(request.palette->elements.size() == 2);
        CHECK(request.palette->elements.at(0).kind.reference() == "kalahari.base:part");
        CHECK(request.palette->elements.at(1).kind.reference() == "kalahari.base:chapter");
    }

    SECTION("The kinds of a book: its type and the kinds it took, or the kinds of a user "
            "project with an element of its main text kind") {
        BookProject novel;
        novel.type = ProjectType{QStringLiteral("kalahari.novel"), QStringLiteral("1.0"), {}};
        novel.kinds = {referenceOf("kalahari.nonfiction", "bibliography")};
        const CustomBookType ofNovel = CustomBookType::of(novel, installed());
        CHECK(ofNovel.typeId == "kalahari.novel");
        CHECK(ofNovel.kinds == novel.kinds);
        CHECK(ofNovel.start.isEmpty());

        BookProject notes;
        notes.kinds = {referenceOf("kalahari.base", "work_note"),
                       referenceOf("kalahari.poetry", "poem"),
                       referenceOf("kalahari.base", "chapter")};
        const CustomBookType ofNotes = CustomBookType::of(notes, installed());
        CHECK(ofNotes.isUserProject());
        REQUIRE(ofNotes.start.size() == 1);
        CHECK(ofNotes.start.first().kind.toString() == "kalahari.poetry:poem");
        CHECK(ofNotes.start.first().place == BookPlace::Main);
    }
}

TEST_CASE("Kinds of other types: a new book of a type takes them", "[core][custom_types]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    kalahari::gui::registerAllIcons();  // the cards take the icons along
    auto& pm = ProjectManager::getInstance();

    // A kind of the type itself is no kind taken
    NewProject request = newBook(dir.path(), QStringLiteral("Desert"), "kalahari.novel");
    request.takenKinds = {kindOf("kalahari.nonfiction", "bibliography"),
                          kindOf("kalahari.base", "chapter")};
    CHECK(pm.newProjectProblems(request).isEmpty());
    REQUIRE(pm.createProject(request));
    REQUIRE(pm.project() != nullptr);
    CHECK(referencesOf(pm.project()->kinds) == QStringList{"kalahari.nonfiction:bibliography"});
    CHECK(jsonOf(pm.getManifestPath()).value("kinds").toArray() ==
          QJsonArray{"kalahari.nonfiction:bibliography"});
    // The book has the card of the package of the kind, and offers the kind after its own
    CHECK(cardsOf(pm.getProjectPath()) ==
          QStringList{"kalahari.base", "kalahari.nonfiction", "kalahari.novel"});
    const QStringList back = referencesOf(pm.kindsFor(BookPlace::Back));
    REQUIRE_FALSE(back.isEmpty());
    CHECK(back.last() == "kalahari.nonfiction:bibliography");
    REQUIRE(pm.closeProject(false));

    // A user project has its kinds in the palette; a kind that is not there is no kind
    NewProject user = newBook(dir.path(), QStringLiteral("Notes"), QString());
    user.takenKinds = {kindOf("kalahari.nonfiction", "bibliography")};
    CHECK_FALSE(pm.newProjectProblems(user).isEmpty());
    NewProject missing = newBook(dir.path(), QStringLiteral("Missing"), "kalahari.novel");
    missing.takenKinds = {KindRef{}};
    CHECK_FALSE(pm.newProjectProblems(missing).isEmpty());
    CHECK_FALSE(pm.createProject(missing));
}

TEST_CASE("Kinds of other types: Project Properties gives an open book new kinds",
          "[core][custom_types]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    kalahari::gui::registerAllIcons();
    auto& pm = ProjectManager::getInstance();
    const KindChanges kindChanges;
    const int& changes = kindChanges.count;

    SECTION("A book of a type: the kinds of other types, saved at once") {
        REQUIRE(pm.createProject(newBook(dir.path(), QStringLiteral("Desert"), "kalahari.novel")));
        CHECK(cardsOf(pm.getProjectPath()) == QStringList{"kalahari.base", "kalahari.novel"});

        // The kinds of the type are its kinds in any case
        REQUIRE(pm.setProjectKinds({kindOf("kalahari.base", "chapter"),
                                    kindOf("kalahari.nonfiction", "annex"),
                                    kindOf("kalahari.poetry", "poem")}));
        CHECK(referencesOf(pm.project()->kinds) ==
              QStringList{"kalahari.nonfiction:annex", "kalahari.poetry:poem"});
        CHECK(changes == 1);
        CHECK(jsonOf(pm.getManifestPath()).value("kinds").toArray() ==
              QJsonArray{"kalahari.nonfiction:annex", "kalahari.poetry:poem"});
        CHECK(cardsOf(pm.getProjectPath()) ==
              QStringList{"kalahari.base", "kalahari.nonfiction", "kalahari.novel",
                          "kalahari.poetry"});
        CHECK(pm.packageCard("kalahari.poetry") != nullptr);

        // The same kinds change nothing
        REQUIRE(pm.setProjectKinds({kindOf("kalahari.nonfiction", "annex"),
                                    kindOf("kalahari.poetry", "poem")}));
        CHECK(changes == 1);

        // Without the kinds of other types the book has the kinds of its type
        REQUIRE(pm.setProjectKinds({}));
        CHECK(pm.project()->kinds.isEmpty());
        CHECK(changes == 2);

        // The book opens with its kinds
        REQUIRE(pm.setProjectKinds({kindOf("kalahari.nonfiction", "bibliography")}));
        const QString manifest = pm.getManifestPath();
        REQUIRE(pm.closeProject(false));
        REQUIRE(pm.openProject(manifest));
        CHECK(referencesOf(pm.project()->kinds) ==
              QStringList{"kalahari.nonfiction:bibliography"});
        REQUIRE(pm.closeProject(false));
    }

    SECTION("A user project: all its kinds, one at least") {
        REQUIRE(pm.createProject(newBook(dir.path(), QStringLiteral("Notes"), QString())));
        const QList<KindReference> before = pm.project()->kinds;
        REQUIRE_FALSE(before.isEmpty());
        CHECK_FALSE(pm.setProjectKinds({}));
        CHECK(pm.project()->kinds == before);
        CHECK(changes == 0);

        REQUIRE(pm.setProjectKinds({kindOf("kalahari.base", "chapter"),
                                    kindOf("kalahari.poetry", "poem")}));
        CHECK(referencesOf(pm.project()->kinds) ==
              QStringList{"kalahari.base:chapter", "kalahari.poetry:poem"});
        CHECK(changes == 1);
        REQUIRE(pm.closeProject(false));
    }

    SECTION("No book, and a kind that is not installed") {
        CHECK_FALSE(pm.setProjectKinds({kindOf("kalahari.poetry", "poem")}));
        REQUIRE(pm.createProject(newBook(dir.path(), QStringLiteral("Desert"), "kalahari.novel")));
        CHECK_FALSE(pm.setProjectKinds({KindRef{}}));
        CHECK(pm.project()->kinds.isEmpty());
        CHECK(changes == 0);
        REQUIRE(pm.closeProject(false));
    }
}

TEST_CASE("Kinds of other types: a book open read-only keeps its kinds",
          "[core][custom_types]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    kalahari::gui::registerAllIcons();
    auto& pm = ProjectManager::getInstance();
    const BuiltInAgain builtIn;

    // The packages of the program in a folder of the test, which loses the novel later
    const QString packages = dir.filePath(QStringLiteral("booktypes"));
    QDirIterator it(BUILT_IN, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = QDir(BUILT_IN).relativeFilePath(it.next());
        const QString target = QDir(packages).filePath(file);
        REQUIRE(QDir().mkpath(QFileInfo(target).absolutePath()));
        REQUIRE(QFile::copy(QDir(BUILT_IN).filePath(file), target));
    }
    pm.loadBookTypes({packages});
    REQUIRE(pm.createProject(newBook(dir.path(), QStringLiteral("Desert"), "kalahari.novel")));
    const QString manifest = pm.getManifestPath();
    REQUIRE(pm.closeProject(false));

    for (const char* id : {"kalahari.novel", "kalahari.short_stories"}) {
        REQUIRE(QDir(QDir(packages).filePath(QString::fromLatin1(id))).removeRecursively());
    }
    pm.loadBookTypes({packages});
    REQUIRE(pm.openProject(manifest));
    REQUIRE(pm.isReadOnly());
    CHECK_FALSE(pm.setProjectKinds({kindOf("kalahari.poetry", "poem")}));
    CHECK(pm.project()->kinds.isEmpty());
    REQUIRE(pm.closeProject(false));
}
