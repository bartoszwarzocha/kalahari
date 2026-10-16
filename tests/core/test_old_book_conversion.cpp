/// @file test_old_book_conversion.cpp
/// @brief Books of the first .klh format, converted to the book project of this version

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/chapter_document.h>
#include <kalahari/core/old_book_conversion.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>

#include <functional>
#include <string>

using namespace kalahari::core;

namespace {

void loadBuiltIn(BookTypeRegistry& registry) {
    registry.load({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(registry.problems().isEmpty());
}

std::string joined(const QStringList& list) {
    return list.join(QStringLiteral("\n")).toStdString();
}

QJsonObject jsonOf(const QByteArray& text) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(text, &error);
    INFO(error.errorString().toStdString());
    REQUIRE(document.isObject());
    return document.object();
}

QDateTime utc(const char* text) {
    return QDateTime::fromString(QString::fromLatin1(text), Qt::ISODate);
}

/// A chapter file @p file in @p folder, with the status @p status
void writeChapter(const QDir& folder, const QString& file, const QString& status) {
    REQUIRE(folder.mkpath(QFileInfo(folder.filePath(file)).path()));
    ChapterDocument chapter;
    chapter.setTitle(QFileInfo(file).completeBaseName());
    chapter.setStatus(status);
    REQUIRE(chapter.save(folder.filePath(file)));
}

/// One line per element: its kind, id, title, file and status, the elements inside it after
/// it, indented
std::string describe(const QList<ProjectElement>& elements) {
    QStringList lines;
    const std::function<void(const QList<ProjectElement>&, const QString&)> add =
        [&lines, &add](const QList<ProjectElement>& list, const QString& indent) {
            for (const ProjectElement& element : list) {
                QStringList parts;
                if (element.hasKind()) {
                    parts << element.kind.toString();
                }
                parts << element.id << QStringLiteral("\"%1\"").arg(element.title);
                if (!element.file.isEmpty()) {
                    parts << element.file;
                }
                if (!element.status.isEmpty()) {
                    parts << element.status;
                }
                lines << indent + parts.join(QLatin1Char(' '));
                add(element.elements, indent + QStringLiteral("  "));
            }
        };
    add(elements, QString());
    return joined(lines);
}

/// The .klh file of the example book of Kalahari 0.3, in the first format
const QByteArray EXAMPLE_NOVEL = R"({
    "document": {
        "author": "Test Author",
        "created": "2026-02-16T05:57:37+00:00",
        "genre": "fiction",
        "id": "1771221457515-61aa",
        "language": "en",
        "modified": "2026-02-16T05:59:59Z",
        "title": "Example Novel"
    },
    "kalahari": {"minVersion": "0.4.0", "version": "1.0"},
    "settings": {"autoSaveInterval": 300, "defaultPerspective": "writer"},
    "statistics": {"lastEdited": "", "totalChapters": 0, "totalWords": 0},
    "structure": {
        "backmatter": [
            {"file": "content/backmatter/epilogue.kchapter", "id": "bm-epilogue",
             "title": "Epilogue", "type": "epilogue"}
        ],
        "body": [
            {
                "chapters": [
                    {"file": "content/body/part_001/chapter_001.kchapter", "id": "ch-001",
                     "title": "Chapter One: The Beginning"},
                    {"file": "content/body/part_001/chapter_002.kchapter", "id": "ch-002",
                     "title": "Chapter Two: The Discovery"}
                ],
                "id": "part-001",
                "title": "Part One: Beginnings"
            }
        ],
        "frontmatter": [
            {"file": "content/frontmatter/title_page.kchapter", "id": "fm-title",
             "title": "Title Page", "type": "title_page"}
        ]
    }
})";

}  // namespace

TEST_CASE("A book of the first format becomes a novel that loses nothing",
          "[old_book_conversion]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);
    QTemporaryDir dir;
    const QDir folder(dir.path());
    writeChapter(folder, QStringLiteral("content/frontmatter/title_page.kchapter"),
                 QStringLiteral("final"));
    writeChapter(folder, QStringLiteral("content/body/part_001/chapter_001.kchapter"),
                 QStringLiteral("revision"));
    writeChapter(folder, QStringLiteral("content/body/part_001/chapter_002.kchapter"),
                 QString());

    const OldBookConversion conversion =
        OldBookConversion::convert(jsonOf(EXAMPLE_NOVEL), dir.path(), registry);
    INFO(joined(conversion.problems));
    REQUIRE(conversion.project);
    CHECK(conversion.problems.isEmpty());
    CHECK(conversion.otherFiles.isEmpty());
    const BookProject& project = *conversion.project;

    // A novel with the id and the dates of the old book
    REQUIRE(project.type);
    CHECK(project.type->id == "kalahari.novel");
    CHECK(project.type->version == registry.package(QStringLiteral("kalahari.novel"))->version);
    CHECK(project.id == "1771221457515-61aa");
    CHECK(project.created == utc("2026-02-16T05:57:37Z"));
    CHECK(project.modified == utc("2026-02-16T05:59:59Z"));
    CHECK(project.kinds.isEmpty());

    // The data of the book, the names of the sections that the first format showed, and its
    // elements with the statuses of their files: the epilogue at the end of the body
    REQUIRE(project.books.size() == 1);
    const ProjectBook& book = project.books.first();
    CHECK_FALSE(book.id.isEmpty());
    CHECK(book.title == "Example Novel");
    CHECK(book.author == "Test Author");
    CHECK(book.language == "en");
    CHECK(book.genre == "fiction");
    CHECK(book.partsLayer);
    CHECK(book.sectionSet == "matter");
    CHECK(book.folder == "book");
    CHECK(book.extra.isEmpty());
    CHECK(describe(book.frontElements) ==
          "kalahari.base:title_page fm-title \"Title Page\" "
          "content/frontmatter/title_page.kchapter final");
    CHECK(describe(book.mainElements) ==
          "kalahari.base:part part-001 \"Part One: Beginnings\"\n"
          "  kalahari.base:chapter ch-001 \"Chapter One: The Beginning\" "
          "content/body/part_001/chapter_001.kchapter revision\n"
          "  kalahari.base:chapter ch-002 \"Chapter Two: The Discovery\" "
          "content/body/part_001/chapter_002.kchapter draft\n"
          "kalahari.novel:epilogue bm-epilogue \"Epilogue\" "
          "content/backmatter/epilogue.kchapter draft");
    CHECK(book.backElements.isEmpty());
    CHECK(book.files.isEmpty());

    // The sections of the old file that the conversion does not use stay as they were
    const QJsonObject old = jsonOf(EXAMPLE_NOVEL);
    CHECK(project.extra == QJsonObject{{QStringLiteral("kalahari"), old.value("kalahari")},
                                       {QStringLiteral("settings"), old.value("settings")},
                                       {QStringLiteral("statistics"), old.value("statistics")}});

    // The book is read back as every .klh file of this version
    QStringList problems;
    const std::optional<BookProject> read = BookProject::fromJson(project.toJson(), problems);
    INFO(joined(problems));
    REQUIRE(read);
    CHECK(*read == project);
}

TEST_CASE("A type of the first format gets its kind, or the element goes among the files",
          "[old_book_conversion]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);
    QTemporaryDir dir;

    const QJsonObject json = jsonOf(R"({
        "document": {"title": "Kinds", "language": "pl"},
        "structure": {
            "frontmatter": [
                {"id": "f1", "title": "Prolog", "file": "content/f1.kchapter", "type": "prologue"},
                {"id": "f2", "title": "Wstęp", "file": "content/f2.kchapter",
                 "type": "introduction"},
                {"id": "f3", "title": "Motto", "file": "content/f3.kchapter", "type": "epigraph"},
                {"id": "f4", "title": "Przedmowa", "file": "content/f4.kchapter",
                 "type": "foreword"},
                {"id": "f5", "title": "Druga przedmowa", "file": "content/f5.kchapter"},
                {"id": "f6", "title": "Dedykacja", "file": "content/f6.kchapter",
                 "type": "dedication"},
                {"id": "f7", "title": "Druga dedykacja", "file": "content/f7.kchapter",
                 "type": "dedication"}
            ],
            "body": [],
            "backmatter": [
                {"id": "b1", "title": "Aneks", "file": "content/b1.kchapter", "type": "appendix"},
                {"id": "b2", "title": "Bibliografia", "file": "content/b2.kchapter",
                 "type": "bibliography"},
                {"id": "b3", "title": "Moje", "file": "content/b3.kchapter", "type": "my_own",
                 "color": "#ff0000"},
                {"id": "b4", "title": "Posłowie", "file": "content/b4.kchapter",
                 "type": "afterword"},
                {"id": "b5", "title": "Epilog", "file": "content/b5.kchapter"},
                {"id": "b6", "title": "Notatka", "file": "content/b6.kchapter",
                 "type": "work_note"},
                {"id": "b7", "title": "Rozdział", "file": "content/b7.kchapter", "type": "chapter"}
            ]
        }
    })");
    const OldBookConversion conversion = OldBookConversion::convert(json, dir.path(), registry);
    INFO(joined(conversion.problems));
    REQUIRE(conversion.project);
    const BookProject& project = *conversion.project;
    const ProjectBook& book = project.books.first();

    // A prologue starts the body and an epilogue ends it; a kind that a novel does not offer
    // is taken from its package
    CHECK(describe(book.frontElements) ==
          "kalahari.base:introduction f2 \"Wstęp\" content/f2.kchapter draft\n"
          "kalahari.base:motto f3 \"Motto\" content/f3.kchapter draft\n"
          "kalahari.base:preface f4 \"Przedmowa\" content/f4.kchapter draft\n"
          "kalahari.base:dedication f6 \"Dedykacja\" content/f6.kchapter draft");
    CHECK(describe(book.mainElements) ==
          "kalahari.novel:prologue f1 \"Prolog\" content/f1.kchapter draft\n"
          "kalahari.base:chapter b7 \"Rozdział\" content/b7.kchapter draft\n"
          "kalahari.novel:epilogue b5 \"Epilog\" content/b5.kchapter draft");
    CHECK(describe(book.backElements) ==
          "kalahari.nonfiction:annex b1 \"Aneks\" content/b1.kchapter draft\n"
          "kalahari.base:afterword b4 \"Posłowie\" content/b4.kchapter draft");
    CHECK(project.kinds == QList<KindReference>{{"kalahari.base", "introduction"},
                                                 {"kalahari.nonfiction", "annex"}});

    // One more of a kind than a book can have, a kind of the Workshop, a kind that is no text
    // and a type that no package has: among the files of the book, with the type they had
    CHECK(describe(book.files) == "f5 \"Druga przedmowa\" content/f5.kchapter\n"
                                  "f7 \"Druga dedykacja\" content/f7.kchapter\n"
                                  "b2 \"Bibliografia\" content/b2.kchapter\n"
                                  "b3 \"Moje\" content/b3.kchapter\n"
                                  "b6 \"Notatka\" content/b6.kchapter");
    CHECK(book.files.at(0).extra.isEmpty());
    CHECK(book.files.at(1).extra == QJsonObject{{QStringLiteral("type"), "dedication"}});
    CHECK(book.files.at(2).extra == QJsonObject{{QStringLiteral("type"), "bibliography"}});
    CHECK(book.files.at(3).extra == QJsonObject{{QStringLiteral("type"), "my_own"},
                                                {QStringLiteral("color"), "#ff0000"}});
    CHECK(book.files.at(4).extra == QJsonObject{{QStringLiteral("type"), "work_note"}});
    CHECK(conversion.otherFiles == QStringList{"Druga przedmowa", "Druga dedykacja",
                                               "Bibliografia", "Moje", "Notatka"});

    // The book had no id and no dates
    CHECK_FALSE(project.id.isEmpty());
    CHECK_FALSE(project.created.isValid());
    CHECK_FALSE(project.modified.isValid());
    CHECK(book.language == "pl");
}

TEST_CASE("The files of the first format are the files that Kalahari 0.3 read",
          "[old_book_conversion]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);
    QTemporaryDir dir;
    const QDir folder(dir.path());
    writeChapter(folder, QStringLiteral("content/chapter_001.kchapter"), QStringLiteral("final"));
    writeChapter(folder, QStringLiteral("content/chapter_002.kchapter"),
                 QStringLiteral("revision"));

    // The file with "/" or "\" between folders, absolute or relative, and always with the
    // extension .kchapter
    QByteArray text = R"({
        "document": {"title": "Files"},
        "structure": {
            "body": [{"title": "Part", "chapters": [
                {"title": "Backslashes", "file": "content\\chapter_001.kchapter"},
                {"title": "No extension", "file": "content/chapter_002"},
                {"title": "Another extension", "file": "./content/chapter_003.txt"},
                {"title": "Absolute", "file": "ABSOLUTE"},
                {"title": "Only an extension", "file": "content/.kchapter"},
                {"title": "No file"}
            ]}]
        }
    })";
    text.replace("ABSOLUTE", folder.filePath(QStringLiteral("content/chapter_005.kchapter"))
                                 .toUtf8());
    const OldBookConversion conversion =
        OldBookConversion::convert(jsonOf(text), dir.path(), registry);
    INFO(joined(conversion.problems));
    REQUIRE(conversion.project);
    const QList<ProjectElement>& main = conversion.project->books.first().mainElements;
    REQUIRE(main.size() == 1);
    const QList<ProjectElement>& chapters = main.first().elements;
    REQUIRE(chapters.size() == 6);
    CHECK(chapters.at(0).file == "content/chapter_001.kchapter");
    CHECK(chapters.at(0).status == "final");
    CHECK(chapters.at(1).file == "content/chapter_002.kchapter");
    CHECK(chapters.at(1).status == "revision");
    CHECK(chapters.at(2).file == "content/chapter_003.kchapter");
    CHECK(chapters.at(2).status == "draft");
    CHECK(chapters.at(3).file == "content/chapter_005.kchapter");
    CHECK(chapters.at(4).file == "content/.kchapter.kchapter");
    CHECK(chapters.at(5).file.isEmpty());

    // The elements without an id, which the first format did not need, get one each
    QSet<QString> ids{main.first().id};
    for (const ProjectElement& chapter : chapters) {
        ids.insert(chapter.id);
    }
    ids.remove(QString());
    CHECK(ids.size() == 7);
}

TEST_CASE("The fields that the conversion does not use stay where they were",
          "[old_book_conversion]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);
    QTemporaryDir dir;

    const OldBookConversion conversion = OldBookConversion::convert(jsonOf(R"({
        "format": 1,
        "custom": {"a": 1},
        "document": {"title": "Fields", "series": "Saga"},
        "structure": {
            "notes": "mine",
            "frontmatter": [{"id": "d", "title": "D", "file": "d.kchapter", "type": "dedication",
                             "color": "#123456"}],
            "body": [{"id": "p", "title": "P", "collapsed": true, "chapters": [
                {"id": "c", "title": "C", "file": "c.kchapter", "words": 12}]}]
        }
    })"), dir.path(), registry);
    INFO(joined(conversion.problems));
    REQUIRE(conversion.project);
    const BookProject& project = *conversion.project;
    const ProjectBook& book = project.books.first();

    CHECK(project.extra ==
          QJsonObject{{QStringLiteral("custom"), QJsonObject{{QStringLiteral("a"), 1}}},
                      {QStringLiteral("structure"),
                       QJsonObject{{QStringLiteral("notes"), "mine"}}}});
    CHECK(book.extra == QJsonObject{{QStringLiteral("series"), "Saga"}});
    CHECK(book.frontElements.first().extra == QJsonObject{{QStringLiteral("color"), "#123456"}});
    const ProjectElement& part = book.mainElements.first();
    CHECK(part.extra == QJsonObject{{QStringLiteral("collapsed"), true}});
    CHECK(part.elements.first().extra == QJsonObject{{QStringLiteral("words"), 12}});
    CHECK(project.toJson().value(QStringLiteral("format")).toInt() == BookProject::FORMAT);
}

TEST_CASE("A book that cannot be converted so is not converted", "[old_book_conversion]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);
    QTemporaryDir dir;

    QByteArray text;
    QStringList expected;
    QString expectedStart;
    SECTION("a file outside the folder of the book") {
        text = R"({"document": {}, "structure": {"body": [{"chapters": [
                    {"title": "Out", "file": "../Other/chapter.kchapter"}]}]}})";
        expected = {QStringLiteral("structure.body[0].chapters[0].file: the file "
                                   "../Other/chapter.kchapter is outside the folder of the "
                                   "book")};
    }
    SECTION("a file on another drive") {
        text = R"({"document": {}, "structure": {"frontmatter": [
                    {"title": "Out", "file": "D:/Books/dedication.kchapter",
                     "type": "dedication"}]}})";
        expected = {QStringLiteral("structure.frontmatter[0].file: the file "
                                   "D:/Books/dedication.kchapter is outside the folder of the "
                                   "book")};
    }
    SECTION("two elements with one id") {
        text = R"({"document": {}, "structure": {
                    "frontmatter": [{"id": "x", "title": "A", "file": "a.kchapter",
                                     "type": "dedication"}],
                    "body": [{"id": "p", "chapters": [{"id": "x", "title": "B",
                                                       "file": "b.kchapter"}]}]}})";
        expectedStart = QStringLiteral("books[0].main[0].elements[0].id: 'x' is already the id");
    }
    SECTION("two elements with one file") {
        text = R"({"document": {}, "structure": {"body": [{"id": "p", "chapters": [
                    {"id": "a", "title": "A", "file": "same.kchapter"},
                    {"id": "b", "title": "B", "file": "same"}]}]}})";
        expectedStart =
            QStringLiteral("books[0].main[0].elements[1].file: 'same.kchapter' is already the "
                           "file");
    }
    SECTION("no data of the book") {
        text = R"({"structure": {}})";
        expected = {QStringLiteral("document: must be the data of the book, which a book of the "
                                   "first format has")};
    }
    SECTION("an element that is no object, and a title that is no text") {
        text = R"({"document": {}, "structure": {"frontmatter": ["text"],
                    "backmatter": [{"title": 5, "file": "a.kchapter"}]}})";
        expected = {QStringLiteral("structure.frontmatter[0]: must be an object"),
                    QStringLiteral("structure.backmatter[0].title: must be a text")};
    }
    SECTION("an element without a file that can be neither in the book nor among its files") {
        text = R"({"document": {}, "structure": {"backmatter": [
                    {"title": "Bibliografia", "file": "b.kchapter", "type": "bibliography"},
                    {"title": "Index", "type": "index"}]}})";
        expected = {QStringLiteral("structure.backmatter[1]: an element without a file whose "
                                   "type has no kind in a novel, so it can be neither in the "
                                   "book nor among its files")};
    }

    const OldBookConversion conversion =
        OldBookConversion::convert(jsonOf(text), dir.path(), registry);
    INFO(joined(conversion.problems));
    CHECK_FALSE(conversion.project);
    CHECK(conversion.otherFiles.isEmpty());
    if (expectedStart.isEmpty()) {
        CHECK(conversion.problems == expected);
    } else {
        REQUIRE(conversion.problems.size() == 1);
        CHECK(conversion.problems.first().startsWith(expectedStart));
    }
}

TEST_CASE("The copy of the old file of a book has a name of its own", "[old_book_conversion]") {
    QTemporaryDir dir;
    const QDir folder(dir.path());
    const QString manifest = folder.filePath(QStringLiteral("My Book.klh"));

    CHECK(OldBookConversion::copyPathFor(manifest) ==
          folder.filePath(QStringLiteral("My Book.format1.klh.bak")));
    for (const char* name : {"My Book.format1.klh.bak", "My Book.format1-2.klh.bak"}) {
        QFile file(folder.filePath(QString::fromUtf8(name)));
        REQUIRE(file.open(QIODevice::WriteOnly));
    }
    CHECK(OldBookConversion::copyPathFor(manifest) ==
          folder.filePath(QStringLiteral("My Book.format1-3.klh.bak")));
}
