/// @file test_book_type_registry.cpp
/// @brief Book type packages: the built-in Base and five types, and packages with problems

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/icon_registry.h>
#include <kalahari/gui/icon_registrar.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <algorithm>
#include <functional>
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

/// Ids of kinds, "?" for a kind that was not found
std::string kindIds(const QList<KindRef>& kinds) {
    QStringList ids;
    for (const KindRef& ref : kinds) {
        ids << (ref ? ref.kind->id : QStringLiteral("?"));
    }
    return joined(ids);
}

template <typename Style>
std::string styleIds(const QList<Style>& styles) {
    QStringList ids;
    for (const Style& style : styles) {
        ids << style.id;
    }
    return joined(ids);
}

/// Problems of the last load, one per line, with the folder of each package
std::string problemsOf(const BookTypeRegistry& registry) {
    QStringList lines;
    for (const BookTypeProblem& problem : registry.problems()) {
        lines << QFileInfo(problem.directory).fileName() + QStringLiteral(": ") + problem.message;
    }
    return lines.join(QLatin1Char('\n')).toStdString();
}

bool hasProblem(const BookTypeRegistry& registry, const QString& folder, const QString& text) {
    return std::any_of(registry.problems().cbegin(), registry.problems().cend(),
                       [&](const BookTypeProblem& problem) {
                           return QFileInfo(problem.directory).fileName() == folder &&
                                  problem.message.contains(text);
                       });
}

void loadBuiltIn(BookTypeRegistry& registry) {
    registry.load({builtInDirectory()});
    INFO(problemsOf(registry));
    REQUIRE(registry.problems().isEmpty());
}

const PackageParagraphStyle* findStyle(const QList<PackageParagraphStyle>& styles,
                                       const QString& id) {
    const auto it = std::find_if(styles.cbegin(), styles.cend(),
                                 [&id](const PackageParagraphStyle& style) {
                                     return style.id == id;
                                 });
    return it == styles.cend() ? nullptr : &*it;
}

// -----------------------------------------------------------------------------
// Test packages: a shared "test.base" and a type "test.type" that uses it
// -----------------------------------------------------------------------------

QJsonObject jsonOf(const char* text) {
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(text), &error);
    INFO(error.errorString().toStdString());
    REQUIRE(document.isObject());
    return document.object();
}

QJsonObject testBase() {
    return jsonOf(R"({
      "format": 1, "id": "test.base", "version": "1.0", "role": "shared",
      "name": { "en": "Test base" },
      "kinds": {
        "title_page": { "form": "text", "places": ["front"], "limit": 1,
                        "name": { "en": "Title page" }, "plural": { "en": "Title pages" } },
        "chapter": { "form": "text", "places": ["main", "part"],
                     "name": { "en": "Chapter" }, "plural": { "en": "Chapters" },
                     "title": { "en": "Chapter %n" } },
        "part": { "form": "group", "places": ["main"],
                  "name": { "en": "Part" }, "plural": { "en": "Parts" } },
        "note": { "form": "text", "places": ["workshop"],
                  "name": { "en": "Note" }, "plural": { "en": "Notes" } },
        "mindmap": { "form": "window", "editor": "mindmap", "places": ["workshop"],
                     "name": { "en": "Mind map" }, "plural": { "en": "Mind maps" } }
      },
      "front": ["title_page"], "main": ["chapter", "part"], "workshop": ["note", "mindmap"],
      "primary": "chapter", "styles": "styles.json"
    })");
}

QJsonObject testBaseStyles() {
    return jsonOf(R"({
      "paragraph_styles": [
        { "id": "normal", "name": { "en": "Normal" }, "properties": { "fontSize": 12 } },
        { "id": "heading", "name": { "en": "Heading" }, "base_style": "normal",
          "next_style": "normal", "properties": { "bold": true } }
      ],
      "character_styles": [
        { "id": "emphasis", "name": { "en": "Emphasis" }, "properties": { "italic": true } }
      ]
    })");
}

QJsonObject testType() {
    return jsonOf(R"({
      "format": 1, "id": "test.type", "version": "1.0", "role": "type",
      "name": { "en": "Test type", "pl": "Typ testowy" },
      "uses": ["test.base"],
      "kinds": {
        "prologue": { "form": "text", "places": ["main"], "limit": 1,
                      "name": { "en": "Prologue" }, "plural": { "en": "Prologues" } }
      },
      "front": ["title_page"], "main": ["prologue", "part", "chapter"], "workshop": ["note"],
      "primary": "chapter", "start": ["title_page", "chapter"]
    })");
}

/// A type "test.other" with an appendix and a style of its own
QJsonObject otherType() {
    return jsonOf(R"({
      "format": 1, "id": "test.other", "version": "1.0", "role": "type",
      "name": { "en": "Other type" },
      "uses": ["test.base"],
      "kinds": {
        "appendix": { "form": "text", "places": ["back"],
                      "name": { "en": "Appendix" }, "plural": { "en": "Appendices" } }
      },
      "main": ["chapter"], "back": ["appendix"], "primary": "chapter", "styles": "styles.json"
    })");
}

QJsonObject otherTypeStyles() {
    return jsonOf(R"({
      "paragraph_styles": [
        { "id": "appendix_title", "name": { "en": "Appendix title" }, "base_style": "heading" }
      ]
    })");
}

/// Set the value at a path such as "kinds.prologue.places"; an undefined value removes it
void setAt(QJsonObject& object, const QString& path, const QJsonValue& value) {
    const qsizetype dot = path.indexOf(QLatin1Char('.'));
    if (dot < 0) {
        if (value.isUndefined()) {
            object.remove(path);
        } else {
            object.insert(path, value);
        }
        return;
    }
    QJsonObject child = object.value(path.left(dot)).toObject();
    setAt(child, path.mid(dot + 1), value);
    object.insert(path.left(dot), child);
}

/// A folder of package folders
class PackageFolder {
public:
    PackageFolder() { REQUIRE(m_directory.isValid()); }

    QString path() const { return m_directory.path(); }

    void write(const QString& package, const QString& file, const QByteArray& content) {
        const QString filePath = m_directory.filePath(package + QLatin1Char('/') + file);
        REQUIRE(QDir().mkpath(QFileInfo(filePath).absolutePath()));
        QFile output(filePath);
        REQUIRE(output.open(QIODevice::WriteOnly));
        REQUIRE(output.write(content) == content.size());
    }

    void write(const QString& package, const QString& file, const QJsonObject& object) {
        write(package, file, QJsonDocument(object).toJson());
    }

    /// The test base with its styles and the test type
    void writeTestPackages(const QJsonObject& type = testType()) {
        write(QStringLiteral("test.base"), QStringLiteral("booktype.json"), testBase());
        write(QStringLiteral("test.base"), QStringLiteral("styles.json"), testBaseStyles());
        write(QStringLiteral("test.type"), QStringLiteral("booktype.json"), type);
    }

    /// The other test type with its styles
    void writeOtherType(const QJsonObject& other = otherType()) {
        write(QStringLiteral("test.other"), QStringLiteral("booktype.json"), other);
        write(QStringLiteral("test.other"), QStringLiteral("styles.json"), otherTypeStyles());
    }

private:
    QTemporaryDir m_directory;
};

}  // namespace

// =============================================================================
// Built-in packages
// =============================================================================

TEST_CASE("Built-in book types: Base and five types load without problems",
          "[core][booktypes]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    QStringList packages;
    for (const BookTypePackage* package : registry.packages()) {
        packages << package->id;
    }
    CHECK(joined(packages) ==
          "kalahari.base, kalahari.nonfiction, kalahari.novel, kalahari.poetry, "
          "kalahari.screenplay, kalahari.short_stories");

    QStringList types;
    for (const BookTypePackage* type : registry.bookTypes()) {
        types << type->id;
    }
    CHECK(joined(types) ==
          "kalahari.nonfiction, kalahari.novel, kalahari.poetry, kalahari.screenplay, "
          "kalahari.short_stories");

    const BookTypePackage* base = registry.package(QStringLiteral("kalahari.base"));
    REQUIRE(base != nullptr);
    CHECK(base->role == PackageRole::Shared);
    CHECK(base->startKinds.isEmpty());
    CHECK(base->name.text(QStringLiteral("pl")) == QStringLiteral("Podstawa"));
}

TEST_CASE("Built-in book types: kinds in each place, main kind and start", "[core][booktypes]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    // Kinds of a type in the front, main and back part and in the Workshop
    const auto places = [&registry](const char* type) {
        const QString id = QString::fromLatin1(type);
        return std::vector<std::string>{kindIds(registry.kindsIn(id, BookPlace::Front)),
                                        kindIds(registry.kindsIn(id, BookPlace::Main)),
                                        kindIds(registry.kindsIn(id, BookPlace::Back)),
                                        kindIds(registry.kindsIn(id, BookPlace::Workshop))};
    };
    // Kinds of the start structure with their places
    const auto start = [&registry](const char* type) {
        QStringList elements;
        for (const StartElement& element : registry.startElements(QString::fromLatin1(type))) {
            elements << (element.kind ? element.kind.kind->id : QStringLiteral("?")) +
                            QLatin1Char('/') + bookPlaceName(element.place);
        }
        return joined(elements);
    };
    const auto primary = [&registry](const char* type) {
        return registry.package(QString::fromLatin1(type))->primaryKind.toStdString();
    };

    SECTION("Base: what a user project without configuration offers, in its order") {
        CHECK(places("kalahari.base") ==
              std::vector<std::string>{
                  "title_page, copyright, dedication, motto, toc, preface, introduction",
                  "chapter, part", "afterword, acknowledgments, glossary, about_author, toc",
                  "work_note, mindmap, timeline, character, location, item, material"});
        CHECK(primary("kalahari.base") == "chapter");
    }

    SECTION("Novel") {
        CHECK(places("kalahari.novel") ==
              std::vector<std::string>{
                  "title_page, copyright, dedication, motto, toc, preface",
                  "prologue, part, chapter, epilogue",
                  "afterword, acknowledgments, glossary, about_author, toc",
                  "work_note, mindmap, timeline, character, location, item, material"});
        CHECK(primary("kalahari.novel") == "chapter");
        CHECK(start("kalahari.novel") == "title_page/front, chapter/main");
    }

    SECTION("Short story collection") {
        CHECK(places("kalahari.short_stories") ==
              std::vector<std::string>{
                  "title_page, copyright, dedication, motto, toc, introduction",
                  "story, section",
                  "contributors, first_publications, acknowledgments, about_author, toc",
                  "work_note, mindmap, character, location, material"});
        CHECK(primary("kalahari.short_stories") == "story");
        CHECK(start("kalahari.short_stories") == "title_page/front, story/main");
    }

    SECTION("Non-fiction") {
        CHECK(places("kalahari.nonfiction") ==
              std::vector<std::string>{
                  "title_page, copyright, dedication, motto, toc, preface, introduction, "
                  "abbreviations",
                  "part, chapter, conclusion",
                  "annex, endnotes, bibliography, glossary, index, illustrations, "
                  "acknowledgments, about_author, toc",
                  "source, work_note, timeline, mindmap, material"});
        CHECK(primary("kalahari.nonfiction") == "chapter");
        CHECK(start("kalahari.nonfiction") ==
              "title_page/front, introduction/front, chapter/main");
    }

    SECTION("Screenplay: no back part") {
        CHECK(places("kalahari.screenplay") ==
              std::vector<std::string>{"title_page", "act, episode", "",
                                       "logline, synopsis, treatment, step_outline, character, "
                                       "location, work_note, material"});
        CHECK(primary("kalahari.screenplay") == "act");
        CHECK(start("kalahari.screenplay") == "title_page/front, act/main");
    }

    SECTION("Poetry collection") {
        CHECK(places("kalahari.poetry") ==
              std::vector<std::string>{
                  "title_page, copyright, dedication, motto, toc, introduction", "cycle, poem",
                  "explanatory_notes, about_author, first_lines, publisher_note, toc",
                  "draft, work_note, mindmap, material"});
        CHECK(primary("kalahari.poetry") == "poem");
        CHECK(start("kalahari.poetry") == "title_page/front, poem/main");
    }
}

TEST_CASE("Built-in book types: groups hold the kinds their type allows inside them",
          "[core][booktypes]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    CHECK(kindIds(registry.kindsInside(QStringLiteral("kalahari.novel"), QStringLiteral("part"))) ==
          "prologue, chapter, epilogue, motto");
    CHECK(kindIds(registry.kindsInside(QStringLiteral("kalahari.nonfiction"),
                                       QStringLiteral("part"))) == "chapter, motto");
    CHECK(kindIds(registry.kindsInside(QStringLiteral("kalahari.short_stories"),
                                       QStringLiteral("section"))) == "story");
    CHECK(kindIds(registry.kindsInside(QStringLiteral("kalahari.screenplay"),
                                       QStringLiteral("episode"))) == "act");
    CHECK(kindIds(registry.kindsInside(QStringLiteral("kalahari.poetry"),
                                       QStringLiteral("cycle"))) == "poem");
    // Not a group
    CHECK(registry.kindsInside(QStringLiteral("kalahari.novel"), QStringLiteral("chapter"))
              .isEmpty());
}

TEST_CASE("Built-in book types: a kind comes from the type or from a package it uses",
          "[core][booktypes]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    const KindRef chapter =
        registry.findKind(QStringLiteral("kalahari.novel"), QStringLiteral("chapter"));
    REQUIRE(chapter);
    CHECK(chapter.package->id == QStringLiteral("kalahari.base"));
    CHECK(chapter.kind->form == ElementForm::Text);

    const KindRef prologue =
        registry.findKind(QStringLiteral("kalahari.novel"), QStringLiteral("prologue"));
    REQUIRE(prologue);
    CHECK(prologue.package->id == QStringLiteral("kalahari.novel"));
    CHECK(prologue.kind->limit == 1);
    CHECK(prologue.kind->allows(BookPlace::Main));
    CHECK_FALSE(prologue.kind->allows(BookPlace::Back));

    // A prologue opens the story and an epilogue closes it, in the book or in a part of it
    CHECK(prologue.kind->position == KindPosition::Start);
    CHECK(prologue.kind->allowsInside(QStringLiteral("part")));
    CHECK(chapter.kind->position == KindPosition::Any);

    // Stories use the novel too, for its styles
    const KindRef epilogue =
        registry.findKind(QStringLiteral("kalahari.short_stories"), QStringLiteral("epilogue"));
    REQUIRE(epilogue);
    CHECK(epilogue.package->id == QStringLiteral("kalahari.novel"));
    CHECK(epilogue.kind->position == KindPosition::End);
    CHECK(epilogue.kind->allowsInside(QStringLiteral("part")));
    CHECK(registry.findKind(QStringLiteral("kalahari.nonfiction"), QStringLiteral("conclusion"))
              .kind->position == KindPosition::End);

    CHECK_FALSE(registry.findKind(QStringLiteral("kalahari.novel"), QStringLiteral("act")));
    CHECK_FALSE(registry.findKind(QStringLiteral("no.such.type"), QStringLiteral("chapter")));
    CHECK(registry.kindsIn(QStringLiteral("no.such.type"), BookPlace::Main).isEmpty());
    CHECK(registry.startElements(QStringLiteral("no.such.type")).isEmpty());

    const KindRef toc = registry.findKind(QStringLiteral("kalahari.base"), QStringLiteral("toc"));
    REQUIRE(toc);
    CHECK(toc.kind->form == ElementForm::Window);
    CHECK(toc.kind->generated);
    CHECK(toc.kind->editor == QStringLiteral("toc"));
    CHECK(toc.kind->allows(BookPlace::Front));
    CHECK(toc.kind->allows(BookPlace::Back));
}

TEST_CASE("Built-in book types: the palette offers every kind of every package",
          "[core][booktypes]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    qsizetype kindCount = 0;
    for (const BookTypePackage* package : registry.packages()) {
        kindCount += package->kinds.size();
    }
    const QList<KindRef> all = registry.allKinds();
    CHECK(all.size() == kindCount);

    QStringList qualified;
    for (const KindRef& ref : all) {
        REQUIRE(ref);
        qualified << ref.package->id + QLatin1Char('/') + ref.kind->id;
    }
    qualified.removeDuplicates();
    CHECK(qualified.size() == kindCount);
    for (const char* kind : {"kalahari.base/chapter", "kalahari.base/material",
                             "kalahari.novel/prologue", "kalahari.screenplay/act",
                             "kalahari.poetry/poem", "kalahari.nonfiction/source"}) {
        INFO(kind);
        CHECK(qualified.contains(QString::fromLatin1(kind)));
    }
    // A package's kinds in the order of its lists
    CHECK(qualified.indexOf(QStringLiteral("kalahari.base/title_page")) <
          qualified.indexOf(QStringLiteral("kalahari.base/chapter")));
}

TEST_CASE("Built-in book types: groups of the Workshop and the parts layer",
          "[core][booktypes]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    // Kinds of the Workshop, each with the group it goes to when the Workshop is grouped
    const auto groups = [&registry](const char* type) {
        QStringList kinds;
        for (const KindRef& ref :
             registry.kindsIn(QString::fromLatin1(type), BookPlace::Workshop)) {
            const QString group = workshopGroupName(ref.kind->workshopGroup);
            kinds << (group.isEmpty() ? ref.kind->id : ref.kind->id + QLatin1Char('/') + group);
        }
        return joined(kinds);
    };
    CHECK(groups("kalahari.novel") ==
          "work_note, mindmap, timeline, character/libraries, location/libraries, item/libraries, "
          "material/resources");
    CHECK(groups("kalahari.nonfiction") ==
          "source/libraries, work_note, timeline, mindmap, material/resources");
    CHECK(groups("kalahari.screenplay") ==
          "logline, synopsis, treatment, step_outline, character/libraries, location/libraries, "
          "work_note, material/resources");
    CHECK(workshopGroupFromName(QStringLiteral("libraries")) == WorkshopGroup::Libraries);
    CHECK_FALSE(workshopGroupFromName(QStringLiteral("archive")));
    CHECK(workshopGroupName(WorkshopGroup::None).isEmpty());

    // A screenplay starts without the front, main and back parts; the other types with them
    QStringList withParts;
    for (const BookTypePackage* type : registry.bookTypes()) {
        if (type->partsLayer) {
            withParts << type->id;
        }
    }
    CHECK(joined(withParts) ==
          "kalahari.nonfiction, kalahari.novel, kalahari.poetry, kalahari.short_stories");
}

TEST_CASE("Built-in book types: default titles", "[core][booktypes]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);
    const auto kind = [&registry](const char* package, const char* id) {
        const KindRef ref =
            registry.findKind(QString::fromLatin1(package), QString::fromLatin1(id));
        REQUIRE(ref);
        return ref.kind;
    };
    const QString pl = QStringLiteral("pl");
    const QString en = QStringLiteral("en");

    CHECK(kind("kalahari.base", "chapter")->defaultTitle(pl, 3) == QStringLiteral("Rozdział 3"));
    CHECK(kind("kalahari.base", "chapter")->defaultTitle(en, 3) == QStringLiteral("Chapter 3"));
    CHECK(kind("kalahari.base", "part")->defaultTitle(pl, 4) == QStringLiteral("Część IV"));
    CHECK(kind("kalahari.base", "part")->defaultTitle(en, 14) == QStringLiteral("Part XIV"));
    CHECK(kind("kalahari.screenplay", "act")->defaultTitle(pl, 1) == QStringLiteral("Akt I"));
    CHECK(kind("kalahari.screenplay", "act")->defaultTitle(en, 1999) ==
          QStringLiteral("Act MCMXCIX"));
    CHECK(kind("kalahari.poetry", "poem")->defaultTitle(pl, 1) == QStringLiteral("Wiersz 1"));
    CHECK(kind("kalahari.short_stories", "story")->defaultTitle(pl, 1) ==
          QStringLiteral("Opowiadanie 1"));
    // Roman numerals end at 3999
    CHECK(kind("kalahari.base", "part")->defaultTitle(en, 4000) == QStringLiteral("Part 4000"));
    // Without a title, the name
    CHECK(kind("kalahari.base", "title_page")->defaultTitle(pl, 1) ==
          QStringLiteral("Strona tytułowa"));
    // The language of a region, and a language the package does not have
    CHECK(kind("kalahari.base", "chapter")->defaultTitle(QStringLiteral("pl_PL"), 2) ==
          QStringLiteral("Rozdział 2"));
    CHECK(kind("kalahari.base", "chapter")->defaultTitle(QStringLiteral("de"), 2) ==
          QStringLiteral("Chapter 2"));
}

TEST_CASE("Built-in book types: styles of a type with those of the packages it uses",
          "[core][booktypes]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);
    const auto paragraphs = [&registry](const char* type) {
        return registry.paragraphStyles(QString::fromLatin1(type));
    };

    CHECK(styleIds(paragraphs("kalahari.base")) ==
          "normal, heading_1, heading_2, heading_3, quote, dialogue");
    CHECK(styleIds(registry.characterStyles(QStringLiteral("kalahari.base"))) ==
          "emphasis, strong");

    CHECK(styleIds(paragraphs("kalahari.novel")) ==
          "normal, heading_1, heading_2, heading_3, quote, dialogue, part_title, chapter_title, "
          "motto, letter");
    CHECK(styleIds(registry.characterStyles(QStringLiteral("kalahari.novel"))) ==
          "emphasis, strong, thought, proper_name");

    // As in the novel, and the title, subtitle and author of a story
    CHECK(styleIds(paragraphs("kalahari.short_stories")) ==
          "normal, heading_1, heading_2, heading_3, quote, dialogue, part_title, chapter_title, "
          "motto, letter, story_title, story_subtitle, story_author");

    // The screenplay's dialogue replaces the Base one where it stands
    const QList<PackageParagraphStyle> screenplay = paragraphs("kalahari.screenplay");
    CHECK(styleIds(screenplay) ==
          "normal, heading_1, heading_2, heading_3, quote, dialogue, action, scene_heading, "
          "character_cue, parenthetical, transition");
    const PackageParagraphStyle* dialogue = findStyle(screenplay, QStringLiteral("dialogue"));
    REQUIRE(dialogue != nullptr);
    CHECK(dialogue->baseStyle == QStringLiteral("action"));
    CHECK(dialogue->nextStyle == QStringLiteral("action"));
    const PackageParagraphStyle* action = findStyle(screenplay, QStringLiteral("action"));
    REQUIRE(action != nullptr);
    CHECK(action->properties.value(QStringLiteral("fontFamily")).toString() ==
          QStringLiteral("Courier New"));
    CHECK(action->properties.value(QStringLiteral("fontSize")).toInt() == 12);
    // After a character, dialogue; after dialogue, action
    const PackageParagraphStyle* cue = findStyle(screenplay, QStringLiteral("character_cue"));
    REQUIRE(cue != nullptr);
    CHECK(cue->nextStyle == QStringLiteral("dialogue"));

    // A verse is neither justified nor indented
    const QList<PackageParagraphStyle> poetry = paragraphs("kalahari.poetry");
    const PackageParagraphStyle* verse = findStyle(poetry, QStringLiteral("verse"));
    REQUIRE(verse != nullptr);
    CHECK(verse->properties.value(QStringLiteral("alignment")).toString() ==
          QStringLiteral("left"));
    CHECK(verse->properties.value(QStringLiteral("firstLineIndent")).toDouble() == 0.0);
}

TEST_CASE("Built-in book types: every text is in Polish and English", "[core][booktypes]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);

    const auto check = [](const LocalizedText& text, const QString& where) {
        INFO(where.toStdString());
        CHECK_FALSE(text.values.value(QStringLiteral("pl")).isEmpty());
        CHECK_FALSE(text.values.value(QStringLiteral("en")).isEmpty());
    };
    for (const BookTypePackage* package : registry.packages()) {
        check(package->name, package->id + QStringLiteral(" name"));
        check(package->description, package->id + QStringLiteral(" description"));
        for (const ElementKind& kind : package->kinds) {
            check(kind.name, package->id + QStringLiteral(" kind ") + kind.id);
            check(kind.plural, package->id + QStringLiteral(" kind ") + kind.id);
            if (!kind.title.isEmpty()) {
                check(kind.title, package->id + QStringLiteral(" kind ") + kind.id);
            }
        }
        for (const PackageParagraphStyle& style : package->paragraphStyles) {
            check(style.name, package->id + QStringLiteral(" style ") + style.id);
        }
        for (const PackageCharacterStyle& style : package->characterStyles) {
            check(style.name, package->id + QStringLiteral(" style ") + style.id);
        }
    }
}

TEST_CASE("Built-in book types: every icon is an icon of the program", "[core][booktypes]") {
    BookTypeRegistry registry;
    loadBuiltIn(registry);
    kalahari::gui::registerAllIcons();
    const IconRegistry& icons = IconRegistry::getInstance();

    for (const BookTypePackage* package : registry.packages()) {
        INFO(package->id.toStdString());
        CHECK(icons.hasIcon(package->icon));
        for (const ElementKind& kind : package->kinds) {
            INFO(kind.id.toStdString() + " icon " + kind.icon.toStdString());
            CHECK(icons.hasIcon(kind.icon));
        }
    }
}

// =============================================================================
// Loading
// =============================================================================

TEST_CASE("Book type packages: test packages load and resolve", "[core][booktypes]") {
    PackageFolder folder;
    folder.writeTestPackages();
    BookTypeRegistry registry;
    registry.load({folder.path()});
    INFO(problemsOf(registry));
    REQUIRE(registry.problems().isEmpty());

    REQUIRE(registry.package(QStringLiteral("test.type")) != nullptr);
    CHECK(kindIds(registry.kindsIn(QStringLiteral("test.type"), BookPlace::Main)) ==
          "prologue, part, chapter");
    CHECK(registry.kindsIn(QStringLiteral("test.type"), BookPlace::Back).isEmpty());
    CHECK(styleIds(registry.paragraphStyles(QStringLiteral("test.type"))) == "normal, heading");
    CHECK(registry.package(QStringLiteral("test.type"))->directory ==
          QDir(folder.path()).absoluteFilePath(QStringLiteral("test.type")));
}

TEST_CASE("Book type packages: a folder without booktype.json is not a package",
          "[core][booktypes]") {
    PackageFolder folder;
    folder.writeTestPackages();
    folder.write(QStringLiteral("notes"), QStringLiteral("readme.txt"),
                 QByteArray("Not a package"));
    BookTypeRegistry registry;
    registry.load({folder.path(), folder.path() + QStringLiteral("/no-such-folder")});
    CHECK(registry.problems().isEmpty());
    CHECK(registry.packages().size() == 2);
}

TEST_CASE("Book type packages: a package uses packages from another folder",
          "[core][booktypes]") {
    PackageFolder builtIn;
    PackageFolder addons;
    builtIn.write(QStringLiteral("test.base"), QStringLiteral("booktype.json"), testBase());
    builtIn.write(QStringLiteral("test.base"), QStringLiteral("styles.json"), testBaseStyles());
    addons.write(QStringLiteral("test.type"), QStringLiteral("booktype.json"), testType());

    BookTypeRegistry registry;
    registry.load({builtIn.path(), addons.path()});
    INFO(problemsOf(registry));
    CHECK(registry.problems().isEmpty());
    CHECK(registry.package(QStringLiteral("test.type")) != nullptr);

    // Loading again replaces what was loaded
    registry.load({builtIn.path()});
    CHECK(registry.package(QStringLiteral("test.type")) == nullptr);
    CHECK(registry.package(QStringLiteral("test.base")) != nullptr);
}

TEST_CASE("Book type packages: of two packages with one id, the first is loaded",
          "[core][booktypes]") {
    PackageFolder builtIn;
    PackageFolder addons;
    builtIn.writeTestPackages();
    QJsonObject newer = testBase();
    newer.insert(QStringLiteral("version"), QStringLiteral("2.0"));
    addons.write(QStringLiteral("test.base"), QStringLiteral("booktype.json"), newer);
    addons.write(QStringLiteral("test.base"), QStringLiteral("styles.json"), testBaseStyles());

    BookTypeRegistry registry;
    registry.load({builtIn.path(), addons.path()});
    REQUIRE(registry.package(QStringLiteral("test.base")) != nullptr);
    CHECK(registry.package(QStringLiteral("test.base"))->version == QStringLiteral("1.0"));
    INFO(problemsOf(registry));
    CHECK(hasProblem(registry, QStringLiteral("test.base"),
                     QStringLiteral("id: package 'test.base' is already loaded from")));
}

TEST_CASE("Book type packages: a type replaces a kind of a package it uses",
          "[core][booktypes]") {
    PackageFolder folder;
    QJsonObject type = testType();
    setAt(type, QStringLiteral("kinds.title_page"), jsonOf(R"({
        "form": "text", "places": ["front"], "limit": 1,
        "name": { "en": "Industry title page" }, "plural": { "en": "Title pages" } })"));
    folder.writeTestPackages(type);
    BookTypeRegistry registry;
    registry.load({folder.path()});
    INFO(problemsOf(registry));
    REQUIRE(registry.problems().isEmpty());

    const KindRef titlePage =
        registry.findKind(QStringLiteral("test.type"), QStringLiteral("title_page"));
    REQUIRE(titlePage);
    CHECK(titlePage.package->id == QStringLiteral("test.type"));
    CHECK(titlePage.kind->name.text(QStringLiteral("en")) == QStringLiteral("Industry title page"));
    // The palette offers both
    int titlePages = 0;
    for (const KindRef& ref : registry.allKinds()) {
        titlePages += ref.kind->id == QStringLiteral("title_page") ? 1 : 0;
    }
    CHECK(titlePages == 2);
}

TEST_CASE("Book type packages: of two used packages, the later one in \"uses\" comes first",
          "[core][booktypes]") {
    PackageFolder folder;
    folder.writeTestPackages();
    QJsonObject extra = jsonOf(R"({
      "format": 1, "id": "test.extra", "version": "1.0", "role": "shared",
      "name": { "en": "Extra" },
      "kinds": {
        "chapter": { "form": "text", "places": ["main", "part"],
                     "name": { "en": "Extra chapter" }, "plural": { "en": "Extra chapters" } },
        "part": { "form": "group", "places": ["main"],
                  "name": { "en": "Extra part" }, "plural": { "en": "Extra parts" } }
      },
      "main": ["chapter"]
    })");
    folder.write(QStringLiteral("test.extra"), QStringLiteral("booktype.json"), extra);
    QJsonObject type = testType();
    type.insert(QStringLiteral("uses"), QJsonArray{QStringLiteral("test.base"),
                                                   QStringLiteral("test.extra")});
    folder.write(QStringLiteral("test.type"), QStringLiteral("booktype.json"), type);

    BookTypeRegistry registry;
    registry.load({folder.path()});
    INFO(problemsOf(registry));
    REQUIRE(registry.problems().isEmpty());
    const KindRef chapter =
        registry.findKind(QStringLiteral("test.type"), QStringLiteral("chapter"));
    REQUIRE(chapter);
    CHECK(chapter.package->id == QStringLiteral("test.extra"));
}

TEST_CASE("Book type packages: a kind named with its package", "[core][booktypes]") {
    // Package and kind, "-" for text that is not a kind name
    const auto parsed = [](const char* text) {
        const std::optional<KindReference> reference =
            KindReference::parse(QString::fromLatin1(text));
        return reference ? (reference->packageId + QLatin1Char('|') + reference->kindId)
                               .toStdString()
                         : std::string("-");
    };
    CHECK(parsed("chapter") == "|chapter");
    CHECK(parsed("kalahari.nonfiction:bibliography") == "kalahari.nonfiction|bibliography");
    for (const char* text : {"", ":", "chapter:", ":chapter", "nonfiction:bibliography",
                             "Kalahari.nonfiction:bibliography", "kalahari.nonfiction:Bibliography",
                             "kalahari.nonfiction:bibliography:index"}) {
        INFO(text);
        CHECK(parsed(text) == "-");
    }
    CHECK(KindReference{QStringLiteral("kalahari.base"), QStringLiteral("chapter")}.toString() ==
          QStringLiteral("kalahari.base:chapter"));
    CHECK(KindReference{QString(), QStringLiteral("chapter")}.toString() ==
          QStringLiteral("chapter"));
    CHECK(KindRef{}.reference().isEmpty());
}

TEST_CASE("Book type packages: a type takes kinds of a package it does not use",
          "[core][booktypes]") {
    PackageFolder folder;
    QJsonObject type = testType();
    type.insert(QStringLiteral("back"), QJsonArray{QStringLiteral("test.other:appendix")});
    type.insert(QStringLiteral("start"),
                QJsonArray{QStringLiteral("title_page"), QStringLiteral("test.base:chapter"),
                           QStringLiteral("test.other:appendix")});
    folder.writeTestPackages(type);
    // Two packages can take each other's kinds
    QJsonObject other = otherType();
    other.insert(QStringLiteral("main"),
                 QJsonArray{QStringLiteral("chapter"), QStringLiteral("test.type:prologue")});
    folder.writeOtherType(other);

    BookTypeRegistry registry;
    registry.load({folder.path()});
    INFO(problemsOf(registry));
    REQUIRE(registry.problems().isEmpty());

    const QList<KindRef> back = registry.kindsIn(QStringLiteral("test.type"), BookPlace::Back);
    REQUIRE(back.size() == 1);
    CHECK(back.first().reference() == QStringLiteral("test.other:appendix"));
    CHECK(registry.findKind(QStringLiteral("test.type"), QStringLiteral("test.other:appendix"))
              .reference() == QStringLiteral("test.other:appendix"));
    CHECK(kindIds(registry.kindsIn(QStringLiteral("test.other"), BookPlace::Main)) ==
          "chapter, prologue");
    // A kind as the other package sees it
    CHECK(registry.findKind(QStringLiteral("test.type"), QStringLiteral("test.other:chapter"))
              .reference() == QStringLiteral("test.base:chapter"));
    // The kind alone: neither the other kinds of its package nor its styles
    CHECK_FALSE(registry.findKind(QStringLiteral("test.type"), QStringLiteral("appendix")));
    CHECK(styleIds(registry.paragraphStyles(QStringLiteral("test.type"))) == "normal, heading");
    CHECK(styleIds(registry.paragraphStyles(QStringLiteral("test.other"))) ==
          "normal, heading, appendix_title");

    // Elements to start with, named either way, in the place of the list that has them
    QStringList start;
    for (const StartElement& element : registry.startElements(QStringLiteral("test.type"))) {
        start << element.kind.reference() + QLatin1Char('/') + bookPlaceName(element.place);
    }
    CHECK(joined(start) ==
          "test.base:title_page/front, test.base:chapter/main, test.other:appendix/back");

    // The palette has each kind once, with the package that defines it
    QStringList palette;
    for (const KindRef& ref : registry.allKinds()) {
        palette << ref.reference();
    }
    CHECK(joined(palette) ==
          "test.base:title_page, test.base:chapter, test.base:part, test.base:note, "
          "test.base:mindmap, test.other:appendix, test.type:prologue");
}

TEST_CASE("Book type packages: a template file of the package", "[core][booktypes]") {
    PackageFolder folder;
    QJsonObject type = testType();
    setAt(type, QStringLiteral("kinds.prologue.template"),
          QStringLiteral("templates/prologue.kchapter"));
    folder.writeTestPackages(type);
    folder.write(QStringLiteral("test.type"), QStringLiteral("templates/prologue.kchapter"),
                 QByteArray("{}"));
    BookTypeRegistry registry;
    registry.load({folder.path()});
    INFO(problemsOf(registry));
    REQUIRE(registry.problems().isEmpty());
    CHECK(registry.findKind(QStringLiteral("test.type"), QStringLiteral("prologue"))
              .kind->templateFile == QStringLiteral("templates/prologue.kchapter"));
}

TEST_CASE("Book type packages: a kind at the start or at the end of the main part",
          "[core][booktypes]") {
    PackageFolder folder;
    QJsonObject type = testType();
    setAt(type, QStringLiteral("kinds.prologue.position"), QStringLiteral("start"));
    setAt(type, QStringLiteral("kinds.epilogue"), jsonOf(R"({
        "form": "text", "places": ["main", "part"], "position": "end",
        "name": { "en": "Epilogue" }, "plural": { "en": "Epilogues" } })"));
    setAt(type, QStringLiteral("main"),
          QJsonArray{QStringLiteral("prologue"), QStringLiteral("part"),
                     QStringLiteral("chapter"), QStringLiteral("epilogue")});
    folder.writeTestPackages(type);
    BookTypeRegistry registry;
    registry.load({folder.path()});
    INFO(problemsOf(registry));
    REQUIRE(registry.problems().isEmpty());

    const auto positionOf = [&registry](const char* kindId) {
        const KindRef ref =
            registry.findKind(QStringLiteral("test.type"), QString::fromLatin1(kindId));
        REQUIRE(ref);
        return ref.kind->position;
    };
    CHECK(positionOf("prologue") == KindPosition::Start);
    CHECK(positionOf("epilogue") == KindPosition::End);
    CHECK(positionOf("chapter") == KindPosition::Any);
    CHECK(positionOf("part") == KindPosition::Any);
}

// =============================================================================
// Packages with problems
// =============================================================================

TEST_CASE("Book type packages: a type with a problem is not loaded", "[core][booktypes]") {
    struct Broken {
        const char* what;
        std::function<void(QJsonObject&)> change;
        const char* problem;
    };
    const auto set = [](const char* path, const QJsonValue& value) {
        return [path, value](QJsonObject& type) { setAt(type, QString::fromLatin1(path), value); };
    };
    const auto jsonValue = [](const char* text) { return QJsonValue(jsonOf(text)); };
    const auto list = [](std::initializer_list<const char*> items) {
        QJsonArray array;
        for (const char* item : items) {
            array.append(QString::fromLatin1(item));
        }
        return QJsonValue(array);
    };
    const QJsonValue removed(QJsonValue::Undefined);

    const std::vector<Broken> cases = {
        {"a newer format", set("format", 2), "format: must be 1"},
        {"an id that is not lowercase words and dots", set("id", "Test Type"),
         "id: must be lowercase words joined by dots"},
        {"a version that is not numbers", set("version", "one"), "version: must be a version"},
        {"an unknown role", set("role", "book"), "role: must be \"type\""},
        {"a misspelt key", set("primray", "chapter"), "booktype.json: unknown key 'primray'"},
        {"a name without English", set("name", jsonValue(R"({ "pl": "Typ" })")),
         "name: the English text (\"en\") is missing"},
        {"an empty name", set("name.en", " "), "name.en: must be a text that is not empty"},
        {"a name in no language", set("name.Polish", "Typ"),
         "name: 'Polish' is not a language code"},
        {"a kind id with capitals", set("kinds.Prologue", jsonValue(R"({ "form": "text" })")),
         "kinds.Prologue: a kind id has lowercase letters"},
        {"a kind named like a place", set("kinds.front", jsonValue(R"({ "form": "text" })")),
         "kinds.front: is the name of a place"},
        {"a misspelt key of a kind", set("kinds.prologue.palces", list({"main"})),
         "kinds.prologue: unknown key 'palces'"},
        {"an unknown form", set("kinds.prologue.form", "page"),
         "kinds.prologue.form: must be \"text\", \"group\" or \"window\""},
        {"a kind without a plural", set("kinds.prologue.plural", removed),
         "kinds.prologue.plural: is missing"},
        {"a kind without places", set("kinds.prologue.places", list({})),
         "kinds.prologue.places: must list where the kind can be"},
        {"an unknown place", set("kinds.prologue.places", list({"main", "prelude"})),
         "kinds.prologue.places: 'prelude' is neither a place"},
        {"a kind inside a text kind", set("kinds.prologue.places", list({"main", "chapter"})),
         "kinds.prologue.places: 'chapter' is not a group kind"},
        {"a group in the front part",
         set("kinds.volume",
             jsonValue(R"({ "form": "group", "places": ["front"], "name": { "en": "Volume" },
                       "plural": { "en": "Volumes" } })")),
         "kinds.volume.places: a group can only be in the main part"},
        {"a limit of 0", set("kinds.prologue.limit", 0),
         "kinds.prologue.limit: must be a whole number of at least 1"},
        {"a limit that is not whole", set("kinds.prologue.limit", 1.5),
         "kinds.prologue.limit: must be a whole number of at least 1"},
        {"a number in one language only",
         set("kinds.prologue.title", jsonValue(R"({ "en": "Prologue %n", "pl": "Prolog" })")),
         "kinds.prologue.title: \"%n\" must be in the title in every language or in none"},
        {"numbering without a number", set("kinds.prologue.numbering", "roman"),
         "kinds.prologue.numbering: needs \"%n\" in the title"},
        {"a template that is not in the package",
         set("kinds.prologue.template", "templates/prologue.kchapter"),
         "kinds.prologue.template: file 'templates/prologue.kchapter' is not in the package"},
        {"a template outside the package",
         set("kinds.prologue.template", "../test.base/prologue.kchapter"),
         "kinds.prologue.template: must be a path inside the package folder"},
        {"a template that is not a chapter", set("kinds.prologue.template", "booktype.json"),
         "kinds.prologue.template: must be a .kchapter file"},
        {"a window kind without its window",
         set("kinds.map", jsonValue(R"({ "form": "window", "places": ["workshop"],
                                    "name": { "en": "Map" }, "plural": { "en": "Maps" } })")),
         "kinds.map.editor: a window kind needs the id of the window"},
        {"a template of a window kind",
         set("kinds.map", jsonValue(R"({ "form": "window", "editor": "map", "places": ["workshop"],
                                    "template": "map.kchapter",
                                    "name": { "en": "Map" }, "plural": { "en": "Maps" } })")),
         "kinds.map.template: only a text kind has a template"},
        {"an unknown position", set("kinds.prologue.position", "middle"),
         "kinds.prologue.position: must be \"start\" or \"end\""},
        {"a position outside the main part",
         set("kinds.motto", jsonValue(R"({ "form": "text", "places": ["front"], "position": "end",
                                      "name": { "en": "Motto" }, "plural": { "en": "Mottos" } })")),
         "kinds.motto.position: only a kind of the main part has a position"},
        {"a window of a text kind", set("kinds.prologue.editor", "text"),
         "kinds.prologue.editor: only a window kind has an editor"},
        {"a text kind made by a tool", set("kinds.prologue.generated", true),
         "kinds.prologue.generated: only a window kind can be made by a tool"},
        {"settings of a text kind", set("kinds.prologue.settings", QJsonObject()),
         "kinds.prologue.settings: only a window kind has settings"},
        {"an unknown group of the Workshop",
         set("kinds.notebook",
             jsonValue(R"({ "form": "text", "places": ["workshop"], "workshopGroup": "archive",
                       "name": { "en": "Notebook" }, "plural": { "en": "Notebooks" } })")),
         "kinds.notebook.workshopGroup: must be \"libraries\" or \"resources\""},
        {"a group of the Workshop for a kind outside it",
         set("kinds.prologue.workshopGroup", "libraries"),
         "kinds.prologue.workshopGroup: only a kind of the Workshop goes to a group of the "
         "Workshop"},
        {"a parts layer that is not true or false", set("partsLayer", "yes"),
         "partsLayer: must be true or false"},
        {"a kind name that is not one", set("main", list({"prologue", "chapter", "test.base:"})),
         "main: 'test.base:' is not a kind id"},
        {"a kind of a package that is not there", set("back", list({"test.missing:appendix"})),
         "back: kind 'test.missing:appendix' is in package 'test.missing', which is not "
         "installed"},
        {"an unknown kind of another package", set("back", list({"test.base:appendix"})),
         "back: unknown kind 'test.base:appendix'"},
        {"one kind named in two ways",
         set("main", list({"prologue", "chapter", "test.base:chapter"})),
         "main: 'test.base:chapter' is a kind that is already in the list"},
        {"a main text kind of another package outside the main part",
         set("primary", "test.base:note"),
         "primary: 'test.base:note' is not in the list of the main part"},
        {"an unknown kind in a list", set("main", list({"prologue", "chapter", "epilogue"})),
         "main: unknown kind 'epilogue'"},
        {"a kind listed twice", set("main", list({"chapter", "chapter"})),
         "main: 'chapter' is listed twice"},
        {"a kind in a place it does not allow", set("front", list({"title_page", "chapter"})),
         "front: kind 'chapter' cannot be here"},
        {"a group nothing can be inside",
         [](QJsonObject& type) {
             setAt(type, QStringLiteral("kinds.volume"), jsonOf(R"({
                 "form": "group", "places": ["main"],
                 "name": { "en": "Volume" }, "plural": { "en": "Volumes" } })"));
             setAt(type, QStringLiteral("main"),
                   QJsonArray{QStringLiteral("chapter"), QStringLiteral("volume")});
         },
         "main: no kind of this package can be inside group 'volume'"},
        {"a type without its main text kind", set("primary", removed),
         "primary: a book type needs its main text kind"},
        {"a group as the main text kind", set("primary", "part"),
         "primary: 'part' is not a text kind"},
        {"a main text kind outside the main part", set("primary", "title_page"),
         "primary: 'title_page' is not in the list of the main part"},
        {"an unknown main text kind", set("primary", "poem"), "primary: unknown kind 'poem'"},
        {"an unknown kind at the start", set("start", list({"title_page", "poem"})),
         "start: unknown kind 'poem'"},
        {"a kind at the start that the type does not offer",
         set("start", list({"title_page", "mindmap"})),
         "start: kind 'mindmap' is in none of the lists"},
        {"a group at the start", set("start", list({"part"})), "start: 'part' is a group"},
        {"more at the start than the limit",
         set("start", list({"title_page", "title_page", "chapter"})),
         "start: more elements of kind 'title_page' than its limit of 1"},
        {"a missing package in uses", set("uses", list({"test.base", "test.missing"})),
         "uses: package 'test.missing' is not installed"},
        {"a package that uses itself", set("uses", list({"test.base", "test.type"})),
         "uses: a package cannot use itself"},
        {"a styles file that is not there", set("styles", "styles.json"),
         "styles: file 'styles.json' is not in the package"},
    };

    for (const Broken& broken : cases) {
        INFO(broken.what);
        PackageFolder folder;
        QJsonObject type = testType();
        broken.change(type);
        folder.writeTestPackages(type);

        BookTypeRegistry registry;
        registry.load({folder.path()});
        INFO(problemsOf(registry));
        CHECK(registry.package(QStringLiteral("test.type")) == nullptr);
        CHECK(registry.package(QStringLiteral("test.base")) != nullptr);
        CHECK(hasProblem(registry, QStringLiteral("test.type"), QString::fromUtf8(broken.problem)));
    }
}

TEST_CASE("Book type packages: styles with a problem", "[core][booktypes]") {
    struct Broken {
        const char* what;
        const char* styles;
        const char* problem;
    };
    const std::vector<Broken> cases = {
        {"not JSON", R"({ "paragraph_styles": [ )", "styles.json: not valid JSON"},
        {"a misspelt list", R"({ "paragraph_style": [] })",
         "styles.json: unknown key 'paragraph_style'"},
        {"an unknown property",
         R"({ "paragraph_styles": [ { "id": "title", "name": { "en": "Title" },
                                      "properties": { "fontsize": 12 } } ] })",
         "paragraph_styles.title.properties: unknown property 'fontsize'"},
        {"an unknown alignment",
         R"({ "paragraph_styles": [ { "id": "title", "name": { "en": "Title" },
                                      "properties": { "alignment": "middle" } } ] })",
         "paragraph_styles.title.properties.alignment: must be \"left\""},
        {"a font size of 0",
         R"({ "paragraph_styles": [ { "id": "title", "name": { "en": "Title" },
                                      "properties": { "fontSize": 0 } } ] })",
         "paragraph_styles.title.properties.fontSize: must be a whole number of points"},
        {"a negative margin",
         R"({ "paragraph_styles": [ { "id": "title", "name": { "en": "Title" },
                                      "properties": { "leftMargin": -5 } } ] })",
         "paragraph_styles.title.properties.leftMargin: must be a number of points, at least 0"},
        {"a color that is not a color",
         R"({ "character_styles": [ { "id": "mark", "name": { "en": "Mark" },
                                      "properties": { "textColor": "red" } } ] })",
         "character_styles.mark.properties.textColor: must be a color"},
        {"a paragraph property of a character style",
         R"({ "character_styles": [ { "id": "mark", "name": { "en": "Mark" },
                                      "properties": { "alignment": "left" } } ] })",
         "character_styles.mark.properties: unknown property 'alignment'"},
        {"a base style of a character style",
         R"({ "character_styles": [ { "id": "mark", "name": { "en": "Mark" },
                                      "base_style": "emphasis" } ] })",
         "character_styles.mark: unknown key 'base_style'"},
        {"a style without a name", R"({ "paragraph_styles": [ { "id": "title" } ] })",
         "paragraph_styles.title.name: is missing"},
        {"a style without an id", R"({ "paragraph_styles": [ { "name": { "en": "Title" } } ] })",
         "paragraph_styles[0]: needs an id"},
        {"a style defined twice",
         R"({ "paragraph_styles": [ { "id": "title", "name": { "en": "Title" } },
                                    { "id": "title", "name": { "en": "Title 2" } } ] })",
         "paragraph_styles.title: is defined twice"},
        {"an unknown base style",
         R"({ "paragraph_styles": [ { "id": "title", "name": { "en": "Title" },
                                      "base_style": "nrmal" } ] })",
         "paragraph_styles.title.base_style: unknown paragraph style 'nrmal'"},
        {"an unknown next style",
         R"({ "paragraph_styles": [ { "id": "title", "name": { "en": "Title" },
                                      "next_style": "body" } ] })",
         "paragraph_styles.title.next_style: unknown paragraph style 'body'"},
        {"a style that inherits from itself",
         R"({ "paragraph_styles": [ { "id": "title", "name": { "en": "Title" },
                                      "base_style": "title" } ] })",
         "paragraph_styles.title.base_style: a style cannot inherit from itself"},
        {"inheritance in a circle through a used package",
         R"({ "paragraph_styles": [ { "id": "normal", "name": { "en": "Normal" },
                                      "base_style": "heading" } ] })",
         "paragraph_styles.normal.base_style: inheritance goes round in a circle: "
         "normal > heading > normal"},
    };

    for (const Broken& broken : cases) {
        INFO(broken.what);
        PackageFolder folder;
        QJsonObject type = testType();
        type.insert(QStringLiteral("styles"), QStringLiteral("styles.json"));
        folder.writeTestPackages(type);
        folder.write(QStringLiteral("test.type"), QStringLiteral("styles.json"),
                     QByteArray(broken.styles));

        BookTypeRegistry registry;
        registry.load({folder.path()});
        INFO(problemsOf(registry));
        CHECK(registry.package(QStringLiteral("test.type")) == nullptr);
        CHECK(hasProblem(registry, QStringLiteral("test.type"), QString::fromUtf8(broken.problem)));
    }
}

TEST_CASE("Book type packages: a package with a problem takes down the packages that use it",
          "[core][booktypes]") {
    PackageFolder folder;
    folder.writeTestPackages();
    QJsonObject base = testBase();

    SECTION("A package that cannot be read") {
        base.insert(QStringLiteral("start"), QJsonArray{QStringLiteral("chapter")});
        folder.write(QStringLiteral("test.base"), QStringLiteral("booktype.json"), base);
        BookTypeRegistry registry;
        registry.load({folder.path()});
        INFO(problemsOf(registry));
        CHECK(registry.packages().isEmpty());
        CHECK(hasProblem(registry, QStringLiteral("test.base"),
                         QStringLiteral("start: only a book type has elements to start with")));
        CHECK(hasProblem(registry, QStringLiteral("test.type"),
                         QStringLiteral("uses: package 'test.base' is not installed or could not "
                                        "be loaded")));
    }

    SECTION("A package with a problem found with the packages it uses") {
        base.insert(QStringLiteral("primary"), QStringLiteral("part"));
        folder.write(QStringLiteral("test.base"), QStringLiteral("booktype.json"), base);
        BookTypeRegistry registry;
        registry.load({folder.path()});
        INFO(problemsOf(registry));
        CHECK(registry.packages().isEmpty());
        CHECK(hasProblem(registry, QStringLiteral("test.base"),
                         QStringLiteral("primary: 'part' is not a text kind")));
        CHECK(hasProblem(registry, QStringLiteral("test.type"),
                         QStringLiteral("uses: package 'test.base' has problems")));
    }
}

TEST_CASE("Book type packages: a package with a problem takes down the packages that take its "
          "kinds",
          "[core][booktypes]") {
    PackageFolder folder;
    QJsonObject type = testType();
    type.insert(QStringLiteral("back"), QJsonArray{QStringLiteral("test.other:appendix")});
    folder.writeTestPackages(type);
    QJsonObject other = otherType();
    other.insert(QStringLiteral("primary"), QStringLiteral("appendix"));
    folder.writeOtherType(other);
    // And the packages that use a package taken down
    folder.write(QStringLiteral("test.third"), QStringLiteral("booktype.json"), jsonOf(R"({
      "format": 1, "id": "test.third", "version": "1.0", "role": "type",
      "name": { "en": "Third type" }, "uses": ["test.type"],
      "main": ["chapter"], "primary": "chapter"
    })"));

    BookTypeRegistry registry;
    registry.load({folder.path()});
    INFO(problemsOf(registry));
    CHECK(registry.packages().size() == 1);
    CHECK(registry.package(QStringLiteral("test.base")) != nullptr);
    CHECK(hasProblem(registry, QStringLiteral("test.other"),
                     QStringLiteral("primary: 'appendix' is not in the list of the main part")));
    CHECK(hasProblem(registry, QStringLiteral("test.type"),
                     QStringLiteral("back: package 'test.other' has problems")));
    CHECK(hasProblem(registry, QStringLiteral("test.third"),
                     QStringLiteral("uses: package 'test.type' has problems")));
}

TEST_CASE("Book type packages: only a book type says whether a new book shows its parts",
          "[core][booktypes]") {
    PackageFolder folder;
    folder.writeTestPackages();
    QJsonObject base = testBase();
    base.insert(QStringLiteral("partsLayer"), false);
    folder.write(QStringLiteral("test.base"), QStringLiteral("booktype.json"), base);

    BookTypeRegistry registry;
    registry.load({folder.path()});
    INFO(problemsOf(registry));
    CHECK(registry.packages().isEmpty());
    CHECK(hasProblem(registry, QStringLiteral("test.base"),
                     QStringLiteral("partsLayer: only a book type says whether a new book shows "
                                    "its parts")));

    // A type without the field shows them
    PackageFolder valid;
    valid.writeTestPackages();
    registry.load({valid.path()});
    REQUIRE(registry.package(QStringLiteral("test.type")) != nullptr);
    CHECK(registry.package(QStringLiteral("test.type"))->partsLayer);
}

TEST_CASE("Book type packages: packages that use each other are not loaded",
          "[core][booktypes]") {
    PackageFolder folder;
    folder.writeTestPackages();
    QJsonObject base = testBase();
    base.insert(QStringLiteral("uses"), QJsonArray{QStringLiteral("test.type")});
    folder.write(QStringLiteral("test.base"), QStringLiteral("booktype.json"), base);

    BookTypeRegistry registry;
    registry.load({folder.path()});
    INFO(problemsOf(registry));
    CHECK(registry.packages().isEmpty());
    CHECK(hasProblem(registry, QStringLiteral("test.type"),
                     QStringLiteral("uses: package 'test.base' uses this package, directly or "
                                    "through others")));
    CHECK(hasProblem(registry, QStringLiteral("test.base"),
                     QStringLiteral("uses: package 'test.type' has problems")));
}

TEST_CASE("Book type packages: a kind of a used package inside a group the type replaced",
          "[core][booktypes]") {
    PackageFolder folder;
    QJsonObject type = testType();
    // "part" of the type is a text kind; the Base chapter can be inside a "part"
    setAt(type, QStringLiteral("kinds.part"), jsonOf(R"({
        "form": "text", "places": ["main"],
        "name": { "en": "Part" }, "plural": { "en": "Parts" } })"));
    folder.writeTestPackages(type);

    BookTypeRegistry registry;
    registry.load({folder.path()});
    INFO(problemsOf(registry));
    CHECK(registry.package(QStringLiteral("test.type")) == nullptr);
    CHECK(hasProblem(registry, QStringLiteral("test.type"),
                     QStringLiteral("main: kind 'chapter' can be inside 'part', which is not a "
                                    "group kind in this package")));
}

TEST_CASE("Book type packages: a manifest that is not JSON", "[core][booktypes]") {
    PackageFolder folder;
    folder.writeTestPackages();
    folder.write(QStringLiteral("test.type"), QStringLiteral("booktype.json"),
                 QByteArray("{ \"format\": 1, "));

    BookTypeRegistry registry;
    registry.load({folder.path()});
    INFO(problemsOf(registry));
    CHECK(registry.package(QStringLiteral("test.type")) == nullptr);
    CHECK(registry.package(QStringLiteral("test.base")) != nullptr);
    CHECK(hasProblem(registry, QStringLiteral("test.type"),
                     QStringLiteral("booktype.json: not valid JSON")));
}
