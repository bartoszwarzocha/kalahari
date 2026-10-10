/// @file test_book_project.cpp
/// @brief Book project and its .klh file: every field, kinds of other types and of packages
/// that are not installed, files with problems and the example project

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/chapter_document.h>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>
#include <string>
#include <vector>

using namespace kalahari::core;

namespace {

QString builtInDirectory() {
    return QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes");
}

std::string joined(const QStringList& list) {
    return list.join(QStringLiteral(", ")).toStdString();
}

/// Kinds with their packages, "?" for a kind that was not found
std::string references(const QList<KindRef>& kinds) {
    QStringList list;
    for (const KindRef& ref : kinds) {
        list << (ref ? ref.reference() : QStringLiteral("?"));
    }
    return joined(list);
}

void loadBuiltIn(BookTypeRegistry& registry) {
    registry.load({builtInDirectory()});
    REQUIRE(registry.problems().isEmpty());
}

QJsonObject jsonOf(const QByteArray& text) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(text, &error);
    INFO(error.errorString().toStdString());
    REQUIRE(document.isObject());
    return document.object();
}

/// Project of @p text, which must have no problems
BookProject projectOf(const QByteArray& text) {
    QStringList problems;
    std::optional<BookProject> project = BookProject::fromJson(jsonOf(text), problems);
    INFO(joined(problems));
    REQUIRE(project.has_value());
    REQUIRE(problems.isEmpty());
    return *project;
}

QDateTime utc(int day, int hour, int minute, int second = 0) {
    return QDateTime(QDate(2026, 10, day), QTime(hour, minute, second), QTimeZone::utc());
}

/// A novel with a kind of literature non-fiction, with every field the program knows
const char* const NOVEL = R"({
  "format": 2,
  "id": "5f0c3a52-8d1e-4f7a-9b20-6c1d2e3f4a5b",
  "created": "2026-10-09T10:00:00Z",
  "modified": "2026-10-09T12:30:00Z",
  "type": { "id": "kalahari.novel", "version": "1.0" },
  "kinds": ["kalahari.nonfiction:bibliography"],
  "books": [
    {
      "id": "c2a1", "title": "My Novel", "author": "Anna Nowak", "language": "pl",
      "genre": "saga", "name": "Text", "partsLayer": false, "sections": "matter",
      "folder": "book",
      "front": [
        { "id": "e1", "kind": "kalahari.base:title_page", "title": "Title page",
          "file": "book/title_page_001.kchapter", "status": "draft" }
      ],
      "main": [
        { "id": "e2", "kind": "kalahari.base:part", "title": "Part One",
          "elements": [
            { "id": "e3", "kind": "kalahari.base:chapter", "title": "Chapter 1",
              "file": "book/chapter_001.kchapter", "status": "revision" }
          ] }
      ],
      "back": [
        { "id": "e5", "kind": "kalahari.nonfiction:bibliography", "title": "Bibliography",
          "file": "book/bibliography_001.json", "status": "final" }
      ]
    }
  ],
  "workshop": {
    "name": "Notebook", "grouped": true,
    "elements": [
      { "id": "e4", "kind": "kalahari.base:character", "title": "Anna",
        "file": "workshop/character_001.json" }
    ]
  }
})";

ProjectElement element(const char* id, const char* packageId, const char* kindId,
                       const char* title) {
    ProjectElement result;
    result.id = QString::fromLatin1(id);
    result.kind = {QString::fromLatin1(packageId), QString::fromLatin1(kindId)};
    result.title = QString::fromLatin1(title);
    return result;
}

/// A series of two books with every field set, also fields the program does not know
BookProject fullProject() {
    ProjectElement titlePage = element("e1", "kalahari.base", "title_page", "Title page");
    titlePage.file = QStringLiteral("book/title_page_001.kchapter");
    titlePage.status = QStringLiteral("final");

    ProjectElement motto = element("e2", "kalahari.base", "motto", "Motto");
    motto.file = QStringLiteral("book/motto_001.kchapter");
    motto.status = QStringLiteral("draft");
    ProjectElement chapter = element("e3", "kalahari.base", "chapter", "Chapter 1");
    chapter.file = QStringLiteral("book/chapter_001.kchapter");
    chapter.status = QStringLiteral("revision");
    chapter.extra.insert(QStringLiteral("pov"), QStringLiteral("Anna"));
    ProjectElement part = element("e4", "kalahari.base", "part", "Part One");
    part.elements = {motto, chapter};

    ProjectElement bibliography =
        element("e5", "kalahari.nonfiction", "bibliography", "Bibliography");
    bibliography.file = QStringLiteral("book/bibliography_001.json");

    ProjectBook first;
    first.id = QStringLiteral("b1");
    first.title = QStringLiteral("My Novel");
    first.author = QStringLiteral("Anna Nowak");
    first.language = QStringLiteral("pl");
    first.genre = QStringLiteral("saga");
    first.name = QStringLiteral("Text");
    first.partsLayer = false;
    first.sectionSet = QString::fromLatin1(ProjectBook::CUSTOM_SECTIONS);
    first.sectionNames = {QStringLiteral("Przedmowa i wstęp"), QStringLiteral("Opowieść"),
                          QStringLiteral("Dodatki")};
    first.sectionNamesExtra.insert(QStringLiteral("workshop"), QStringLiteral("Notatki"));
    first.folder = QStringLiteral("book");
    first.frontElements = {titlePage};
    first.mainElements = {part};
    first.backElements = {bibliography};
    first.extra.insert(QStringLiteral("cover"), QStringLiteral("cover.jpg"));

    ProjectElement nextChapter = element("e6", "kalahari.base", "chapter", "Chapter 1");
    nextChapter.file = QStringLiteral("book2/chapter_001.kchapter");
    nextChapter.status = QStringLiteral("draft");

    ProjectBook second;
    second.id = QStringLiteral("b2");
    second.title = QStringLiteral("My Novel II");
    second.author = QStringLiteral("Anna Nowak");
    second.language = QStringLiteral("en");
    second.sectionSet = QStringLiteral("arc");
    second.folder = QStringLiteral("book2");
    second.mainElements = {nextChapter};

    ProjectElement character = element("e7", "kalahari.base", "character", "Anna");
    character.file = QStringLiteral("workshop/character_001.json");
    ProjectElement note = element("e8", "kalahari.base", "work_note", "Ideas");
    note.file = QStringLiteral("workshop/work_note_001.kchapter");
    note.status = QStringLiteral("draft");

    BookProject project;
    project.id = QStringLiteral("5f0c3a52-8d1e-4f7a-9b20-6c1d2e3f4a5b");
    project.created = utc(9, 10, 0);
    project.modified = utc(9, 12, 30, 15);
    project.type = ProjectType{QStringLiteral("kalahari.novel"), QStringLiteral("1.0"), {}};
    project.type->extra.insert(QStringLiteral("channel"), QStringLiteral("beta"));
    project.kinds = {{QStringLiteral("kalahari.nonfiction"), QStringLiteral("bibliography")},
                     {QStringLiteral("kalahari.screenplay"), QStringLiteral("logline")}};
    project.books = {first, second};
    project.workshop.name = QStringLiteral("Notebook");
    project.workshop.grouped = true;
    project.workshop.elements = {character, note};
    project.workshop.extra.insert(QStringLiteral("sort"), QStringLiteral("title"));
    project.extra.insert(QStringLiteral("series"),
                         QJsonObject{{QStringLiteral("name"), QStringLiteral("Saga")}});
    return project;
}

/// Problems of @p text, one per line
std::string problemsOf(const QByteArray& text) {
    QStringList problems;
    const std::optional<BookProject> project = BookProject::fromJson(jsonOf(text), problems);
    CHECK_FALSE(project.has_value());
    return problems.join(QLatin1Char('\n')).toStdString();
}

/// A project whose only book has @p element in its main part
QByteArray withElement(const char* element) {
    return QStringLiteral(R"({ "format": 2, "id": "p", "books": [
                                 { "id": "b", "folder": "book", "main": [ %1 ] } ] })")
        .arg(QString::fromUtf8(element))
        .toUtf8();
}

}  // namespace

// =============================================================================
// Fields
// =============================================================================

TEST_CASE("Book project: a .klh file with every field", "[core][bookproject]") {
    const BookProject project = projectOf(NOVEL);

    CHECK(project.id == QStringLiteral("5f0c3a52-8d1e-4f7a-9b20-6c1d2e3f4a5b"));
    CHECK(project.created == utc(9, 10, 0));
    CHECK(project.modified == utc(9, 12, 30));
    REQUIRE(project.type.has_value());
    CHECK_FALSE(project.isUserProject());
    CHECK(project.type->id == QStringLiteral("kalahari.novel"));
    CHECK(project.type->version == QStringLiteral("1.0"));
    REQUIRE(project.kinds.size() == 1);
    CHECK(project.kinds.at(0).toString() == QStringLiteral("kalahari.nonfiction:bibliography"));

    REQUIRE(project.books.size() == 1);
    const ProjectBook& book = project.books.at(0);
    CHECK(book.id == QStringLiteral("c2a1"));
    CHECK(book.title == QStringLiteral("My Novel"));
    CHECK(book.author == QStringLiteral("Anna Nowak"));
    CHECK(book.language == QStringLiteral("pl"));
    CHECK(book.genre == QStringLiteral("saga"));
    CHECK(book.name == QStringLiteral("Text"));
    CHECK_FALSE(book.partsLayer);
    CHECK(book.sectionSet == QStringLiteral("matter"));
    CHECK(book.sectionNames.isEmpty());
    CHECK(book.sectionName(BookPlace::Main) == QStringLiteral("Tekst główny"));
    CHECK(book.folder == QStringLiteral("book"));

    REQUIRE(book.frontElements.size() == 1);
    const ProjectElement& titlePage = book.frontElements.at(0);
    CHECK(titlePage.id == QStringLiteral("e1"));
    CHECK(titlePage.kind.packageId == QStringLiteral("kalahari.base"));
    CHECK(titlePage.kind.kindId == QStringLiteral("title_page"));
    CHECK(titlePage.title == QStringLiteral("Title page"));
    CHECK(titlePage.file == QStringLiteral("book/title_page_001.kchapter"));
    CHECK(titlePage.status == QStringLiteral("draft"));
    CHECK(titlePage.elements.isEmpty());
    CHECK(titlePage.extra.isEmpty());

    REQUIRE(book.mainElements.size() == 1);
    const ProjectElement& part = book.mainElements.at(0);
    CHECK(part.kind.toString() == QStringLiteral("kalahari.base:part"));
    CHECK(part.file.isEmpty());
    CHECK(part.status.isEmpty());
    REQUIRE(part.elements.size() == 1);
    CHECK(part.elements.at(0).id == QStringLiteral("e3"));
    CHECK(part.elements.at(0).status == QStringLiteral("revision"));

    REQUIRE(book.backElements.size() == 1);
    CHECK(book.backElements.at(0).kind.toString() ==
          QStringLiteral("kalahari.nonfiction:bibliography"));
    CHECK(book.backElements.at(0).status == QStringLiteral("final"));

    CHECK(project.workshop.name == QStringLiteral("Notebook"));
    CHECK(project.workshop.grouped);
    REQUIRE(project.workshop.elements.size() == 1);
    CHECK(project.workshop.elements.at(0).file == QStringLiteral("workshop/character_001.json"));
    CHECK(project.workshop.elements.at(0).status.isEmpty());

    // Written back, the file has the same fields
    CHECK(project.toJson() == jsonOf(NOVEL));
}

TEST_CASE("Book project: a project is saved and read back unchanged", "[core][bookproject]") {
    const BookProject project = fullProject();

    QStringList problems;
    const std::optional<BookProject> fromJson = BookProject::fromJson(project.toJson(), problems);
    INFO(joined(problems));
    REQUIRE(fromJson.has_value());
    CHECK(*fromJson == project);

    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("My Novel.klh"));
    REQUIRE(project.save(path));
    const std::optional<BookProject> loaded = BookProject::load(path, problems);
    INFO(joined(problems));
    REQUIRE(loaded.has_value());
    CHECK(*loaded == project);

    // A field that changes makes another project
    BookProject changed = project;
    changed.books[0].mainElements[0].elements[1].status = QStringLiteral("final");
    CHECK_FALSE(changed == project);
    changed = project;
    changed.books[0].sectionNames[1] = QStringLiteral("Historia");
    CHECK_FALSE(changed == project);
}

TEST_CASE("Book project: fields this version does not know are kept", "[core][bookproject]") {
    const char* const text = R"({
      "format": 2, "id": "p", "series": { "name": "Saga" },
      "type": { "id": "kalahari.novel", "version": "1.0", "channel": "beta" },
      "kinds": [],
      "books": [
        { "id": "b", "title": "", "author": "", "language": "", "genre": "", "partsLayer": true,
          "sections": { "front": "Przedmowa", "main": "Opowieść", "back": "Dodatki",
                        "workshop": "Notatki" },
          "folder": "book", "cover": "cover.jpg", "front": [], "back": [],
          "main": [
            { "id": "e1", "kind": "kalahari.base:part", "title": "Part One", "pov": "Anna",
              "elements": [
                { "id": "e2", "kind": "kalahari.base:chapter", "title": "Chapter 1",
                  "words": 1200 }
              ] }
          ] }
      ],
      "workshop": { "grouped": false, "elements": [], "sort": "title" }
    })";
    const BookProject project = projectOf(text);

    const auto textOf = [](const QJsonObject& object, const char* key) {
        return object.value(QLatin1String(key)).toString();
    };
    CHECK(project.extra.keys() == QStringList{QStringLiteral("series")});
    CHECK(textOf(project.extra.value(QStringLiteral("series")).toObject(), "name") ==
          QStringLiteral("Saga"));
    CHECK(textOf(project.type->extra, "channel") == QStringLiteral("beta"));
    CHECK(textOf(project.books.at(0).extra, "cover") == QStringLiteral("cover.jpg"));
    CHECK(project.books.at(0).sectionNames ==
          QStringList{QStringLiteral("Przedmowa"), QStringLiteral("Opowieść"),
                      QStringLiteral("Dodatki")});
    CHECK(textOf(project.books.at(0).sectionNamesExtra, "workshop") == QStringLiteral("Notatki"));
    const ProjectElement& part = project.books.at(0).mainElements.at(0);
    CHECK(textOf(part.extra, "pov") == QStringLiteral("Anna"));
    CHECK(part.elements.at(0).extra.value(QStringLiteral("words")).toInt() == 1200);
    CHECK(textOf(project.workshop.extra, "sort") == QStringLiteral("title"));

    CHECK(project.toJson() == jsonOf(text));
}

// =============================================================================
// Elements
// =============================================================================

TEST_CASE("Book project: elements are found, added, moved and taken out",
          "[core][bookproject]") {
    BookProject project = projectOf(NOVEL);
    const auto order = [&project]() {
        QStringList ids;
        for (const ProjectElement* element : project.readingOrder()) {
            ids << element->id;
        }
        return joined(ids);
    };

    // The front, main and back parts with the elements inside groups; not the Workshop
    CHECK(order() == "e1, e2, e3, e5");
    CHECK(project.elementsIn(BookPlace::Front).at(0).id == QStringLiteral("e1"));
    CHECK(project.elementsIn(BookPlace::Back).at(0).id == QStringLiteral("e5"));
    CHECK(project.elementsIn(BookPlace::Workshop).at(0).id == QStringLiteral("e4"));

    REQUIRE(project.findElement(QStringLiteral("e3")) != nullptr);
    CHECK(project.findElement(QStringLiteral("e3"))->title == QStringLiteral("Chapter 1"));
    REQUIRE(project.findElement(QStringLiteral("e4")) != nullptr);
    CHECK(project.findElement(QStringLiteral("e4"))->kind.kindId == QStringLiteral("character"));
    CHECK(project.findElement(QStringLiteral("e9")) == nullptr);
    const BookProject& constProject = project;
    REQUIRE(constProject.findElement(QStringLiteral("e5")) != nullptr);
    CHECK(constProject.findElement(QStringLiteral("e5"))->status == QStringLiteral("final"));

    qsizetype index = -1;
    QList<ProjectElement>* list = project.listOf(QStringLiteral("e3"), &index);
    REQUIRE(list != nullptr);
    CHECK(list == &project.findElement(QStringLiteral("e2"))->elements);
    CHECK(index == 0);
    CHECK(project.listOf(QStringLiteral("e9")) == nullptr);

    // Added to a group and to a part
    project.findElement(QStringLiteral("e2"))
        ->elements.append(element("e6", "kalahari.base", "chapter", "Chapter 2"));
    project.elementsIn(BookPlace::Main)
        .append(element("e7", "kalahari.novel", "epilogue", "Epilogue"));
    CHECK(order() == "e1, e2, e3, e6, e7, e5");

    // Moved within their lists
    CHECK(project.moveElement(QStringLiteral("e6"), 0));
    CHECK(order() == "e1, e2, e6, e3, e7, e5");
    CHECK(project.moveElement(QStringLiteral("e7"), 0));
    CHECK(order() == "e1, e7, e2, e6, e3, e5");
    CHECK_FALSE(project.moveElement(QStringLiteral("e7"), 2));
    CHECK_FALSE(project.moveElement(QStringLiteral("e7"), -1));
    CHECK_FALSE(project.moveElement(QStringLiteral("e9"), 0));
    CHECK(order() == "e1, e7, e2, e6, e3, e5");

    // A group is taken out with the elements inside it
    const std::optional<ProjectElement> part = project.takeElement(QStringLiteral("e2"));
    REQUIRE(part.has_value());
    CHECK(part->elements.size() == 2);
    CHECK(order() == "e1, e7, e5");
    CHECK(project.findElement(QStringLiteral("e3")) == nullptr);
    CHECK_FALSE(project.takeElement(QStringLiteral("e2")).has_value());
    CHECK(project.takeElement(QStringLiteral("e4")).has_value());
    CHECK(project.workshop.elements.isEmpty());
}

TEST_CASE("Book project: places of elements, and the elements that open and close a part",
          "[core][bookproject]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);
    BookProject project = projectOf(R"({
      "format": 2, "id": "p", "type": { "id": "kalahari.novel", "version": "1.0" },
      "books": [ { "id": "b", "folder": "book",
        "front": [ { "id": "t", "kind": "kalahari.base:title_page", "title": "Title page" } ],
        "main": [
          { "id": "pro", "kind": "kalahari.novel:prologue", "title": "Prologue" },
          { "id": "p1", "kind": "kalahari.base:part", "title": "Part One", "elements": [
            { "id": "c1", "kind": "kalahari.base:chapter", "title": "Chapter 1" },
            { "id": "epi", "kind": "kalahari.novel:epilogue", "title": "Epilogue" } ] },
          { "id": "p2", "kind": "kalahari.base:part", "title": "Part Two" }
        ] } ],
      "workshop": { "elements": [
        { "id": "n", "kind": "kalahari.base:work_note", "title": "Ideas" } ] }
    })");
    const QList<ProjectElement>& body = project.elementsIn(BookPlace::Main);
    const auto ids = [](const QList<const ProjectElement*>& elements) {
        QStringList list;
        for (const ProjectElement* element : elements) {
            list << element->id;
        }
        return joined(list);
    };

    // In the part of the book or the Workshop, in the list of a group or of the part
    CHECK(project.placeOf(QStringLiteral("t")) == ElementPlace{BookPlace::Front, QString(), 0});
    CHECK(project.placeOf(QStringLiteral("p2")) == ElementPlace{BookPlace::Main, QString(), 2});
    CHECK(project.placeOf(QStringLiteral("epi")) ==
          ElementPlace{BookPlace::Main, QStringLiteral("p1"), 1});
    CHECK(project.placeOf(QStringLiteral("n")) ==
          ElementPlace{BookPlace::Workshop, QString(), 0});
    CHECK_FALSE(project.placeOf(QStringLiteral("x")).has_value());

    // The content of the body as the reader meets it, without the parts: the prologue opens
    // it and the epilogue at the end of Part One closes it, Part Two being empty
    CHECK(ids(BookProject::contentOf(registry, body)) == "pro, c1, epi");
    CHECK(ids(BookProject::openingElementsOf(registry, body)) == "pro");
    CHECK(ids(BookProject::closingElementsOf(registry, body)) == "epi");

    // Elements of the start and of the end inside groups, and at the end of the body
    project.findElement(QStringLiteral("p1"))
        ->elements.prepend(element("pro2", "kalahari.novel", "prologue", "Prologue II"));
    project.elementsIn(BookPlace::Main)
        .append(element("epi2", "kalahari.novel", "epilogue", "Epilogue II"));
    CHECK(ids(BookProject::openingElementsOf(registry, body)) == "pro, pro2");
    CHECK(ids(BookProject::closingElementsOf(registry, body)) == "epi, epi2");

    // A chapter after them: nothing closes the body; before them: nothing opens it
    project.findElement(QStringLiteral("p2"))
        ->elements.append(element("c2", "kalahari.base", "chapter", "Chapter 2"));
    CHECK(ids(BookProject::closingElementsOf(registry, body)) == "epi2");
    project.findElement(QStringLiteral("p2"))
        ->elements.append(element("c3", "kalahari.base", "chapter", "Chapter 3"));
    project.elementsIn(BookPlace::Main).removeLast();
    CHECK(ids(BookProject::closingElementsOf(registry, body)).empty());
    project.elementsIn(BookPlace::Main)
        .prepend(element("c0", "kalahari.base", "chapter", "Chapter 0"));
    CHECK(ids(BookProject::openingElementsOf(registry, body)).empty());

    // A text element of a kind that is not installed stands anywhere, like a chapter
    project.elementsIn(BookPlace::Main).removeFirst();
    ProjectElement prophecy = element("x", "addon.fantasy", "prophecy", "The Prophecy");
    prophecy.file = QStringLiteral("book/prophecy_001.kchapter");
    project.elementsIn(BookPlace::Main).prepend(prophecy);
    CHECK(ids(BookProject::contentOf(registry, body)) == "x, pro, pro2, c1, epi, c2, c3");
    CHECK(ids(BookProject::openingElementsOf(registry, body)).empty());

    // Lists without content
    CHECK(BookProject::contentOf(registry, {}).isEmpty());
    CHECK(BookProject::openingElementsOf(registry, {}).isEmpty());
    CHECK(BookProject::closingElementsOf(registry, {}).isEmpty());
}

TEST_CASE("Book project: names of the front, main and back part", "[core][bookproject]") {
    QStringList sets;
    for (const SectionNameSet& set : ProjectBook::sectionNameSets()) {
        sets << set.id;
    }
    CHECK(joined(sets) == "sections, matter, fragments, arc");

    ProjectBook book;
    const auto names = [&book]() {
        return joined({book.sectionName(BookPlace::Front), book.sectionName(BookPlace::Main),
                       book.sectionName(BookPlace::Back)});
    };

    // Without a set, the first one, in the language of the book; a language without names of
    // its own gets the English ones
    book.language = QStringLiteral("pl");
    CHECK(names() == "Sekcja początkowa, Sekcja główna, Sekcja końcowa");
    book.language = QStringLiteral("pl_PL");
    CHECK(names() == "Sekcja początkowa, Sekcja główna, Sekcja końcowa");
    book.language = QStringLiteral("en");
    CHECK(names() == "Front Section, Main Section, Back Section");
    book.language = QStringLiteral("de");
    CHECK(names() == "Front Section, Main Section, Back Section");

    struct Set {
        const char* id;
        const char* polish;
        const char* english;
    };
    const std::vector<Set> all = {
        {"sections", "Sekcja początkowa, Sekcja główna, Sekcja końcowa",
         "Front Section, Main Section, Back Section"},
        {"matter", "Strony początkowe, Tekst główny, Strony końcowe",
         "Front Matter, Body, Back Matter"},
        {"fragments", "Fragment początkowy, Fragment główny, Fragment końcowy",
         "Opening Fragment, Main Fragment, Closing Fragment"},
        {"arc", "Otwarcie, Rozwinięcie, Zamknięcie", "Opening, Development, Closing"},
    };
    for (const Set& set : all) {
        INFO(set.id);
        book.sectionSet = QString::fromLatin1(set.id);
        CHECK(book.sectionNameSet().id == book.sectionSet);
        book.language = QStringLiteral("pl");
        CHECK(names() == set.polish);
        book.language = QStringLiteral("en");
        CHECK(names() == set.english);
    }

    // A set this version does not know gives the first one
    book.sectionSet = QStringLiteral("acts");
    CHECK(book.sectionNameSet().id == QStringLiteral("sections"));
    CHECK(names() == "Front Section, Main Section, Back Section");

    // The writer's own names are the same in every language
    book.sectionSet = QString::fromLatin1(ProjectBook::CUSTOM_SECTIONS);
    book.sectionNames = {QStringLiteral("Przedmowa i wstęp"), QStringLiteral("Opowieść"),
                         QStringLiteral("Dodatki")};
    CHECK(names() == "Przedmowa i wstęp, Opowieść, Dodatki");
    book.language = QStringLiteral("pl");
    CHECK(names() == "Przedmowa i wstęp, Opowieść, Dodatki");
    CHECK(book.sectionNameSet().id == QStringLiteral("sections"));

    // The Workshop is no part of the book
    CHECK(book.sectionName(BookPlace::Workshop).isEmpty());
    book.sectionSet.clear();
    CHECK(book.sectionName(BookPlace::Workshop).isEmpty());
}

TEST_CASE("Book project: the names of the parts are saved and read back",
          "[core][bookproject]") {
    // The project of a book with "sections": @p sections, and the field as it is saved back
    const auto withSections = [](const char* sections) {
        return projectOf(QStringLiteral(R"({ "format": 2, "id": "p", "books": [
                                               { "id": "b", "folder": "book",
                                                 "sections": %1 } ] })")
                             .arg(QString::fromUtf8(sections))
                             .toUtf8());
    };
    const auto saved = [](const BookProject& project) {
        return project.toJson()
            .value(QStringLiteral("books"))
            .toArray()
            .at(0)
            .toObject()
            .value(QStringLiteral("sections"));
    };

    // A set
    const BookProject matter = withSections(R"("matter")");
    CHECK(matter.books.at(0).sectionSet == QStringLiteral("matter"));
    CHECK(saved(matter) == QJsonValue(QStringLiteral("matter")));

    // A set this version does not know gives the first one and is saved back
    const BookProject acts = withSections(R"("acts")");
    CHECK(acts.books.at(0).sectionSet == QStringLiteral("acts"));
    CHECK(acts.books.at(0).sectionNameSet().id == QStringLiteral("sections"));
    CHECK(saved(acts) == QJsonValue(QStringLiteral("acts")));

    // The writer's own names
    const char* const names =
        R"({ "front": "Przedmowa i wstęp", "main": "Opowieść", "back": "Dodatki" })";
    const BookProject own = withSections(names);
    const ProjectBook& book = own.books.at(0);
    CHECK(book.sectionSet == QStringLiteral("custom"));
    CHECK(book.sectionNames == QStringList{QStringLiteral("Przedmowa i wstęp"),
                                           QStringLiteral("Opowieść"),
                                           QStringLiteral("Dodatki")});
    CHECK(book.sectionNamesExtra.isEmpty());
    CHECK(book.sectionName(BookPlace::Front) == QStringLiteral("Przedmowa i wstęp"));
    CHECK(book.sectionName(BookPlace::Back) == QStringLiteral("Dodatki"));
    CHECK(saved(own) == QJsonValue(jsonOf(names)));

    // With a set, the writer's names are not saved
    BookProject changed = own;
    changed.books[0].sectionSet = QStringLiteral("arc");
    CHECK(saved(changed) == QJsonValue(QStringLiteral("arc")));

    // None: the first set, and no field in the file
    const BookProject none =
        projectOf(R"({ "format": 2, "id": "p", "books": [ { "id": "b", "folder": "book" } ] })");
    CHECK(none.books.at(0).sectionSet.isEmpty());
    CHECK(none.books.at(0).sectionNameSet().id == QStringLiteral("sections"));
    CHECK(saved(none).isUndefined());
}

TEST_CASE("Book project: the sections are shown or hidden, and named", "[core][bookproject]") {
    ProjectBook book;
    book.language = QStringLiteral("pl");
    ProjectElement dedication;
    dedication.id = QStringLiteral("e1");
    dedication.title = QStringLiteral("Dedykacja");
    book.frontElements << dedication;
    const auto names = [&book]() {
        return joined({book.sectionName(BookPlace::Front), book.sectionName(BookPlace::Main),
                       book.sectionName(BookPlace::Back)});
    };
    const QString custom = QString::fromLatin1(ProjectBook::CUSTOM_SECTIONS);

    // A set of names
    book.setSections({true, QStringLiteral("matter"), {}});
    CHECK(book.partsLayer);
    CHECK(book.sectionSet == QStringLiteral("matter"));
    CHECK(book.sectionNames.isEmpty());
    CHECK(book.sections() == BookSections{true, QStringLiteral("matter"), {}});
    CHECK(names() == "Strony początkowe, Tekst główny, Strony końcowe");

    // Hidden sections keep their names, and each element stays in its section
    book.setSections({false, QStringLiteral("matter"), {}});
    CHECK_FALSE(book.partsLayer);
    CHECK(book.sections() == BookSections{false, QStringLiteral("matter"), {}});
    CHECK(names() == "Strony początkowe, Tekst główny, Strony końcowe");
    CHECK(book.frontElements.size() == 1);
    CHECK(book.mainElements.isEmpty());

    // The writer's own names lose the spaces at their ends; an empty or missing name is the
    // name of the first set in the language of the book
    book.setSections({true, custom, {QStringLiteral("  Przedmowa  "), QStringLiteral(" ")}});
    CHECK(book.sectionSet == custom);
    CHECK(names() == "Przedmowa, Sekcja główna, Sekcja końcowa");
    CHECK(book.sections() ==
          BookSections{true, custom,
                       {QStringLiteral("Przedmowa"), QStringLiteral("Sekcja główna"),
                        QStringLiteral("Sekcja końcowa")}});
    book.language = QStringLiteral("en");
    book.setSections({true, custom, {}});
    CHECK(names() == "Front Section, Main Section, Back Section");

    // Fields of the writer's names that this version does not know stay with the writer's
    // names, and go with them
    book.sectionNamesExtra.insert(QStringLiteral("workshop"), QStringLiteral("Notatki"));
    book.setSections({true, custom,
                      {QStringLiteral("Wstęp"), QStringLiteral("Opowieść"),
                       QStringLiteral("Dodatki")}});
    CHECK(book.sectionNamesExtra.value(QStringLiteral("workshop")).toString() ==
          QStringLiteral("Notatki"));
    book.setSections({true, QStringLiteral("arc"),
                      {QStringLiteral("Wstęp"), QStringLiteral("Opowieść"),
                       QStringLiteral("Dodatki")}});
    CHECK(book.sectionNames.isEmpty());  // names only with the writer's names
    CHECK(book.sectionNamesExtra.isEmpty());
    CHECK(book.sections() == BookSections{true, QStringLiteral("arc"), {}});
    CHECK(names() == "Opening, Development, Closing");

    // No set: the first one
    book.setSections({true, QString(), {}});
    CHECK(book.sectionNameSet().id == QStringLiteral("sections"));
    CHECK(book.frontElements.size() == 1);
}

TEST_CASE("Book project: changing an element of a copy leaves the project as it was",
          "[core][bookproject]") {
    const BookProject project = projectOf(NOVEL);
    BookProject copy = project;

    ProjectElement* chapter = copy.findElement(QStringLiteral("e3"));
    REQUIRE(chapter != nullptr);
    chapter->title = QStringLiteral("Chapter One");
    REQUIRE(copy.moveElement(QStringLiteral("e1"), 0));
    REQUIRE(copy.takeElement(QStringLiteral("e4")).has_value());

    CHECK(project.findElement(QStringLiteral("e3"))->title == QStringLiteral("Chapter 1"));
    CHECK(project.workshop.elements.size() == 1);
    CHECK(project.toJson() == jsonOf(NOVEL));
}

TEST_CASE("Book project: files of the elements and new ids", "[core][bookproject]") {
    const BookProject project = projectOf(NOVEL);

    CHECK(project.hasFile(QStringLiteral("book/chapter_001.kchapter")));
    CHECK(project.hasFile(QStringLiteral("workshop/character_001.json")));
    CHECK_FALSE(project.hasFile(QStringLiteral("book/chapter_002.kchapter")));
    CHECK_FALSE(project.hasFile(QString()));

    const QString id = project.newElementId();
    CHECK_FALSE(id.isEmpty());
    CHECK(project.findElement(id) == nullptr);
    CHECK(project.newElementId() != id);
}

// =============================================================================
// Kinds
// =============================================================================

TEST_CASE("Book project: a user project has no type and offers the kinds it took",
          "[core][bookproject]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    const char* const text = R"({
      "format": 2, "id": "p",
      "kinds": ["kalahari.base:title_page", "kalahari.base:part", "kalahari.base:chapter",
                "kalahari.poetry:poem", "kalahari.base:work_note", "kalahari.base:toc"],
      "books": [ { "id": "b", "folder": "book", "partsLayer": false } ]
    })";
    const BookProject project = projectOf(text);

    CHECK(project.isUserProject());
    CHECK_FALSE(project.toJson().contains(QStringLiteral("type")));
    CHECK(references(project.kindsIn(registry, BookPlace::Front)) ==
          "kalahari.base:title_page, kalahari.base:toc");
    CHECK(references(project.kindsIn(registry, BookPlace::Main)) ==
          "kalahari.base:part, kalahari.base:chapter, kalahari.poetry:poem");
    CHECK(references(project.kindsIn(registry, BookPlace::Back)) == "kalahari.base:toc");
    CHECK(references(project.kindsIn(registry, BookPlace::Workshop)) ==
          "kalahari.base:work_note");
    CHECK(references(project.kindsInside(registry, QStringLiteral("part"))) ==
          "kalahari.base:chapter");
    CHECK(references(project.kindsInside(registry, QStringLiteral("cycle"))) ==
          "kalahari.poetry:poem");
    CHECK(project.missingPackages(registry).isEmpty());
}

TEST_CASE("Book project: a book takes kinds of other types", "[core][bookproject]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    BookProject project = projectOf(NOVEL);
    project.kinds = {{QStringLiteral("kalahari.nonfiction"), QStringLiteral("bibliography")},
                     {QStringLiteral("kalahari.screenplay"), QStringLiteral("logline")},
                     {QStringLiteral("kalahari.nonfiction"), QStringLiteral("glossary")}};

    // The kinds of the type first, then the kinds taken; the glossary of literature
    // non-fiction is the glossary of Base, which the novel already has
    CHECK(references(project.kindsIn(registry, BookPlace::Back)) ==
          "kalahari.base:afterword, kalahari.base:acknowledgments, kalahari.base:glossary, "
          "kalahari.base:about_author, kalahari.base:toc, kalahari.nonfiction:bibliography");
    CHECK(references(project.kindsIn(registry, BookPlace::Workshop)) ==
          "kalahari.base:work_note, kalahari.base:mindmap, kalahari.base:timeline, "
          "kalahari.base:character, kalahari.base:location, kalahari.base:item, "
          "kalahari.base:material, kalahari.screenplay:logline");
    CHECK(references(project.kindsIn(registry, BookPlace::Main)) ==
          "kalahari.novel:prologue, kalahari.base:part, kalahari.base:chapter, "
          "kalahari.novel:epilogue");
    CHECK(references(project.kindsInside(registry, QStringLiteral("part"))) ==
          "kalahari.novel:prologue, kalahari.base:chapter, kalahari.novel:epilogue, "
          "kalahari.base:motto");

    const KindRef bibliography =
        BookProject::kindOf(registry, project.books.at(0).backElements.at(0));
    REQUIRE(bibliography);
    CHECK(bibliography.reference() == QStringLiteral("kalahari.nonfiction:bibliography"));
    CHECK(bibliography.kind->form == ElementForm::Window);
    CHECK(project.missingPackages(registry).isEmpty());
}

TEST_CASE("Book project: a kind taken out of the project stays with its elements",
          "[core][bookproject]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    BookProject project = projectOf(NOVEL);
    project.kinds.clear();

    // No new bibliography...
    CHECK(references(project.kindsIn(registry, BookPlace::Back)) ==
          "kalahari.base:afterword, kalahari.base:acknowledgments, kalahari.base:glossary, "
          "kalahari.base:about_author, kalahari.base:toc");

    // ...but the one in the book keeps its kind, and the file keeps the element
    const ProjectElement& element = project.books.at(0).backElements.at(0);
    const KindRef kind = BookProject::kindOf(registry, element);
    REQUIRE(kind);
    CHECK(kind.reference() == QStringLiteral("kalahari.nonfiction:bibliography"));
    CHECK(project.missingPackages(registry).isEmpty());

    QStringList problems;
    const std::optional<BookProject> again = BookProject::fromJson(project.toJson(), problems);
    REQUIRE(again.has_value());
    CHECK(again->kinds.isEmpty());
    CHECK(again->books.at(0).backElements == project.books.at(0).backElements);
}

TEST_CASE("Book project: kinds of packages that are not installed", "[core][bookproject]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    const char* const text = R"({
      "format": 2, "id": "p",
      "type": { "id": "addon.fantasy", "version": "2.1" },
      "kinds": ["addon.fantasy:spell_book", "kalahari.base:chapter", "addon.maps:map"],
      "books": [
        { "id": "b", "title": "Dragons", "author": "", "language": "en", "genre": "",
          "partsLayer": true, "folder": "book", "front": [], "back": [],
          "main": [
            { "id": "e1", "kind": "addon.fantasy:prophecy", "title": "The Prophecy",
              "file": "book/prophecy_001.kchapter", "status": "draft" },
            { "id": "e2", "kind": "kalahari.base:chapter", "title": "Chapter 1",
              "file": "book/chapter_001.kchapter", "status": "draft" }
          ] }
      ],
      "workshop": { "grouped": false, "elements": [
        { "id": "e3", "kind": "addon.maps:map", "title": "The North",
          "file": "workshop/map_001.kmap" } ] }
    })";

    // Packages that are not installed are no problem of the file...
    const BookProject project = projectOf(text);
    CHECK(joined(project.missingPackages(registry)) == "addon.fantasy, addon.maps");

    // ...the project offers only the kinds it has...
    CHECK(references(project.kindsIn(registry, BookPlace::Main)) == "kalahari.base:chapter");
    CHECK(project.kindsIn(registry, BookPlace::Workshop).isEmpty());

    // ...the elements of the missing kinds stay, without a kind, in the form of their file...
    const QList<ProjectElement>& main = project.books.at(0).mainElements;
    CHECK_FALSE(BookProject::kindOf(registry, main.at(0)));
    CHECK(BookProject::kindOf(registry, main.at(1)));
    CHECK_FALSE(BookProject::kindOf(registry, project.workshop.elements.at(0)));
    CHECK(BookProject::formOf(registry, main.at(0)) == ElementForm::Text);
    CHECK(BookProject::formOf(registry, project.workshop.elements.at(0)) == ElementForm::Window);

    // ...and saving keeps them
    CHECK(project.toJson() == jsonOf(text));
}

// =============================================================================
// Problems
// =============================================================================

TEST_CASE("Book project: a .klh file with problems", "[core][bookproject]") {
    struct Broken {
        const char* what;
        QByteArray json;
        const char* problem;
    };
    const auto project = [](const char* text) { return QByteArray(text); };

    const std::vector<Broken> cases = {
        {"no format", project(R"({ "id": "p", "books": [ { "id": "b", "folder": "book" } ] })"),
         "format: must be 2, the format this version of Kalahari reads"},
        {"the file of an older version",
         project(R"({ "kalahari": { "version": "1.0" }, "document": { "title": "Old" } })"),
         "format: must be 2"},
        {"a newer format",
         project(R"({ "format": 3, "id": "p", "books": [ { "id": "b", "folder": "b" } ] })"),
         "format: must be 2"},
        {"no id", project(R"({ "format": 2, "books": [ { "id": "b", "folder": "book" } ] })"),
         "id: must be a text that is not empty"},
        {"a date that is not a date",
         project(R"({ "format": 2, "id": "p", "created": "yesterday",
                      "books": [ { "id": "b", "folder": "book" } ] })"),
         "created: must be a date and time"},
        {"a type that is not an object",
         project(R"({ "format": 2, "id": "p", "type": "kalahari.novel",
                      "books": [ { "id": "b", "folder": "book" } ] })"),
         "type: must be an object with the id and version of the type"},
        {"a type without an id",
         project(R"({ "format": 2, "id": "p", "type": { "version": "1.0" },
                      "books": [ { "id": "b", "folder": "book" } ] })"),
         "type.id: must be a text that is not empty"},
        {"a type that is not a package id",
         project(R"({ "format": 2, "id": "p", "type": { "id": "Novel" },
                      "books": [ { "id": "b", "folder": "book" } ] })"),
         "type.id: must be the id of a package"},
        {"a kind without its package",
         project(R"({ "format": 2, "id": "p", "kinds": ["chapter"],
                      "books": [ { "id": "b", "folder": "book" } ] })"),
         "kinds[0]: must be a kind with its package"},
        {"a kind twice",
         project(R"({ "format": 2, "id": "p",
                      "kinds": ["kalahari.base:chapter", "kalahari.base:chapter"],
                      "books": [ { "id": "b", "folder": "book" } ] })"),
         "kinds[1]: 'kalahari.base:chapter' is already in the list"},
        {"kinds that are not a list",
         project(R"({ "format": 2, "id": "p", "kinds": "kalahari.base:chapter",
                      "books": [ { "id": "b", "folder": "book" } ] })"),
         "kinds: must be a list"},
        {"no books", project(R"({ "format": 2, "id": "p" })"),
         "books: must be a list of at least one book"},
        {"an empty list of books", project(R"({ "format": 2, "id": "p", "books": [] })"),
         "books: must be a list of at least one book"},
        {"a book that is not an object",
         project(R"({ "format": 2, "id": "p", "books": ["My Novel"] })"),
         "books[0]: must be an object"},
        {"two books with one id",
         project(R"({ "format": 2, "id": "p", "books": [ { "id": "b", "folder": "book" },
                                                        { "id": "b", "folder": "book2" } ] })"),
         "books[1].id: 'b' is already the id of books[0]"},
        {"two books with one folder",
         project(R"({ "format": 2, "id": "p", "books": [ { "id": "b1", "folder": "book" },
                                                        { "id": "b2", "folder": "book" } ] })"),
         "books[1].folder: 'book' is already the folder of books[0]"},
        {"a book without a folder",
         project(R"({ "format": 2, "id": "p", "books": [ { "id": "b" } ] })"),
         "books[0].folder: must be a folder inside the project folder"},
        {"a book folder outside the project",
         project(R"({ "format": 2, "id": "p", "books": [ { "id": "b", "folder": "../b" } ] })"),
         "books[0].folder: must be a folder inside the project folder"},
        {"a parts layer that is not true or false",
         project(R"({ "format": 2, "id": "p",
                      "books": [ { "id": "b", "folder": "book", "partsLayer": "yes" } ] })"),
         "books[0].partsLayer: must be true or false"},
        {"names of the parts that are a number",
         project(R"({ "format": 2, "id": "p",
                      "books": [ { "id": "b", "folder": "book", "sections": 3 } ] })"),
         "books[0].sections: must be the id of a set of names, e.g. \"matter\", or the names "
         "of the front, main and back part"},
        {"the writer's names without the names",
         project(R"({ "format": 2, "id": "p",
                      "books": [ { "id": "b", "folder": "book", "sections": "custom" } ] })"),
         "books[0].sections: must be the id of a set of names"},
        {"the writer's names without the back part",
         project(R"({ "format": 2, "id": "p", "books": [ { "id": "b", "folder": "book",
                      "sections": { "front": "Wstęp", "main": "Opowieść" } } ] })"),
         "books[0].sections.back: must be a text that is not empty"},
        {"an empty name of a part",
         project(R"({ "format": 2, "id": "p", "books": [ { "id": "b", "folder": "book",
                      "sections": { "front": "", "main": "Opowieść", "back": "Dodatki" } } ] })"),
         "books[0].sections.front: must be a text that is not empty"},
        {"a name of a part that is not a text",
         project(R"({ "format": 2, "id": "p", "books": [ { "id": "b", "folder": "book",
                      "sections": { "front": "Wstęp", "main": 2, "back": "Dodatki" } } ] })"),
         "books[0].sections.main: must be a text that is not empty"},
        {"a title that is not a text",
         project(R"({ "format": 2, "id": "p",
                      "books": [ { "id": "b", "folder": "book", "title": 7 } ] })"),
         "books[0].title: must be a text"},
        {"a part that is not a list",
         project(R"({ "format": 2, "id": "p",
                      "books": [ { "id": "b", "folder": "book", "front": {} } ] })"),
         "books[0].front: must be a list"},
        {"a Workshop that is not an object",
         project(R"({ "format": 2, "id": "p", "books": [ { "id": "b", "folder": "book" } ],
                      "workshop": [] })"),
         "workshop: must be an object"},
        {"a grouping that is not true or false",
         project(R"({ "format": 2, "id": "p", "books": [ { "id": "b", "folder": "book" } ],
                      "workshop": { "grouped": "no" } })"),
         "workshop.grouped: must be true or false"},
        {"an element that is not an object", withElement(R"("Chapter 1")"),
         "books[0].main[0]: must be an object"},
        {"an element without an id", withElement(R"({ "kind": "kalahari.base:chapter" })"),
         "books[0].main[0].id: must be a text that is not empty"},
        {"an element without a kind", withElement(R"({ "id": "e1" })"),
         "books[0].main[0].kind: must be a kind with its package"},
        {"an element kind without its package",
         withElement(R"({ "id": "e1", "kind": "chapter" })"),
         "books[0].main[0].kind: must be a kind with its package"},
        {"a file outside the project",
         withElement(R"({ "id": "e1", "kind": "kalahari.base:chapter",
                          "file": "../chapter_001.kchapter" })"),
         "books[0].main[0].file: must be a path inside the project folder"},
        {"a file with its whole path",
         withElement(R"({ "id": "e1", "kind": "kalahari.base:chapter",
                          "file": "/home/anna/chapter_001.kchapter" })"),
         "books[0].main[0].file: must be a path inside the project folder"},
        {"a file on a drive",
         withElement(R"({ "id": "e1", "kind": "kalahari.base:chapter",
                          "file": "C:/Books/chapter_001.kchapter" })"),
         "books[0].main[0].file: must be a path inside the project folder"},
        {"a file with backslashes",
         withElement(R"({ "id": "e1", "kind": "kalahari.base:chapter",
                          "file": "book\\chapter_001.kchapter" })"),
         "books[0].main[0].file: must be a path inside the project folder"},
        {"a status that is not a text",
         withElement(R"({ "id": "e1", "kind": "kalahari.base:chapter", "status": 1 })"),
         "books[0].main[0].status: must be a text"},
        {"elements inside that are not a list",
         withElement(R"({ "id": "e1", "kind": "kalahari.base:part", "elements": {} })"),
         "books[0].main[0].elements: must be a list"},
        {"two elements with one id",
         withElement(R"({ "id": "e1", "kind": "kalahari.base:chapter" },
                        { "id": "e2", "kind": "kalahari.base:part", "elements": [
                          { "id": "e1", "kind": "kalahari.base:chapter" } ] })"),
         "books[0].main[1].elements[0].id: 'e1' is already the id of books[0].main[0]"},
        {"an element of the Workshop with the id of one in the book",
         project(R"({ "format": 2, "id": "p",
                      "books": [ { "id": "b", "folder": "book", "main": [
                        { "id": "e1", "kind": "kalahari.base:chapter" } ] } ],
                      "workshop": { "elements": [
                        { "id": "e1", "kind": "kalahari.base:work_note" } ] } })"),
         "workshop.elements[0].id: 'e1' is already the id of books[0].main[0]"},
        {"two elements with one file",
         withElement(R"({ "id": "e1", "kind": "kalahari.base:chapter",
                          "file": "book/chapter_001.kchapter" },
                        { "id": "e2", "kind": "kalahari.base:chapter",
                          "file": "book/chapter_001.kchapter" })"),
         "books[0].main[1].file: 'book/chapter_001.kchapter' is already the file of "
         "books[0].main[0]"},
    };

    for (const Broken& broken : cases) {
        INFO(broken.what);
        const std::string problems = problemsOf(broken.json);
        INFO(problems);
        CHECK(problems.find(broken.problem) != std::string::npos);
    }
}

TEST_CASE("Book project: every problem of a file is reported", "[core][bookproject]") {
    const std::string problems = problemsOf(R"({
      "format": 2, "id": "p", "kinds": ["chapter"],
      "books": [ { "id": "b", "folder": "book", "partsLayer": 1, "main": [
        { "id": "e1", "kind": "chapter" } ] } ]
    })");
    CHECK(problems == "kinds[0]: must be a kind with its package, e.g. \"kalahari.base:chapter\"\n"
                      "books[0].partsLayer: must be true or false\n"
                      "books[0].main[0].kind: must be a kind with its package, e.g. "
                      "\"kalahari.base:chapter\"");
}

TEST_CASE("Book project: a .klh file that cannot be read", "[core][bookproject]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto write = [&directory](const char* name, const QByteArray& content) {
        const QString path = directory.filePath(QString::fromLatin1(name));
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        REQUIRE(file.write(content) == content.size());
        return path;
    };
    const auto problemsOfFile = [](const QString& path) {
        QStringList problems;
        CHECK_FALSE(BookProject::load(path, problems).has_value());
        return joined(problems);
    };

    CHECK(problemsOfFile(directory.filePath(QStringLiteral("Missing.klh"))) ==
          "Missing.klh: cannot be read");
    CHECK(problemsOfFile(write("Broken.klh", "{ \"format\": 2, "))
              .starts_with("Broken.klh: not valid JSON"));
    CHECK(problemsOfFile(write("List.klh", "[]")) == "List.klh: must be a JSON object");
}

TEST_CASE("Book project: saving to a folder that does not exist fails", "[core][bookproject]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("missing/My Novel.klh"));
    CHECK_FALSE(fullProject().save(path));
    CHECK_FALSE(QFile::exists(path));
}

// =============================================================================
// The example project
// =============================================================================

TEST_CASE("Book project: the example project is in the format of this version",
          "[core][bookproject][examples]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    const QDir folder(QStringLiteral(KALAHARI_SOURCE_DIR "/examples/ExampleNovel"));
    QStringList problems;
    const std::optional<BookProject> project =
        BookProject::load(folder.filePath(QStringLiteral("ExampleNovel.klh")), problems);
    INFO(joined(problems));
    REQUIRE(project.has_value());
    CHECK(project->missingPackages(registry).isEmpty());

    // Every element has its kind, and every text element a chapter file with its title and
    // status
    int texts = 0;
    for (const ProjectElement* element : project->readingOrder()) {
        INFO(element->id.toStdString());
        CHECK(BookProject::kindOf(registry, *element));
        if (BookProject::formOf(registry, *element) == ElementForm::Text) {
            ++texts;
            const std::optional<ChapterDocument> chapter =
                ChapterDocument::load(folder.filePath(element->file));
            REQUIRE(chapter.has_value());
            CHECK(chapter->title() == element->title);
            CHECK(chapter->status() == element->status);
        }
    }
    CHECK(texts == 4);
}
